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
 *       Ben Bucksch <mozilla@bucksch.org>
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
#include "nsMsgI18N.h"

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
  char        *formatCstr = nsnull;
  nsCString   formatString;
  
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
  case 1:     // large
    formatString.Append("font-size: large; ");
    break;
  case 2:     // small
    formatString.Append("font-size: small; ");
    break;
  }
  
  if (citationColor && nsCRT::strlen(citationColor) != 0)
  {
    formatString += "color: ";
    formatString += citationColor;
    formatString += ';';
  }

  formatCstr = formatString.ToNewCString();
  return formatCstr;
}

static int
MimeInlineTextPlain_parse_begin (MimeObject *obj)
{
  int status = 0;
  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )       );  // The output will be inserted in the composer as quotation
  PRBool plainHTML = quoting || (obj->options &&
       obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs);
       // Just good(tm) HTML. No reliance on CSS.

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
      MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;
      text->mCiteLevel = 0;

      // Get the prefs

      // Quoting
      text->mBlockquoting = PR_TRUE; // mail.quoteasblock

      // Viewing
      text->mQuotedSizeSetting = 0;   // mail.quoted_size
      text->mQuotedStyleSetting = 0;  // mail.quoted_style
      text->mCitationColor = nsnull;  // mail.citation_color
      PRBool graphicalQuote = PR_TRUE; // mail.quoted_graphical

      nsIPref *prefs = GetPrefServiceManager(obj->options);
      if (prefs)
      {
        prefs->GetIntPref("mail.quoted_size", &(text->mQuotedSizeSetting));
        prefs->GetIntPref("mail.quoted_style", &(text->mQuotedStyleSetting));
        prefs->CopyCharPref("mail.citation_color", &(text->mCitationColor));
        prefs->GetBoolPref("mail.quoted_graphical", &graphicalQuote);
        prefs->GetBoolPref("mail.quoteasblock", &(text->mBlockquoting));
      }

      // Get font
      // only used for viewing (!plainHTML)
      nsCAutoString fontstyle;
      if (nsMimeOutput::nsMimeMessageBodyDisplay == obj->options->format_out ||
          nsMimeOutput::nsMimeMessagePrintOutput == obj->options->format_out)
      {
        /* Use a langugage sensitive default font
           (otherwise unicode font will be used since the data is UTF-8). */
        char fontName[128];     // default font name
        PRInt32 fontSize;       // default font size
        nsresult rv = GetMailNewsFont(obj,
                           !obj->options->variable_width_plaintext_p,
                           fontName, 128, &fontSize);
        if (NS_SUCCEEDED(rv))
        {
          fontstyle = "font-family: ";
          fontstyle += fontName;
          fontstyle += "; font-size: ";
          fontstyle.AppendInt(fontSize);
          fontstyle += "px;";
        }
        else
        {
          if (!obj->options->variable_width_plaintext_p)
            fontstyle = "font-family: -moz-fixed";
        }
      }
      else  // DELETEME: makes sense?
      {
        if (!obj->options->variable_width_plaintext_p)
          fontstyle = "font-family: -moz-fixed";
      }

      // Opening <div>. We currently have to add formatting here. :-(
      nsCAutoString openingDiv;
      if (!quoting)
           /* 4.x' editor can't break <div>s (e.g. to interleave comments).
              We'll add the class to the <blockquote type=cite> later. */
      {
        openingDiv = "<div class=\"moz-text-plain\"";
        if (!plainHTML)
        {
          if (obj->options->wrap_long_lines_p)
            openingDiv += " wrap=true";
          else
            openingDiv += " wrap=false";

          if (graphicalQuote)
            openingDiv += " graphical-quote=true";
          else
            openingDiv += " graphical-quote=false";

          if (!fontstyle.IsEmpty())
          {
            openingDiv += " style=\"";
            openingDiv += fontstyle;
            openingDiv += '\"';
          }
        }
        openingDiv += "><pre wrap>";
      }
      else
        openingDiv = "<pre wrap>";
	  status = MimeObject_write(obj, openingDiv,openingDiv.Length(), PR_FALSE);
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
  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )           );  // see above
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn &&
	  !abort_p)
	{
      MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;
      if (text->mIsSig && !quoting)
      {
        status = MimeObject_write(obj, "</div>", 6, PR_FALSE);  // .moz-txt-sig
        if (status < 0) return status;
      }
      status = MimeObject_write(obj, "</pre>", 6, PR_FALSE);
      if (status < 0) return status;
      if (!quoting)
      {
        status = MimeObject_write(obj, "</div>", 6, PR_FALSE);
                                        // .moz-text-plain
        if (status < 0) return status;
      }

      /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects.
	   */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}


