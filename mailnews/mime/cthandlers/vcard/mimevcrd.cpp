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
#define NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "msgCore.h"
#include "prlog.h"
#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "libi18n.h"
#include "nsVCardObj.h"
#include "nsVCard.h"
#include "mimecth.h"
#include "mimexpcom.h"
#include "mimevcrd.h"
#include "nsEscape.h"
#include "nsIURI.h"

#include "nsIEventQueueService.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsVCardStringResources.h"

#include "nsVCard.h"
#include "nsVCardObj.h"

//#include "vobject.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

static int MimeInlineTextVCard_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextVCard_parse_eof (MimeObject *, PRBool);
static int MimeInlineTextVCard_parse_begin (MimeObject *obj);

static int s_unique = 0;

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

// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

/*
 * These functions are the public interface for this content type
 * handler and will be called in by the mime component.
 */
#define      VCARD_CONTENT_TYPE  "text/x-vcard"

 /* This is the object definition. Note: we will set the superclass 
    to NULL and manually set this on the class creation */
MimeDefClass(MimeInlineTextVCard, MimeInlineTextVCardClass,
			 mimeInlineTextVCardClass, NULL);

extern "C" char *
MIME_GetContentType(void)
{
  return VCARD_CONTENT_TYPE;
}

extern "C" MimeObjectClass *
MIME_CreateContentTypeHandlerClass(const char *content_type, 
                                   contentTypeHandlerInitStruct *initStruct)
{
  MimeObjectClass *clazz = (MimeObjectClass *)&mimeInlineTextVCardClass;
  /*
   * Must set the superclass by hand.
   */
  if (!COM_GetmimeInlineTextClass())
    return NULL;

  clazz->superclass = (MimeObjectClass *)COM_GetmimeInlineTextClass();
  initStruct->force_inline_display = PR_TRUE;
  return clazz;
}

/* 
 * Implementation of VCard clazz 
 */
static int
MimeInlineTextVCardClassInitialize(MimeInlineTextVCardClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextVCard_parse_begin;
  oclass->parse_line  = MimeInlineTextVCard_parse_line;
  oclass->parse_eof   = MimeInlineTextVCard_parse_eof;

return 0;
}

static int
MimeInlineTextVCard_parse_begin (MimeObject *obj)
{
//    int status = ((MimeObjectClass*)&mimeLeafClass)->parse_begin(obj);
  int status = ((MimeObjectClass*)COM_GetmimeLeafClass())->parse_begin(obj);
  MimeInlineTextVCardClass *clazz;
    if (status < 0) return status;

    if (!obj->output_p) return 0;
    if (!obj->options || !obj->options->write_html_p) return 0;

    /* This is a fine place to write out any HTML before the real meat begins.
       In this sample code, we tell it to start a table. */

	clazz = ((MimeInlineTextVCardClass *) obj->clazz);
	/* initialize vcard string to empty; */
	vCard_SACopy(&(clazz->vCardString), "");

    return 0;
}

char *strcpySafe (char *dest, const char *src, size_t destLength)
{
	char *result = strncpy (dest, src, --destLength);
	dest[destLength] = '\0';
	return result;
}

