/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "mimevcrd.h"
#include "xpgetstr.h"
#include "xp_core.h"
#include "xp_mem.h"
#include "vcc.h"
#include "vobject.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextVCard, MimeInlineTextVCardClass,
			 mimeInlineTextVCardClass, &MIME_SUPERCLASS);

static int MimeInlineTextVCard_parse_line (char *, int32, MimeObject *);
static int MimeInlineTextVCard_parse_eof (MimeObject *, XP_Bool);
static int MimeInlineTextVCard_parse_begin (MimeObject *obj);

static int unique = 0;

extern int MK_OUT_OF_MEMORY;

extern int MK_LDAP_COMMON_NAME;   
extern int MK_LDAP_FAX_NUMBER;    
extern int MK_LDAP_GIVEN_NAME;    
extern int MK_LDAP_LOCALITY;      
extern int MK_LDAP_PHOTOGRAPH;    
extern int MK_LDAP_EMAIL_ADDRESS; 
extern int MK_LDAP_MANAGER;       
extern int MK_LDAP_ORGANIZATION;  
extern int MK_LDAP_OBJECT_CLASS;  
extern int MK_LDAP_ORG_UNIT;      
extern int MK_LDAP_POSTAL_ADDRESS;
extern int MK_LDAP_SECRETARY;     
extern int MK_LDAP_SURNAME;       
extern int MK_LDAP_STREET;        
extern int MK_LDAP_PHONE_NUMBER;  
extern int MK_LDAP_CAR_LICENSE;
extern int MK_LDAP_BUSINESS_CAT;
extern int MK_LDAP_DEPT_NUMBER;
extern int MK_LDAP_DESCRIPTION;
extern int MK_LDAP_EMPLOYEE_TYPE;
extern int MK_LDAP_POSTAL_CODE;
extern int MK_LDAP_TITLE;
extern int MK_LDAP_REGION;
extern int MK_LDAP_DOM_TYPE;
extern int MK_LDAP_INTL_TYPE;
extern int MK_LDAP_POSTAL_TYPE;
extern int MK_LDAP_PARCEL_TYPE;
extern int MK_LDAP_WORK_TYPE;
extern int MK_LDAP_HOME_TYPE;
extern int MK_LDAP_PREF_TYPE;
extern int MK_LDAP_VOICE_TYPE;
extern int MK_LDAP_FAX_TYPE;
extern int MK_LDAP_MSG_TYPE;
extern int MK_LDAP_CELL_TYPE;
extern int MK_LDAP_PAGER_TYPE;
extern int MK_LDAP_BBS_TYPE;
extern int MK_LDAP_MODEM_TYPE;
extern int MK_LDAP_CAR_TYPE;
extern int MK_LDAP_ISDN_TYPE;
extern int MK_LDAP_VIDEO_TYPE;
extern int MK_LDAP_AOL_TYPE;
extern int MK_LDAP_APPLELINK_TYPE;
extern int MK_LDAP_ATTMAIL_TYPE;
extern int MK_LDAP_CSI_TYPE;
extern int MK_LDAP_EWORLD_TYPE;
extern int MK_LDAP_INTERNET_TYPE;
extern int MK_LDAP_IBMMAIL_TYPE;
extern int MK_LDAP_MCIMAIL_TYPE;
extern int MK_LDAP_POWERSHARE_TYPE;
extern int MK_LDAP_PRODIGY_TYPE;
extern int MK_LDAP_TLX_TYPE;
extern int MK_LDAP_MIDDLE_NAME;
extern int MK_LDAP_NAME_PREFIX;
extern int MK_LDAP_NAME_SUFFIX;
extern int MK_LDAP_TZ;
extern int MK_LDAP_GEO;
extern int MK_LDAP_SOUND;
extern int MK_LDAP_REVISION;
extern int MK_LDAP_VERSION;
extern int MK_LDAP_KEY;
extern int MK_LDAP_LOGO;
extern int MK_LDAP_X400;
extern int MK_LDAP_BIRTHDAY;
extern int MK_LDAP_ADDRESS;
extern int MK_LDAP_LABEL;
extern int MK_LDAP_MAILER;
extern int MK_LDAP_ROLE;
extern int MK_LDAP_UPDATEURL;
extern int MK_LDAP_COOLTALKADDRESS;
extern int MK_LDAP_USEHTML;
extern int MK_ADDR_VIEW_COMPLETE_VCARD;
extern int MK_ADDR_VIEW_CONDENSED_VCARD;
extern int MK_MSG_ADD_TO_ADDR_BOOK;
extern int MK_ADDR_DEFAULT_DLS;
extern int MK_ADDR_SPECIFIC_DLS;
extern int MK_ADDR_HOSTNAMEIP;
extern int MK_ADDR_CONFINFO;
extern int MK_ADDR_ADDINFO;

static int BeginVCard (MimeObject *obj);
static int EndVCard (MimeObject *obj);
static int WriteOutVCard (MimeObject *obj, VObject* v);
static int WriteOutEachVCardProperty (MimeObject *obj, VObject* v, int* numEmail);
static int WriteOutVCardProperties (MimeObject *obj, VObject* v, int* numEmail);
static int WriteLineToStream (MimeObject *obj, const char *line);

static void GetEmailProperties (VObject* o, char ** attribName);
static void GetTelephoneProperties (VObject* o, char ** attribName);
static void GetAddressProperties (VObject* o, char ** attribName);
static int WriteValue (MimeObject *obj, const char *);
static int WriteAttribute (MimeObject *obj, const char *);
static int WriteOutVCardPhoneProperties (MimeObject *obj, VObject* v);
static int WriteOutEachVCardPhoneProperty (MimeObject *obj, VObject* o);

