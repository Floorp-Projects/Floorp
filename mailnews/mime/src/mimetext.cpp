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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "mimetext.h"
#include "mimebuf.h"
#include "mimethtm.h"
#include "comi18n.h"
#include "mimemoz2.h"

#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsMimeTypes.h"
#include "nsReadableUtils.h"

#define MIME_SUPERCLASS mimeLeafClass
MimeDefClass(MimeInlineText, MimeInlineTextClass, mimeInlineTextClass,
			 &MIME_SUPERCLASS);

static int MimeInlineText_initialize (MimeObject *);
static void MimeInlineText_finalize (MimeObject *);
static int MimeInlineText_rot13_line (MimeObject *, char *line, PRInt32 length);
static int MimeInlineText_parse_eof (MimeObject *obj, PRBool abort_p);
static int MimeInlineText_parse_end  (MimeObject *, PRBool);
static int MimeInlineText_parse_decoded_buffer (char *, PRInt32, MimeObject *);
static int MimeInlineText_rotate_convert_and_parse_line(char *, PRInt32,
                 MimeObject *);
static int MimeInlineText_open_dam(char *line, PRInt32 length, MimeObject *obj);
static int MimeInlineText_initializeCharset(MimeObject *obj);

static int
MimeInlineTextClassInitialize(MimeInlineTextClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  MimeLeafClass   *lclass = (MimeLeafClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize           = MimeInlineText_initialize;
  oclass->finalize             = MimeInlineText_finalize;
  oclass->parse_eof			   = MimeInlineText_parse_eof;
  oclass->parse_end            = MimeInlineText_parse_end;
  clazz->rot13_line            = MimeInlineText_rot13_line;
  lclass->parse_decoded_buffer = MimeInlineText_parse_decoded_buffer;
  return 0;
}

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

static int
MimeInlineText_initialize (MimeObject *obj)
{
  /* This is an abstract class; it shouldn't be directly instanciated. */
  PR_ASSERT(obj->clazz != (MimeObjectClass *) &mimeInlineTextClass);

  ((MimeInlineText *) obj)->initializeCharset = PR_FALSE;
  ((MimeInlineText *) obj)->needUpdateMsgWinCharset = PR_FALSE;
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(obj);
}

static int MimeInlineText_initializeCharset(MimeObject *obj)
{
  MimeInlineText  *text = (MimeInlineText *) obj;

  text->inputAutodetect = PR_FALSE;
  text->charsetOverridable = PR_FALSE;
  
  /* Figure out an appropriate charset for this object.
  */
  if (!text->charset && obj->headers)
  {
    if (obj->options && obj->options->override_charset)
    {
      text->charset = nsCRT::strdup(obj->options->default_charset);
    }
    else
    {
      char *ct = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE,
                                  PR_FALSE, PR_FALSE);
      if (ct)
      {
        text->charset = MimeHeaders_get_parameter (ct, "charset", NULL, NULL);
        PR_Free(ct);
      }
    
      if (!text->charset)
      {
        /* If we didn't find "Content-Type: ...; charset=XX" then look
           for "X-Sun-Charset: XX" instead.  (Maybe this should be done
           in MimeSunAttachmentClass, but it's harder there than here.)
         */
        text->charset = MimeHeaders_get (obj->headers,
                                          HEADER_X_SUN_CHARSET,
                                          PR_FALSE, PR_FALSE);
      }

      if (!text->charset)
      {
        nsresult res;
        nsXPIDLString detector_name;
        
        text->charsetOverridable = PR_TRUE;

        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &res)); 
        if (NS_SUCCEEDED(res)) {
          if (NS_SUCCEEDED(prefs->GetLocalizedUnicharPref("intl.charset.detector", getter_Copies(detector_name)))) {
            //only if we can get autodetector name correctly, do we set this to true
            text->inputAutodetect = PR_TRUE;
          }
        }
        
        if (obj->options && obj->options->default_charset)
          text->charset = nsCRT::strdup(obj->options->default_charset);
        else
        {
          // New change for falling back to a default view charset
          nsresult        rv;
          nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
          if ( NS_SUCCEEDED(rv) && prefs)
          {
            nsXPIDLString value;
            rv = prefs->GetLocalizedUnicharPref("mailnews.view_default_charset", getter_Copies(value));
            if(NS_SUCCEEDED(rv)) {
              text->charset = ToNewUTF8String(value);
            }
          }

          if (!text->charset)
            text->charset = nsCRT::strdup("");
        }
      } 
    }
  }
  
  if (text->inputAutodetect)
  {
    //we need to prepare lineDam for charset detection
    text->lineDamBuffer = (char*)PR_Malloc(DAM_MAX_BUFFER_SIZE);
    text->lineDamPtrs = (char**)PR_Malloc(DAM_MAX_LINES*sizeof(char*));
    text->curDamOffset = 0;
    text->lastLineInDam = 0;
    if (!text->lineDamBuffer || !text->lineDamPtrs)
    {
      text->inputAutodetect = PR_FALSE;
      PR_FREEIF(text->lineDamBuffer);
      PR_FREEIF(text->lineDamPtrs);
    }
  }

  //update MsgWindow charset if we are instructed to do so
  if (text->needUpdateMsgWinCharset && *text->charset) {
    if (!nsCRT::strcasecmp(text->charset, "us-ascii"))
      SetMailCharacterSetToMsgWindow(obj, NS_LITERAL_STRING("ISO-8859-1").get());
    else
      SetMailCharacterSetToMsgWindow(obj, NS_ConvertASCIItoUCS2(text->charset).get());
  }

  text->initializeCharset = PR_TRUE;

  return 0;
}


