/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "mimetpla.h"
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMimeTransition.h"
#include "mozITXTToHTMLConv.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsMimeStringResources.h"
#include "mimemoz2.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "prprf.h"

static NS_DEFINE_CID(kTXTToHTMLConvCID, MOZITXTTOHTMLCONV_CID);
static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID);

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextPlain, MimeInlineTextPlainClass,
			 mimeInlineTextPlainClass, &MIME_SUPERCLASS);

static int MimeInlineTextPlain_parse_begin (MimeObject *);
static int MimeInlineTextPlain_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextPlain_parse_eof (MimeObject *, PRBool);

static int
MimeInlineTextPlainClassInitialize(MimeInlineTextPlainClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "class not initialized");
  oclass->parse_begin = MimeInlineTextPlain_parse_begin;
  oclass->parse_line  = MimeInlineTextPlain_parse_line;
  oclass->parse_eof   = MimeInlineTextPlain_parse_eof;
  return 0;
}

extern "C"
char *
MimeTextBuildPrefixCSS(PRInt32    quotedSizeSetting,   // mail.quoted_size
                       PRInt32    quotedStyleSetting,  // mail.quoted_style
                       char       *citationColor)      // mail.citation_color
{
  char        *openDiv = nsnull;
  nsCString   formatString;
  
  formatString = "<DIV name=\"text-cite\" style=\"";
  
  switch (quotedStyleSetting)
  {
  case 0:     // regular
    break;
  case 1:     // bold
    formatString.Append("font-weight: bold; ");
    break;
  case 2:     // italic
    formatString.Append("font-style: italic; ");
    break;
  case 3:     // bold-italic
    formatString.Append("font-weight: bold; font-style: italic; ");
    break;
  }
  
  switch (quotedSizeSetting)
  {
  case 0:     // regular
    break;
  case 1:     // bigger
    formatString.Append("font-size: bigger; ");
    break;
  case 2:     // smaller
    formatString.Append("font-size: smaller; ");
    break;
  }
  
  if (citationColor)
  {
    formatString.Append("color: %s;\">");
    openDiv = PR_smprintf(formatString, citationColor);
  }
  else
  {
    formatString.Append("\">");
    openDiv = formatString.ToNewCString();
  }
  
  return openDiv;
}

static int
MimeInlineTextPlain_parse_begin (MimeObject *obj)
{
  int status = 0;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (nsMimeOutput::nsMimeMessageBodyDisplay == obj->options->format_out ||
      nsMimeOutput::nsMimeMessagePrintOutput == obj->options->format_out)
    status = BeginMailNewsFont(obj);  

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
    MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;
	  char* strs[4];
	  char* s;
	  strs[0] = "<PRE>";
	  strs[1] = "<PRE style=\"font-family: serif;\">";
	  strs[2] = "<PRE WRAP>";
	  strs[3] = "<PRE WRAP style=\"font-family: serif;\">";

    // Ok, first get the quoting settings.
    text->mInsideQuote = PR_FALSE;
    text->mQuotedSizeSetting = 0;   // mail.quoted_size
    text->mQuotedStyleSetting = 0;  // mail.quoted_style
    text->mCitationColor = nsnull;  // mail.citation_color

    nsIPref *prefs = GetPrefServiceManager(obj->options);
    if (prefs)
    {
      prefs->GetIntPref("mail.quoted_size", &(text->mQuotedSizeSetting));
      prefs->GetIntPref("mail.quoted_style", &(text->mQuotedStyleSetting));
      prefs->CopyCharPref("mail.citation_color", &(text->mCitationColor));
    }

    // For quoting, keep it simple...
    if ( (obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting) ||
         (obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting) )
    {
      if (obj->options->wrap_long_lines_p)
        s = nsCRT::strdup(strs[2]);
      else
        s = nsCRT::strdup(strs[0]);
    }
    else
    {
  	  s = nsCRT::strdup(strs[(obj->options->variable_width_plaintext_p ? 1 : 0) +
	  					(obj->options->wrap_long_lines_p ? 2 : 0)]);
    }

	  if (!s) return MIME_OUT_OF_MEMORY;
	  status = MimeObject_write(obj, s, nsCRT::strlen(s), PR_FALSE);
	  PR_Free(s);
	  if (status < 0) return status;

	  /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects. */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}