typedef struct
	{
		const char *attributeName;
		int resourceId;
	} AttributeName;

#define kNumAttributes 12

static int
MimeInlineTextVCardClassInitialize(MimeInlineTextVCardClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;
  XP_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextVCard_parse_begin;
  oclass->parse_line  = MimeInlineTextVCard_parse_line;
  oclass->parse_eof   = MimeInlineTextVCard_parse_eof;

return 0;
}

static int
MimeInlineTextVCard_parse_begin (MimeObject *obj)
{
    int status = ((MimeObjectClass*)&mimeLeafClass)->parse_begin(obj);
	MimeInlineTextVCardClass *class;
    if (status < 0) return status;

    if (!obj->output_p) return 0;
    if (!obj->options || !obj->options->write_html_p) return 0;

    /* This is a fine place to write out any HTML before the real meat begins.
       In this sample code, we tell it to start a table. */

	class = ((MimeInlineTextVCardClass *) obj->class);
	/* initialize vcard string to empty; */
	StrAllocCopy(class->vCardString, "");

    return 0;
}


static int
MimeInlineTextVCard_parse_line (char *line, int32 length, MimeObject *obj)
{
    /* This routine gets fed each line of data, one at a time.  In my
       sample, I spew it out as a table row, putting everything
       between colons in its own table cell.*/

	char* linestring;
	MimeInlineTextVCardClass *class = ((MimeInlineTextVCardClass *) obj->class);
 
    if (!obj->output_p) return 0;
    if (!obj->options || !obj->options->output_fn) return 0;
    if (!obj->options->write_html_p) {
		return MimeObject_write(obj, line, length, TRUE);
    }

	linestring = (char *) XP_ALLOC (length + 1);

	if (linestring) {
		XP_STRNCPY_SAFE((char *)linestring, line, length + 1);
		StrAllocCat (class->vCardString, linestring);
		XP_FREE (linestring);
	}

    return 0;
}


static int
MimeInlineTextVCard_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
    int status = 0;
	MimeInlineTextVCardClass *class = ((MimeInlineTextVCardClass *) obj->class);
	VObject *t, *v;

    if (obj->closed_p) return 0;

    /* Run parent method first, to flush out any buffered data. */
    status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
    if (status < 0) return status;

	if (!class->vCardString) return 0;

	v = Parse_MIME(class->vCardString, XP_STRLEN(class->vCardString));

	if (class->vCardString) {
		XP_FREE ((char*) class->vCardString);
		class->vCardString = NULL;
	}

    if (obj->output_p && obj->options && obj->options->write_html_p) {
		/* This is a fine place to write any closing HTML.  In fact, you may
		   want all the writing to be here, and all of the above would just
		   collect data into datastructures, though that isn't very
		   "streaming". */
		t = v;
		while (v && status >= 0) {
			/* write out html */
			status = WriteOutVCard (obj, v);
			/* parse next vcard incase they're embedded */
			v = nextVObjectInList(v);
		}

	    cleanVObject(t);
    }
	if (status < 0) return status;

    return 0;
}

static int WriteEachLineToStream (MimeObject *obj, const char *line)
{
	int status = 0;
	char *htmlLine;
	int htmlLen = XP_STRLEN(line) + 1;

	htmlLine = (char *) XP_ALLOC (htmlLen);
	if (htmlLine)
	{
		htmlLine[0] = '\0';
		XP_STRCAT (htmlLine, line);
	    status = MimeObject_write(obj, htmlLine, XP_STRLEN(htmlLine), TRUE);
#ifdef DEBUG_mwatkins
		if (XP_STRLEN (htmlLine) < 500)
			XP_Trace("%s%c", htmlLine, '\n');
#endif
		XP_FREE ((void*) htmlLine);
	}
	else
		status = MK_OUT_OF_MEMORY;

	return status;
}

static int OutputTable (MimeObject *obj, XP_Bool endTable, XP_Bool border, char *cellspacing, char *cellpadding, char *bgcolor)
{
	int status = 0;
	char * htmlLine = NULL;

	if (endTable)
	{
		status = WriteEachLineToStream (obj, "</TABLE>");
	}
	else
	{
		int htmlLen = XP_STRLEN("<TABLE>") + 1;
		if (border)
			htmlLen += XP_STRLEN (" BORDER");
		if (cellspacing)
			htmlLen += XP_STRLEN(" CELLSPACING=") + XP_STRLEN(cellspacing);
		if (cellpadding)
			htmlLen += XP_STRLEN(" CELLPADDING=") + XP_STRLEN(cellpadding);
		if (bgcolor)
			htmlLen += XP_STRLEN(" BGCOLOR=") + XP_STRLEN(bgcolor);
		if (border || cellspacing || cellpadding || bgcolor)
			htmlLen++;

		htmlLine = (char *) XP_ALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
			XP_STRCAT (htmlLine, "<TABLE");
			if (border)
				XP_STRCAT (htmlLine, " BORDER");
			if (cellspacing)
			{	
				XP_STRCAT (htmlLine, " CELLSPACING=");
				XP_STRCAT (htmlLine, cellspacing);
			}
			if (cellpadding)
			{	
				XP_STRCAT (htmlLine, " CELLPADDING=");
				XP_STRCAT (htmlLine, cellpadding);
			}
			if (bgcolor)
			{	
				XP_STRCAT (htmlLine, " BGCOLOR=");
				XP_STRCAT (htmlLine, bgcolor);
			}

			if (border || cellspacing || cellpadding || bgcolor)
				XP_STRCAT (htmlLine, " ");

			XP_STRCAT (htmlLine, ">");

			status = MimeObject_write(obj, htmlLine, XP_STRLEN(htmlLine), TRUE);
#ifdef DEBUG_mwatkins
			XP_Trace("%s%c", htmlLine, '\n');
#endif
			XP_FREE ((void*) htmlLine);
		}
		else
			status = MK_OUT_OF_MEMORY;
	}	
	return status;
}

