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

#include "mimetpfl.h"
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMimeTransition.h"
#include "mozITXTToHTMLConv.h"
#include "nsString.h"
#include "nsMimeStringResources.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "mimemoz2.h"
#include "prprf.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextPlainFlowed, MimeInlineTextPlainFlowedClass,
			 mimeInlineTextPlainFlowedClass, &MIME_SUPERCLASS);

static int MimeInlineTextPlainFlowed_parse_begin (MimeObject *);
static int MimeInlineTextPlainFlowed_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextPlainFlowed_parse_eof (MimeObject *, PRBool);

static MimeInlineTextPlainFlowedExData *MimeInlineTextPlainFlowedExDataList = 0;

extern "C" char *MimeTextBuildPrefixCSS(
                       PRInt32 quotedSizeSetting,      // mail.quoted_size
                       PRInt32    quotedStyleSetting,  // mail.quoted_style
                       char       *citationColor);     // mail.citation_color


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
       // Just good(tm) HTML. No reliance on CSS (only for prefs).

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
    nsresult rv = prefs->GetBoolPref( "mail.fixed_width_messages",
         &(exdata->fixedwidthfont)  );  // Check at least the success of one
    NS_ASSERTION(NS_SUCCEEDED(rv),
         "failed to get the mail.fixed_width_messages pref"); 
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
    nsresult rv = GetMailNewsFont(obj, exdata->fixedwidthfont,
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
       /* HACK: 4.x' editor can't break <div>s (e.g. to interleave comments).
          So, I just don't put it out until we have a better solution.
          Downside: This removes the information about the original format,
          which might be useful for styling/processing on the recipient
          side :(. */
  {
    nsCAutoString openingDiv("<div class=text-flowed");
    // We currently have to add formatting here. :-(
    if (!plainHTML && !fontstyle.IsEmpty())
    {
      openingDiv += " style=\"";
      openingDiv += fontstyle;
      openingDiv += '\"';
    }
    openingDiv += ">";
    status = MimeObject_write(obj, openingDiv, openingDiv.Length(), PR_FALSE);
    if (status < 0) return status;
  }

  return 0;
}

static int
MimeInlineTextPlainFlowed_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  MimeInlineTextPlainFlowed *text = (MimeInlineTextPlainFlowed *) nsnull;
  PRBool quoting = ( obj->options
    && ( obj->options->format_out == nsMimeOutput::nsMimeMessageQuoting ||
         obj->options->format_out == nsMimeOutput::nsMimeMessageBodyQuoting
       )           );  // see above

  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  struct MimeInlineTextPlainFlowedExData *exdata;
  struct MimeInlineTextPlainFlowedExData *prevexdata;
  exdata = MimeInlineTextPlainFlowedExDataList;
  prevexdata = MimeInlineTextPlainFlowedExDataList;
  while(exdata && (exdata->ownerobj != obj)) {
    prevexdata = exdata;
    exdata = exdata->next;
  }
  NS_ASSERTION(exdata, "The extra data has disappeared!");

  if(exdata == MimeInlineTextPlainFlowedExDataList) {
    // No previous.
    MimeInlineTextPlainFlowedExDataList = exdata->next;
  } else {
    prevexdata->next = exdata->next;
  }

  for(; exdata->quotelevel > 0; exdata->quotelevel--) {
    status = MimeObject_write(obj, "</blockquote>", 13, PR_FALSE);
    if(status<0) goto EarlyOut;
  }
  if (exdata->isSig && !quoting) {
    status = MimeObject_write(obj, "</div>", 6, PR_FALSE);      // txt-sig
    if (status<0) goto EarlyOut;
  }
  if (!quoting) // HACK (see above)
  {
    status = MimeObject_write(obj, "</div>", 6, PR_FALSE);  // text-flowed
    if (status<0) goto EarlyOut;
  }

  status = 0;
  text = (MimeInlineTextPlainFlowed *) obj;

