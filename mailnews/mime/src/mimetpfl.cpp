/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *       Ben Bucksch <mozilla@bucksch.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mimetpfl.h"
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "mozITXTToHTMLConv.h"
#include "nsString.h"
#include "nsMimeStringResources.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "mimemoz2.h"
#include "prprf.h"
#include "nsMsgI18N.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static const PRUint32 kSpacesForATab = 4; // Must be at least 1.
static const PRUint32 kInitialBufferSize = 100;

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextPlainFlowed, MimeInlineTextPlainFlowedClass,
			 mimeInlineTextPlainFlowedClass, &MIME_SUPERCLASS);

static int MimeInlineTextPlainFlowed_parse_begin (MimeObject *);
static int MimeInlineTextPlainFlowed_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextPlainFlowed_parse_eof (MimeObject *, PRBool);

static MimeInlineTextPlainFlowedExData *MimeInlineTextPlainFlowedExDataList = nsnull;

// From mimetpla.cpp
extern "C" char *MimeTextBuildPrefixCSS(
                       PRInt32 quotedSizeSetting,      // mail.quoted_size
                       PRInt32    quotedStyleSetting,  // mail.quoted_style
                       char       *citationColor);     // mail.citation_color

// Definition below
extern "C"
nsresult Line_convert_whitespace(const nsString& a_line,
                                 const PRBool a_convert_all_whitespace,
                                 nsString& a_out_line);

static int
MimeInlineTextPlainFlowedClassInitialize(MimeInlineTextPlainFlowedClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "class not initialized");
  oclass->parse_begin = MimeInlineTextPlainFlowed_parse_begin;
  oclass->parse_line  = MimeInlineTextPlainFlowed_parse_line;
  oclass->parse_eof   = MimeInlineTextPlainFlowed_parse_eof;
  
  return 0;
}

static int
MimeInlineTextPlainFlowed_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  status =  MimeObject_write(obj, "", 0, PR_TRUE); /* force out any separators... */
  if(status<0) return status;

  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )       );  // The output will be inserted in the composer as quotation
  PRBool plainHTML = quoting || (obj->options &&
       obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs);
       // Just good(tm) HTML. No reliance on CSS.

  // Setup the data structure that is connected to the actual document
  // Saved in a linked list in case this is called with several documents
  // at the same time.
  /* This memory is freed when parse_eof is called. So it better be! */
  struct MimeInlineTextPlainFlowedExData *exdata =
    (MimeInlineTextPlainFlowedExData *)PR_MALLOC(sizeof(struct MimeInlineTextPlainFlowedExData));
  if(!exdata) return MIME_OUT_OF_MEMORY;

  MimeInlineTextPlainFlowed *text = (MimeInlineTextPlainFlowed *) obj;

  // Link it up.
  exdata->next = MimeInlineTextPlainFlowedExDataList;
  MimeInlineTextPlainFlowedExDataList = exdata;

  // Initialize data

  exdata->ownerobj = obj;
  exdata->inflow = PR_FALSE;
  exdata->quotelevel = 0;
  exdata->isSig = PR_FALSE;

  // Get Prefs for viewing

  exdata->fixedwidthfont = PR_FALSE;
  //  Quotes
  text->mQuotedSizeSetting = 0;   // mail.quoted_size
  text->mQuotedStyleSetting = 0;  // mail.quoted_style
  text->mCitationColor = nsnull;  // mail.citation_color

  nsIPref *prefs = GetPrefServiceManager(obj->options);
  if (prefs)
  {
    prefs->GetIntPref("mail.quoted_size", &(text->mQuotedSizeSetting));
    prefs->GetIntPref("mail.quoted_style", &(text->mQuotedStyleSetting));
    prefs->CopyCharPref("mail.citation_color", &(text->mCitationColor));
    nsresult rv = prefs->GetBoolPref("mail.fixed_width_messages",
                                     &(exdata->fixedwidthfont));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get pref");
         // Check at least the success of one
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
    PRInt32 fontSizePercentage;   // size percentage
    nsresult rv = GetMailNewsFont(obj, exdata->fixedwidthfont,
                                  fontName, 128, &fontSize, &fontSizePercentage);
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
      if (exdata->fixedwidthfont)
        fontstyle = "font-family: -moz-fixed";
    }
  }
  else  // DELETEME: makes sense?
  {
    if (exdata->fixedwidthfont)
      fontstyle = "font-family: -moz-fixed";
  }

  // Opening <div>.
  if (!quoting)
       /* 4.x' editor can't break <div>s (e.g. to interleave comments).
          We'll add the class to the <blockquote type=cite> later. */
  {
    nsCAutoString openingDiv("<div class=\"moz-text-flowed\"");
    // We currently have to add formatting here. :-(
    if (!plainHTML && !fontstyle.IsEmpty())
    {
      openingDiv += " style=\"";
      openingDiv += fontstyle;
      openingDiv += '"';
    }
    openingDiv += ">";
    status = MimeObject_write(obj, NS_CONST_CAST(char*, openingDiv.get()), openingDiv.Length(), PR_FALSE);
    if (status < 0) return status;
  }

  return 0;
}