static int OutputTableRowOrData(MimeObject *obj, XP_Bool outputRow, 
								XP_Bool end, char * align, 
								char* valign, char* colspan,
								char* width)
{
	int status = 0;
	char * htmlLine = NULL;

	if (end)
		if (outputRow)
			status = WriteEachLineToStream (obj, "</TR>");
		else
			status = WriteEachLineToStream (obj, "</TD>");
	else
	{
		int htmlLen = XP_STRLEN("<TR>") + 1;
		if (align)
			htmlLen += XP_STRLEN(" ALIGN=") + XP_STRLEN(align);
		if (colspan)
			htmlLen += XP_STRLEN(" COLSPAN=") + XP_STRLEN(colspan);
		if (width)
			htmlLen += XP_STRLEN(" WIDTH=") + XP_STRLEN(width);
		if (valign)
			htmlLen += XP_STRLEN(" VALIGN=") + XP_STRLEN(valign);
		if (align || valign || colspan || width)
			htmlLen++;

		htmlLine = (char *) XP_ALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
			if (outputRow)
				XP_STRCAT (htmlLine, "<TR");
			else
				XP_STRCAT (htmlLine, "<TD");
			if (align)
			{	
				XP_STRCAT (htmlLine, " ALIGN=");
				XP_STRCAT (htmlLine, align);
			}
			if (valign)
			{	
				XP_STRCAT (htmlLine, " VALIGN=");
				XP_STRCAT (htmlLine, valign);
			}
			if (colspan)
			{	
				XP_STRCAT (htmlLine, " COLSPAN=");
				XP_STRCAT (htmlLine, colspan);
			}
			if (width)
			{	
				XP_STRCAT (htmlLine, " WIDTH=");
				XP_STRCAT (htmlLine, width);
			}
			if (align || valign || colspan || width)
				XP_STRCAT (htmlLine, " ");

			XP_STRCAT (htmlLine, ">");

			status = MimeObject_write(obj, htmlLine, XP_STRLEN(htmlLine), TRUE);
#ifdef DEBUG_mwatkins
			XP_Trace("%s%c", htmlLine, '\n');
#endif
			XP_FREE ((void*) htmlLine);
		}
		else
			status = MK_OUT_OF_MEMORY;
	}

	return status;
}


static int OutputFont(MimeObject *obj, XP_Bool endFont, char * size, char* color)
{
	int status = 0;
	char * htmlLine = NULL;

	if (endFont)
		status = WriteEachLineToStream (obj, "</FONT>");
	else
	{
		int htmlLen = XP_STRLEN("<FONT>") + 1;
		if (size)
			htmlLen += XP_STRLEN(" SIZE=") + XP_STRLEN(size);
		if (color)
			htmlLen += XP_STRLEN(" COLOR=") + XP_STRLEN(color);
		if (size || color)
			htmlLen++;

		htmlLine = (char *) XP_ALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
				XP_STRCAT (htmlLine, "<FONT");
			if (size)
			{	
				XP_STRCAT (htmlLine, " SIZE=");
				XP_STRCAT (htmlLine, size);
			}
			if (color)
			{	
				XP_STRCAT (htmlLine, " COLOR=");
				XP_STRCAT (htmlLine, color);
			}
			if (size || color)
				XP_STRCAT (htmlLine, " ");

			XP_STRCAT (htmlLine, ">");

			status = MimeObject_write(obj, htmlLine, XP_STRLEN(htmlLine), TRUE);
#ifdef DEBUG_mwatkins
			XP_Trace("%s%c", htmlLine, '\n');
#endif
			XP_FREE ((void*) htmlLine);
		}
		else
			status = MK_OUT_OF_MEMORY;
	}

	return status;
}

static int OutputVcardAttribute(MimeObject *obj, VObject *v, const char* id) 
{
	int status = 0;
	VObject *prop = NULL;
	char *string = NULL;

	prop = isAPropertyOf(v, id);
	if (prop)
		if (VALUE_TYPE(prop))
		{
			string = fakeCString (vObjectUStringZValue(prop));
			if (string) {
				status = OutputFont(obj, FALSE, "-1", NULL);
				if (status < 0) {
					XP_FREEIF (string);
					return status;
				}
				status = WriteLineToStream (obj, string);
				XP_FREEIF (string);
				if (status < 0) return status;
				status = OutputFont(obj, TRUE, NULL, NULL);
				if (status < 0) return status;
			}
		}

	return 0;
}


