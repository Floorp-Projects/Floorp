/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "msgCore.h"
#include "prlog.h"
#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsIMsgVCardService.h"
#include "mimecth.h"
#include "mimexpcom.h"
#include "mimevcrd.h"
#include "nsEscape.h"
#include "nsIURI.h"
#include "nsMsgI18N.h"
#include "nsMsgUtils.h"
#include "nsReadableUtils.h"

#include "nsIStringBundle.h"
#include "nsVCardStringResources.h"
#include "nsPrintfCString.h"

#include "nsCRT.h"
#include "prprf.h"

// String bundles...
#ifndef XP_MAC
static nsCOMPtr<nsIStringBundle>   stringBundle = nsnull;
#endif

static int MimeInlineTextVCard_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextVCard_parse_eof (MimeObject *, PRBool);
static int MimeInlineTextVCard_parse_begin (MimeObject *obj);

static int s_unique = 0;

static int BeginVCard (MimeObject *obj);
static int EndVCard (MimeObject *obj);
static int WriteOutVCard (MimeObject *obj, VObject* v);

#ifdef MOZ_THUNDERBIRD
static int GenerateVCardData(MimeObject * aMimeObj, VObject* aVcard);
static int OutputVcardAttribute(MimeObject *aMimeObj, VObject *aVcard, const char* id, nsACString& vCardOutput);
static int OutputBasicVcard(MimeObject *aMimeObj, VObject *aVcard, nsACString& vCardOutput);
#else
static int WriteOutEachVCardProperty (MimeObject *obj, VObject* v, int* numEmail);
static int WriteOutVCardProperties (MimeObject *obj, VObject* v, int* numEmail);
static int WriteLineToStream (MimeObject *obj, const char *line, PRBool aDoCharConversion);

static void GetEmailProperties (VObject* o, char ** attribName);
static void GetTelephoneProperties (VObject* o, char ** attribName);
static void GetAddressProperties (VObject* o, char ** attribName);
static int WriteValue (MimeObject *obj, const char *);
static int WriteAttribute (MimeObject *obj, const char *);
static int WriteOutVCardPhoneProperties (MimeObject *obj, VObject* v);
static int WriteOutEachVCardPhoneProperty (MimeObject *obj, VObject* o);
#endif

typedef struct
  {
    const char *attributeName;
    int resourceId;
  } AttributeName;

#define kNumAttributes 12

#define     VCARD_URL     "chrome://messenger/locale/vcard.properties"
#define     MSGVCARDSERVICE_CONTRACT_ID "@mozilla.org/addressbook/msgvcardservice;1"

/* This is the object definition. Note: we will set the superclass 
   to NULL and manually set this on the class creation */
MimeDefClass(MimeInlineTextVCard, MimeInlineTextVCardClass,
             mimeInlineTextVCardClass, NULL);

extern "C" MimeObjectClass *
MIME_VCardCreateContentTypeHandlerClass(const char *content_type, 
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
  NS_ASSERTION(!oclass->class_initialized, "1.1 <rhp@netscape.com> 19 Mar 1999 12:11");
  oclass->parse_begin = MimeInlineTextVCard_parse_begin;
  oclass->parse_line  = MimeInlineTextVCard_parse_line;
  oclass->parse_eof   = MimeInlineTextVCard_parse_eof;
  return 0;
}