EarlyOut:
  if (exdata) 
    PR_Free(exdata);
  PR_FREEIF(text->mCitationColor);

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

  // Grows the buffer if needed for this line
  // calculate needed buffersize. Use the linelength
  // and then double it to compensate for text converted
  // to links. Then add 15 for each '>' (blockquote) and
  // 20 for each ':' and '@'. (overhead in conversion to
  // links). Also add 5 for every '\r' or \n' and 7 for each
  // space in case they have to be replaced by &nbsp;
  int32 buffersizeneeded = length * 2 + 15*exdata->quotelevel;
  for(int32 i=0; i<length; i++) {
    switch(line[i]) {
    case '>': buffersizeneeded += 25; break; // '>' -> '<blockquote type=cite>'
    case '<': buffersizeneeded += 5; break; // '<' -> '&lt;'
    case ':': buffersizeneeded += 20; break;
    case '@': buffersizeneeded += 20; break;
    case '\r': buffersizeneeded += 5; break;
    case '\n': buffersizeneeded += 5; break;
    case ' ': buffersizeneeded += 7; break; // Not very good for other charsets
    case '\t': buffersizeneeded += 30; break;
    default: break; // Nothing
    }
  }
  buffersizeneeded += 30; // For possible <nobr> ... </nobr>

  /* There is the issue of guessing how much space we will need for emoticons.
     So what we will do is count the total number of "special" chars and
     multiply by 82 (max len for a smiley line) and add one for good measure.*/
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

  char *templine = (char *)PR_CALLOC(buffersizeneeded);
  if(!templine) 
    return MIME_OUT_OF_MEMORY;

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
    
  if (!skipConversion)
  {
    nsString strline;
    strline.AssignWithConversion(linep, (length - (linep - line)) );
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

    /* This is the main TXT to HTML conversion:
       escaping (very important), eventually recognizing etc. */
    rv = conv->ScanTXT(strline.GetUnicode(), whattodo, &wresult);
    if (NS_FAILED(rv))
    {
      PR_Free(templine);
      return -1;
    }

    /* avoid an extra string copy by using nsSubsumeStr, this transfers
       ownership of wresult to strresult so don't try to free wresult later. */
    nsSubsumeStr strresult(wresult, PR_TRUE /* assume ownership */);

    /* avoid yet another extra string copy of the line by using .ToCString
       which will convert and copy directly into the buffer we have already
       allocated. */
    strresult.ToCString(templine, buffersizeneeded - 10); 
  }
  else
  {
    nsCRT::memcpy(templine, line, length);
    templine[length] = '\0';
    status = NS_OK;
  }

  if (status != NS_OK) {
    PR_Free(templine);
    return status;
  }

  //  NS_ASSERTION(*line == 0 || *obj->obuffer, "have line or buffer");
  NS_ASSERTION(*line == 0 || *templine, "have line or buffer");

  char *templinep = templine;
  char *outlinep = obj->obuffer;


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
    /* Output <blockquote> */
    *outlinep='<'; outlinep++;
    *outlinep='b'; outlinep++;
    *outlinep='l'; outlinep++;
    *outlinep='o'; outlinep++;
    *outlinep='c'; outlinep++;
    *outlinep='k'; outlinep++;
    *outlinep='q'; outlinep++;
    *outlinep='u'; outlinep++;
    *outlinep='o'; outlinep++;
    *outlinep='t'; outlinep++;
    *outlinep='e'; outlinep++;
    *outlinep=' '; outlinep++;
    *outlinep='t'; outlinep++;
    *outlinep='y'; outlinep++;
    *outlinep='p'; outlinep++;
    *outlinep='e'; outlinep++;
    *outlinep='='; outlinep++;
    *outlinep='c'; outlinep++;
    *outlinep='i'; outlinep++;
    *outlinep='t'; outlinep++;
    *outlinep='e'; outlinep++;
    // This is to have us observe the user pref settings for citations
    MimeInlineTextPlainFlowed *tObj = (MimeInlineTextPlainFlowed *) obj;
    char *style = MimeTextBuildPrefixCSS(tObj->mQuotedSizeSetting,
                                         tObj->mQuotedStyleSetting,
                                         tObj->mCitationColor);
    if (!plainHTML && style && strlen(style))
    {
      *outlinep=' '; outlinep++;
      *outlinep='s'; outlinep++;
      *outlinep='t'; outlinep++;
      *outlinep='y'; outlinep++;
      *outlinep='l'; outlinep++;
      *outlinep='e'; outlinep++;
      *outlinep='='; outlinep++;
      *outlinep='\"'; outlinep++;
      strcpy(outlinep, style);
      outlinep += nsCRT::strlen(style);
      *outlinep='\"'; outlinep++;
      PR_FREEIF(style);
    }
    *outlinep='>'; outlinep++;
  }
  while(quoteleveldiff<0) {
    quoteleveldiff++;
    /* Output </blockquote> */
    *outlinep='<'; outlinep++;
    *outlinep='/'; outlinep++;
    *outlinep='b'; outlinep++;
    *outlinep='l'; outlinep++;
    *outlinep='o'; outlinep++;
    *outlinep='c'; outlinep++;
    *outlinep='k'; outlinep++;
    *outlinep='q'; outlinep++;
    *outlinep='u'; outlinep++;
    *outlinep='o'; outlinep++;
    *outlinep='t'; outlinep++;
    *outlinep='e'; outlinep++;
    *outlinep='>'; outlinep++;
  }
  exdata->quotelevel = linequotelevel;

  if(flowed) {
    // Check RFC 2646 "4.3. Usenet Signature Convention": "-- "+CRLF is
    // not a flowed line
    if
      (  // is "-- "LINEBREAK
        templinep[0] == '-'
        && PL_strlen(templinep) >= 4
        &&
        (
          !PL_strncmp(templinep, "-- \r", 4) ||
          !PL_strncmp(templinep, "-- \n", 4)
        )
      )
    {
      if (linequotelevel > 0 || exdata->isSig)
      {
        *outlinep='-'; outlinep++;
        *outlinep='-'; outlinep++;
        *outlinep='&'; outlinep++;
        *outlinep='n'; outlinep++;
        *outlinep='b'; outlinep++;
        *outlinep='s'; outlinep++;
        *outlinep='p'; outlinep++;
        *outlinep=';'; outlinep++;
        *outlinep='<'; outlinep++;
        *outlinep='b'; outlinep++;
        *outlinep='r'; outlinep++;
        *outlinep='>'; outlinep++;
      } else {
        exdata->isSig = PR_TRUE;

        const char *const sig_mark_start =
         "<div class=\"txt-sig\"><span class=\"txt-tag\">--&nbsp;<br></span>";
        PRUint32 sig_mark_start_length = PL_strlen(sig_mark_start);
        memcpy(outlinep, sig_mark_start, sig_mark_start_length);
        outlinep += sig_mark_start_length;
      }
      templinep += 3;  // Above, we already outputted the equvivalent to "-- "
    } else {
      // Output templinep
      while(*templinep && (*templinep != '\r') && (*templinep != '\n'))
      {
        uint32 intag = 0;
        const PRBool firstSpaceNbsp = PR_FALSE;   /* Convert all spaces but
             the first in a row into nbsp (i.e. always wrap) */
        PRBool nextSpaceIsNbsp = firstSpaceNbsp;

        if ('<' == *templinep)
          intag = 1;
        if (!intag) {
          if ((' ' == *templinep) && !exdata->inflow) {
            if (nextSpaceIsNbsp)
            {
              *outlinep='&'; outlinep++;
              *outlinep='n'; outlinep++;
              *outlinep='b'; outlinep++;
              *outlinep='s'; outlinep++;
              *outlinep='p'; outlinep++;
              *outlinep=';'; outlinep++;
            }
            else
            {
              *outlinep=' '; outlinep++;
            }
            nextSpaceIsNbsp = PR_TRUE;
            templinep++;
          } else if(('\t' == *templinep) && !exdata->inflow) {
            // Output 4 "spaces"
            for(int spaces=0; spaces<4; spaces++) {
              if (nextSpaceIsNbsp)
              {
                *outlinep='&'; outlinep++;
                *outlinep='n'; outlinep++;
                *outlinep='b'; outlinep++;
                *outlinep='s'; outlinep++;
                *outlinep='p'; outlinep++;
                *outlinep=';'; outlinep++;
              }
              else
              {
                *outlinep=' '; outlinep++;
              }
              nextSpaceIsNbsp = PR_TRUE;
            }
            templinep++;
          } else {
            nextSpaceIsNbsp = firstSpaceNbsp;
            *outlinep = *templinep;
            outlinep++;
            templinep++;
          } 
        } else {
          // In tag. Don't change anything
          nextSpaceIsNbsp = firstSpaceNbsp;
          *outlinep = *templinep;
          outlinep++;
          templinep++;
        }
      }
    }

    exdata->inflow=PR_TRUE;
  } else {
    // Fixed

    /*    // Output the stripped '>':s
    while(linequotelevel) {
      // output '>'
      *outlinep='&'; outlinep++;
      *outlinep='g'; outlinep++;
      *outlinep='t'; outlinep++;
      *outlinep=';'; outlinep++;
      linequotelevel--;
    }
    */
    
    uint32 intag = 0; 
    const PRBool firstSpaceNbsp = !plainHTML &&
                                  !obj->options->wrap_long_lines_p;
         /* If wrap, convert all spaces but the first in a row into nbsp,
            otherwise all. */
    PRBool nextSpaceIsNbsp = firstSpaceNbsp;
    while(*templinep && (*templinep != '\r') && (*templinep != '\n')) {
      if('<' == *templinep) intag = 1;
      if(!intag) {
        if((' ' == *templinep) && !exdata->inflow) {
          if (nextSpaceIsNbsp)
          {
            *outlinep='&'; outlinep++;
            *outlinep='n'; outlinep++;
            *outlinep='b'; outlinep++;
            *outlinep='s'; outlinep++;
            *outlinep='p'; outlinep++;
            *outlinep=';'; outlinep++;
          }
          else
          {
            *outlinep=' '; outlinep++;
          }
          nextSpaceIsNbsp = PR_TRUE;
          templinep++;
        } else if(('\t' == *templinep) && !exdata->inflow) {
          // Output 4 "spaces"
          for(int spaces=0; spaces<4; spaces++) {
            if (nextSpaceIsNbsp)
            {
              *outlinep='&'; outlinep++;
              *outlinep='n'; outlinep++;
              *outlinep='b'; outlinep++;
              *outlinep='s'; outlinep++;
              *outlinep='p'; outlinep++;
              *outlinep=';'; outlinep++;
            }
            else
            {
              *outlinep=' '; outlinep++;
            }
            nextSpaceIsNbsp = PR_TRUE;
          }
          templinep++;
        } else {
          nextSpaceIsNbsp = firstSpaceNbsp;
          *outlinep = *templinep;
          outlinep++;
          templinep++;
        } 
      } else {
        // In tag. Don't change anything
        nextSpaceIsNbsp = firstSpaceNbsp;
        *outlinep = *templinep;
        outlinep++;
        templinep++;
      }
    }

    *outlinep='<'; outlinep++;
    *outlinep='b'; outlinep++;
    *outlinep='r'; outlinep++;
    *outlinep='>'; outlinep++;

    exdata->inflow = PR_FALSE;
  } // End Fixed line

  *outlinep='\0'; outlinep++;

  PR_Free(templine);

  // Calculate linelength as
  // <pointer to the next free char>-<pointer to the beginning>-1
  // '-1' for the terminating '\0'
  if (!(exdata->isSig && quoting))
    return MimeObject_write(obj, obj->obuffer, outlinep-obj->obuffer-1, PR_TRUE);
  else
    return NS_OK;
}