static int OutputBasicVcard(MimeObject *obj, VObject *v)
{
	int status = 0;
	char * htmlLine1 = NULL;
	char * htmlLine2 = NULL;
	char * htmlLine = NULL;
	VObject *prop = NULL;
	VObject* prop2 = NULL;
	char * urlstring = NULL;
	char * namestring = NULL;
	char * emailstring = NULL;

	/* get the name and email */
	prop = isAPropertyOf(v, VCFullNameProp);
	if (prop)
	{
		if (VALUE_TYPE(prop))
		{
			namestring  = fakeCString (vObjectUStringZValue(prop));
			if (namestring)
			{
				prop = isAPropertyOf(v, VCURLProp);
				if (prop)
				{
					urlstring  = fakeCString (vObjectUStringZValue(prop));
					if (urlstring)
						htmlLine1 = PR_smprintf ("<A HREF=""%s"">%s</A> ", urlstring, namestring);
					else
						htmlLine1 = PR_smprintf ("%s ", namestring);
					XP_FREEIF (urlstring);
				}
				else
					htmlLine1 = PR_smprintf ("%s ", namestring);

				/* get the email address */
				prop = isAPropertyOf(v, VCEmailAddressProp);
				if (prop)
				{
					emailstring  = fakeCString (vObjectUStringZValue(prop));
					if (emailstring)
					{
						/* if its an internet address prepend the mailto url */
						prop2 = isAPropertyOf(prop, VCInternetProp);
						if (prop2)
							htmlLine2 = PR_smprintf ("&lt;<A HREF=""mailto:%s"">%s</A>&gt;", emailstring, emailstring);
						else
							htmlLine2 = PR_smprintf ("%s", emailstring);
						XP_FREEIF (emailstring);
					}
				}

				if (!htmlLine1 && !htmlLine2)
				{
					XP_FREEIF (htmlLine1);
					XP_FREEIF (htmlLine2);
					return MK_OUT_OF_MEMORY;
				}
				else
				{
					htmlLine = StrAllocCat (htmlLine, htmlLine1);
					htmlLine = StrAllocCat (htmlLine, htmlLine2);
				}

				XP_FREEIF (htmlLine1);
				XP_FREEIF (htmlLine2);
				XP_FREEIF (namestring);
			}
		}
	}

	status = OutputTable (obj, FALSE, FALSE, "0", "0", NULL);
	if (status < 0) {
		XP_FREEIF (htmlLine); 
		return status;
	}
	if (htmlLine)
	{
		status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
		if (status < 0) {
			XP_FREE (htmlLine);
			return status;
		}
		status = OutputTableRowOrData (obj, FALSE, FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) {
			XP_FREE (htmlLine);
			return status;
		}

		status = WriteLineToStream (obj, htmlLine);
		XP_FREE (htmlLine); 
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}
	else
		status = MK_OUT_OF_MEMORY;

	status = OutputTableRowOrData(obj, TRUE, FALSE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, FALSE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	/* output the title */
	status = OutputVcardAttribute (obj, v, VCTitleProp);
	if (status < 0) return status;
	/* write out the org name and company name */
	prop = isAPropertyOf(v, VCOrgProp);
	if (prop)
	{
		status = OutputVcardAttribute (obj, prop, VCOrgNameProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnitProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnit2Prop);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnit3Prop);
		if (status < 0) return status;
	}
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	return 0;
}