static int
MimeInlineTextVCard_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
    /* This routine gets fed each line of data, one at a time.  In my
       sample, I spew it out as a table row, putting everything
       between colons in its own table cell.*/

	char* linestring;
	MimeInlineTextVCardClass *clazz = ((MimeInlineTextVCardClass *) obj->clazz);
 
    if (!obj->output_p) return 0;
    if (!obj->options || !obj->options->output_fn) return 0;
    if (!obj->options->write_html_p) {
		return COM_MimeObject_write(obj, line, length, PR_TRUE);
    }

	linestring = (char *) PR_MALLOC (length + 1);

	if (linestring) {
    strcpySafe((char *)linestring, line, length + 1);
		vCard_SACat (&clazz->vCardString, linestring);
		PR_Free (linestring);
	}

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
static PRInt32 INTL_ConvertCharset(const char* from_charset, const char* to_charset,
                                   const char* inBuffer, const PRInt32 inLength,
                                   char** outBuffer, PRInt32* outLength)
{
  char *dstPtr = nsnull;
  PRInt32 dstLength = 0;
  nsresult res;

  // invalid input
  if (nsnull == from_charset || nsnull == to_charset || 
      '\0' == *from_charset || '\0' == *to_charset || nsnull == inBuffer) 
    return -1;

  // from to identical
  if (!PL_strcasecmp(from_charset, to_charset))
    return -1;

  // us-ascii is a subset of utf-8
  if ((!PL_strcasecmp(from_charset, "us-ascii") && !PL_strcasecmp(to_charset, "utf-8")) ||
      (!PL_strcasecmp(from_charset, "utf-8") && !PL_strcasecmp(to_charset, "us-ascii")))
    return -1;

  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsString aCharset(from_charset);
    nsIUnicodeDecoder* decoder = nsnull;
    PRUnichar *unichars;
    PRInt32 unicharLength;

    // convert to unicode
    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) {
      PRInt32 srcLen = inLength;
      res = decoder->GetMaxLength(inBuffer, srcLen, &unicharLength);
      // temporary buffer to hold unicode string
      unichars = new PRUnichar[unicharLength];
      if (unichars == nsnull) {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        res = decoder->Convert(inBuffer, &srcLen, unichars, &unicharLength);

        // convert from unicode
        nsIUnicodeEncoder* encoder = nsnull;
        aCharset.SetString(to_charset);
        res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
        if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
          res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
          // allocale an output buffer
          dstPtr = (char *) PR_Malloc(dstLength + 1);
          if (dstPtr == nsnull) {
            res = NS_ERROR_OUT_OF_MEMORY;
          }
          else {
            res = encoder->Convert(unichars, &unicharLength, dstPtr, &dstLength);
          }
          NS_IF_RELEASE(encoder);
        }
        delete [] unichars;
      }
      NS_IF_RELEASE(decoder);
    }
  }

  // set the outputs
  if (NS_SUCCEEDED(res) && nsnull != dstPtr) {
    dstPtr[dstLength] = '\0';
    *outBuffer = dstPtr;
    *outLength = dstLength;
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}
////////////////////////////////////////////////////////////////////////////////
static int
MimeInlineTextVCard_parse_eof (MimeObject *obj, PRBool abort_p)
{
    int status = 0;
	MimeInlineTextVCardClass *clazz = ((MimeInlineTextVCardClass *) obj->clazz);

	VObject *t, *v;

    if (obj->closed_p) return 0;

    /* Run parent method first, to flush out any buffered data. */
//    status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
    status = ((MimeObjectClass*)COM_GetmimeInlineTextClass())->parse_eof(obj, abort_p);
    if (status < 0) return status;

	if (!clazz->vCardString) return 0;

	v = Parse_MIME(clazz->vCardString, PL_strlen(clazz->vCardString));

	if (clazz->vCardString) {
		PR_Free ((char*) clazz->vCardString);
		clazz->vCardString = NULL;
	}

    if (obj->output_p && obj->options && obj->options->write_html_p &&
		obj->options->headers != MimeHeadersCitation) {
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
	int htmlLen = PL_strlen(line) + 1;

	htmlLine = (char *) PR_MALLOC (htmlLen);
	if (htmlLine)
	{
		htmlLine[0] = '\0';
		PL_strcat (htmlLine, line);
	    status = COM_MimeObject_write(obj, htmlLine, PL_strlen(htmlLine), PR_TRUE);
		PR_Free ((void*) htmlLine);
	}
	else
		status = VCARD_OUT_OF_MEMORY;

	return status;
}

static int OutputTable (MimeObject *obj, PRBool endTable, PRBool border, char *cellspacing, char *cellpadding, char *bgcolor)
{
	int status = 0;
	char * htmlLine = NULL;

	if (endTable)
	{
		status = WriteEachLineToStream (obj, "</TABLE>");
	}
	else
	{
		int htmlLen = PL_strlen("<TABLE>") + 1;
		if (border)
			htmlLen += PL_strlen (" BORDER");
		if (cellspacing)
			htmlLen += PL_strlen(" CELLSPACING=") + PL_strlen(cellspacing);
		if (cellpadding)
			htmlLen += PL_strlen(" CELLPADDING=") + PL_strlen(cellpadding);
		if (bgcolor)
			htmlLen += PL_strlen(" BGCOLOR=") + PL_strlen(bgcolor);
		if (border || cellspacing || cellpadding || bgcolor)
			htmlLen++;

		htmlLine = (char *) PR_MALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
			PL_strcat (htmlLine, "<TABLE");
			if (border)
				PL_strcat (htmlLine, " BORDER");
			if (cellspacing)
			{	
				PL_strcat (htmlLine, " CELLSPACING=");
				PL_strcat (htmlLine, cellspacing);
			}
			if (cellpadding)
			{	
				PL_strcat (htmlLine, " CELLPADDING=");
				PL_strcat (htmlLine, cellpadding);
			}
			if (bgcolor)
			{	
				PL_strcat (htmlLine, " BGCOLOR=");
				PL_strcat (htmlLine, bgcolor);
			}

			if (border || cellspacing || cellpadding || bgcolor)
				PL_strcat (htmlLine, " ");

			PL_strcat (htmlLine, ">");

			status = COM_MimeObject_write(obj, htmlLine, PL_strlen(htmlLine), PR_TRUE);
			PR_Free ((void*) htmlLine);
		}
		else
			status = VCARD_OUT_OF_MEMORY;
	}	
	return status;
}

static int OutputTableRowOrData(MimeObject *obj, PRBool outputRow, 
								PRBool end, char * align, 
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
		int htmlLen = PL_strlen("<TR>") + 1;
		if (align)
			htmlLen += PL_strlen(" ALIGN=") + PL_strlen(align);
		if (colspan)
			htmlLen += PL_strlen(" COLSPAN=") + PL_strlen(colspan);
		if (width)
			htmlLen += PL_strlen(" WIDTH=") + PL_strlen(width);
		if (valign)
			htmlLen += PL_strlen(" VALIGN=") + PL_strlen(valign);
		if (align || valign || colspan || width)
			htmlLen++;

		htmlLine = (char *) PR_MALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
			if (outputRow)
				PL_strcat (htmlLine, "<TR");
			else
				PL_strcat (htmlLine, "<TD");
			if (align)
			{	
				PL_strcat (htmlLine, " ALIGN=");
				PL_strcat (htmlLine, align);
			}
			if (valign)
			{	
				PL_strcat (htmlLine, " VALIGN=");
				PL_strcat (htmlLine, valign);
			}
			if (colspan)
			{	
				PL_strcat (htmlLine, " COLSPAN=");
				PL_strcat (htmlLine, colspan);
			}
			if (width)
			{	
				PL_strcat (htmlLine, " WIDTH=");
				PL_strcat (htmlLine, width);
			}
			if (align || valign || colspan || width)
				PL_strcat (htmlLine, " ");

			PL_strcat (htmlLine, ">");

			status = COM_MimeObject_write(obj, htmlLine, PL_strlen(htmlLine), PR_TRUE);
			PR_Free ((void*) htmlLine);
		}
		else
			status = VCARD_OUT_OF_MEMORY;
	}

	return status;
}