static int
MimeInlineTextVCard_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)COM_GetmimeLeafClass())->parse_begin(obj);
  MimeInlineTextVCardClass *clazz;
  if (status < 0) return status;
  
  if (!obj->output_p) return 0;
  if (!obj->options || !obj->options->write_html_p) return 0;
  
  /* This is a fine place to write out any HTML before the real meat begins.
  In this sample code, we tell it to start a table. */
  
  clazz = ((MimeInlineTextVCardClass *) obj->clazz);
  /* initialize vcard string to empty; */
  NS_MsgSACopy(&(clazz->vCardString), "");
  
  obj->options->state->separator_suppressed_p = PR_TRUE;
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
  // This routine gets fed each line of data, one at a time.    
  char* linestring;
  MimeInlineTextVCardClass *clazz = ((MimeInlineTextVCardClass *) obj->clazz);
  
  if (!obj->output_p) return 0;
  if (!obj->options || !obj->options->output_fn) return 0;
  if (!obj->options->write_html_p) 
  {
    return COM_MimeObject_write(obj, line, length, PR_TRUE);
  }
  
  linestring = (char *) PR_MALLOC (length + 1);
  memset(linestring, 0, (length + 1));
  
  if (linestring) 
  {
    strcpySafe((char *)linestring, line, length + 1);
    NS_MsgSACat (&clazz->vCardString, linestring);
    PR_Free (linestring);
  }
  
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
static PRInt32 INTL_ConvertCharset(const char* from_charset, const char* to_charset,
                                   const char* inBuffer, const PRInt32 inLength,
                                   char** outBuffer, PRInt32* outLength)
{
  nsresult res;

  // invalid input
  if (nsnull == from_charset || nsnull == to_charset || 
      '\0' == *from_charset || '\0' == *to_charset || nsnull == inBuffer) 
    return -1;

  // from to identical
  if (!nsCRT::strcasecmp(from_charset, to_charset))
    return -1;

  // us-ascii is a subset of utf-8
  if ((!nsCRT::strcasecmp(from_charset, "us-ascii") && !nsCRT::strcasecmp(to_charset, "UTF-8")) ||
      (!nsCRT::strcasecmp(from_charset, "UTF-8") && !nsCRT::strcasecmp(to_charset, "us-ascii")))
    return -1;


  nsAutoString outString;
  
  res = ConvertToUnicode(from_charset, inBuffer, outString);

  // known bug in 4.x, it mixes Shift_JIS (or EUC-JP) and ISO-2022-JP in vCard fields
  if (NS_ERROR_MODULE_UCONV == NS_ERROR_GET_MODULE(res)) {
    if (!nsCRT::strcasecmp("ISO-2022-JP", from_charset)) {
      res = ConvertToUnicode("Shift_JIS", inBuffer, outString);
      if (NS_ERROR_MODULE_UCONV == NS_ERROR_GET_MODULE(res)) {
        res = ConvertToUnicode("EUC-JP", inBuffer, outString);
      }
    }
  }

  if (NS_SUCCEEDED(res)) {
    nsCAutoString result;
    res = ConvertFromUnicode(to_charset, outString, result);
    if (NS_SUCCEEDED(res)) {
      *outLength = result.Length();
      *outBuffer = PL_strdup(result.get());
    }
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}
////////////////////////////////////////////////////////////////////////////////
static int
MimeInlineTextVCard_parse_eof (MimeObject *obj, PRBool abort_p)
{
  nsCOMPtr<nsIMsgVCardService> vCardService = 
             do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;

  int status = 0;
  MimeInlineTextVCardClass *clazz = ((MimeInlineTextVCardClass *) obj->clazz);
  
  VObject *t, *v;
  
  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  //    status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  status = ((MimeObjectClass*)COM_GetmimeInlineTextClass())->parse_eof(obj, abort_p);
  if (status < 0) return status;
 
  // Don't quote vCards...
  if (  (obj->options) && 
    ((obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting) ||
    (obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting))
    )
    return 0;

  if (!clazz->vCardString) return 0;
  
  v = vCardService->Parse_MIME(clazz->vCardString, strlen(clazz->vCardString));
  NS_ASSERTION(v, "parse of vCard failed");
  
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
      v = vCardService->NextVObjectInList(v);
    }
    
    (void)vCardService->CleanVObject(t);
  }
  
  if (status < 0) 
    return status;
  
  return 0;
}

static int EndVCard (MimeObject *obj)
{
  int status = 0;

  /* Scribble HTML-ending stuff into the stream */
  char htmlFooters[32];
  PR_snprintf (htmlFooters, sizeof(htmlFooters), "</BODY>%s</HTML>%s", MSG_LINEBREAK, MSG_LINEBREAK);
  status = COM_MimeObject_write(obj, htmlFooters, strlen(htmlFooters), PR_FALSE);

  if (status < 0) return status;

  return 0;
}

static int BeginVCard (MimeObject *obj)
{
  int status = 0;

  /* Scribble HTML-starting stuff into the stream */
  char htmlHeaders[32];

  s_unique++;
  PR_snprintf (htmlHeaders, sizeof(htmlHeaders), "<HTML>%s<BODY>%s", MSG_LINEBREAK, MSG_LINEBREAK);
    status = COM_MimeObject_write(obj, htmlHeaders, strlen(htmlHeaders), PR_TRUE);

  if (status < 0) return status;

  return 0;
}

#ifndef MOZ_THUNDERBIRD