static int OutputAdvancedVcard(MimeObject *obj, VObject *v)
{
	int status = 0;
	char * htmlLine1 = NULL;
	char * htmlLine2 = NULL;
	VObject *prop = NULL;
	VObject* prop2 = NULL;
	char * urlstring = NULL;
	char * namestring = NULL;
	char * emailstring = NULL;
	int numEmail = 0;

	status = OutputTable (obj, FALSE, FALSE, "0", "0", NULL);
	if (status < 0) return status;
	/* beginning of first row */
	status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", "3", NULL);
	if (status < 0) return status;

	/* get the name and email */
	prop = isAPropertyOf(v, VCFullNameProp);
	if (prop)
	{
		if (VALUE_TYPE(prop))
		{
			namestring  = fakeCString (vObjectUStringZValue(prop));
			if (namestring)
			{
				prop = isAPropertyOf(v, VCURLProp);
				if (prop)
				{
					urlstring  = fakeCString (vObjectUStringZValue(prop));
					if (urlstring)
						htmlLine1 = PR_smprintf ("<A HREF=""%s"">%s</A> ", urlstring, namestring);
					else
						htmlLine1 = PR_smprintf ("%s ", namestring);
					XP_FREEIF (urlstring);
				}
				else
					htmlLine1 = PR_smprintf ("%s ", namestring);

				XP_FREEIF (namestring);
			}
			if (!htmlLine1)
			{
				return MK_OUT_OF_MEMORY;
			}
		}
	}
	/* output the name if there was one */
	if (htmlLine1)
	{
		status = WriteLineToStream (obj, htmlLine1);
		XP_FREEIF (htmlLine1);
		if (status < 0) return status;
	}

	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	
	/* beginning of second row */
	status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	/* output the title */
	status = OutputVcardAttribute (obj, v, VCTitleProp);
	if (status < 0) return status;
	/* write out the org name and company name */
	prop = isAPropertyOf(v, VCOrgProp);
	if (prop)
	{
		status = OutputVcardAttribute (obj, prop, VCOrgNameProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnitProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnit2Prop);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCOrgUnit3Prop);
		if (status < 0) return status;
	}
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, FALSE, FALSE , "LEFT", "TOP", NULL, "\"10\"");
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;

	/* get the email address */
	prop = isAPropertyOf(v, VCEmailAddressProp);
	if (prop)
	{
		emailstring  = fakeCString (vObjectUStringZValue(prop));
		if (emailstring)
		{
			numEmail++;
			/* if its an internet address prepend the mailto url */
			prop2 = isAPropertyOf(prop, VCInternetProp);
			if (prop2)
				htmlLine2 = PR_smprintf ("&lt;<A HREF=""mailto:%s"">%s</A>&gt;", emailstring, emailstring);
			else
				htmlLine2 = PR_smprintf ("%s", emailstring);

			XP_FREEIF (emailstring);
		}
		if (!htmlLine2)
		{
			return MK_OUT_OF_MEMORY;
		}
	}
	/* output email address */
	if (htmlLine2)
	{
		status = OutputFont(obj, FALSE, "-1", NULL);
		if (status < 0) {
			XP_FREEIF (htmlLine2);
			return status;
		}
		status = WriteLineToStream (obj, htmlLine2);
		XP_FREEIF (htmlLine2);
		if (status < 0) return status;
		status = OutputFont(obj, TRUE, NULL, NULL);
		if (status < 0) return status;
		/* output html mail setting only if its true */
		prop = isAPropertyOf(v, VCUseHTML);
		if (prop)
		{
			if (VALUE_TYPE(prop))
			{
				namestring  = fakeCString (vObjectUStringZValue(prop));
				if (namestring)
					if (strcasecomp (namestring, "TRUE") == 0)
					{
						XP_FREEIF (namestring);
						status = OutputFont(obj, FALSE, "-1", NULL);
						if (status < 0) return status;
						status = WriteLineToStream (obj, XP_GetString (MK_LDAP_USEHTML));
						if (status < 0) return status;
						status = OutputFont(obj, TRUE, NULL, NULL);
						if (status < 0) return status;
					}
					else
						XP_FREEIF (namestring);
			}
		}
	}
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* beginning of third row */
	/* write out address information if we have any */
	status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	/* first column */
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	prop = isAPropertyOf(v, VCAdrProp);
	if (prop)
	{
		status = OutputVcardAttribute (obj, prop, VCPostalBoxProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCExtAddressProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCStreetAddressProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCCityProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCRegionProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCPostalCodeProp);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, prop, VCCountryNameProp);
		if (status < 0) return status;
	}
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* second column */
	status = OutputTableRowOrData (obj, FALSE, FALSE , NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* third column */
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	/* output telephone fields */
	status = WriteOutVCardPhoneProperties (obj, v);
	if (status < 0) return status;
	/* output conference fields */
	status = OutputFont(obj, FALSE, "-1", NULL);
	if (status < 0) return status;
	prop = isAPropertyOf(v, VCCooltalk);
	if (prop)
	{
		WriteLineToStream (obj, XP_GetString (MK_ADDR_CONFINFO));
		if (status < 0) return status;
		prop2 = isAPropertyOf(prop, VCUseServer);
		if (prop2)
		{
			if (VALUE_TYPE(prop2)) {
				namestring  = fakeCString (vObjectUStringZValue(prop2));
				if (XP_STRCMP (namestring, "0") == 0)
					status = WriteLineToStream (obj, XP_GetString (MK_ADDR_DEFAULT_DLS));
				else {
					if (XP_STRCMP (namestring, "1") == 0)
						status = WriteLineToStream (obj, XP_GetString (MK_ADDR_SPECIFIC_DLS));
					else
						if (XP_STRCMP (namestring, "2") == 0)
							status = WriteLineToStream (obj, XP_GetString (MK_ADDR_HOSTNAMEIP));
				}
				XP_FREEIF (namestring);
				if (status < 0) return status;
			}
		}
		status = OutputVcardAttribute (obj, prop, VCCooltalkAddress);
		if (status < 0) return status;
	}

	status = OutputFont(obj, TRUE, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* beginning of last row */
	/* output notes field */
	prop = isAPropertyOf(v, VCCommentProp);
	if (prop)
	{
		status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", "3", NULL);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, v, VCCommentProp);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}

	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	/* output second table containing all the additional info */
	status = OutputTable (obj, FALSE, FALSE, "0", "0", NULL);
	if (status < 0) return status;
	/* beginning of first row */
	status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", "2", NULL);
	if (status < 0) return status;
	/* output the additional info header */
	status = OutputFont(obj, FALSE, "-1", NULL);
	if (status < 0) return status;
	status = WriteLineToStream (obj, XP_GetString (MK_ADDR_ADDINFO));
	if (status < 0) return status;
	status = OutputFont(obj, TRUE, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	
	/* beginning of remaining rows */
	status = WriteOutVCardProperties (obj, v, &numEmail);
	if (status < 0) return status;

	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	return 0;
}

static int OutputButtons(MimeObject *obj, XP_Bool basic, VObject *v)
{
	int status = 0;
	char * htmlLine = NULL;
	char * htmlLine1 = NULL;
	char * htmlLine2 = NULL;
	char* vCard = NULL;
	char* vEscCard = NULL;
	int len = 0;

	if (!obj->options->output_vcard_buttons_p)
		return status;

	vCard = writeMemVObjects(0, &len, v);

	if (!vCard)
		return MK_OUT_OF_MEMORY;

	vEscCard = NET_Escape (vCard, URL_XALPHAS);

	XP_FREEIF (vCard);

	if (!vEscCard)
		return MK_OUT_OF_MEMORY;

	if (basic)
	{
		htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showAdvanced%d();\\\"></INPUT></FORM>", 
			XP_GetString(MK_ADDR_VIEW_COMPLETE_VCARD), unique);
		htmlLine2 = PR_smprintf ("<FORM name=form1 METHOD=get ACTION=\"addbook:add\"><INPUT TYPE=hidden name=vcard VALUE=\"%s\"><INPUT type=submit value=\"%s\"></INPUT></FORM>", 
			vEscCard, XP_GetString(MK_MSG_ADD_TO_ADDR_BOOK));
	}
	else 
	{
		htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showBasic%d();\\\"></INPUT></FORM>", 
			XP_GetString(MK_ADDR_VIEW_CONDENSED_VCARD), unique);

		htmlLine2 = PR_smprintf ("<FORM name=form1 METHOD=get ACTION=\"addbook:add\"><INPUT TYPE=hidden name=vcard VALUE=\"%s\"><INPUT type=submit value=\"%s\"></INPUT></FORM>",
			vEscCard, XP_GetString(MK_MSG_ADD_TO_ADDR_BOOK));
	}

	if (!htmlLine1 && !htmlLine2)
	{
		XP_FREEIF (vEscCard);
		XP_FREEIF (htmlLine1);
		XP_FREEIF (htmlLine2);
		return MK_OUT_OF_MEMORY;
	}
	
	status = OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) goto FAIL;
	status = WriteEachLineToStream (obj, "<SCRIPT>document.write(\"");
	if (status < 0) goto FAIL;
	status = WriteLineToStream (obj, htmlLine1);
	if (status < 0) goto FAIL;
	status = WriteEachLineToStream (obj, "\")</SCRIPT>");
	if (status < 0) goto FAIL;
	status = WriteLineToStream (obj, htmlLine2);
	if (status < 0) goto FAIL;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) goto FAIL;

	FAIL:
	XP_FREEIF (vEscCard);
	XP_FREEIF (htmlLine1);
	XP_FREEIF (htmlLine2);

	return status;
}