static int OutputFont(MimeObject *obj, PRBool endFont, char * size, char* color)
{
	int status = 0;
	char * htmlLine = NULL;

	if (endFont)
		status = WriteEachLineToStream (obj, "</FONT>");
	else
	{
		int htmlLen = PL_strlen("<FONT>") + 1;
		if (size)
			htmlLen += PL_strlen(" SIZE=") + PL_strlen(size);
		if (color)
			htmlLen += PL_strlen(" COLOR=") + PL_strlen(color);
		if (size || color)
			htmlLen++;

		htmlLine = (char *) PR_MALLOC (htmlLen);
		if (htmlLine)
		{
			htmlLine[0] = '\0';
				PL_strcat (htmlLine, "<FONT");
			if (size)
			{	
				PL_strcat (htmlLine, " SIZE=");
				PL_strcat (htmlLine, size);
			}
			if (color)
			{	
				PL_strcat (htmlLine, " COLOR=");
				PL_strcat (htmlLine, color);
			}
			if (size || color)
				PL_strcat (htmlLine, " ");

			PL_strcat (htmlLine, ">");

			status = COM_MimeObject_write(obj, htmlLine, PL_strlen(htmlLine), PR_TRUE);
			PR_Free ((void*) htmlLine);
		}
		else
			status = VCARD_OUT_OF_MEMORY;
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
			if (VALUE_TYPE(prop) != VCVT_RAW)
				string = fakeCString (vObjectUStringZValue(prop));
			else
			{
				string = (char *)PR_MALLOC(PL_strlen((char *) vObjectAnyValue(prop)) + 1);
				if (string)
					PL_strcpy(string, (char *) vObjectAnyValue(prop));
			}
			if (string) {
				status = OutputFont(obj, PR_FALSE, "-1", NULL);
				if (status < 0) {
					PR_FREEIF (string);
					return status;
				}
				status = WriteLineToStream (obj, string);
				PR_FREEIF (string);
				if (status < 0) return status;
				status = OutputFont(obj, PR_TRUE, NULL, NULL);
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
			if (VALUE_TYPE(prop) != VCVT_RAW)
				namestring  = fakeCString (vObjectUStringZValue(prop));
			else
			{
				namestring = (char *)PR_MALLOC(PL_strlen((char *) vObjectAnyValue(prop)) + 1);
				if (namestring)
					PL_strcpy(namestring, (char *) vObjectAnyValue(prop));
			}
			if (namestring)
			{
				prop = isAPropertyOf(v, VCURLProp);
				if (prop)
				{
					urlstring  = fakeCString (vObjectUStringZValue(prop));
					if (urlstring)
						htmlLine1 = PR_smprintf ("<A HREF=""%s"" PRIVATE>%s</A> ", urlstring, namestring);
					else
						htmlLine1 = PR_smprintf ("%s ", namestring);
					PR_FREEIF (urlstring);
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
							htmlLine2 = PR_smprintf ("&lt;<A HREF=""mailto:%s"" PRIVATE>%s</A>&gt;", emailstring, emailstring);
						else
							htmlLine2 = PR_smprintf ("%s", emailstring);
						PR_FREEIF (emailstring);
					}
				}

				if (!htmlLine1 && !htmlLine2)
				{
					PR_FREEIF (htmlLine1);
					PR_FREEIF (htmlLine2);
					return VCARD_OUT_OF_MEMORY;
				}
				else
				{
					htmlLine = vCard_SACat (&htmlLine, htmlLine1);
					htmlLine = vCard_SACat (&htmlLine, htmlLine2);
				}

				PR_FREEIF (htmlLine1);
				PR_FREEIF (htmlLine2);
				PR_FREEIF (namestring);
			}
		}
	}

	status = OutputTable (obj, PR_FALSE, PR_FALSE, "0", "0", NULL);
	if (status < 0) {
		PR_FREEIF (htmlLine); 
		return status;
	}
	if (htmlLine)
	{
		status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
		if (status < 0) {
			PR_Free (htmlLine);
			return status;
		}
		status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) {
			PR_Free (htmlLine);
			return status;
		}

		status = WriteLineToStream (obj, htmlLine);
		PR_Free (htmlLine); 
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}
	else
		status = VCARD_OUT_OF_MEMORY;

	status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, NULL, NULL, NULL, NULL);
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
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
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

	status = OutputTable (obj, PR_FALSE, PR_FALSE, "0", "0", NULL);
	if (status < 0) return status;
	/* beginning of first row */
	status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", "3", NULL);
	if (status < 0) return status;

	/* get the name and email */
	prop = isAPropertyOf(v, VCFullNameProp);
	if (prop)
	{
		if (VALUE_TYPE(prop))
		{
			if (VALUE_TYPE(prop) != VCVT_RAW)
				namestring  = fakeCString (vObjectUStringZValue(prop));
			else
			{
				namestring = (char *)PR_MALLOC(PL_strlen((char *) vObjectAnyValue(prop)) + 1);
				if (namestring)
					PL_strcpy(namestring, (char *) vObjectAnyValue(prop));
			}
			if (namestring)
			{
				prop = isAPropertyOf(v, VCURLProp);
				if (prop)
				{
					urlstring  = fakeCString (vObjectUStringZValue(prop));
					if (urlstring)
						htmlLine1 = PR_smprintf ("<A HREF=""%s"" PRIVATE>%s</A> ", urlstring, namestring);
					else
						htmlLine1 = PR_smprintf ("%s ", namestring);
					PR_FREEIF (urlstring);
				}
				else
					htmlLine1 = PR_smprintf ("%s ", namestring);

				PR_FREEIF (namestring);
			}
			if (!htmlLine1)
			{
				return VCARD_OUT_OF_MEMORY;
			}
		}
	}
	/* output the name if there was one */
	if (htmlLine1)
	{
		status = WriteLineToStream (obj, htmlLine1);
		PR_FREEIF (htmlLine1);
		if (status < 0) return status;
	}

	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	
	/* beginning of second row */
	status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
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
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE , "LEFT", "TOP", NULL, "\"10\"");
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
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
				htmlLine2 = PR_smprintf ("&lt;<A HREF=""mailto:%s"" PRIVATE>%s</A>&gt;", emailstring, emailstring);
			else
				htmlLine2 = PR_smprintf ("%s", emailstring);

			PR_FREEIF (emailstring);
		}
		if (!htmlLine2)
		{
			return VCARD_OUT_OF_MEMORY;
		}
	}
	/* output email address */
	if (htmlLine2)
	{
		status = OutputFont(obj, PR_FALSE, "-1", NULL);
		if (status < 0) {
			PR_FREEIF (htmlLine2);
			return status;
		}
		status = WriteLineToStream (obj, htmlLine2);
		PR_FREEIF (htmlLine2);
		if (status < 0) return status;
		status = OutputFont(obj, PR_TRUE, NULL, NULL);
		if (status < 0) return status;
		/* output html mail setting only if its true */
		prop = isAPropertyOf(v, VCUseHTML);
		if (prop)
		{
			if (VALUE_TYPE(prop))
			{
				namestring  = fakeCString (vObjectUStringZValue(prop));
				if (namestring)
					if (PL_strcasecmp (namestring, "PR_TRUE") == 0)
					{
						PR_FREEIF (namestring);
						status = OutputFont(obj, PR_FALSE, "-1", NULL);
						if (status < 0) return status;
            char *tString = VCardGetStringByID(VCARD_LDAP_USEHTML);
						status = WriteLineToStream (obj, tString);
            PR_FREEIF(tString);
						if (status < 0) return status;
						status = OutputFont(obj, PR_TRUE, NULL, NULL);
						if (status < 0) return status;
					}
					else
						PR_FREEIF (namestring);
			}
		}
	}
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* beginning of third row */
	/* write out address information if we have any */
	status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	/* first column */
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
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
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* second column */
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE , NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* third column */
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	/* output telephone fields */
	status = WriteOutVCardPhoneProperties (obj, v);
	if (status < 0) return status;
	/* output conference fields */
	status = OutputFont(obj, PR_FALSE, "-1", NULL);
	if (status < 0) return status;
	prop = isAPropertyOf(v, VCCooltalk);
	if (prop)
	{
    char *tString = VCardGetStringByID(VCARD_ADDR_CONFINFO);
		WriteLineToStream (obj, tString);
    PR_FREEIF(tString);
		if (status < 0) return status;
		prop2 = isAPropertyOf(prop, VCUseServer);
		if (prop2)
		{
			if (VALUE_TYPE(prop2)) {
				namestring  = fakeCString (vObjectUStringZValue(prop2));
        char *tString1 = NULL;
				if (PL_strcmp (namestring, "0") == 0)
        {
          tString1 = VCardGetStringByID(VCARD_ADDR_DEFAULT_DLS);
        }
				else 
        {
					if (PL_strcmp (namestring, "1") == 0)
						tString1 = VCardGetStringByID(VCARD_ADDR_SPECIFIC_DLS);
					else
						if (PL_strcmp (namestring, "2") == 0)
							tString1 = VCardGetStringByID(VCARD_ADDR_HOSTNAMEIP);
				}

        status = WriteLineToStream (obj, tString1);
        PR_FREEIF(tString1);
				PR_FREEIF (namestring);
				if (status < 0) return status;
			}
		}
		status = OutputVcardAttribute (obj, prop, VCCooltalkAddress);
		if (status < 0) return status;
	}

	status = OutputFont(obj, PR_TRUE, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	/* beginning of last row */
	/* output notes field */
	prop = isAPropertyOf(v, VCCommentProp);
	if (prop)
	{
		status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", "3", NULL);
		if (status < 0) return status;
		status = OutputVcardAttribute (obj, v, VCCommentProp);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}

	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	/* output second table containing all the additional info */
	status = OutputTable (obj, PR_FALSE, PR_FALSE, "0", "0", NULL);
	if (status < 0) return status;
	/* beginning of first row */
	status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", "2", NULL);
	if (status < 0) return status;
	/* output the additional info header */
	status = OutputFont(obj, PR_FALSE, "-1", NULL);
	if (status < 0) return status;
  char *tString = VCardGetStringByID(VCARD_ADDR_ADDINFO);
	status = WriteLineToStream (obj, tString);
  PR_FREEIF(tString);
	if (status < 0) return status;
	status = OutputFont(obj, PR_TRUE, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	
	/* beginning of remaining rows */
	status = WriteOutVCardProperties (obj, v, &numEmail);
	if (status < 0) return status;

	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	return 0;
}

static int OutputButtons(MimeObject *obj, PRBool basic, VObject *v)
{
	int status = 0;
	char * htmlLine1 = NULL;
	char * htmlLine2 = NULL;
	char* vCard = NULL;
	char* vEscCard = NULL;
	int len = 0;
	char* charset = NULL;
	char* rsrcString = NULL;
	char* converted = NULL;
  PRInt32 converted_length;
  PRInt32   res;

	if (!obj->options->output_vcard_buttons_p)
		return status;

	vCard = writeMemVObjects(0, &len, v);

	if (!vCard)
		return VCARD_OUT_OF_MEMORY;

	vEscCard = nsEscape (vCard, url_XAlphas);

	PR_FREEIF (vCard);

	if (!vEscCard)
		return VCARD_OUT_OF_MEMORY;

	/* parse a content type for the charset */
	charset = PL_strstr(obj->content_type, "charset=");

  if (!charset)
    charset = "ISO-8859-1";

	if (basic)
	{
		rsrcString = VCardGetStringByID(VCARD_ADDR_VIEW_COMPLETE_VCARD);

		// convert from the resource charset. 
    res = INTL_ConvertCharset(charset, "UTF-8", rsrcString, PL_strlen(rsrcString), 
                              &converted, &converted_length);
    if ( (res != 0) || (converted == NULL) )
			converted = rsrcString;

    htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showAdvanced%d();\\\"></INPUT></FORM>", 
			                        converted, s_unique);
	}
	else 
	{
		rsrcString = VCardGetStringByID(VCARD_ADDR_VIEW_CONDENSED_VCARD);
    res = INTL_ConvertCharset(charset, "UTF-8", rsrcString, PL_strlen(rsrcString), 
                              &converted, &converted_length);
    if ( (res != 0) || (converted == NULL) )
			converted = rsrcString;

		htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showBasic%d();\\\"></INPUT></FORM>", 
			converted, s_unique);
	}

	if (converted != rsrcString)
  {
		PR_FREEIF(converted);
  }
  else
  {
    PR_FREEIF(rsrcString);
  }

	rsrcString = VCardGetStringByID(VCARD_MSG_ADD_TO_ADDR_BOOK);

  res = INTL_ConvertCharset(charset, "UTF-8", rsrcString, PL_strlen(rsrcString), 
                            &converted, &converted_length);
  if ( (res != 0) || (converted == NULL) )
		converted = rsrcString;

	htmlLine2 = PR_smprintf ("<FORM name=form1 METHOD=get ACTION=\"addbook:add\"><INPUT TYPE=hidden name=vcard VALUE=\"%s\"><INPUT type=submit value=\"%s\"></INPUT></FORM>",
		vEscCard, converted);

	if (converted != rsrcString)
  {
		PR_FREEIF(converted);
  }
  else
  {
    PR_FREEIF(rsrcString);
  }

	if (!htmlLine1 && !htmlLine2)
	{
		PR_FREEIF (vEscCard);
		PR_FREEIF (htmlLine1);
		PR_FREEIF (htmlLine2);
		return VCARD_OUT_OF_MEMORY;
	}
	
	status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
	if (status < 0) goto FAIL;

	/*
	 * WARNING: This HTML is written to the stream using JavaScript,
	 * because we don't want this button to be displayed when JS has
	 * been switched off (in the whole client or just in email). The
	 * button won't work when JS is switched off (since it calls a JS
	 * function), so then we don't want the button to be displayed.
	 *   -- erik (and jfriend)
	 */
	status = WriteEachLineToStream (obj, "<SCRIPT>document.write(\"");
	if (status < 0) goto FAIL;

	status = WriteLineToStream (obj, htmlLine1);
	if (status < 0) goto FAIL;

	status = WriteEachLineToStream (obj, "\")</SCRIPT>");
	if (status < 0) goto FAIL;

	status = WriteLineToStream (obj, htmlLine2);
	if (status < 0) goto FAIL;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) goto FAIL;

	FAIL:
	PR_FREEIF (vEscCard);
	PR_FREEIF (htmlLine1);
	PR_FREEIF (htmlLine2);

	return status;
}