static int WriteEachLineToStream (MimeObject *obj, const char *line)
{
  int status = 0;
  char *htmlLine;
  int htmlLen = strlen(line) + 1;

  htmlLine = (char *) PR_MALLOC (htmlLen);
  if (htmlLine)
  {
    htmlLine[0] = '\0';
    PL_strcat (htmlLine, line);
      status = COM_MimeObject_write(obj, htmlLine, strlen(htmlLine), PR_TRUE);
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
    int htmlLen = strlen("<TABLE>") + 1;
    if (border)
      htmlLen += strlen (" BORDER");
    if (cellspacing)
      htmlLen += strlen(" CELLSPACING=") + strlen(cellspacing);
    if (cellpadding)
      htmlLen += strlen(" CELLPADDING=") + strlen(cellpadding);
    if (bgcolor)
      htmlLen += strlen(" BGCOLOR=") + strlen(bgcolor);
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

      status = COM_MimeObject_write(obj, htmlLine, strlen(htmlLine), PR_TRUE);
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
    int htmlLen = strlen("<TR>") + 1;
    if (align)
      htmlLen += strlen(" ALIGN=") + strlen(align);
    if (colspan)
      htmlLen += strlen(" COLSPAN=") + strlen(colspan);
    if (width)
      htmlLen += strlen(" WIDTH=") + strlen(width);
    if (valign)
      htmlLen += strlen(" VALIGN=") + strlen(valign);
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

      status = COM_MimeObject_write(obj, htmlLine, strlen(htmlLine), PR_TRUE);
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
    int htmlLen = strlen("<FONT>") + 1;
    if (size)
      htmlLen += strlen(" SIZE=") + strlen(size);
    if (color)
      htmlLen += strlen(" COLOR=") + strlen(color);
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

      status = COM_MimeObject_write(obj, htmlLine, strlen(htmlLine), PR_TRUE);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
             do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;

  prop = vCardService->IsAPropertyOf(v, id);
  if (prop)
    if (VALUE_TYPE(prop))
    {
      if (VALUE_TYPE(prop) != VCVT_RAW)
      {
        string = vCardService->FakeCString(prop);
      }
      else
      {
        string = vCardService->VObjectAnyValue(prop);
      }
      if (string) {
        status = OutputFont(obj, PR_FALSE, "-1", NULL);
        if (status < 0) {
          PR_FREEIF (string);
          return status;
        }
        status = WriteLineToStream (obj, string, PR_TRUE);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
      do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;
  
  /* get the name and email */
  prop = vCardService->IsAPropertyOf(v, VCFullNameProp);
  if (prop)
  {
    if (VALUE_TYPE(prop))
    {
      if (VALUE_TYPE(prop) != VCVT_RAW)
      {
        namestring = vCardService->FakeCString(prop);
      }
      else
      {
        namestring = vCardService->VObjectAnyValue(prop);
      }
      if (namestring)
      {
        prop = vCardService->IsAPropertyOf(v, VCURLProp);
        if (prop)
        {
          urlstring = vCardService->FakeCString(prop);
          if (urlstring)
            htmlLine1 = PR_smprintf ("<A HREF=""%s"" PRIVATE>%s</A> ", urlstring, namestring);
          else
            htmlLine1 = PR_smprintf ("%s ", namestring);
          PR_FREEIF (urlstring);
        }
        else
          htmlLine1 = PR_smprintf ("%s ", namestring);

        /* get the email address */
        prop = vCardService->IsAPropertyOf(v, VCEmailAddressProp);
        if (prop)
        {
         emailstring = vCardService->FakeCString(prop);
          if (emailstring)
          {
            /* if its an internet address prepend the mailto url */
            prop2 = vCardService->IsAPropertyOf(prop, VCInternetProp);
            if (prop2)
              htmlLine2 = PR_smprintf ("&lt;<A HREF=""mailto:%s"" PRIVATE>%s</A>&gt;", emailstring, emailstring);
            else
              htmlLine2 = PR_smprintf ("%s", emailstring);
            PR_FREEIF (emailstring);
          }
        }

        PR_FREEIF (namestring);
        if (!htmlLine1 && !htmlLine2)
        {
          PR_FREEIF (htmlLine1);
          PR_FREEIF (htmlLine2);
          return VCARD_OUT_OF_MEMORY;
        }
        else
        {
          htmlLine = NS_MsgSACat (&htmlLine, htmlLine1);
          htmlLine = NS_MsgSACat (&htmlLine, htmlLine2);
        }

        PR_FREEIF (htmlLine1);
        PR_FREEIF (htmlLine2);
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

    status = WriteLineToStream (obj, htmlLine, PR_TRUE);
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
  prop = vCardService->IsAPropertyOf(v, VCOrgProp);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
      do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;
  
  status = OutputTable (obj, PR_FALSE, PR_FALSE, "0", "0", NULL);
  if (status < 0) return status;
  /* beginning of first row */
  status = OutputTableRowOrData(obj, PR_TRUE, PR_FALSE, "LEFT", "TOP", NULL, NULL);
  if (status < 0) return status;
  status = OutputTableRowOrData (obj, PR_FALSE, PR_FALSE, "LEFT", "TOP", "3", NULL);
  if (status < 0) return status;

  /* get the name and email */
  prop = vCardService->IsAPropertyOf(v, VCFullNameProp);
  if (prop)
  {
    if (VALUE_TYPE(prop))
    {
      if (VALUE_TYPE(prop) != VCVT_RAW) 
      {
        namestring = vCardService->FakeCString(prop);
      }
      else
      {
        namestring = vCardService->VObjectAnyValue(prop);
      }
      if (namestring)
      {
        prop = vCardService->IsAPropertyOf(v, VCURLProp);
        if (prop)
        {
          urlstring = vCardService->FakeCString(prop);
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
    status = WriteLineToStream (obj, htmlLine1, PR_TRUE);
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
  prop = vCardService->IsAPropertyOf(v, VCOrgProp);
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
  prop = vCardService->IsAPropertyOf(v, VCEmailAddressProp);
  if (prop)
  {
    emailstring = vCardService->FakeCString(prop);
    if (emailstring)
    {
      numEmail++;
      /* if its an internet address prepend the mailto url */
      prop2 = vCardService->IsAPropertyOf(prop, VCInternetProp);
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
    status = WriteLineToStream (obj, htmlLine2, PR_TRUE);
    PR_FREEIF (htmlLine2);
    if (status < 0) return status;
    status = OutputFont(obj, PR_TRUE, NULL, NULL);
    if (status < 0) return status;
    /* output html mail setting only if its true */
    prop = vCardService->IsAPropertyOf(v, VCUseHTML);
    if (prop)
    {
      if (VALUE_TYPE(prop))
      {
        namestring = vCardService->FakeCString(prop);
        if (namestring)
          if (nsCRT::strcasecmp (namestring, "TRUE") == 0)
          {
            PR_FREEIF (namestring);
            status = OutputFont(obj, PR_FALSE, "-1", NULL);
            if (status < 0) return status;
            char *tString = VCardGetStringByID(VCARD_LDAP_USEHTML);
            status = WriteLineToStream (obj, tString, PR_FALSE);
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
  prop = vCardService->IsAPropertyOf(v, VCAdrProp);
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
  prop = vCardService->IsAPropertyOf(v, VCCooltalk);
  if (prop)
  {
    char *tString = VCardGetStringByID(VCARD_ADDR_CONFINFO);
    WriteLineToStream (obj, tString, PR_FALSE);
    PR_FREEIF(tString);
    if (status < 0) return status;
    prop2 = vCardService->IsAPropertyOf(prop, VCUseServer);
    if (prop2)
    {
      if (VALUE_TYPE(prop2)) 
      {
        namestring = vCardService->FakeCString(prop2);
        char *tString1 = NULL;
        if (nsCRT::strcmp (namestring, "0") == 0)
        {
          tString1 = VCardGetStringByID(VCARD_ADDR_DEFAULT_DLS);
        }
        else 
        {
          if (nsCRT::strcmp (namestring, "1") == 0)
            tString1 = VCardGetStringByID(VCARD_ADDR_SPECIFIC_DLS);
          else
            if (nsCRT::strcmp (namestring, "2") == 0)
              tString1 = VCardGetStringByID(VCARD_ADDR_HOSTNAMEIP);
        }
        
        if (tString1) 
        {
          status = WriteLineToStream (obj, tString1, PR_FALSE);
        }
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
  prop = vCardService->IsAPropertyOf(v, VCCommentProp);
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
  status = WriteLineToStream (obj, tString, PR_FALSE);
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
  char *htmlLine2 = NULL;
  char* vCard = NULL;
  char* vEscCard = NULL;
  int len = 0;
  char* rsrcString = NULL;
  
  if (!obj->options->output_vcard_buttons_p)
    return status;

  nsCOMPtr<nsIMsgVCardService> vCardService = 
      do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;

  vCard = vCardService->WriteMemoryVObjects(0, &len, v, PR_FALSE);

  if (!vCard)
    return VCARD_OUT_OF_MEMORY;

  vEscCard = nsEscape (vCard, url_XAlphas);

  PR_FREEIF (vCard);

  if (!vEscCard)
    return VCARD_OUT_OF_MEMORY;

  if (basic)
  {
    rsrcString = VCardGetStringByID(VCARD_ADDR_VIEW_COMPLETE_VCARD);
    htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showAdvanced%d();\\\"></INPUT></FORM>", 
                              rsrcString, s_unique);
  }
  else 
  {
    rsrcString = VCardGetStringByID(VCARD_ADDR_VIEW_CONDENSED_VCARD);
    htmlLine1 = PR_smprintf ("<FORM name=form1><INPUT type=reset value=\\\"%s\\\" onClick=\\\"showBasic%d();\\\"></INPUT></FORM>", 
                              rsrcString, s_unique);
  }
  PR_FREEIF(rsrcString);

  rsrcString = VCardGetStringByID(VCARD_MSG_ADD_TO_ADDR_BOOK);
  htmlLine2 = PR_smprintf("<FORM name=form1 METHOD=get ACTION=\"addbook:add?action=add\"><INPUT TYPE=hidden name=vcard VALUE=\"%s\"><INPUT type=submit value=\"%s\"></INPUT></FORM>",
                           vEscCard, rsrcString); 
  PR_FREEIF(rsrcString);

  if (!htmlLine1 || !htmlLine2)
  {
    status = VCARD_OUT_OF_MEMORY;
    goto FAIL;
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

  status = WriteLineToStream (obj, htmlLine1, PR_FALSE);
  if (status < 0) goto FAIL;

  status = WriteEachLineToStream (obj, "\")</SCRIPT>");
  if (status < 0) goto FAIL;

  status = WriteLineToStream (obj, htmlLine2, PR_FALSE);
  if (status < 0) goto FAIL;

  status = OutputTableRowOrData (obj, PR_FALSE, PR_TRUE, NULL, NULL, NULL, NULL);
  if (status < 0) goto FAIL;

  FAIL:
  PR_FREEIF (vEscCard);
  PR_FREEIF (htmlLine1);
  PR_FREEIF(htmlLine2);

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

    captionLine = PR_smprintf (
"function showAdvanced%d()\
{\
  var longDiv  = document.getElementById(\"advanced%d\");\
  var shortDiv = document.getElementById(\"basic%d\");\
  longDiv.setAttribute(\"style\", \"display:block;\");\
  shortDiv.setAttribute(\"style\", \"display:none;\");\
};\
function showBasic%d()\
{\
  var longDiv  = document.getElementById(\"advanced%d\");\
  var shortDiv = document.getElementById(\"basic%d\");\
  longDiv.setAttribute(\"style\", \"display:none;\");\
  shortDiv.setAttribute(\"style\", \"display:block;\");\
};", 
                        s_unique, s_unique, s_unique, 
                        s_unique, s_unique, s_unique);
    if (captionLine)
      status = WriteEachLineToStream (obj, captionLine);
    PR_FREEIF (captionLine);
    if (status < 0) return status;


    status = WriteEachLineToStream (obj, "</SCRIPT></P>");
  } 
  else {
    //CSS: END DIV
    status = WriteEachLineToStream (obj, "</DIV>");
  }

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
  nsCOMPtr<nsIMsgVCardService> vCardService = 
        do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
    return;
    
  VObject* domProp = vCardService->IsAPropertyOf(o, VCDomesticProp);
  VObject* intlProp = vCardService->IsAPropertyOf(o, VCInternationalProp);
  VObject* postal = vCardService->IsAPropertyOf(o, VCPostalProp);
  VObject* parcel = vCardService->IsAPropertyOf(o, VCParcelProp);
  VObject* home = vCardService->IsAPropertyOf(o, VCHomeProp);
  VObject* work = vCardService->IsAPropertyOf(o, VCWorkProp);
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
    NS_MsgSACat (&(*attribName), " ");
    NS_MsgSACat (&(*attribName), tString);
    PR_FREEIF(tString);
  }
}


static void GetTelephoneProperties (VObject* o, char ** attribName)
{
  nsCOMPtr<nsIMsgVCardService> vCardService = 
    do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
    return;

  VObject* prefProp = vCardService->IsAPropertyOf(o, VCPreferredProp);
  VObject* home = vCardService->IsAPropertyOf(o, VCHomeProp);
  VObject* work = vCardService->IsAPropertyOf(o, VCWorkProp);
  VObject* voiceProp = vCardService->IsAPropertyOf(o, VCVoiceProp);
  VObject* fax = vCardService->IsAPropertyOf(o, VCFaxProp);
  VObject* msg = vCardService->IsAPropertyOf(o, VCMessageProp);
  VObject* cell = vCardService->IsAPropertyOf(o, VCCellularProp);
  VObject* pager = vCardService->IsAPropertyOf(o, VCPagerProp);
  VObject* bbs = vCardService->IsAPropertyOf(o, VCBBSProp);
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
    NS_MsgSACat (&(*attribName), " ");
    NS_MsgSACat (&(*attribName), tString);
    PR_FREEIF(tString);
  }
}

static void GetEmailProperties (VObject* o, char ** attribName)
{
  nsCOMPtr<nsIMsgVCardService> vCardService = 
    do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
    return;

  VObject* prefProp = vCardService->IsAPropertyOf(o, VCPreferredProp);
  VObject* home = vCardService->IsAPropertyOf(o, VCHomeProp);
  VObject* work = vCardService->IsAPropertyOf(o, VCWorkProp);
  VObject* aol = vCardService->IsAPropertyOf(o, VCAOLProp);
  VObject* applelink = vCardService->IsAPropertyOf(o, VCAppleLinkProp);
  VObject* att = vCardService->IsAPropertyOf(o, VCATTMailProp);
  VObject* cis = vCardService->IsAPropertyOf(o, VCCISProp);
  VObject* eworld = vCardService->IsAPropertyOf(o, VCEWorldProp);
  VObject* internet = vCardService->IsAPropertyOf(o, VCInternetProp);
  VObject* ibmmail = vCardService->IsAPropertyOf(o, VCIBMMailProp);
  VObject* mcimail = vCardService->IsAPropertyOf(o, VCMCIMailProp);
  VObject* powershare = vCardService->IsAPropertyOf(o, VCPowerShareProp);
  VObject* prodigy = vCardService->IsAPropertyOf(o, VCProdigyProp);
  VObject* telex = vCardService->IsAPropertyOf(o, VCTLXProp);
  VObject* x400 = vCardService->IsAPropertyOf(o, VCX400Prop);
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
    NS_MsgSACat (&(*attribName), " ");
    NS_MsgSACat (&(*attribName), tString);
    PR_FREEIF(tString);
  }
}

static int WriteOutEachVCardPhoneProperty (MimeObject *obj, VObject* o)
{
  char *attribName = NULL;
  char *value = NULL;
  int status = 0;

  nsCOMPtr<nsIMsgVCardService> vCardService = 
      do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;
  
  if (vCardService->VObjectName(o)) 
  {
    if (nsCRT::strcasecmp (VCTelephoneProp, vCardService->VObjectName(o)) == 0) 
    {
      if (VALUE_TYPE(o)) 
      {
        GetTelephoneProperties(o, &attribName);
        if (!attribName)
          attribName = VCardGetStringByID(VCARD_LDAP_PHONE_NUMBER);
        attribName = NS_MsgSACat(&attribName, ": ");
        value = vCardService->FakeCString(o);
        if (value)
        {
          if (attribName)
          {
            status = OutputFont(obj, PR_FALSE, "-1", NULL);
            if (status < 0) {
              PR_FREEIF (value);
              PR_FREEIF (attribName);
              return status;
            }
            // write a label without charset conversion
            status = WriteLineToStream (obj, attribName, PR_FALSE);
            if (status < 0) {
              PR_FREEIF (value);
              PR_FREEIF (attribName);
              return status;
            }
            // write value with charset conversion
            status = WriteLineToStream (obj, value, PR_TRUE);
            if (status < 0) {
              PR_FREEIF (value);
              PR_FREEIF (attribName);
              return status;
            }
            status = OutputFont(obj, PR_TRUE, NULL, NULL);
            if (status < 0) {
              PR_FREEIF (value);
              PR_FREEIF (attribName);
              return status;
            }
          }
          PR_FREEIF (attribName);
        }
        PR_FREEIF (value);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
    do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
    return -1;

  WriteOutEachVCardPhoneProperty (obj, v);
  vCardService->InitPropIterator(&t,v);
  while (vCardService->MoreIteration(&t) && status >= 0)
  {
    eachProp = vCardService->NextVObject(&t);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
      do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;
  
  if (vCardService->VObjectName(o)) {

    if (nsCRT::strcasecmp (VCPhotoProp, vCardService->VObjectName(o)) == 0) {
      VObject* urlProp = vCardService->IsAPropertyOf(o, VCURLProp);
      if (urlProp) {
        attribName = VCardGetStringByID(VCARD_LDAP_PHOTOGRAPH);
        /* format the value string to the url */
        value = vCardService->FakeCString(o);
        if (value)
          url = PR_smprintf ("<IMG SRC = ""%s""", value);
        PR_FREEIF (value);
        value = NULL;
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCBirthDateProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_BIRTHDAY);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCDeliveryLabelProp, vCardService->VObjectName(o)) == 0) {
      attribName = VCardGetStringByID(VCARD_LDAP_LABEL);
      GetAddressProperties(o, &attribName);
      value = vCardService->FakeCString(o);
      goto DOWRITE;
    }

    if (nsCRT::strcasecmp (VCEmailAddressProp, vCardService->VObjectName(o)) == 0) {
      if ((*numEmail) != 1)
      {
        if (VALUE_TYPE(o)) {
          (*numEmail)++;
          attribName = VCardGetStringByID(VCARD_LDAP_EMAIL_ADDRESS);
          GetEmailProperties(o, &attribName);
          value = vCardService->FakeCString(o);
          goto DOWRITE;
        };
      }
    }

    if (nsCRT::strcasecmp (VCFamilyNameProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_SURNAME);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCGivenNameProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_GIVEN_NAME);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCNamePrefixesProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_NAME_PREFIX);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCNameSuffixesProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_NAME_SUFFIX);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCAdditionalNamesProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_MIDDLE_NAME);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCMailerProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_MAILER);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCTimeZoneProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_TZ);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCGeoProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_GEO);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCBusinessRoleProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_ROLE);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCLogoProp, vCardService->VObjectName(o)) == 0) {
      VObject* urlProp = vCardService->IsAPropertyOf(o, VCURLProp);
      if (urlProp) {
        attribName = VCardGetStringByID(VCARD_LDAP_LOGO);
        /* format the value string to the url */
        value = vCardService->FakeCString(o);
        if (value)
          url = PR_smprintf ("<IMG SRC = ""%s""", value);
        PR_FREEIF (value);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCAgentProp, vCardService->VObjectName(o)) == 0) {
      attribName = VCardGetStringByID(VCARD_LDAP_SECRETARY);
      goto DOWRITE;
    }

    if (nsCRT::strcasecmp (VCLastRevisedProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_REVISION);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }

    if (nsCRT::strcasecmp (VCPronunciationProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_SOUND);
        value = vCardService->FakeCString(o);
        goto DOWRITE;
      }
    }


    if (nsCRT::strcasecmp (VCVersionProp, vCardService->VObjectName(o)) == 0) {
      if (VALUE_TYPE(o)) {
        attribName = VCardGetStringByID(VCARD_LDAP_VERSION);
        value = vCardService->FakeCString(o);
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

  nsCOMPtr<nsIMsgVCardService> vCardService = 
    do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
    return -1;

  WriteOutEachVCardProperty (obj, v, numEmail);
  vCardService->InitPropIterator(&t,v);
  while (vCardService->MoreIteration(&t) && status >= 0)
  {
    eachProp = vCardService->NextVObject(&t);
    status = WriteOutVCardProperties (obj, eachProp, numEmail);
  }

  if (status < 0) return status;

  return 0;
}

char *
FindCharacterSet(MimeObject *obj)
{
  char    *retCharSet = nsnull;
  char    *tCharSet = nsnull;
  char    *workString = nsnull;

  if ( (!obj->headers) || (!obj->headers->all_headers) )
    return nsnull;

  workString = (char *)PR_Malloc(obj->headers->all_headers_size + 1);
  if (!workString)
    return nsnull;

  memset(workString, 0, obj->headers->all_headers_size + 1);
  memcpy(workString, obj->headers->all_headers, obj->headers->all_headers_size);

  char *cTypePtr = (char *) PL_strcasestr(workString, HEADER_CONTENT_TYPE);
  if (!cTypePtr)
  {
    PR_FREEIF(workString);
    return nsnull;
  }

  while ( (*cTypePtr) && (*cTypePtr != nsCRT::CR) && (*cTypePtr != nsCRT::LF) )
  {
    tCharSet  = (char *) PL_strcasestr(cTypePtr, "charset=");
    if (tCharSet )
      break;

    ++cTypePtr;
  }

  if (tCharSet)
  {
    if (strlen(tCharSet) > 8)
    {
      retCharSet = PL_strdup( (tCharSet + 8) );
      char  *ptr = retCharSet;

      while (*ptr)
      {
        if ( (*ptr == ' ') || (*ptr == ';') || (*ptr == nsCRT::CR) || (*ptr == nsCRT::LF) ) 
        {
          *ptr = '\0';
          break;
        }

        ptr++;
      }
    }
  }

  PR_FREEIF(workString);
  return retCharSet;
}

static int 
WriteLineToStream (MimeObject *obj, const char *line, PRBool aDoCharConversion) 
{                                                                              
  int     status = 0;
  char    *htmlLine;
  int     htmlLen ;
  char    *converted = NULL;
  PRInt32 converted_length;
  PRInt32 res;
  char    *charset = nsnull;

  if ( (!line) || (!*line) )
    return 0;

  if (aDoCharConversion)
  {
    // Seek out a charset!
    charset = PL_strcasestr(obj->content_type, "charset=");
    if (!charset)
      charset = FindCharacterSet(obj);

    if ( (!charset) || ( (charset) && (!nsCRT::strcasecmp(charset, "us-ascii"))) )
    {
      PR_Free(charset);
      charset = nsCRT::strdup("ISO-8859-1");
    }
    // convert from the resource charset.
    res = INTL_ConvertCharset(charset, "UTF-8", line, strlen(line),
                              &converted, &converted_length);
    if ( (res != 0) || (converted == NULL) )
      converted = (char *)line;
    else
      converted[converted_length] = '\0';
  }
  else
    converted = (char *)line;
  
  htmlLen = strlen(converted) + strlen("<DT></DT>") + 1;
  htmlLine = (char *) PR_MALLOC (htmlLen);
  if (htmlLine)
  {
    htmlLine[0] = '\0';
    PL_strcat (htmlLine, "<DT>");
    PL_strcat (htmlLine, converted);
    PL_strcat (htmlLine, "</DT>");
    status = COM_MimeObject_write(obj, htmlLine, strlen(htmlLine), PR_TRUE);
    PR_Free ((void*) htmlLine);
  }
  else
    status = VCARD_OUT_OF_MEMORY;
  
  if (converted != line)
    PR_FREEIF(converted);

  PR_FREEIF(charset);
  return status;
}

static int WriteAttribute (MimeObject *obj, const char *attrib)
{
  int status = 0;
  OutputFont(obj, PR_FALSE, "-1", NULL);
  status  = WriteLineToStream (obj, attrib, PR_FALSE);
  OutputFont(obj, PR_TRUE, NULL, NULL);
  return status;
}


static int WriteValue (MimeObject *obj, const char *value)
{
  int status = 0;
  OutputFont(obj, PR_FALSE, "-1", NULL);
  status  = WriteLineToStream (obj, value, PR_TRUE);
  OutputFont(obj, PR_TRUE, NULL, NULL);
  return status;
}

extern "C" 
char *
VCardGetStringByID(PRInt32 aMsgId)
{
  char          *tempString = nsnull;
  nsresult res = NS_OK;

#ifdef XP_MAC
nsCOMPtr<nsIStringBundle>   stringBundle = nsnull;
#endif

  if (!stringBundle)
  {
    char*       propertyURL = NULL;

    propertyURL = VCARD_URL;

    nsCOMPtr<nsIStringBundleService> sBundleService = 
             do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res); 
    if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
    {
      res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(stringBundle));
    }
  }

  if (stringBundle)
  {
    PRUnichar *ptrv = nsnull;
    res = stringBundle->GetStringFromID(aMsgId, &ptrv);

    if (NS_FAILED(res)) 
      return nsCRT::strdup("???");
    else
    {
      nsAutoString v;
      v.Append(ptrv);
      PR_FREEIF(ptrv);
      tempString = ToNewUTF8String(v);
    }
  }

  if (!tempString)
    return nsCRT::strdup("???");
  else
    return tempString;
}

#else // THUNDERBIRD specific vCard formatting

static int WriteOutVCard (MimeObject * aMimeObj, VObject* aVcard)
{
  int status = 0;
  BeginVCard (aMimeObj);

  GenerateVCardData(aMimeObj, aVcard);

  return EndVCard (aMimeObj);
}


static int GenerateVCardData(MimeObject * aMimeObj, VObject* aVcard)
{
  // style is driven from CSS not here. Just layout the minimal vCard data
  nsCString vCardOutput;

  vCardOutput = "<table class=\"moz-vcard-table\"> <tr> ";  // outer table plus the first (and only row) we use for this table

  // we need to get an escaped vCard url to bind to our add to address book button 
  nsCOMPtr<nsIMsgVCardService> vCardService = do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;

  nsCAutoString vCard;
  nsCAutoString vEscCard;
  int len = 0;

  vCard.Adopt(vCardService->WriteMemoryVObjects(0, &len, aVcard, PR_FALSE));
  vEscCard.Adopt(nsEscape (vCard.get(), url_XAlphas));

  // first cell in the outer table row is a clickable image which brings up the rich address book UI for the vcard
  vCardOutput += "<td valign=\"top\"> <a class=\"moz-vcard-badge\" href=\"addbook:add?action=add?vcard=";
  vCardOutput += vEscCard; // the href is the vCard
  vCardOutput += "\"></a></td>";
  
  // the 2nd cell in the outer table row is a nested table containing the actual vCard properties
  vCardOutput += "<td> <table id=\"moz-vcard-properties-table\"> <tr> ";

  OutputBasicVcard(aMimeObj, aVcard, vCardOutput);

  // close the properties table
  vCardOutput += "</table> </td> ";

  // 2nd  cell in the outer table is our vCard image

  vCardOutput += "</tr> </table>";

  // now write out the vCard
  return COM_MimeObject_write(aMimeObj, (char *) vCardOutput.get(), vCardOutput.Length(), PR_TRUE);
}


static int OutputBasicVcard(MimeObject *aMimeObj, VObject *aVcard, nsACString& vCardOutput)
{
  int status = 0;

  VObject *prop = NULL;
  VObject *prop2 = NULL;
  nsCAutoString urlstring;
  nsCAutoString namestring;
  nsCAutoString emailstring;

  nsCOMPtr<nsIMsgVCardService> vCardService =  do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;
  
  /* get the name and email */
  prop = vCardService->IsAPropertyOf(aVcard, VCFullNameProp);
  if (prop)
  {
    if (VALUE_TYPE(prop))
    {
      if (VALUE_TYPE(prop) != VCVT_RAW)
        namestring.Adopt(vCardService->FakeCString(prop));
      else
        namestring.Adopt(vCardService->VObjectAnyValue(prop));
      
      if (!namestring.IsEmpty())
      {
        vCardOutput += "<td class=\"moz-vcard-title-property\"> ";

        prop = vCardService->IsAPropertyOf(aVcard, VCURLProp);
        if (prop)
        {
          urlstring.Adopt(vCardService->FakeCString(prop));
          if (urlstring.IsEmpty())
            vCardOutput += namestring;
          else
            vCardOutput += nsPrintfCString(512, "<a href=""%s"" private>%s</a>", urlstring.get(), namestring.get());
        }
        else 
          vCardOutput += namestring;

        /* get the email address */
        prop = vCardService->IsAPropertyOf(aVcard, VCEmailAddressProp);
        if (prop)
        {
          emailstring.Adopt(vCardService->FakeCString(prop));
          if (!emailstring.IsEmpty())
          {
            /* if its an internet address prepend the mailto url */
            prop2 = vCardService->IsAPropertyOf(prop, VCInternetProp);
            if (prop2)
              vCardOutput += nsPrintfCString(512, "&nbsp;&lt;<a href=""mailto:%s"" private>%s</a>&gt;", emailstring.get(), emailstring.get());
            else
              vCardOutput += emailstring;
          }
        } // if email address property

        vCardOutput += "</td> </tr> "; // end the cell for the name/email address
      } // if we have a name property
    }
  } // if full name property

  // now each basic property goes on its own line

  // title
  status = OutputVcardAttribute (aMimeObj, aVcard, VCTitleProp, vCardOutput);

  // org name and company name
  prop = vCardService->IsAPropertyOf(aVcard, VCOrgProp);
  if (prop)
  {
    OutputVcardAttribute (aMimeObj, prop, VCOrgUnitProp, vCardOutput);
    OutputVcardAttribute (aMimeObj, prop, VCOrgNameProp, vCardOutput);
  }

  return 0;
}

static int OutputVcardAttribute(MimeObject *aMimeObj, VObject *aVcard, const char* id, nsACString& vCardOutput) 
{
  int status = 0;
  VObject *prop = NULL;
  nsCAutoString string;

  nsCOMPtr<nsIMsgVCardService> vCardService = do_GetService(MSGVCARDSERVICE_CONTRACT_ID);
  if (!vCardService)
      return -1;

  prop = vCardService->IsAPropertyOf(aVcard, id);
  if (prop)
    if (VALUE_TYPE(prop))
    {
      if (VALUE_TYPE(prop) != VCVT_RAW)
        string.Adopt(vCardService->FakeCString(prop));
      else
        string.Adopt(vCardService->VObjectAnyValue(prop));
      
      if (!string.IsEmpty()) 
      {
        vCardOutput += "<tr> <td class=\"moz-vcard-property\">";
        vCardOutput += string;
        vCardOutput += "</td> </tr> ";
      }
    }

  return 0;
}

#endif