static int BeginLayer(MimeObject *obj, XP_Bool basic)
{
	int status = 0;
	char * captionLine = NULL;

	if (basic)
	{
		captionLine = PR_smprintf ("<LAYER NAME=basic%d>", unique);
	}
	else
	{
		captionLine = PR_smprintf ("<ILAYER NAME=advanced%d VISIBILITY=\"hide\">", unique);
	}

	if (captionLine)
	{
		status = WriteEachLineToStream (obj, captionLine);
		XP_FREE(captionLine);
		if (status < 0) return status;
		status = OutputTable (obj, FALSE, FALSE, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, TRUE, FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, FALSE, FALSE, NULL, "TOP", NULL, NULL);
		if (status < 0) return status;
		status = OutputTable (obj, FALSE, TRUE, "0", "0", "#FFFFFF");
		if (status < 0) return status;
		if (basic)
		{
			status = OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
			if (status < 0) return status;
			status = OutputTableRowOrData(obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
			if (status < 0) return status;
		}
		else
		{
			status = OutputTableRowOrData(obj, TRUE, FALSE, NULL, NULL, NULL, NULL);
			if (status < 0) return status;
			status = OutputTableRowOrData(obj, FALSE, FALSE, NULL, NULL, NULL, NULL);
			if (status < 0) return status;
		}

		status = OutputTable (obj, FALSE, FALSE, "4", NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, TRUE, FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, FALSE, FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}
	else
		status = MK_OUT_OF_MEMORY;

	return status;
}

static int EndLayer(MimeObject *obj, XP_Bool basic, VObject* v)
{
	int status = 0;
	char * captionLine = NULL;

	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputButtons(obj, basic, v);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, TRUE, FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	if (!basic) 
	{
		status = WriteEachLineToStream (obj, "</ILAYER>");
		if (status < 0) return status;
		status = WriteEachLineToStream (obj, "<P><SCRIPT>");
		if (status < 0) return status;
		captionLine = PR_smprintf ("function showAdvanced%d() {", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		XP_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
		captionLine = PR_smprintf ("document.layers[\"basic%d\"].visibility = \"hide\";", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		XP_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
		captionLine = PR_smprintf ("document.layers[\"advanced%d\"].visibility = \"inherit\";", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		XP_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
		status = WriteEachLineToStream (obj, "};");
		if (status < 0) return status;
		captionLine = PR_smprintf ("function showBasic%d() {", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		XP_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
		captionLine = PR_smprintf ("document.layers[\"advanced%d\"].visibility = \"hide\";", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		if (status < 0) return status;
		captionLine = PR_smprintf ("document.layers[\"basic%d\"].visibility = \"inherit\";", unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		XP_FREEIF (captionLine);
		if (status < 0) return status;
		status = WriteEachLineToStream (obj, "}; </SCRIPT></P>");
	} 
	else {
		status = WriteEachLineToStream (obj, "</LAYER>");
	}

	if (status < 0) return status;
	return 0;
}

static int EndVCard (MimeObject *obj)
{
	int status = 0;

	/* Scribble HTML-ending stuff into the stream */
	char htmlFooters[32];
	PR_snprintf (htmlFooters, sizeof(htmlFooters), "</BODY>%s</HTML>%s", LINEBREAK, LINEBREAK);
	status = MimeObject_write(obj, htmlFooters, XP_STRLEN(htmlFooters), FALSE);

	if (status < 0) return status;

    if (obj->options && obj->options->set_html_state_fn) {
        status = obj->options->set_html_state_fn(obj->options->stream_closure,
                                                 TRUE,   /* layer_encapulate_p */
                                                 FALSE,  /* start_p */
                                                 FALSE); /* abort_p */
        if (status < 0) return status;
    }
    
	return 0;
}

static int BeginVCard (MimeObject *obj)
{
	int status = 0;

	/* Scribble HTML-starting stuff into the stream */
	char htmlHeaders[32];

    if (obj->options && obj->options->set_html_state_fn) {
        status = obj->options->set_html_state_fn(obj->options->stream_closure,
                                                 TRUE,   /* layer_encapulate_p */
                                                 TRUE,   /* start_p */
                                                 FALSE); /* abort_p */
        if (status < 0) return status;
    }
    
	unique++;
	PR_snprintf (htmlHeaders, sizeof(htmlHeaders), "<HTML>%s<BODY>%s", LINEBREAK, LINEBREAK);
    status = MimeObject_write(obj, htmlHeaders, XP_STRLEN(htmlHeaders), TRUE);

	if (status < 0) return status;

	return 0;
}

static int WriteOutVCard (MimeObject *obj, VObject* v)
{
	int status = 0;

	status = BeginVCard (obj);
	if (status < 0) return status;
	
	/* write out basic layer */
	status = BeginLayer(obj, TRUE);
	if (status < 0) return status;
	status = OutputBasicVcard(obj, v);
	if (status < 0) return status;
	status = EndLayer(obj, TRUE, v);
	if (status < 0) return status;

	/* write out advanced layer */
	status = BeginLayer(obj, FALSE);
	if (status < 0) return status;
	status = OutputAdvancedVcard(obj, v);
	if (status < 0) return status;
	status = EndLayer(obj, FALSE, v);
	if (status < 0) return status;

	status = EndVCard (obj);
	if (status < 0) return status;

	return 0;
}

static void GetAddressProperties (VObject* o, char ** attribName)
{
	VObject* domProp = isAPropertyOf(o, VCDomesticProp);
	VObject* intlProp = isAPropertyOf(o, VCInternationalProp);
	VObject* postal = isAPropertyOf(o, VCPostalProp);
	VObject* parcel = isAPropertyOf(o, VCParcelProp);
	VObject* home = isAPropertyOf(o, VCHomeProp);
	VObject* work = isAPropertyOf(o, VCWorkProp);
	if (domProp) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_DOM_TYPE));
	}
	if (intlProp) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_INTL_TYPE));
	}
	if (postal) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_POSTAL_TYPE));
	}
	if (parcel) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_PARCEL_TYPE));
	}
	if (home) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_HOME_TYPE));
	}
	if (work) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_WORK_TYPE));
	}
}


