/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"
#include "abcom.h"
#include "abcinfo.h"
#include "prefapi.h"

#include "xp_mem.h"
#include "xpassert.h"
#include "xp_mcom.h"
#include "bkmks.h"
#include "vcc.h"
#include "vobject.h"
#include "xpgetstr.h"
#include "dirprefs.h"
#include "intl_csi.h"

#ifdef MOZ_LDAP
	#define NEEDPROTOS
	#define LDAP_REFERRALS
	#include "ldap.h"
	#include "ldif.h"
#endif /* MOZ_LDAP */

extern "C"
{
#if !defined(MOZADDRSTANDALONE)
extern int	MK_ADDR_FIRST_LAST_SEP;
extern int	MK_ADDR_LAST_FIRST_SEP;
extern int  MK_MSG_ENTRY_EXISTS_UPDATE;
extern int  MK_ADDR_UNABLE_TO_IMPORT;
#endif
}


static char * extractPropertyAndValue (char * sourceString, char ** property, char ** value)
{
	if (property)
		*property = NULL;
	if (value)
		*property = NULL;

	// extract the entire line first
	if (sourceString)
	{
		char * line = XP_STRTOK_R(nil, "\x0D\x0A", &sourceString);
		if (line) // we extracted a line
		{
			// now we want to tokenize off the : to extract the property
			char * prop = XP_STRTOK_R(nil, ":", &line);
			if (property)
				*property = prop;

			// what's left of the line is the value...should we filter white space? hmmm...
			if (value)
				*value = line;
		}
	}

	return sourceString;
}

AB_API int AB_ExportVCardToPrefs(const char * VCardString)
{
	const char * root= "mail.identity.vcard"; /* note to self...hide this string in an API so we can easily adapt this to multiple identities */
	const char * beginPhrase = "begin";  // i can't believe vobject.h doesn't have this as a #define...*sigh*....
	// parse each line in the vcard
	char * vcard = NULL;
	if (VCardString)
		vcard = XP_STRDUP(VCardString);

	char * property = NULL;
	char * value = NULL;

	while (vcard && *vcard != 0)
	{
		vcard = extractPropertyAndValue(vcard, &property, &value);
		// now turn any ; in property name to '.'
		if (property && (XP_STRNCASECMP(property,beginPhrase, XP_STRLEN(beginPhrase)) != 0)
			&& (XP_STRNCASECMP(property, VCEndProp, XP_STRLEN(VCEndProp)) != 0) )
		{
			char * marker = XP_STRCHR(property, ';');
			while (marker)
			{
				marker[0] = '.'; 
				marker = XP_STRCHR(property, ';');
			}
			char * prefString = PR_smprintf ("%s.%s", root, property);
			if (prefString && value)
				PREF_SetCharPref (prefString, value);
			XP_FREEIF(prefString);
		}
	}

	XP_FREEIF(vcard);

	return AB_SUCCESS;
}

static int addProperty(char ** currentVCard, const char * currentRoot, const char * mask /* part of the root to always ignore...i.e. mail.identity.vcard */)
{
	// keep in mind as we add properties that we want to filter out any begin and end vcard types....because
	// we add those automatically...

	const char * beginPhrase = "begin";
	char * children = NULL;
	int status = AB_SUCCESS;
	
	if (currentVCard)
	{
		if (PREF_CreateChildList(currentRoot, &children) == 0)
		{
			int index = 0;
			char * child = PREF_NextChild(children, &index);
			while (child)
			{
				// first iterate over the child in case the child has children...so make a recursive call
				addProperty(currentVCard, child, mask);

				// child length should be greater than the mask....
				if (XP_STRLEN(child) > XP_STRLEN(mask) + 1)  // + 1 for the '.' in .property
				{
					char * value = NULL;
					PREF_CopyCharPref(child, &value);
					if (mask)
						child += XP_STRLEN(mask) + 1;  // eat up the "mail.identity.vcard" part...
					// turn all '.' into ';' which is what vcard format uses
					char * marker = XP_STRCHR(child, '.');
					while (marker)
					{
						*marker = ';';
						marker = XP_STRCHR(child, '.');
					}

					// filter property to make sure it is one we want to add.....
					if ((XP_STRNCASECMP(child, beginPhrase, XP_STRLEN(beginPhrase)) != 0) && (XP_STRNCASECMP(child, VCEndProp, XP_STRLEN(VCEndProp)) != 0))
					{
						if (value && *value) // only add the value if it exists....and is not an empty string...
							if (*currentVCard)
							{
								char * tempString = *currentVCard;
								*currentVCard = PR_smprintf ("%s%s:%s%s", tempString, child, value, "\n");
								XP_FREEIF(tempString);
							}
							else
								*currentVCard = PR_smprintf ("%s:%s%s",child, value, "\n");
					}

					XP_FREEIF(value);
				}
				else
					XP_ASSERT(0); // I made a wrong assumption....hmmmm
				
				child = PREF_NextChild(children, &index);

			} // while

			XP_FREEIF(children);
		}  // if child list
	} // if currentVCard...

	return status;
}

