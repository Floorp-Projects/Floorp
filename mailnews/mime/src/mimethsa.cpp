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
 * The Original Code is mozilla.org libmime code.
 *
 * The Initial Developer of the Original Code is
 * Ben Bucksch <http://www.bucksch.org/1/projects/mozilla/>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Most of this code is copied from mimethpl; see there for source comments.
   If you find a bug here, check that class, too.
*/

#include "mimethsa.h"
#include "prmem.h"
#include "prlog.h"
#include "msgCore.h"
#include "mimemoz2.h"
#include "nsString.h"
#include "nsReadableUtils.h"

//#define DEBUG_BenB

#define MIME_SUPERCLASS mimeInlineTextHTMLClass
MimeDefClass(MimeInlineTextHTMLSanitized, MimeInlineTextHTMLSanitizedClass,
			 mimeInlineTextHTMLSanitizedClass, &MIME_SUPERCLASS);

static int MimeInlineTextHTMLSanitized_parse_line (char *, PRInt32,
                                                   MimeObject *);
static int MimeInlineTextHTMLSanitized_parse_begin (MimeObject *obj);
static int MimeInlineTextHTMLSanitized_parse_eof (MimeObject *, PRBool);
static void MimeInlineTextHTMLSanitized_finalize (MimeObject *obj);

static int
MimeInlineTextHTMLSanitizedClassInitialize(MimeInlineTextHTMLSanitizedClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "problem with superclass");
  oclass->parse_line  = MimeInlineTextHTMLSanitized_parse_line;
  oclass->parse_begin = MimeInlineTextHTMLSanitized_parse_begin;
  oclass->parse_eof   = MimeInlineTextHTMLSanitized_parse_eof;
  oclass->finalize    = MimeInlineTextHTMLSanitized_finalize;

  return 0;
}

static int
MimeInlineTextHTMLSanitized_parse_begin (MimeObject *obj)
{
#ifdef DEBUG_BenB
printf("parse_begin\n");
#endif
  MimeInlineTextHTMLSanitized *textHTMLSan =
                                       (MimeInlineTextHTMLSanitized *) obj;
  textHTMLSan->complete_buffer = new nsString();
#ifdef DEBUG_BenB
printf(" B1\n");
printf(" cbp: %d\n", textHTMLSan->complete_buffer);
#endif
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0)
    return status;
#ifdef DEBUG_BenB
printf(" B2\n");
#endif

  // charset
  /* honestly, I don't know how that charset stuff works in libmime.
     The part in mimethtm doesn't make much sense to me either.
     I'll just dump the charset we get in the mime headers into a
     HTML meta http-equiv.
     XXX Not sure, if that is correct, though. */
  char *content_type =
    (obj->headers
     ? MimeHeaders_get(obj->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE)
     : 0);
  if (content_type)
  {
    char* charset = MimeHeaders_get_parameter(content_type,
                                              HEADER_PARM_CHARSET,
                                              NULL, NULL);
    PR_Free(content_type);
    if (charset)
    {
      nsCAutoString charsetline(
        "\n<meta http-equiv=\"Context-Type\" content=\"text/html; charset=");
      charsetline += charset;
      charsetline += "\">\n";
      int status = MimeObject_write(obj,
                                    charsetline.get(),
                                    charsetline.Length(),
                                    PR_TRUE);
      PR_Free(charset);
      if (status < 0)
        return status;
    }
  }
#ifdef DEBUG_BenB
printf("/parse_begin\n");
#endif
  return 0;
}

static int
MimeInlineTextHTMLSanitized_parse_eof (MimeObject *obj, PRBool abort_p)
{
#ifdef DEBUG_BenB
printf("parse_eof\n");
#endif

  if (obj->closed_p)
    return 0;
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) 
    return status;
  MimeInlineTextHTMLSanitized *textHTMLSan =
                                       (MimeInlineTextHTMLSanitized *) obj;