static void GetTelephoneProperties (VObject* o, char ** attribName)
{
	VObject* prefProp = isAPropertyOf(o, VCPreferredProp);
	VObject* home = isAPropertyOf(o, VCHomeProp);
	VObject* work = isAPropertyOf(o, VCWorkProp);
	VObject* voiceProp = isAPropertyOf(o, VCVoiceProp);
	VObject* fax = isAPropertyOf(o, VCFaxProp);
	VObject* msg = isAPropertyOf(o, VCMessageProp);
	VObject* cell = isAPropertyOf(o, VCCellularProp);
	VObject* pager = isAPropertyOf(o, VCPagerProp);
	VObject* bbs = isAPropertyOf(o, VCBBSProp);
	if (prefProp) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_PREF_TYPE));
	}
	if (home) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_HOME_TYPE));
	}
	if (work) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_WORK_TYPE));
	}
	if (voiceProp) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_VOICE_TYPE));
	}
	if (fax) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_FAX_TYPE));
	}
	if (msg) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_MSG_TYPE));
	}
	if (cell) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_CELL_TYPE));
	}
	if (pager) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_PAGER_TYPE));
	}
	if (bbs) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_BBS_TYPE));
	}
}

static void GetEmailProperties (VObject* o, char ** attribName)
{
	
	VObject* prefProp = isAPropertyOf(o, VCPreferredProp);
	VObject* home = isAPropertyOf(o, VCHomeProp);
	VObject* work = isAPropertyOf(o, VCWorkProp);
	VObject* aol = isAPropertyOf(o, VCAOLProp);
	VObject* applelink = isAPropertyOf(o, VCAppleLinkProp);
	VObject* att = isAPropertyOf(o, VCATTMailProp);
	VObject* cis = isAPropertyOf(o, VCCISProp);
	VObject* eworld = isAPropertyOf(o, VCEWorldProp);
	VObject* internet = isAPropertyOf(o, VCInternetProp);
	VObject* ibmmail = isAPropertyOf(o, VCIBMMailProp);
	VObject* mcimail = isAPropertyOf(o, VCMCIMailProp);
	VObject* powershare = isAPropertyOf(o, VCPowerShareProp);
	VObject* prodigy = isAPropertyOf(o, VCProdigyProp);
	VObject* telex = isAPropertyOf(o, VCTLXProp);
	VObject* x400 = isAPropertyOf(o, VCX400Prop);
	if (prefProp) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_PREF_TYPE));
	}
	if (home) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_HOME_TYPE));
	}
	if (work) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_WORK_TYPE));
	}
	if (aol) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_AOL_TYPE));
	}
	if (applelink) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_APPLELINK_TYPE));
	}
	if (att) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_ATTMAIL_TYPE));
	}
	if (cis) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_CSI_TYPE));
	}
	if (eworld) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_EWORLD_TYPE));
	}
	if (internet) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_INTERNET_TYPE));
	}
	if (ibmmail) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_IBMMAIL_TYPE));
	}
	if (mcimail) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_MCIMAIL_TYPE));
	}
	if (powershare) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_POWERSHARE_TYPE));
	}
	if (prodigy) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_PRODIGY_TYPE));
	}
	if (telex) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_TLX_TYPE));
	}
	if (x400) {
		StrAllocCat ((*attribName), " ");
		StrAllocCat ((*attribName), XP_GetString (MK_LDAP_X400));
	}

}

static int WriteOutEachVCardPhoneProperty (MimeObject *obj, VObject* o)
{
	char *attribName = NULL;
	char *value = NULL;
	int status = 0;

	if (vObjectName(o)) 
	{
		if (strcasecomp (VCTelephoneProp, vObjectName(o)) == 0) 
		{
			if (VALUE_TYPE(o)) 
			{
				GetTelephoneProperties(o, &attribName);
				if (!attribName)
					attribName = XP_STRDUP (XP_GetString (MK_LDAP_PHONE_NUMBER));
				attribName = StrAllocCat(attribName, ": ");
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
				{
					attribName = StrAllocCat (attribName, value);
					XP_FREEIF (value);
					if (attribName)
					{
						status = OutputFont(obj, FALSE, "-1", NULL);
						if (status < 0) {
							XP_FREEIF (attribName);
							return status;
						}
						status = WriteLineToStream (obj, attribName);
						if (status < 0) {
							XP_FREEIF (attribName);
							return status;
						}
						status = OutputFont(obj, TRUE, NULL, NULL);
						if (status < 0) {
							XP_FREEIF (attribName);
							return status;
						}
					}
					XP_FREEIF (attribName);
				}
			}
		}
	}
	return status;
}