static int
MimeInlineTextPlainFlowed_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status = 0;

  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )           );  // see above

  // Has this method already been called for this object?
  // In that case return.
  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) goto EarlyOut;

  // Look up and unlink "our" extended data structure
  // We do it in the beginning so that if an error occur, we can
  // just free |exdata|.
  struct MimeInlineTextPlainFlowedExData *exdata;
  struct MimeInlineTextPlainFlowedExData **prevexdata;
  prevexdata = &MimeInlineTextPlainFlowedExDataList;

  while ((exdata = *prevexdata) != nsnull) {
    if (exdata->ownerobj == obj) {
      // Fill hole
      *prevexdata = exdata->next;
      break;
    }
    prevexdata = &exdata->next;
  }
  NS_ASSERTION (exdata, "The extra data has disappeared!");

  if (!obj->output_p) {
    status = 0;
    goto EarlyOut;
  }
    
  for(; exdata->quotelevel > 0; exdata->quotelevel--) {
    status = MimeObject_write(obj, "</blockquote>", 13, PR_FALSE);
    if(status<0) goto EarlyOut;
  }
    
  if (exdata->isSig && !quoting) {
    status = MimeObject_write(obj, "</div>", 6, PR_FALSE); // .moz-txt-sig
    if (status<0) goto EarlyOut;
  }
  if (!quoting) // HACK (see above)
  {
    status = MimeObject_write(obj, "</div>", 6, PR_FALSE); // .moz-text-flowed
    if (status<0) goto EarlyOut;
  }
    
  status = 0;

EarlyOut:  
  PR_Free(exdata);

  // Free mCitationColor
  MimeInlineTextPlainFlowed *text = (MimeInlineTextPlainFlowed *) obj;
  PR_FREEIF(text->mCitationColor);
  text->mCitationColor = nsnull;
  
  return status;
}