/**********************************************************************************
	API to load a Vcard from prefs which reflects the user's identity....
**********************************************************************************/
			
AB_API int AB_LoadIdentityVCard(char ** IdentityVCard)
{
	const char * root= "mail.identity.vcard";
	char * vCardString = NULL;
	vCardString = XP_STRDUP("begin:vcard \n");

	addProperty(&vCardString, root, root);

	vCardString = PR_smprintf("%send:vcard\n", vCardString);

	// just to make sure we got everything right, load it into a vobject which puts it through a parser
	// and then read it back out as a vcard...if it succeeded, then we have a valid vcard..
	VObject * obj = Parse_MIME(vCardString, XP_STRLEN(vCardString));

	int len = 0;
	if (IdentityVCard && obj)
		*IdentityVCard = writeMemVObjects(0, &len, obj);

	XP_FREEIF(vCardString);
	return AB_SUCCESS;
}

/*************************************************************************************************************************

  New Address Book implementations for importing Vcards....once the address book is turned on for everyone, 
  we can get rid of all of the above import code 

**************************************************************************************************************************/

// prototypes for some helper functions....
static AB_AttribID getAttribIDForVObject(VObject * o);
static int convertVObject(XP_List * attributes, VObject * vObj);
static int convertNameValue(XP_List * attributes, VObject * vObj);
int AB_ConvertVObjectToAttribValues(VObject * vobj, AB_AttributeValue ** values, uint16 * numItems);

/*****************************************************************************************
  Iterate over each vcard object in the vcard string and add each object to the dest container.
  If the destination container does not accept new entries, then this method will not
  add the entries.

  The entries are added without UI.
*******************************************************************************************/
AB_API int AB_ImportVCards(AB_ContainerInfo * destContainer, const char * vCardString)
{
	int status = AB_SUCCESS;
	VObject *t, *v;
	if (destContainer /* ctr to add to */)
		if (destContainer->AcceptsNewEntries())
		{
			v = Parse_MIME(vCardString, XP_STRLEN(vCardString));
			t = v;
			while (v)  // while we have a vobject to import....
			{
				AB_AttributeValue * values = NULL;
				uint16 numItems = 0;

				AB_ConvertVObjectToAttribValues(v, &values, &numItems);
				if (numItems) // did we actually have any attributes to convert??
					AB_AddUserAB2(destContainer, values, numItems, NULL);

				AB_FreeEntryAttributeValues(values, numItems);
				v = nextVObjectInList(v);
			}

			cleanVObject(t);

		}
		else
			status = AB_INVALID_CONTAINER;
	else
		status = ABError_NullPointer;

	return status;
}


AB_API int AB_ConvertVCardToAttribValues(const char * vCard, AB_AttributeValue ** values, uint16 * numItems)
{
	VObject *t, *v;
	v = Parse_MIME(vCard, XP_STRLEN(vCard));
	t = v;
	AB_ConvertVObjectToAttribValues(v, values, numItems);
	cleanVObject(t);

	return AB_SUCCESS;
}

/****************************************************************************
  This function allocates an array of attribute values which reflect the vCard. 
  The caller is responsible for freeing this array using AB_FreeEntryAttributes
******************************************************************************/
int AB_ConvertVObjectToAttribValues(VObject * vObj, AB_AttributeValue ** values, uint16 * numItems)
{
	uint16 numAttributes = 0;
	AB_AttributeValue * valuesArray = NULL;
	int status = AB_SUCCESS;

	XP_List * attributes = XP_ListNew(); 

	convertVObject(attributes, vObj);

	// now turn the list into an array of attribute values....
	numAttributes = XP_ListCount(attributes);
	if (numAttributes)
	{
		valuesArray = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
		if (valuesArray)
		{
			// iterate through the list adding each attribute to our static array
			for (uint16 index = 1; index <= numAttributes; index++)
			{
				AB_AttributeValue * value = (AB_AttributeValue *) XP_ListGetObjectNum(attributes, index);
				AB_CopyEntryAttributeValue(value, &valuesArray[index-1]);
			}
		}
		else
			status = ABError_NullPointer;
	}

	// now free the attributes in our list
	
	if (attributes)
	{
		for (int i = 1; i <= XP_ListCount(attributes); i++)
		{
			AB_AttributeValue * value = (AB_AttributeValue *) XP_ListGetObjectNum(attributes, i);
			AB_FreeEntryAttributeValue(value);

		}

		XP_ListDestroy (attributes);
	}
	
	if (numItems)
		*numItems = numAttributes;
	if (values)
		*values = valuesArray;
	else
		AB_FreeEntryAttributeValues(valuesArray, numAttributes);

	return status;
}