static int WriteOutVCardPhoneProperties (MimeObject *obj, VObject* v)
{
	int status = 0;
	VObjectIterator t;
	VObject *eachProp;

    WriteOutEachVCardPhoneProperty (obj, v);
	initPropIterator(&t,v);
	while (moreIteration(&t) && status >= 0)
	{
		eachProp = nextVObject(&t);
		status = WriteOutEachVCardPhoneProperty (obj, eachProp);
	}

	if (status < 0) return status;

	return 0;
}

static int WriteOutEachVCardProperty (MimeObject *obj, VObject* o, int* numEmail)
{
	char *attribName = NULL;
	char * url = NULL;
	char *value = NULL;
	char *ptr = NULL;
	int status = 0;

	if (vObjectName(o)) {

		if (strcasecomp (VCPhotoProp, vObjectName(o)) == 0) {
			VObject* urlProp = isAPropertyOf(o, VCURLProp);
			if (urlProp) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_PHOTOGRAPH));
				/* format the value string to the url */
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
					url = PR_smprintf ("<IMG SRC = ""%s""", value);
				XP_FREEIF (value);
				value = NULL;
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCBirthDateProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_BIRTHDAY));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCDeliveryLabelProp, vObjectName(o)) == 0) {
			attribName = XP_STRDUP (XP_GetString (MK_LDAP_LABEL));
			GetAddressProperties(o, &attribName);
			value = fakeCString (vObjectUStringZValue(o));
			goto DOWRITE;
		}

		if (strcasecomp (VCEmailAddressProp, vObjectName(o)) == 0) {
			if ((*numEmail) != 1)
			{
				if (VALUE_TYPE(o)) {
					(*numEmail)++;
					attribName = XP_STRDUP (XP_GetString (MK_LDAP_EMAIL_ADDRESS));
					GetEmailProperties(o, &attribName);
					value = fakeCString (vObjectUStringZValue(o));
					goto DOWRITE;
				};
			}
		}

		if (strcasecomp (VCFamilyNameProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_SURNAME));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCGivenNameProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_GIVEN_NAME));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCNamePrefixesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_NAME_PREFIX));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCNameSuffixesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_NAME_SUFFIX));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCAdditionalNamesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_MIDDLE_NAME));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCMailerProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_MAILER));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCTimeZoneProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_TZ));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCGeoProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_GEO));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCBusinessRoleProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_ROLE));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCLogoProp, vObjectName(o)) == 0) {
			VObject* urlProp = isAPropertyOf(o, VCURLProp);
			if (urlProp) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_LOGO));
				/* format the value string to the url */
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
					url = PR_smprintf ("<IMG SRC = ""%s""", value);
				XP_FREEIF (value);
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCAgentProp, vObjectName(o)) == 0) {
			attribName = XP_STRDUP (XP_GetString (MK_LDAP_SECRETARY));
			goto DOWRITE;
		}

		if (strcasecomp (VCLastRevisedProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_REVISION));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (strcasecomp (VCPronunciationProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_SOUND));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}


		if (strcasecomp (VCVersionProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = XP_STRDUP (XP_GetString (MK_LDAP_VERSION));
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (!attribName) 
			return 0;
	
	DOWRITE:
		OutputTableRowOrData(obj, TRUE, FALSE, "LEFT", "TOP", NULL, NULL);
		OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
		if (attribName) { 
			OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteAttribute (obj, attribName);
			XP_FREE (attribName);
			OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
		}

		if (value) {
			OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteValue (obj, value);
			XP_FREE (value);
			OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
		}

		if (url) {
			OutputTableRowOrData (obj, FALSE, FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteValue (obj, url);
			XP_FREE (url);
			OutputTableRowOrData (obj, FALSE, TRUE, NULL, NULL, NULL, NULL);
		}
		OutputTableRowOrData(obj, TRUE, TRUE, NULL, NULL, NULL, NULL);
	}

	if (status < 0) return status;

	return 0;
}


static int WriteOutVCardProperties (MimeObject *obj, VObject* v, int* numEmail)
{
	int status = 0;
	VObjectIterator t;
	VObject *eachProp;

    WriteOutEachVCardProperty (obj, v, numEmail);
	initPropIterator(&t,v);
	while (moreIteration(&t) && status >= 0)
	{
		eachProp = nextVObject(&t);
		status = WriteOutVCardProperties (obj, eachProp, numEmail);
	}

	if (status < 0) return status;

	return 0;
}

static int WriteLineToStream (MimeObject *obj, const char *line)
{
	int status = 0;
	char *htmlLine;
	int htmlLen = XP_STRLEN(line) + XP_STRLEN("<DT></DT>") + 1;;

	htmlLine = (char *) XP_ALLOC (htmlLen);
	if (htmlLine)
	{
		htmlLine[0] = '\0';
		XP_STRCAT (htmlLine, "<DT>");
		XP_STRCAT (htmlLine, line);
		XP_STRCAT (htmlLine, "</DT>");
	    status = MimeObject_write(obj, htmlLine, XP_STRLEN(htmlLine), TRUE);
#ifdef DEBUG_mwatkins
		if (XP_STRLEN (htmlLine) < 500)
			XP_Trace("%s%c", htmlLine, '\n');
#endif
		XP_FREE ((void*) htmlLine);
	}
	else
		status = MK_OUT_OF_MEMORY;

	return status;
}

static int WriteAttribute (MimeObject *obj, const char *attrib)
{
	int status = 0;
	OutputFont(obj, FALSE, "-1", NULL);
	status  = WriteLineToStream (obj, attrib);
	OutputFont(obj, TRUE, NULL, NULL);
	return status;
}


static int WriteValue (MimeObject *obj, const char *value)
{
	int status = 0;
	OutputFont(obj, FALSE, "-1", NULL);
	status  = WriteLineToStream (obj, value);
	OutputFont(obj, TRUE, NULL, NULL);
	return status;
}