static int
MimeInlineTextPlainFlowed_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  int status;
  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )           );  // see above
  PRBool plainHTML = quoting || (obj->options &&
       obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs);
       // see above

  struct MimeInlineTextPlainFlowedExData *exdata;
  exdata = MimeInlineTextPlainFlowedExDataList;
  while(exdata && (exdata->ownerobj != obj)) {
    exdata = exdata->next;
  }

  NS_ASSERTION(exdata, "The extra data has disappeared!");

  NS_ASSERTION(length > 0, "zero length");
  if (length <= 0) return 0;

  uint32 linequotelevel = 0;
  char *linep = line;
  // Space stuffed?
  if(' ' == *linep) {
    linep++;
  } else {
    // count '>':s before the first non-'>'
    while('>' == *linep) {
      linep++;
      linequotelevel++;
    }
    // Space stuffed?
    if(' ' == *linep) {
      linep++;
    }
  }

  // Look if the last character (after stripping ending end
  // of lines and quoting stuff) is a SPACE. If it is, we are looking at a
  // flowed line. Normally we assume that the last two chars
  // are CR and LF as said in RFC822, but that doesn't seem to
  // be the case always.
  PRBool flowed = PR_FALSE;
  PRInt32 index = length-1;
  while(index >= 0 && ('\r' == line[index] || '\n' == line[index])) {
    index--;
  }
  if (index > linep - line && ' ' == line[index])
       /* Ignore space stuffing, i.e. lines with just
          (quote marks and) a space count as empty */
    flowed = PR_TRUE;

  mozITXTToHTMLConv *conv = GetTextConverter(obj->options);

  PRBool skipConversion = !conv ||
                          (obj->options && obj->options->force_user_charset);

  nsString lineSource;
  nsString lineResult;
  lineSource.SetCapacity(kInitialBufferSize);
  lineResult.SetCapacity(kInitialBufferSize);
    
  if (!skipConversion)
  {
    lineSource.AssignWithConversion(linep, (length - (linep - line)) );
    PRUnichar* wresult = nsnull;
    nsresult rv = NS_OK;
    PRBool whattodo = obj->options->whattodo;
    if (plainHTML)
    {
      if (quoting)
        whattodo = 0;
      else
        whattodo = whattodo & ~mozITXTToHTMLConv::kGlyphSubstitution;
                   /* Do recognition for the case, the result is viewed in
                      Mozilla, but not GlyphSubstitution, because other UAs
                      might not be able to display the glyphs. */
    }

    // Get a mail charset of this message.
    MimeInlineText  *inlinetext = (MimeInlineText *) obj;
    char *mailCharset = NULL;
    if (inlinetext->charset && *(inlinetext->charset))
      mailCharset = inlinetext->charset;

    if (obj->options->format_out != nsMimeOutput::nsMimeMessageSaveAs ||
        !mailCharset || !nsMsgI18Nstateful_charset(mailCharset))
    {
      /* This is the main TXT to HTML conversion:
	       escaping (very important), eventually recognizing etc. */
      rv = conv->ScanTXT(lineSource.get(), whattodo, &wresult);
      if (NS_FAILED(rv)) return -1;
      lineResult = wresult;
      Recycle(wresult);
    }
    else
    {
      // If nsMimeMessageSaveAs, the string is in mail charset (and stateful, e.g. ISO-2022-JP).
      // convert to unicode so it won't confuse ScanTXT.
      char *newcstr;

      newcstr = lineSource.ToNewCString();      // lineSource uses nsString but the string is NOT unicode
      if (!newcstr) return -1;

      nsAutoString ustr;
      nsCAutoString cstr;
      nsCAutoString mailCharsetStr(mailCharset);

      cstr.Assign(newcstr);
      Recycle(newcstr);

      rv = nsMsgI18NConvertToUnicode(mailCharsetStr, cstr, ustr);
      if (NS_SUCCEEDED(rv))
      {
        PRUnichar *u;
        rv = conv->ScanTXT(ustr.get(), whattodo, &u);
        if (NS_SUCCEEDED(rv))
        {
          ustr.Assign(u);
          Recycle(u);
          rv = nsMsgI18NConvertFromUnicode(mailCharsetStr, ustr, cstr);
          if (NS_SUCCEEDED(rv))
            lineResult.AssignWithConversion(cstr);   // create nsString which contains NON unicode 
                                                     // as the following code expecting it
        }
      }
      if (NS_FAILED(rv))
        return -1;
    }
  }
  else
  {
    lineResult.AssignWithConversion(line, length);
    status = NS_OK;
  }

  nsCAutoString preface;

  /* Correct number of blockquotes */
  int32 quoteleveldiff=linequotelevel - exdata->quotelevel;
  if((quoteleveldiff != 0) && flowed && exdata->inflow) {
    // From RFC 2646 4.5
    // The receiver SHOULD handle this error by using the 'quote-depth-wins' rule,
    // which is to ignore the flowed indicator and treat the line as fixed.  That
    // is, the change in quote depth ends the paragraph.

    // We get that behaviour by just going on.
  }
  while(quoteleveldiff>0) {
    quoteleveldiff--;
    preface += "<blockquote type=cite";
    // This is to have us observe the user pref settings for citations
    MimeInlineTextPlainFlowed *tObj = (MimeInlineTextPlainFlowed *) obj;
    char *style = MimeTextBuildPrefixCSS(tObj->mQuotedSizeSetting,
                                         tObj->mQuotedStyleSetting,
                                         tObj->mCitationColor);
    if (!plainHTML && style && strlen(style))
    {
      preface += " style=\"";
      preface += style;
      preface += '"';
      PR_FREEIF(style);
    }
    preface += '>';
  }
  while(quoteleveldiff<0) {
    quoteleveldiff++;
    preface += "</blockquote>";
  }
  exdata->quotelevel = linequotelevel;

  nsString lineResult2;
  lineResult2.SetCapacity(kInitialBufferSize);

  if(flowed) {
    // Check RFC 2646 "4.3. Usenet Signature Convention": "-- "+CRLF is
    // not a flowed line
    if
      (  // is "-- "LINEBREAK
        lineSource.Length() >= 4
        && lineSource[0] == '-'
        &&
        (
          lineSource.EqualsWithConversion("-- \r", PR_FALSE, 4) ||
          lineSource.EqualsWithConversion("-- \n", PR_FALSE, 4)
        )
      )
    {
      if (linequotelevel > 0 || exdata->isSig)
      {
        preface += "--&nbsp;<br>";
      } else {
        exdata->isSig = PR_TRUE;
        preface += "<div class=\"moz-txt-sig\"><span class=\"moz-txt-tag\">"
                   "--&nbsp;<br></span>";
      }
    } else {
      Line_convert_whitespace(lineResult, PR_FALSE /* Allow wraps */,
                              lineResult2);
    }

    exdata->inflow=PR_TRUE;
  } else {
    // Fixed paragraph.
    Line_convert_whitespace(lineResult,
                            !plainHTML && !obj->options->wrap_long_lines_p
                              /* If wrap, convert all spaces but the last in
                                 a row into nbsp, otherwise all. */,
                            lineResult2);
    lineResult2 += NS_LITERAL_STRING("<br>");
    exdata->inflow = PR_FALSE;
  } // End Fixed line

  if (!(exdata->isSig && quoting))
  {
    char* tmp = preface.ToNewCString();
    status = MimeObject_write(obj, tmp, preface.Length(), PR_TRUE);
    Recycle(tmp);
    tmp = lineResult2.ToNewCString();
    status = MimeObject_write(obj, tmp, lineResult2.Length(), PR_TRUE);
    Recycle(tmp);
    return status;
  }
  else
    return NS_OK;
}