/*************************************************************************

	Iterate over each VObject, adding its properties to the attributes list 

******************************************************************************/
static int convertVObject(XP_List * attributes, VObject * vObj)
{
	if (vObj && attributes)
	{
		VObjectIterator t;
		convertNameValue(attributes, vObj);
		initPropIterator(&t, vObj);
		while (moreIteration(&t))
		{
			VObject * nextObject = nextVObject(&t);
			convertVObject(attributes, nextObject);
		}
	}

	return AB_SUCCESS;
}

/********************************************************************
	Pass in an attribute value and a vobject, we'll fill in the attribute
	value with the correct attribute and data field. 
*********************************************************************/
static int convertNameValue(XP_List * attributes, VObject * vObj)
{
	int status = AB_SUCCESS;
	char * tempString = NULL; 

	if (attributes && vObjectName(vObj))
	{
		// most just fill in one attribute value so let's create one here....
		AB_AttributeValue * value = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (value)
		{
			AB_AttribID attrib = getAttribIDForVObject(vObj);
			if (attrib != AB_attribUnknown) // if it is a unknown attribute then the vcard prop must not be a root property
			{
				// it must be a root property...we know how to handle those
				value->attrib = attrib;
				switch(attrib)
				{
					/* ints */
					case AB_attribWinCSID:
					case AB_attribUseServer:
						if (VALUE_TYPE(vObj))
							value->u.shortValue = vObjectIntegerValue(vObj);
						else
							AB_CopyDefaultAttributeValue(attrib, value);
						break;
					/* bools */
					case AB_attribHTMLMail:
						if (VALUE_TYPE(vObj))
							tempString = fakeCString(vObjectUStringZValue(vObj));
						if (tempString)
						{
							if (XP_STRCASECMP(tempString, "TRUE") == 0)
								value->u.boolValue = TRUE;
							else
								value->u.boolValue = FALSE;
							XP_FREE(tempString);
						}
						else 
							AB_CopyDefaultAttributeValue(attrib, value);
						break;
					
					/* strings */
					case AB_attribInfo:
					case AB_attribFullName:
					case AB_attribGivenName:
					case AB_attribFamilyName:
					case AB_attribCompanyName:
					case AB_attribLocality:
					case AB_attribRegion:
					case AB_attribEmailAddress:
					case AB_attribTitle:
					case AB_attribPOAddress:
					case AB_attribStreetAddress:
					case AB_attribZipCode:
					case AB_attribCountry:
					case AB_attribWorkPhone:
					case AB_attribFaxPhone:
					case AB_attribHomePhone:
					case AB_attribCoolAddress:
					case AB_attribCellularPhone:
					case AB_attribDisplayName:
						value->u.string = NULL;
						if (VALUE_TYPE(vObj))
							value->u.string = fakeCString (vObjectUStringZValue(vObj));
						if (value->u.string == NULL) // assignments were not successful
							AB_CopyDefaultAttributeValue(attrib, value);
						break;
					default:
						status = ABError_InvalidAttribute;
						break;
				}

				if (status == AB_SUCCESS) // if it is good, then add it to the list else free the memory
					XP_ListAddObjectToEnd(attributes, (void *) value);
				else
					XP_FREE(value);
			}
			else
				XP_FREE(value); // unknown attribute....
		}
		else
			status = AB_OUT_OF_MEMORY;
	}
	else
		status = ABError_NullPointer; 
	return status;
}

