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
#include "plstr.h"
#include "mimethtm.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"
#include "mimemoz2.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextHTML, MimeInlineTextHTMLClass,
			 mimeInlineTextHTMLClass, &MIME_SUPERCLASS);

static int MimeInlineTextHTML_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextHTML_parse_eof (MimeObject *, PRBool);
static int MimeInlineTextHTML_parse_begin (MimeObject *obj);

static int
MimeInlineTextHTMLClassInitialize(MimeInlineTextHTMLClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextHTML_parse_begin;
  oclass->parse_line  = MimeInlineTextHTML_parse_line;
  oclass->parse_eof   = MimeInlineTextHTML_parse_eof;

  return 0;
}

static int
MimeInlineTextHTML_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)&mimeLeafClass)->parse_begin(obj);
  if (status < 0) return status;
  
  if (!obj->output_p) return 0;

  // Set a default font (otherwise unicode font will be used since the data is UTF-8).
  if (nsMimeOutput::nsMimeMessageBodyDisplay == obj->options->format_out ||
      nsMimeOutput::nsMimeMessagePrintOutput == obj->options->format_out)
  {
    char buf[256];            // local buffer for html tag
    char fontName[128];       // default font name
    PRInt32 fontSize;         // default font size
    if (NS_SUCCEEDED(GetMailNewsFont(obj, PR_FALSE, fontName, sizeof(fontName), &fontSize)))
    {
      PR_snprintf(buf, 256, "<div class=\"moz-text-html\" style=\"font-family: %s; font-size: %dpx;\">", (const char *) fontName, fontSize);
      status = MimeObject_write(obj, buf, nsCRT::strlen(buf), PR_FALSE);
    }
    else
    {
      status = MimeObject_write(obj, "<div class=\"moz-text-html\">", 27, PR_FALSE);
    }
    if(status<0) return status;
  }

  MimeInlineTextHTML  *textHTML = (MimeInlineTextHTML *) obj;
  
  textHTML->charset = nsnull;
  
  /* If this HTML part has a Content-Base header, and if we're displaying
	 to the screen (that is, not writing this part "raw") then translate
   that Content-Base header into a <BASE> tag in the HTML.
  */
  if (obj->options &&
    obj->options->write_html_p &&
    obj->options->output_fn)
  {
    char *base_hdr = MimeHeaders_get (obj->headers, HEADER_CONTENT_BASE,
      PR_FALSE, PR_FALSE);
    
    /* rhp - for MHTML Spec changes!!! */
    if (!base_hdr)
    {
      base_hdr = MimeHeaders_get (obj->headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE);
    }
    /* rhp - for MHTML Spec changes!!! */
    
    /* Encapsulate the entire text/html part inside an in-flow
    layer.  This will provide it a private coordinate system and
    prevent it from escaping the bounds of its clipping so that
    it might, for example, spoof a mail header. */
    if (obj->options->set_html_state_fn)
    {
      status = obj->options->set_html_state_fn(obj->options->stream_closure,
        PR_TRUE,   /* layer_encapulate_p */
        PR_TRUE,   /* start_p */
        PR_FALSE); /* abort_p */
      if (status < 0) return status;
    }
    
    if (base_hdr)
    {
      char *buf = (char *) PR_MALLOC(nsCRT::strlen(base_hdr) + 20);
      const char *in;
      char *out;
      if (!buf)
        return MIME_OUT_OF_MEMORY;
      
        /* The value of the Content-Base header is a number of "words".
        Whitespace in this header is not significant -- it is assumed
        that any real whitespace in the URL has already been encoded,
        and whitespace has been inserted to allow the lines in the
        mail header to be wrapped reasonably.  Creators are supposed
        to insert whitespace every 40 characters or less.
      */
      PL_strcpy(buf, "<BASE HREF=\"");
      out = buf + nsCRT::strlen(buf);
      
      for (in = base_hdr; *in; in++)
        /* ignore whitespace and quotes */
        if (!nsCRT::IsAsciiSpace(*in) && *in != '"')
          *out++ = *in;
        
        /* Close the tag and argument. */
        *out++ = '"';
        *out++ = '>';
        *out++ = 0;
        
        PR_Free(base_hdr);
        
        status = MimeObject_write(obj, buf, nsCRT::strlen(buf), PR_FALSE);
        PR_Free(buf);
        if (status < 0) return status;
    }
  }
  
  // rhp: For a change, we will write out a separator after formatted text
  //      bodies.
  status = MimeObject_write_separator(obj);
  if (status < 0) return status;
  
  return 0;
}


static int
MimeInlineTextHTML_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  MimeInlineTextHTML  *textHTML = (MimeInlineTextHTML *) obj;

  if (!obj->output_p) 
    return 0;

  if (!obj->options || !obj->options->output_fn)
    return 0;

  if (!textHTML->charset)
  {
    // First, try to detect a charset via a META tag!
    if (PL_strncasestr(line, "META", length) && 
        PL_strncasestr(line, "HTTP-EQUIV", length) && 
        PL_strncasestr(line, "CONTENT", length) && 
        PL_strncasestr(line, "CHARSET", length) 
        ) 
    { 
      char *workLine = (char *)PR_Malloc(length + 1);
      if (workLine)
      {
        nsCRT::memset(workLine, 0, length + 1);
        PL_strncpy(workLine, line, length);
        char *cp1 = PL_strncasestr(workLine, "CHARSET", length);
        if (cp1)
        {
          char *cp = PL_strncasestr(cp1, "=", (length - (int)(cp1-line)) ) + 1; 

          char seps[]   = " \"\'"; 
          char *token; 
          char* newStr; 
          token = nsCRT::strtok(cp, seps, &newStr); 
          if (token != NULL) 
          { 
            textHTML->charset = nsCRT::strdup(token); 
          } 
        }

        PR_FREEIF(workLine);
      }

      // Eat the META tag line that specifies CHARSET!
      if (textHTML->charset)
        return 0;
    }
  }

  // Now, just write out the data...
  return MimeObject_write(obj, line, length, PR_TRUE);
}

/* This method is the same as that of MimeInlineTextRichtext (and thus
   MimeInlineTextEnriched); maybe that means that MimeInlineTextHTML
   should share a common parent with them which is not also shared by
   MimeInlineTextPlain?
 */
static int
MimeInlineTextHTML_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  MimeInlineTextHTML  *textHTML = (MimeInlineTextHTML *) obj;
  if (obj->closed_p) return 0;

  PR_FREEIF(textHTML->charset);

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (nsMimeOutput::nsMimeMessageBodyDisplay == obj->options->format_out ||
      nsMimeOutput::nsMimeMessagePrintOutput == obj->options->format_out)
    status = MimeObject_write(obj, "</div>", 6, PR_FALSE);

  if (obj->output_p &&
	  obj->options &&
	  obj->options->write_html_p &&
	  obj->options->set_html_state_fn)
	{
      return obj->options->set_html_state_fn(obj->options->stream_closure,
                                             PR_TRUE,  /* layer_encapulate_p */
                                             PR_FALSE, /* start_p */
                                             abort_p);
	}
  return 0;
}