#ifdef DEBUG_BenB
printf(" cbp: %d\n", textHTMLSan->complete_buffer);
printf(" closed_p: %s\n", obj->closed_p?"true":"false");
#endif
  if (!textHTMLSan || !textHTMLSan->complete_buffer)
  {
#ifdef DEBUG_BenB
printf("/parse_eof (early exit)\n");
#endif
    return 0;
  }
#ifdef DEBUG_BenB
printf(" E1\n");
printf("buffer: -%s-\n", NS_LossyConvertUCS2toASCII(*textHTMLSan->complete_buffer).get());
#endif

  char* allowedTags = 0;
  nsIPref *prefs = GetPrefServiceManager(obj->options);
  if (prefs)
    prefs->CopyCharPref("mailnews.display.html_sanitizer.allowed_tags",
                        &allowedTags);

#ifdef DEBUG_BenB
printf(" E2\n");
#endif
  nsString& cb = *(textHTMLSan->complete_buffer);
#ifdef DEBUG_BenB
printf(" E3\n");
#endif
  nsString sanitized;
#ifdef DEBUG_BenB
printf(" E4\n");
#endif
  HTMLSanitize(cb, sanitized, 0, NS_ConvertASCIItoUCS2(allowedTags));
#ifdef DEBUG_BenB
printf(" E5\n");
#endif

  NS_ConvertUTF16toUTF8 resultCStr(sanitized);
#ifdef DEBUG_BenB
printf(" E6\n");
#endif
  // TODO parse each line independently
  /* That function doesn't work correctly, if the first META tag is no
     charset spec. (It assumes that it's on its own line.)
     Most likely not fatally wrong, however. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_line(
                             resultCStr.BeginWriting(),
                             resultCStr.Length(),
                             obj);
#ifdef DEBUG_BenB
printf(" E7\n");
#endif

#ifdef DEBUG_BenB
printf(" E8\n");
#endif

  cb.Truncate();

#ifdef DEBUG_BenB
printf("/parse_eof\n");
#endif

  return status;
}

void
MimeInlineTextHTMLSanitized_finalize (MimeObject *obj)
{
#ifdef DEBUG_BenB
printf("finalize\n");
#endif
  MimeInlineTextHTMLSanitized *textHTMLSan =
                                        (MimeInlineTextHTMLSanitized *) obj;
#ifdef DEBUG_BenB
printf(" cbp: %d\n", textHTMLSan->complete_buffer);
printf(" F1\n");
#endif

  if (textHTMLSan && textHTMLSan->complete_buffer)
  {
    obj->clazz->parse_eof(obj, PR_FALSE);
#ifdef DEBUG_BenB
printf(" F2\n");
#endif
    delete textHTMLSan->complete_buffer;
#ifdef DEBUG_BenB
printf(" cbp: %d\n", textHTMLSan->complete_buffer);
printf(" F3\n");
#endif
    textHTMLSan->complete_buffer = NULL;
  }

#ifdef DEBUG_BenB
printf(" cbp: %d\n", textHTMLSan->complete_buffer);
printf(" F4\n");
#endif
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize (obj);
#ifdef DEBUG_BenB
printf("/finalize\n");
#endif
}

static int
MimeInlineTextHTMLSanitized_parse_line (char *line, PRInt32 length,
                                          MimeObject *obj)
{
#ifdef DEBUG_BenB
printf("p");
#endif
  MimeInlineTextHTMLSanitized *textHTMLSan =
                                       (MimeInlineTextHTMLSanitized *) obj;
#ifdef DEBUG_BenB
printf("%d", textHTMLSan->complete_buffer);
#endif

  if (!textHTMLSan || !(textHTMLSan->complete_buffer))
  {
#ifdef DEBUG
printf("Can't output: %s\n", line);
#endif
    return -1;
  }

  nsCString linestr(line, length);
  NS_ConvertUTF8toUCS2 line_ucs2(linestr.get());
  if (length && line_ucs2.IsEmpty())
    line_ucs2.AssignWithConversion(linestr.get());
  (textHTMLSan->complete_buffer)->Append(line_ucs2);

#ifdef DEBUG_BenB
printf("l ");
#endif
  return 0;
}