/* VCard importing helper function to determine an address book AttribID foe the vcard property */
static AB_AttribID getAttribIDForVObject(VObject * o)
{
	// WARNING: if the VCard property is not a root property then we need to determine its exact property.
	// a good example of this is VCTelephoneProp, this prop has four objects underneath it:
	// fax, work and home and cellular.
	if (XP_STRCASECMP (VCCityProp, vObjectName(o)) == 0)
		return AB_attribLocality;
	if (XP_STRCASECMP (VCTelephoneProp, vObjectName(o)) == 0)
	{
		VObject* workprop = isAPropertyOf(o, VCWorkProp);
		VObject* faxprop = isAPropertyOf(o, VCFaxProp);
		VObject* homeprop = isAPropertyOf(o, VCHomeProp);
		VObject * cellphone = isAPropertyOf(o, VCCellularProp);
		if (faxprop)
			return AB_attribFaxPhone;
		else
			if (workprop)
				return AB_attribWorkPhone;
		else
			if (homeprop)
				return (AB_attribHomePhone);
		else
			if (cellphone)
				return AB_attribCellularPhone;
		else
			return AB_attribUnknown;
	}

	if (XP_STRCASECMP (VCEmailAddressProp, vObjectName(o)) == 0)
	{	
		// only treat it as a match if it is an internet property
		VObject* iprop = isAPropertyOf(o, VCInternetProp);
		if (iprop)
			return AB_attribEmailAddress;
		else
			return AB_attribUnknown;
	}

	if (XP_STRCASECMP (VCFamilyNameProp, vObjectName(o)) == 0) 
		return AB_attribFamilyName;
	if (XP_STRCASECMP (VCFullNameProp, vObjectName(o)) == 0)
		return AB_attribDisplayName;
	if (XP_STRCASECMP (VCGivenNameProp, vObjectName(o)) == 0)
		return AB_attribGivenName;

	if (XP_STRCASECMP (VCOrgNameProp, vObjectName(o)) == 0) 
		return AB_attribCompanyName;

	if (XP_STRCASECMP (VCPostalCodeProp, vObjectName(o)) == 0) 
		return AB_attribZipCode;
	if (XP_STRCASECMP (VCRegionProp, vObjectName(o)) == 0)
		return AB_attribRegion;
	if (XP_STRCASECMP (VCStreetAddressProp, vObjectName(o)) == 0)
		return AB_attribStreetAddress;
	if (XP_STRCASECMP (VCPostalBoxProp, vObjectName(o)) == 0) 
		return AB_attribPOAddress;
	if (XP_STRCASECMP (VCCountryNameProp, vObjectName(o)) == 0)
		return AB_attribCountry;

	if (XP_STRCASECMP (VCTitleProp, vObjectName(o)) == 0) 
		return AB_attribTitle;

	if (XP_STRCASECMP (VCCooltalkAddress, vObjectName(o)) == 0)
		return AB_attribCoolAddress;
	if (XP_STRCASECMP (VCUseServer, vObjectName(o)) == 0) 
		return AB_attribUseServer;

	if (XP_STRCASECMP (VCUseHTML, vObjectName(o)) == 0) 
		return AB_attribHTMLMail;

	if (XP_STRCASECMP (VCNoteProp, vObjectName(o)) == 0) 
		return AB_attribInfo;

	if (XP_STRCASECMP(VCCellularProp,vObjectName(o)) == 0)
		return AB_attribCellularPhone;
	
	if (XP_STRCASECMP(VCCIDProp,vObjectName(o)) == 0)
		return AB_attribWinCSID;


	// if we got this far, then the object isn't a type we support
	return AB_attribUnknown;
}


/************************************************************************************************

	OLD ADDRESS BOOK IMPORT CODE WHICH CAN BE REMOVED ONCE EVERYONE IS BUILDING THE 
	NEW ADDRESS BOOK.....


***************************************************************************************************/

struct BM_Entry_struct
{
  BM_Type type;

  uint16 flags;
  BM_Entry*     next;
  BM_Entry*     parent;
  char* name;
  char* description;
  BM_Date addition_date;
  char* nickname;                               /* Used only by address book, alas. */

  union {
	struct BM_Header {
	  BM_Entry*     children;                       /* a linked list of my children */
	  uint32 childCount;                    /* the number of "children" */
	  BM_Entry*     lastChild;                      /* the last child in "children" */
	} header;
	struct BM_Url {
	  char* address;                        /* address */
	  char* content_type;           /* content-type */
	  BM_Date last_visit;           /* when last visited */
	  BM_Date last_modified;        /* when the contents of the URL was last
								   modified. */
	} url;
	struct BM_Address {
	  char* address;                        /* e-mail address */
	} address;
	struct BM_Alias {
	  BM_Entry*     original;               /* the original bm */
	} alias;
  } d;

};

#define READ_BUFFER_SIZE                2048

#define BM_DESCRIPTION          0xC000
#define BM_HEADER_BEGIN         0xD000
#define BM_HEADER_END           0xE000
#define BM_UNKNOWN                      0xF000