/**
 * Maintains a small state machine with three states. "Not in tag",
 * "In tag, but not in quote" and "In quote inside a tag". It also
 * remembers what character started the quote (" or '). The state
 * variables are kept outside this function and are included as
 * parameters.
 *
 * @param in/out a_in_tag, if we are in a tag right now.
 * @param in/out a_in_quote_in_tag, if we are in a quote inside a tag.
 * @param in/out a_quote_char, the kind of quote (" or ').
 * @param in a_current_char, the next char. It decides which state
 *                           will be next.
 */
static void Update_in_tag_info(PRBool *a_in_tag, /* IN/OUT */
                   PRBool *a_in_quote_in_tag, /* IN/OUT */
                   PRUnichar *a_quote_char, /* IN/OUT (pointer to single char) */
                   PRUnichar a_current_char) /* IN */
{
  if(*a_in_tag) {
    // Keep us informed of what's quoted so that we
    // don't end the tag too soon. For instance in
    // <font face="weird>font<name">
    if(*a_in_quote_in_tag) {
      // We are in a quote. A quote is ended by the same
      // character that started it ('...' or "...")
      if(*a_quote_char == a_current_char) {
        *a_in_quote_in_tag = PR_FALSE;
      }
    } else {
      // We are not currently in a quote, but we may enter
      // one right this minute.
      switch(a_current_char) {
      case '"':
      case '\'':
        *a_in_quote_in_tag = PR_TRUE;
        *a_quote_char = a_current_char;
        break;       
      case '>':
        // Tag is ended
        *a_in_tag = PR_FALSE;
        break;
      default:
        // Do nothing
        ;
      }
    }
    return;
  }

  // Not in a tag. 
  // Check if we are entering a tag by looking for '<'.
  // All normal occurances of '<' should have been replaced
  // by &lt;
  if ('<' == a_current_char) {
    *a_in_tag = PR_TRUE;
    *a_in_quote_in_tag = PR_FALSE;
  }
}