static int
MimeInlineTextPlain_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn &&
	  !abort_p)
	{
	  char s[] = "</PRE>";
	  status = MimeObject_write(obj, s, nsCRT::strlen(s), PR_FALSE);
	  if (status < 0) return status;

    // Make sure we close out any <DIV>'s if they are open!
    MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;
    PR_FREEIF(text->mCitationColor);

    if (text->mInsideQuote)
    {
      char *closeDiv = "</DIV>";
	    status = MimeObject_write(obj, closeDiv, nsCRT::strlen(closeDiv), PR_FALSE);
  	  if (status < 0) return status;
    }

    /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects.
	   */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  if (nsMimeOutput::nsMimeMessageBodyDisplay == obj->options->format_out ||
      nsMimeOutput::nsMimeMessagePrintOutput == obj->options->format_out)
    status = EndMailNewsFont(obj);

  return 0;
}


static int
MimeInlineTextPlain_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  // this routine gets called for every line of data that comes through the mime
  // converter. It's important to make sure we are efficient with 
  // how we allocate memory in this routine. be careful if you go to add
  // more to this routine.

  int status;

  NS_ASSERTION(length > 0, "zero length");
  if (length <= 0) return 0;

  PRInt32 buffersizeneeded = (length * 2);

  // Ok, there is always the issue of guessing how much space we will need for emoticons.
  // So what we will do is count the total number of "special" chars and multiply by 82 
  // (max len for a smiley line) and add one for good measure
  //XXX: make dynamic
  PRInt32   specialCharCount = 0;
  for (PRInt32 z=0; z<length; z++)
  {
    if ( (line[z] == ')') || (line[z] == '(') || (line[z] == ':') || (line[z] == ';') )
      ++specialCharCount;
  }
  buffersizeneeded += 82 * (specialCharCount + 1); 

  status = MimeObject_grow_obuffer (obj, buffersizeneeded);
  if (status < 0) return status;

  /* Copy `line' to `out', quoting HTML along the way.
	 Note: this function does no charset conversion; that has already
	 been done.
   */
  *obj->obuffer = 0;

  mozITXTToHTMLConv *conv = GetTextConverter(obj->options);

  PRBool skipConversion =
       !conv ||
       ( obj->options &&
         (
           obj->options->force_user_charset ||
           obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs
         ) );

  // Before we do any other processing, we should figure out if we need to
  // put this information in a <DIV> tag
  MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;
  if (text->mInsideQuote)
  {
    if (line[0] != '>')
    {
      char *closeDiv = "</DIV>";
      status = MimeObject_write(obj, closeDiv, nsCRT::strlen(closeDiv), PR_FALSE);
      if (status < 0) return status;
      text->mInsideQuote = PR_FALSE;
    }
  }
  else if ( (line[0] == '>') &&
            ( (obj) && 
               obj->options->format_out != nsMimeOutput::nsMimeMessageQuoting &&
               obj->options->format_out != nsMimeOutput::nsMimeMessageBodyQuoting ) )
  {        

    char *openDiv = MimeTextBuildPrefixCSS(text->mQuotedSizeSetting,
                                           text->mQuotedStyleSetting,
                                           text->mCitationColor);
    if (openDiv)
    {
      status = MimeObject_write(obj, openDiv, nsCRT::strlen(openDiv), PR_FALSE);
      if (status < 0) return status;
      text->mInsideQuote = PR_TRUE;
    }

    PR_FREEIF(openDiv);
  }

  if (!skipConversion)
  {
    nsString strline(line, length);
    nsresult rv = NS_OK;
    PRUnichar* wresult = nsnull;
    PRBool whattodo = obj->options->whattodo;
    if
      (
        obj->options
          &&
          (
            obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
            obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
          )
      )
      whattodo = 0;

    rv = conv->ScanTXT(strline.GetUnicode(), whattodo, &wresult);
    if (NS_FAILED(rv))
      return -1;

    //XXX I18N Converting PRUnichar* to char*
    // avoid an extra string copy by using nsSubsumeStr, this transfers ownership of
    // wresult to strresult so don't try to free wresult later.
    nsString strresult(nsSubsumeStr(wresult, PR_TRUE /* assume ownership */, nsCRT::strlen(wresult)));

    // avoid yet another extra string copy of the line by using .ToCString which will
    // convert and copy directly into the buffer we have already allocated.
    strresult.ToCString(obj->obuffer, obj->obuffer_size - 10); 
  }
  else
  {
    nsCRT::memcpy(obj->obuffer, line, length);
    obj->obuffer[length] = '\0';
    status = NS_OK;
  }

  if (status != NS_OK)
    return status;

  NS_ASSERTION(*line == 0 || *obj->obuffer, "have line or buffer");
  return MimeObject_write(obj, obj->obuffer, nsCRT::strlen(obj->obuffer), PR_TRUE);
}