static void
MimeInlineText_finalize (MimeObject *obj)
{
  MimeInlineText *text = (MimeInlineText *) obj;

  obj->clazz->parse_eof (obj, PR_FALSE);
  obj->clazz->parse_end (obj, PR_FALSE);

  PR_FREEIF(text->charset);

  /* Should have been freed by parse_eof, but just in case... */
  PR_ASSERT(!text->cbuffer);
  PR_FREEIF (text->cbuffer);

  if (text->inputAutodetect) {
    PR_FREEIF(text->lineDamBuffer);
    PR_FREEIF(text->lineDamPtrs);
    text->inputAutodetect = PR_FALSE;
  }

  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize (obj);
}


static int
MimeInlineText_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;

  if (obj->closed_p) return 0;
  NS_ASSERTION(!obj->parsed_p, "obj already parsed");

  MimeInlineText *text = (MimeInlineText *) obj;

  //we haven't find charset yet? now its the time
  if (text->inputAutodetect)
     status = MimeInlineText_open_dam(nsnull, 0, obj);
   
  /* If there is still data in the ibuffer, that means that the last line of
	 this part didn't end in a newline; so push it out anyway (this means that
	 the parse_line method will be called with a string with no trailing
	 newline, which isn't the usual case.)  We do this here, rather than in 
	 MimeObject_parse_eof, because MimeObject likes to shove things through
	 parse_line, and we have to shove it through the magic rotating-and-converting
	 code.  So, we do that and digest the buffer before MimeObject has a chance
	 to do the wrong thing.  See bug #26276 for more painful details.
   */
  if (!abort_p &&
	  obj->ibuffer_fp > 0)
	{
	  status = MimeInlineText_rotate_convert_and_parse_line (obj->ibuffer,
	  															 obj->ibuffer_fp,
	  															 obj);
	  obj->ibuffer_fp = 0;
	  if (status < 0)
		{
		  obj->closed_p = PR_TRUE;
		  return status;
		}
	}
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof (obj, abort_p);
}

static int
MimeInlineText_parse_end (MimeObject *obj, PRBool abort_p)
{
  MimeInlineText *text = (MimeInlineText *) obj;

  if (obj->parsed_p)
	{
	  PR_ASSERT(obj->closed_p);
	  return 0;
	}

  /* We won't be needing this buffer any more; nuke it. */
  PR_FREEIF(text->cbuffer);
  text->cbuffer_size = 0;

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_end (obj, abort_p);
}


/* This maps A-M to N-Z and N-Z to A-M.  All other characters are left alone.
   (Comments in GNUS imply that for Japanese, one should rotate by 47?)
 */
static const unsigned char MimeInlineText_rot13_table[256] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
  65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 91, 92, 93, 94, 95, 96,
  110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 97, 98,
  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 123, 124, 125, 126,
  127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
  142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156,
  157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171,
  172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
  187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201,
  202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216,
  217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
  232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
  247, 248, 249, 250, 251, 252, 253, 254, 255 };

static int
MimeInlineText_rot13_line (MimeObject *obj, char *line, PRInt32 length)
{
  unsigned char *s, *end;
  PR_ASSERT(line);
  if (!line) return -1;
  s = (unsigned char *) line;
  end = s + length;
  while (s < end)
	{
	  *s = MimeInlineText_rot13_table[*s];
	  s++;
	}
  return 0;
}