static int BeginLayer(MimeObject *obj, PRBool basic)
{
	int status = 0;
	char * captionLine = NULL;

	if (basic)
	{
    //CSS: START OF DIV		
    captionLine = PR_smprintf ("<DIV ID=basic%d style=\"position: 'absolute';\">", s_unique);
	}
	else
	{
    //CSS: START OF DIV		
		captionLine = PR_smprintf ("<DIV ID=advanced%d style=\"position: 'absolute'; display: none;\">", s_unique);
  }

	if (captionLine)
	{
		status = WriteEachLineToStream (obj, captionLine);
		PR_Free(captionLine);
		if (status < 0) return status;
		status = OutputTable (obj, PR_FALSE, PR_FALSE, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, NULL, "TOP", NULL, NULL);
		if (status < 0) return status;
		status = OutputTable (obj, PR_FALSE, PR_TRUE, "0", "0", "#FFFFFF");
		if (status < 0) return status;
		if (basic)
		{
			status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
			if (status < 0) return status;
			status = OutputTableRowOrData(obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
			if (status < 0) return status;
		}
		else
		{
			status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL, NULL);
			if (status < 0) return status;
			status = OutputTableRowOrData(obj, PR_FALSE, PR_FALSE, NULL, NULL, NULL, NULL);
			if (status < 0) return status;
		}

		status = OutputTable (obj, PR_FALSE, PR_FALSE, "4", NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
		status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, NULL, NULL, NULL, NULL);
		if (status < 0) return status;
	}
	else
		status = VCARD_OUT_OF_MEMORY;

	return status;
}