/**
 * Converts whitespace to |&nbsp;|, if appropriate.
 *
 * @param in a_current_char, the char to convert.
 * @param in a_next_char, the char after the char to convert.
 * @param in a_convert_all_whitespace, if also the last whitespace
 *                                     in a sequence should be
 *                                     converted.
 * @param out a_out_string, result will be appended.
*/
static void Convert_whitespace(const PRUnichar a_current_char,
                               const PRUnichar a_next_char,
                               const PRBool a_convert_all_whitespace,
                               nsString& a_out_string)
{
  NS_ASSERTION('\t' == a_current_char || ' ' == a_current_char,
               "Convert_whitespace got something else than a whitespace!");

  PRUint32 number_of_nbsp = 0;
  PRUint32 number_of_space = 1; // Assume we're going to output one space.

  /* Output the spaces for a tab. All but the last are made into &nbsp;.
     The last is treated like a normal space. 
  */
  if('\t' == a_current_char) {
    number_of_nbsp = kSpacesForATab - 1;
  }

  if(' ' == a_next_char || '\t' == a_next_char || a_convert_all_whitespace) {
    number_of_nbsp += number_of_space;
    number_of_space = 0;
  }

  while(number_of_nbsp--) {
    a_out_string += NS_LITERAL_STRING("&nbsp;");
  }

  while(number_of_space--) {
    // a_out_string += ' '; gives error
    a_out_string += NS_LITERAL_STRING(" ");
  }

  return;
}

/**
 * Passes over the line and converts whitespace to |&nbsp;|, if appropriate
 *
 * @param in a_convert_all_whitespace, if also the last whitespace
 *                                     in a sequence should be
 *                                     converted.
 * @param out a_out_string, result will be appended.
*/
extern "C"
nsresult Line_convert_whitespace(const nsString& a_line,
                                 const PRBool a_convert_all_whitespace,
                                 nsString& a_out_line)
{
  PRBool in_tag = PR_FALSE;
  PRBool in_quote_in_tag = PR_FALSE;
  PRUnichar quote_char;

  for (PRUint32 i = 0; a_line.Length() > i; i++)
  {
    const PRUnichar ic = a_line[i];  // Cache

    Update_in_tag_info(&in_tag, &in_quote_in_tag, &quote_char, ic);
    // We don't touch anything inside a tag.
    if (!in_tag) {
      if (ic == ' ' || ic == '\t') {
        // Convert the whitespace to something appropriate
        Convert_whitespace(ic, a_line.Length() > i + 1 ? a_line[i + 1] : '\0',
                           a_convert_all_whitespace ||
                           !i, // First char on line
                           a_out_line);
      } else if (ic == '\r') {
        // strip CRs
      } else {
        a_out_line += ic;
      } 
    } else {
      // In tag. Don't change anything
      a_out_line += ic;
    }
  }
  return NS_OK;
}