static int
MimeInlineTextPlain_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  int status;
  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )           );  // see above
  PRBool plainHTML = quoting || (obj->options &&
       obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs);
       // see above

  // this routine gets called for every line of data that comes through the
  // mime converter. It's important to make sure we are efficient with 
  // how we allocate memory in this routine. be careful if you go to add
  // more to this routine.

  NS_ASSERTION(length > 0, "zero length");
  if (length <= 0) return 0;

#ifdef USE_OBUFFER
  /* There is the issue of guessing how much space we will need for emoticons.
     So what we will do is count the total number of "special" chars and
     multiply by 82 (max len for a smiley line) and add one for good measure.*/
  // Do we need obj->obuffer at all here? Bug 39226
  PRInt32 buffersizeneeded = (length * 2);
  PRInt32   specialCharCount = 0;
  for (PRInt32 z=0; z<length; z++)
  {
    if ( (line[z] == ')') || (line[z] == '(') || (line[z] == ':')
         || (line[z] == ';') || (line[z] == '>') )
      ++specialCharCount;
  }
  buffersizeneeded += 82 * (specialCharCount + 1); 

  status = MimeObject_grow_obuffer (obj, buffersizeneeded);
  if (status < 0) return status;
  *obj->obuffer = 0;
#endif

  mozITXTToHTMLConv *conv = GetTextConverter(obj->options);
  MimeInlineTextPlain *text = (MimeInlineTextPlain *) obj;

  PRBool skipConversion = !conv ||
                          (obj->options && obj->options->force_user_charset);

  if (!skipConversion)
  {
    nsString lineSourceStr;
    lineSourceStr.AssignWithConversion(line, length);
    nsresult rv;
    nsCAutoString prefaceResultStr;  // Quoting stuff before the real text

    // Recognize quotes
    PRUint32 oldCiteLevel = text->mCiteLevel;
    PRUint32 logicalLineStart = 0;
    rv = conv->CiteLevelTXT(lineSourceStr.GetUnicode(),
                            &logicalLineStart, &(text->mCiteLevel));
    if (NS_FAILED(rv))
      return -1;

    // Find out, which recognitions to do
    PRBool whattodo = obj->options->whattodo;
    if (plainHTML)
    {
      if (quoting)
        whattodo = 0;  // This is done on Send. Don't do it twice.
      else
        whattodo = whattodo & ~mozITXTToHTMLConv::kGlyphSubstitution;
                   /* Do recognition for the case, the result is viewed in
                      Mozilla, but not GlyphSubstitution, because other UAs
                      might not be able to display the glyphs. */
      if (!text->mBlockquoting)
        text->mCiteLevel = 0;
    }

    // Write blockquote
    if (text->mCiteLevel > oldCiteLevel)
    {
      prefaceResultStr += "</pre>";
      for (PRUint32 i = 0; i < text->mCiteLevel - oldCiteLevel; i++)
      {
        char *style = MimeTextBuildPrefixCSS(text->mQuotedSizeSetting,
                                             text->mQuotedStyleSetting,
                                             text->mCitationColor);
        if (!plainHTML && style && nsCRT::strlen(style))
        {
          prefaceResultStr += "<blockquote type=cite style=\"";
          prefaceResultStr += style;
          prefaceResultStr += "\">";
        }
        else
          prefaceResultStr += "<blockquote type=cite>";
        Recycle(style);
      }
      prefaceResultStr += "<pre wrap>";
    }
    else if (text->mCiteLevel < oldCiteLevel)
    {
      prefaceResultStr += "</pre>";
      for (PRUint32 i = 0; i < oldCiteLevel - text->mCiteLevel; i++)
        prefaceResultStr += "</blockquote>";
      prefaceResultStr += "<pre wrap>";
      if (text->mCiteLevel == 0)
        prefaceResultStr += "<!---->";   /* Make sure, NGLayout puts out
                                            a linebreak */
    }

    // Write plain text quoting tags
    if (logicalLineStart != 0 && !(plainHTML && text->mBlockquoting))
    {
      if (!plainHTML)
        prefaceResultStr += "<span class=\"moz-txt-citetags\">";

      nsAutoString citeTagsSource;
      lineSourceStr.Mid(citeTagsSource, 0, logicalLineStart);
      NS_ASSERTION(citeTagsSource.IsASCII(), "Non-ASCII-Chars are about to be "
                   "added to nsCAutoString prefaceResultStr. "
                   "Change the latter to nsAutoString.");
        /* I'm currently using nsCAutoString, because currently citeTagsSource
           is always ASCII and I save 2 conversions this way. */

      // Convert to HTML
      PRUnichar* citeTagsResultUnichar = nsnull;
      rv = conv->ScanTXT(citeTagsSource.GetUnicode(), 0 /* no recognition */,
                         &citeTagsResultUnichar);
      if (NS_FAILED(rv)) return -1;

      // Convert to char* and write out
      nsSubsumeStr citeTagsResultStr(citeTagsResultUnichar,
                                     PR_TRUE /* assume ownership */);
      char* citeTagsResultCStr = citeTagsResultStr.ToNewCString();

      prefaceResultStr += citeTagsResultCStr;
      Recycle(citeTagsResultCStr);
      if (!plainHTML)
        prefaceResultStr += "</span>";
    }


    // recognize signature
    if ((lineSourceStr.Length() >= 4)
        && lineSourceStr.First() == '-'
        && lineSourceStr.EqualsWithConversion("-- ", PR_FALSE, 3)
        && (lineSourceStr[3] == '\r' || lineSourceStr[3] == '\n') )
    {
      text->mIsSig = PR_TRUE;
      if (!quoting)
        prefaceResultStr += "<div class=\"moz-txt-sig\">";
    }

    // Get a mail charset of this message.
    MimeInlineText  *inlinetext = (MimeInlineText *) obj;
    char *mailCharset = NULL;
    if (inlinetext->charset && *(inlinetext->charset))
      mailCharset = inlinetext->charset;

    /* This is the main TXT to HTML conversion:
       escaping (very important), eventually recognizing etc. */
    PRUnichar* lineResultUnichar = nsnull;

    if (obj->options->format_out != nsMimeOutput::nsMimeMessageSaveAs ||
        !mailCharset || !nsMsgI18Nstateful_charset(mailCharset))
    {
      rv = conv->ScanTXT(lineSourceStr.GetUnicode() + logicalLineStart,
                         whattodo, &lineResultUnichar);
      if (NS_FAILED(rv)) return -1;
    }
    else
    {
      // If nsMimeMessageSaveAs, the string is in mail charset (and stateful, e.g. ISO-2022-JP).
      // convert to unicode so it won't confuse ScanTXT.
      nsAutoString ustr;
      nsCAutoString cstr(line, length);

      rv = nsMsgI18NConvertToUnicode((nsCAutoString) mailCharset, cstr, ustr);
      if (NS_SUCCEEDED(rv))
      {
        PRUnichar *u;
        rv = conv->ScanTXT(ustr.GetUnicode() + logicalLineStart, whattodo, &u);
        if (NS_SUCCEEDED(rv))
        {
          ustr.Assign(u);
          Recycle(u);
          rv = nsMsgI18NConvertFromUnicode((nsCAutoString) mailCharset, ustr, cstr);
          if (NS_SUCCEEDED(rv))
          {
            // create PRUnichar* which contains NON unicode 
            // as the following code expecting it
            ustr.AssignWithConversion(cstr);                                                  
            lineResultUnichar = ustr.ToNewUnicode();
            if (!lineResultUnichar) return -1;
          }
        }
      }
      if (NS_FAILED(rv))
        return -1;
    }


    // avoid an extra string copy by using nsSubsumeStr, this transfers
    // ownership of wresult to strresult so don't try to free wresult later.
    nsSubsumeStr lineResultStr(lineResultUnichar,
                               PR_TRUE /* assume ownership */);

    if (!(text->mIsSig && quoting))
    {
#ifdef USE_OBUFFER
      /* Do we need obj->obuffer at all here? Bug 39226 */
      prefaceResultStr.ToCString(obj->obuffer, obj->obuffer_size - 10);
      lineResultStr.ToCString(obj->obuffer + prefaceResultStr.Length(),
                          obj->obuffer_size - 10 - prefaceResultStr.Length());
#else
      char* tmp = prefaceResultStr.ToNewCString();
      status = MimeObject_write(obj, tmp, prefaceResultStr.Length(), PR_TRUE);
      if (status < 0) return status;
      Recycle(tmp);
      tmp = lineResultStr.ToNewCString();
      status = MimeObject_write(obj, tmp, lineResultStr.Length(), PR_TRUE);
      if (status < 0) return status;
      Recycle(tmp);
#endif
    }
    else
    {
      status = NS_OK;
    }
  }
  else
  {
#ifdef USE_OBUFFER
    nsCRT::memcpy(obj->obuffer, line, length);
    obj->obuffer[length] = '\0';
    status = NS_OK;
#else
    status = MimeObject_write(obj, line, length, PR_TRUE);
    if (status < 0) return status;
#endif
  }

#ifdef USE_OBUFFER
  NS_ASSERTION(*line == 0 || *obj->obuffer, "have line or buffer");
  status = MimeObject_write(obj, obj->obuffer, nsCRT::strlen(obj->obuffer),
                            PR_TRUE);
#endif
  return status;
}