static int EndLayer(MimeObject *obj, PRBool basic, VObject* v)
{
	int status = 0;
	char * captionLine = NULL;

	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;

	status = OutputButtons(obj, basic, v);
	if (status < 0) return status;

	status = OutputTableRowOrData (obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
	if (status < 0) return status;
	status = OutputTable (obj, PR_TRUE, PR_FALSE, NULL, NULL, NULL);
	if (status < 0) return status;

	if (!basic) 
	{
    //CSS: END OF DIV		
		status = WriteEachLineToStream (obj, "</DIV>");
    if (status < 0) return status;
		status = WriteEachLineToStream (obj, "<P><SCRIPT>");
		if (status < 0) return status;
		captionLine = PR_smprintf ("function showAdvanced%d() {", s_unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		PR_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
    //CSS: JS
		captionLine = PR_smprintf ("document.getElementById(\"basic%d\").style.display = \"none\";", s_unique);
    if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		PR_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
    //CSS: JS
    captionLine = PR_smprintf ("document.getElementById(\"advanced%d\").style.display = \"block\";", s_unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		PR_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
		status = WriteEachLineToStream (obj, "};");
		if (status < 0) return status;
		captionLine = PR_smprintf ("function showBasic%d() {", s_unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		PR_FREEIF (captionLine);
		captionLine = NULL;
		if (status < 0) return status;
    //CSS: JS
 		captionLine = PR_smprintf ("document.getElementById(\"advanced%d\").style.display = \"none\";", s_unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		if (status < 0) return status;
    //CSS: JS
 		captionLine = PR_smprintf ("document.getElementById(\"basic%d\").style.display = \"block\";", s_unique);
		if (captionLine)
			status = WriteEachLineToStream (obj, captionLine);
		PR_FREEIF (captionLine);
		if (status < 0) return status;
		status = WriteEachLineToStream (obj, "}; </SCRIPT></P>");
	} 
	else {
    //CSS: END DIV
		status = WriteEachLineToStream (obj, "</DIV>");
  }

	if (status < 0) return status;
	return 0;
}

static int EndVCard (MimeObject *obj)
{
	int status = 0;

	/* Scribble HTML-ending stuff into the stream */
	char htmlFooters[32];
	PR_snprintf (htmlFooters, sizeof(htmlFooters), "</BODY>%s</HTML>%s", MSG_LINEBREAK, MSG_LINEBREAK);
	status = COM_MimeObject_write(obj, htmlFooters, PL_strlen(htmlFooters), PR_FALSE);

	if (status < 0) return status;

    if (obj->options && obj->options->set_html_state_fn) {
        status = obj->options->set_html_state_fn(obj->options->stream_closure,
                                                 PR_TRUE,   /* layer_encapulate_p */
                                                 PR_FALSE,  /* start_p */
                                                 PR_FALSE); /* abort_p */
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
                                                 PR_TRUE,   /* layer_encapulate_p */
                                                 PR_TRUE,   /* start_p */
                                                 PR_FALSE); /* abort_p */
        if (status < 0) return status;
    }
    
	s_unique++;
	PR_snprintf (htmlHeaders, sizeof(htmlHeaders), "<HTML>%s<BODY>%s", LINEBREAK, LINEBREAK);
    status = COM_MimeObject_write(obj, htmlHeaders, PL_strlen(htmlHeaders), PR_TRUE);

	if (status < 0) return status;

	return 0;
}

static int WriteOutVCard (MimeObject *obj, VObject* v)
{
	int status = 0;

	status = BeginVCard (obj);
	if (status < 0) return status;
	
	/* write out basic layer */
	status = BeginLayer(obj, PR_TRUE);
	if (status < 0) return status;
	status = OutputBasicVcard(obj, v);
	if (status < 0) return status;
	status = EndLayer(obj, PR_TRUE, v);
	if (status < 0) return status;

	/* write out advanced layer */
	status = BeginLayer(obj, PR_FALSE);
	if (status < 0) return status;
	status = OutputAdvancedVcard(obj, v);
	if (status < 0) return status;
	status = EndLayer(obj, PR_FALSE, v);
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
  char    *tString = NULL;

	if (domProp) {
    tString = VCardGetStringByID(VCARD_LDAP_DOM_TYPE);
	}
	if (intlProp) {
		tString = VCardGetStringByID(VCARD_LDAP_INTL_TYPE);
	}
	if (postal) {
		tString = VCardGetStringByID(VCARD_LDAP_POSTAL_TYPE);
	}
	if (parcel) {
		tString = VCardGetStringByID(VCARD_LDAP_PARCEL_TYPE);
	}
	if (home) {
		tString = VCardGetStringByID(VCARD_LDAP_HOME_TYPE);
	}
	if (work) {
		tString = VCardGetStringByID(VCARD_LDAP_WORK_TYPE);
	}

  if (tString)
  {
    vCard_SACat (&(*attribName), " ");
    vCard_SACat (&(*attribName), tString);
    PR_FREEIF(tString);
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
  char    *tString = NULL;

	if (prefProp) {
		tString = VCardGetStringByID(VCARD_LDAP_PREF_TYPE);
	}
	if (home) {
		tString = VCardGetStringByID(VCARD_LDAP_HOME_TYPE);
	}
	if (work) {
		tString = VCardGetStringByID(VCARD_LDAP_WORK_TYPE);
	}
	if (voiceProp) {
		tString = VCardGetStringByID(VCARD_LDAP_VOICE_TYPE);
	}
	if (fax) {
		tString = VCardGetStringByID(VCARD_LDAP_FAX_TYPE);
	}
	if (msg) {
		tString = VCardGetStringByID(VCARD_LDAP_MSG_TYPE);
	}
	if (cell) {
		tString = VCardGetStringByID(VCARD_LDAP_CELL_TYPE);
	}
	if (pager) {
		tString = VCardGetStringByID(VCARD_LDAP_PAGER_TYPE);
	}
	if (bbs) {
		tString = VCardGetStringByID(VCARD_LDAP_BBS_TYPE);
	}

  if (tString)
  {
    vCard_SACat (&(*attribName), " ");
    vCard_SACat (&(*attribName), tString);
    PR_FREEIF(tString);
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
  char     *tString = NULL;

	if (prefProp) {
		tString = VCardGetStringByID(VCARD_LDAP_PREF_TYPE);
	}
	if (home) {
		tString = VCardGetStringByID(VCARD_LDAP_HOME_TYPE);
	}
	if (work) {
		tString = VCardGetStringByID(VCARD_LDAP_WORK_TYPE);
	}
	if (aol) {
		tString = VCardGetStringByID(VCARD_LDAP_AOL_TYPE);
	}
	if (applelink) {
		tString = VCardGetStringByID(VCARD_LDAP_APPLELINK_TYPE);
	}
	if (att) {
		tString = VCardGetStringByID(VCARD_LDAP_ATTMAIL_TYPE);
	}
	if (cis) {
		tString = VCardGetStringByID(VCARD_LDAP_CSI_TYPE);
	}
	if (eworld) {
		tString = VCardGetStringByID(VCARD_LDAP_EWORLD_TYPE);
	}
	if (internet) {
		tString = VCardGetStringByID(VCARD_LDAP_INTERNET_TYPE);
	}
	if (ibmmail) {
		tString = VCardGetStringByID(VCARD_LDAP_IBMMAIL_TYPE);
	}
	if (mcimail) {
		tString = VCardGetStringByID(VCARD_LDAP_MCIMAIL_TYPE);
	}
	if (powershare) {
		tString = VCardGetStringByID(VCARD_LDAP_POWERSHARE_TYPE);
	}
	if (prodigy) {
		tString = VCardGetStringByID(VCARD_LDAP_PRODIGY_TYPE);
	}
	if (telex) {
		tString = VCardGetStringByID(VCARD_LDAP_TLX_TYPE);
	}
	if (x400) {
		tString = VCardGetStringByID(VCARD_LDAP_X400);
	}

  if (tString)
  {
    vCard_SACat (&(*attribName), " ");
    vCard_SACat (&(*attribName), tString);
    PR_FREEIF(tString);
  }
}

static int WriteOutEachVCardPhoneProperty (MimeObject *obj, VObject* o)
{
	char *attribName = NULL;
	char *value = NULL;
	int status = 0;

	if (vObjectName(o)) 
	{
		if (PL_strcasecmp (VCTelephoneProp, vObjectName(o)) == 0) 
		{
			if (VALUE_TYPE(o)) 
			{
				GetTelephoneProperties(o, &attribName);
				if (!attribName)
					attribName = VCardGetStringByID(VCARD_LDAP_PHONE_NUMBER);
				attribName = vCard_SACat(&attribName, ": ");
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
				{
					attribName = vCard_SACat (&attribName, value);
					PR_FREEIF (value);
					if (attribName)
					{
						status = OutputFont(obj, PR_FALSE, "-1", NULL);
						if (status < 0) {
							PR_FREEIF (attribName);
							return status;
						}
						status = WriteLineToStream (obj, attribName);
						if (status < 0) {
							PR_FREEIF (attribName);
							return status;
						}
						status = OutputFont(obj, PR_TRUE, NULL, NULL);
						if (status < 0) {
							PR_FREEIF (attribName);
							return status;
						}
					}
					PR_FREEIF (attribName);
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
	int status = 0;

	if (vObjectName(o)) {

		if (PL_strcasecmp (VCPhotoProp, vObjectName(o)) == 0) {
			VObject* urlProp = isAPropertyOf(o, VCURLProp);
			if (urlProp) {
				attribName = VCardGetStringByID(VCARD_LDAP_PHOTOGRAPH);
				/* format the value string to the url */
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
					url = PR_smprintf ("<IMG SRC = ""%s""", value);
				PR_FREEIF (value);
				value = NULL;
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCBirthDateProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_BIRTHDAY);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCDeliveryLabelProp, vObjectName(o)) == 0) {
			attribName = VCardGetStringByID(VCARD_LDAP_LABEL);
			GetAddressProperties(o, &attribName);
			value = fakeCString (vObjectUStringZValue(o));
			goto DOWRITE;
		}

		if (PL_strcasecmp (VCEmailAddressProp, vObjectName(o)) == 0) {
			if ((*numEmail) != 1)
			{
				if (VALUE_TYPE(o)) {
					(*numEmail)++;
					attribName = VCardGetStringByID(VCARD_LDAP_EMAIL_ADDRESS);
					GetEmailProperties(o, &attribName);
					value = fakeCString (vObjectUStringZValue(o));
					goto DOWRITE;
				};
			}
		}

		if (PL_strcasecmp (VCFamilyNameProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_SURNAME);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCGivenNameProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_GIVEN_NAME);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCNamePrefixesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_NAME_PREFIX);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCNameSuffixesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_NAME_SUFFIX);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCAdditionalNamesProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_MIDDLE_NAME);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCMailerProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_MAILER);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCTimeZoneProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_TZ);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCGeoProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_GEO);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCBusinessRoleProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_ROLE);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCLogoProp, vObjectName(o)) == 0) {
			VObject* urlProp = isAPropertyOf(o, VCURLProp);
			if (urlProp) {
				attribName = VCardGetStringByID(VCARD_LDAP_LOGO);
				/* format the value string to the url */
				value = fakeCString (vObjectUStringZValue(o));
				if (value)
					url = PR_smprintf ("<IMG SRC = ""%s""", value);
				PR_FREEIF (value);
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCAgentProp, vObjectName(o)) == 0) {
			attribName = VCardGetStringByID(VCARD_LDAP_SECRETARY);
			goto DOWRITE;
		}

		if (PL_strcasecmp (VCLastRevisedProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_REVISION);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (PL_strcasecmp (VCPronunciationProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_SOUND);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}


		if (PL_strcasecmp (VCVersionProp, vObjectName(o)) == 0) {
			if (VALUE_TYPE(o)) {
				attribName = VCardGetStringByID(VCARD_LDAP_VERSION);
				value = fakeCString (vObjectUStringZValue(o));
				goto DOWRITE;
			}
		}

		if (!attribName) 
			return 0;
	
	DOWRITE:
		OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
		OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
		if (attribName) { 
			OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteAttribute (obj, attribName);
			PR_Free (attribName);
			OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
		}

		if (value) {
			OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteValue (obj, value);
			PR_Free (value);
			OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
		}

		if (url) {
			OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
			status = WriteValue (obj, url);
			PR_Free (url);
			OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
		}
		OutputTableRowOrData(obj, PR_TRUE, PR_TRUE, NULL, NULL, NULL, NULL);
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
	int     status = 0;
	char    *htmlLine;
	int     htmlLen ;
  char    *charset = PL_strstr(obj->content_type, "charset=");
  char    *converted = NULL;
  PRInt32 converted_length;
  PRInt32 res;

  if (!charset)
    charset = "ISO-8859-1";

  // convert from the resource charset. 
  res = INTL_ConvertCharset(charset, "UTF-8", line, PL_strlen(line), 
                            &converted, &converted_length);
  if ( (res != 0) || (converted == NULL) )
    converted = (char *)line;
  else
    converted[converted_length] = '\0';

  htmlLen = PL_strlen(converted) + PL_strlen("<DT></DT>") + 1;;

	htmlLine = (char *) PR_MALLOC (htmlLen);
	if (htmlLine)
	{
    htmlLine[0] = '\0';
    PL_strcat (htmlLine, "<DT>");
    PL_strcat (htmlLine, converted);
    PL_strcat (htmlLine, "</DT>");
    status = COM_MimeObject_write(obj, htmlLine, PL_strlen(htmlLine), PR_TRUE);
    PR_Free ((void*) htmlLine);
	}
	else
		status = VCARD_OUT_OF_MEMORY;

  if (converted != line)
    PR_FREEIF(converted);
	return status;
}

static int WriteAttribute (MimeObject *obj, const char *attrib)
{
	int status = 0;
	OutputFont(obj, PR_FALSE, "-1", NULL);
	status  = WriteLineToStream (obj, attrib);
	OutputFont(obj, PR_TRUE, NULL, NULL);
	return status;
}


static int WriteValue (MimeObject *obj, const char *value)
{
	int status = 0;
	OutputFont(obj, PR_FALSE, "-1", NULL);
	status  = WriteLineToStream (obj, value);
	OutputFont(obj, PR_TRUE, NULL, NULL);
	return status;
}

/* Strip CR+LF+<whitespace> runs within (original).
   Since the string at (original) can only shrink,
   this conversion is done in place. (original)
   is returned. */
extern "C" char *
MIME_StripContinuations(char *original)
{
	char *p1, *p2;

	/* If we were given a null string, return it as is */
	if (!original) return NULL;

	/* Start source and dest pointers at the beginning */
	p1 = p2 = original;

	while(*p2)
	{
		/* p2 runs ahead at (CR and/or LF) + <space> */
		if ((p2[0] == CR) || (p2[0] == LF))
		{
			/* move past (CR and/or LF) + whitespace following */	
			do
			{
				p2++;
			}
			while((*p2 == CR) || (*p2 == LF) || IS_SPACE(*p2));

			if (*p2 == '\0') continue; /* drop out of loop at end of string*/
		}

		/* Copy the next non-linebreaking char */
		*p1 = *p2;
		p1++; p2++;
	}
	*p1 = '\0';

	return original;
}

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
vCard_SACopy (char **destination, const char *source)
{
  if(*destination)
  {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source)
  {
    *destination = NULL;
  }
  else 
  {
    *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (*destination == NULL) 
      return(NULL);
    
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
extern "C"  char *
vCard_SACat (char **destination, const char *source)
{
  if (source && *source)
  {
    if (*destination)
    {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination + length, source);
    }
    else
    {
      *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination, source);
    }
  }
  return *destination;
}


/* This is the next generation string retrieval call */
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

char *VCARD_URL = {"resource:/chrome/messenger/content/default/vcard_en.properties"};

extern "C" 
char *
VCardGetStringByIDREAL(PRInt32 stringID)
{
  nsresult    res = NS_OK;
  char*       propertyURL;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->CopyCharPref("mail.strings.vcard", &propertyURL);

  if (!NS_SUCCEEDED(res) || !prefs)
    propertyURL = VCARD_URL;

  nsCOMPtr<nsIURI> pURI;
  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
  {
    if (propertyURL != VCARD_URL)
      PR_FREEIF(propertyURL);
    return PL_strdup("???");   // Don't I18N this string...failsafe return value
  }

  res = pService->NewURI(propertyURL, nsnull, getter_AddRefs(pURI));
  if (NS_FAILED(res))
  {
    if (propertyURL != VCARD_URL)
      PR_FREEIF(propertyURL);
    return PL_strdup("???");   // Don't I18N this string...failsafe return value
  }

  if (propertyURL != VCARD_URL)
  {
    PR_FREEIF(propertyURL);
    propertyURL = nsnull;
  }

  NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
  if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
  {
    nsILocale   *locale = nsnull;
#if 1
    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(VCARD_URL, locale, &sBundle);
#else
   res = pNetService->CreateURL(&url, nsString(VCARD_URL), nsnull, nsnull, nsnull);
    if (NS_FAILED(res)) 
    {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
    }

    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(pURI, locale, &sBundle);
#endif

    if (NS_FAILED(res)) 
    {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
    }

    nsAutoString v("");
#if 1
    PRUnichar *ptrv = nsnull;
    res = sBundle->GetStringFromID(stringID, &ptrv);
    v = ptrv;
#else
    res = sBundle->GetStringFromID(stringID, v);
#endif
    if (NS_FAILED(res)) 
    {
      char    buf[128];

      PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
      return PL_strdup(buf);
    }

    // Here we need to return a new copy of the string
    char      *returnBuffer = NULL;
    PRInt32   bufferLen = v.Length() + 1;

    returnBuffer = (char *)PR_MALLOC(bufferLen);
    if (returnBuffer)
    {
      v.ToCString(returnBuffer, bufferLen);
      return returnBuffer;
    }
  }

  return PL_strdup("???");   // Don't I18N this string...failsafe return value
}

extern "C" 
char *
VCardGetStringByID(PRInt32 stringID)
{
  if (-1000 == stringID) return PL_strdup("Application is out of memory.");
  if (1001 == stringID) return PL_strdup("State");
  if (1002 == stringID) return PL_strdup("Domestic");
  if (1003 == stringID) return PL_strdup("International");
  if (1004 == stringID) return PL_strdup("Postal");
  if (1005 == stringID) return PL_strdup("Parcel");
  if (1006 == stringID) return PL_strdup("Work");
  if (1007 == stringID) return PL_strdup("Home");
  if (1008 == stringID) return PL_strdup("Preferred");
  if (1009 == stringID) return PL_strdup("Voice");
  if (1010 == stringID) return PL_strdup("Fax");
  if (1011 == stringID) return PL_strdup("Message");
  if (1012 == stringID) return PL_strdup("Cellular");
  if (1013 == stringID) return PL_strdup("Pager");
  if (1014 == stringID) return PL_strdup("BBS");
  if (1015 == stringID) return PL_strdup("Modem");
  if (1016 == stringID) return PL_strdup("Car");
  if (1017 == stringID) return PL_strdup("ISDN");
  if (1018 == stringID) return PL_strdup("Video");
  if (1019 == stringID) return PL_strdup("AOL");
  if (1020 == stringID) return PL_strdup("Applelink");
  if (1021 == stringID) return PL_strdup("AT&T Mail");
  if (1022 == stringID) return PL_strdup("Compuserve");
  if (1023 == stringID) return PL_strdup("eWorld");
  if (1024 == stringID) return PL_strdup("Internet");
  if (1025 == stringID) return PL_strdup("IBM Mail");
  if (1026 == stringID) return PL_strdup("MCI Mail");
  if (1027 == stringID) return PL_strdup("Powershare");
  if (1028 == stringID) return PL_strdup("Prodigy");
  if (1029 == stringID) return PL_strdup("Telex");
  if (1030 == stringID) return PL_strdup("Additional Name");
  if (1031 == stringID) return PL_strdup("Prefix");
  if (1032 == stringID) return PL_strdup("Suffix");
  if (1033 == stringID) return PL_strdup("Time Zone");
  if (1034 == stringID) return PL_strdup("Geographic Position");
  if (1035 == stringID) return PL_strdup("Sound");
  if (1036 == stringID) return PL_strdup("Revision");
  if (1037 == stringID) return PL_strdup("Version");
  if (1038 == stringID) return PL_strdup("Public Key");
  if (1039 == stringID) return PL_strdup("Logo");
  if (1040 == stringID) return PL_strdup("Birthday");
  if (1041 == stringID) return PL_strdup("X400");
  if (1042 == stringID) return PL_strdup("Address");
  if (1043 == stringID) return PL_strdup("Label");
  if (1044 == stringID) return PL_strdup("Mailer");
  if (1045 == stringID) return PL_strdup("Role");
  if (1046 == stringID) return PL_strdup("Update From");
  if (1047 == stringID) return PL_strdup("Conference Address");
  if (1048 == stringID) return PL_strdup("HTML Mail");
  if (1049 == stringID) return PL_strdup("Add to Personal Address Book");
  if (1050 == stringID) return PL_strdup("Additional Information:");
  if (1051 == stringID) return PL_strdup("View Complete Card");
  if (1052 == stringID) return PL_strdup("View Condensed Card");
  if (1053 == stringID) return PL_strdup("Conference Address");
  if (1054 == stringID) return PL_strdup("Default Directory Server");
  if (1055 == stringID) return PL_strdup("Specific Directory Server");
  if (1056 == stringID) return PL_strdup("Hostname or IP Address");
  if (1057 == stringID) return PL_strdup("Phone Number");
  if (1058 == stringID) return PL_strdup("Photograph");
  if (1059 == stringID) return PL_strdup("Email");
  if (1060 == stringID) return PL_strdup("Last Name");
  if (1061 == stringID) return PL_strdup("First Name");
  if (1062 == stringID) return PL_strdup("Administrative Assistant");

  char    buf[128];
  
  PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
  return PL_strdup(buf);
}