static int
MimeInlineText_parse_decoded_buffer (char *buf, PRInt32 size, MimeObject *obj)
{
  PR_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  /* MimeLeaf takes care of this. */
  PR_ASSERT(obj->output_p && obj->options && obj->options->output_fn);
  if (!obj->options) return -1;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (!obj->options->write_html_p)
	return MimeObject_write(obj, buf, size, PR_TRUE);

  /* This is just like the parse_decoded_buffer method we inherit from the
	 MimeLeaf class, except that we line-buffer to our own wrapper on the
	 `parse_line' method instead of calling the `parse_line' method directly.
   */
  return mime_LineBuffer (buf, size,
						 &obj->ibuffer, &obj->ibuffer_size, &obj->ibuffer_fp,
						 PR_TRUE,
						 ((int (*PR_CALLBACK) (char *, PRInt32, void *))
						  /* This cast is to turn void into MimeObject */
						  MimeInlineText_rotate_convert_and_parse_line),
						 obj);
}


#define MimeInlineText_grow_cbuffer(text, desired_size) \
  (((desired_size) >= (text)->cbuffer_size) ? \
   mime_GrowBuffer ((desired_size), sizeof(char), 100, \
				   &(text)->cbuffer, &(text)->cbuffer_size) \
   : 0)

static int 
MimeInlineText_convert_and_parse_line(char *line, PRInt32 length, MimeObject *obj)
{
  int status;
  char *converted = 0;
  PRInt32 converted_len = 0;
  
  MimeInlineText *text = (MimeInlineText *) obj;

  //in case of charset autodetection, charset can be override by meta charset
  if (text->charsetOverridable) {
    if (mime_typep(obj, (MimeObjectClass *) &mimeInlineTextHTMLClass))
    {
      MimeInlineTextHTML  *textHTML = (MimeInlineTextHTML *) obj;
      if (textHTML->charset && 
          *textHTML->charset &&
          nsCRT::strcmp(textHTML->charset, text->charset))
      {
        //if meta tag specified charset is different from our detected result, use meta charset.
        //but we don't want to redo previous lines
        MIME_get_unicode_decoder(textHTML->charset, getter_AddRefs(text->inputDecoder));
        PR_FREEIF(text->charset);
        text->charset = nsCRT::strdup(textHTML->charset);

        //update MsgWindow charset if we are instructed to do so
        if (text->needUpdateMsgWinCharset && *text->charset) {
          if (!nsCRT::strcasecmp(text->charset, "us-ascii"))
            SetMailCharacterSetToMsgWindow(obj, NS_LITERAL_STRING("ISO-8859-1").get());
          else
            SetMailCharacterSetToMsgWindow(obj, NS_ConvertASCIItoUCS2(text->charset).get());
        }
      }
    }
  }

  //initiate decoder if not yet
  if (text->inputDecoder == nsnull)
    MIME_get_unicode_decoder(text->charset, getter_AddRefs(text->inputDecoder));
  if (text->utf8Encoder == nsnull)
    MIME_get_unicode_encoder("UTF-8", getter_AddRefs(text->utf8Encoder));

  PRBool useInputCharsetConverter = obj->options->m_inputCharsetToUnicodeDecoder && !nsCRT::strcasecmp(text->charset, obj->options->charsetForCachedInputDecoder.get());

  if (useInputCharsetConverter)
    status = obj->options->charset_conversion_fn(/*input_autodetect*/PR_FALSE, line, length,
                         text->charset,
												 "UTF-8",
												 &converted,
												 &converted_len,
                         obj->options->stream_closure, obj->options->m_inputCharsetToUnicodeDecoder,
                       obj->options->m_unicodeToUTF8Encoder);
  else
    status = obj->options->charset_conversion_fn(/*input_autodetect*/PR_FALSE, line, length,
                         text->charset,
												 "UTF-8",
												 &converted,
												 &converted_len,
                         obj->options->stream_closure, (nsIUnicodeDecoder*)text->inputDecoder,
                         (nsIUnicodeEncoder*)text->utf8Encoder);

  if (status < 0)
  {
    PR_FREEIF(converted);
    return status;
  }

  if (converted)
  {
    line = converted;
    length = converted_len;
  }

  /* Now that the line has been converted, call the subclass's parse_line
	 method with the decoded data. */
  status = obj->clazz->parse_line(line, length, obj);
  PR_FREEIF(converted);

  return status;
}

//In this function call, all buffered lines in lineDam will be sent to charset detector 
// and a charset will be used to parse all those line and following lines in this mime obj.
static int 
MimeInlineText_open_dam(char *line, PRInt32 length, MimeObject *obj)
{
  MimeInlineText *text = (MimeInlineText *) obj;
  const char* detectedCharset = nsnull;
  nsresult res;
  int status;
  PRInt32 i;

  if (text->curDamOffset <= 0) {
    //there is nothing in dam, use current line for detection
    if (length > 0) {
      res = MIME_detect_charset(line, length, &detectedCharset);  
    }
  } else {
    //we have stuff in dam, use the one 
    if (text->curDamOffset > length)
      res = MIME_detect_charset(text->lineDamBuffer, text->curDamOffset, &detectedCharset);  
    else 
      res = MIME_detect_charset(text->lineDamBuffer, text->curDamOffset, &detectedCharset);  

  }

  //set the charset for this obj
  if (NS_SUCCEEDED(res) && detectedCharset && *detectedCharset)  {
    PR_FREEIF(text->charset);
    text->charset = nsCRT::strdup(detectedCharset);

    //update MsgWindow charset if we are instructed to do so
    if (text->needUpdateMsgWinCharset && *text->charset) {
      if (!nsCRT::strcasecmp(text->charset, "us-ascii"))
        SetMailCharacterSetToMsgWindow(obj, NS_LITERAL_STRING("ISO-8859-1").get());
      else
        SetMailCharacterSetToMsgWindow(obj, NS_ConvertASCIItoUCS2(text->charset).get());
    }
  }

  //process dam and line using the charset
  if (text->curDamOffset) {
    for (i = 0; i < text->lastLineInDam-1; i++)
    {
      status = MimeInlineText_convert_and_parse_line(
                text->lineDamPtrs[i],  
                text->lineDamPtrs[i+1] - text->lineDamPtrs[i],
                obj  );
    }
    status = MimeInlineText_convert_and_parse_line(
                text->lineDamPtrs[i],
                text->lineDamBuffer + text->curDamOffset - text->lineDamPtrs[i],
                obj );
  }

  if (length)
    status = MimeInlineText_convert_and_parse_line(line, length, obj);

  PR_Free(text->lineDamPtrs);
  PR_Free(text->lineDamBuffer);
  text->lineDamPtrs = nsnull;
  text->lineDamBuffer = nsnull;
  text->inputAutodetect = PR_FALSE;

  return status;
}


static int
MimeInlineText_rotate_convert_and_parse_line(char *line, PRInt32 length,
											 MimeObject *obj)
{
  int status = 0;
  MimeInlineTextClass *textc = (MimeInlineTextClass *) obj->clazz;

  PR_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  /* Rotate the line, if desired (this happens on the raw data, before any
	 charset conversion.) */
  if (obj->options && obj->options->rot13_p)
	{
	  status = textc->rot13_line(obj, line, length);
	  if (status < 0) return status;
	}

  // Now convert to the canonical charset, if desired.
  //
  PRBool  doConvert = PR_TRUE;
  // Don't convert vCard data
  if ( ( (obj->content_type) && (!PL_strcasecmp(obj->content_type, TEXT_VCARD)) ) ||
       (obj->options->format_out == nsMimeOutput::nsMimeMessageSaveAs) )
    doConvert = PR_FALSE;
    
  // Only convert if the user prefs is false 
  if ( (obj->options && obj->options->charset_conversion_fn) &&
       (!obj->options->force_user_charset) &&
       (doConvert)       
     )
	{
    MimeInlineText  *text = (MimeInlineText *) obj;

    if (!text->initializeCharset)
      MimeInlineText_initializeCharset(obj);

    //if autodetect is on, push line to dam
    if (text->inputAutodetect)
    {
      //see if we reach the lineDam buffer limit, if so, there is no need to keep buffering
      if (text->lastLineInDam >= DAM_MAX_LINES ||
          DAM_MAX_BUFFER_SIZE - text->curDamOffset <= length) {
        //we let open dam process this line as well as thing that already in Dam
        //In case there is nothing in dam because this line is too big, we need to 
        //perform autodetect on this line
        status = MimeInlineText_open_dam(line, length, obj);
      }
      else {
        //buffering current line
        text->lineDamPtrs[text->lastLineInDam] = text->lineDamBuffer + text->curDamOffset;
        nsCRT::memcpy(text->lineDamPtrs[text->lastLineInDam], line, length);
        text->lastLineInDam++;
        text->curDamOffset += length;
      }
    }
    else 
      status = MimeInlineText_convert_and_parse_line(line, length, obj);
	}
  else
    status = obj->clazz->parse_line(line, length, obj);

  return status;
}
