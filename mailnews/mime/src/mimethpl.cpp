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

/* TODO:
  - If you Save As File .html with this mode, you get a total mess.
  - Print is untested (crashes in all modes).
*/
/* If you fix a bug here, check, if the same is also in mimethsa, because that
   class is based on this class. */

#include "mimethpl.h"
#include "prlog.h"
#include "msgCore.h"
#include "mimemoz2.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDocumentEncoder.h" // for output flags

#define MIME_SUPERCLASS mimeInlineTextPlainClass
/* I should use the Flowed class as base (because our HTML->TXT converter
   can generate flowed, and we tell it to) - this would get a bit nicer
   rendering. However, that class is more picky about line endings
   and I currently don't feel like splitting up the generated plaintext
   into separate lines again. So, I just throw the whole message at once
   at the TextPlain_parse_line function - it happens to work *g*. */
MimeDefClass(MimeInlineTextHTMLAsPlaintext, MimeInlineTextHTMLAsPlaintextClass,
			 mimeInlineTextHTMLAsPlaintextClass, &MIME_SUPERCLASS);

static int MimeInlineTextHTMLAsPlaintext_parse_line (char *, PRInt32,
                                                     MimeObject *);
static int MimeInlineTextHTMLAsPlaintext_parse_begin (MimeObject *obj);
static int MimeInlineTextHTMLAsPlaintext_parse_eof (MimeObject *, PRBool);
static void MimeInlineTextHTMLAsPlaintext_finalize (MimeObject *obj);

static int
MimeInlineTextHTMLAsPlaintextClassInitialize(MimeInlineTextHTMLAsPlaintextClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "problem with superclass");
  oclass->parse_line  = MimeInlineTextHTMLAsPlaintext_parse_line;
  oclass->parse_begin = MimeInlineTextHTMLAsPlaintext_parse_begin;
  oclass->parse_eof   = MimeInlineTextHTMLAsPlaintext_parse_eof;
  oclass->finalize    = MimeInlineTextHTMLAsPlaintext_finalize;

  return 0;
}

static int
MimeInlineTextHTMLAsPlaintext_parse_begin (MimeObject *obj)
{
  MimeInlineTextHTMLAsPlaintext *textHTMLPlain =
                                       (MimeInlineTextHTMLAsPlaintext *) obj;
  textHTMLPlain->complete_buffer = new nsString();
     // Let's just hope that libmime won't have the idea to call begin twice...
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
}

static int
MimeInlineTextHTMLAsPlaintext_parse_eof (MimeObject *obj, PRBool abort_p)
{
  if (obj->closed_p)
    return 0;

  // This is a hack. We need to call parse_eof() of the super class to flush out any buffered data.
  // We can't call it yet for our direct super class, because it would "close" the output 
  // (write tags such as </pre> and </div>). We'll do that after parsing the buffer.
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->superclass->parse_eof(obj, abort_p);
  if (status < 0)
    return status;
  
  MimeInlineTextHTMLAsPlaintext *textHTMLPlain =
                                       (MimeInlineTextHTMLAsPlaintext *) obj;

  if (!textHTMLPlain || !textHTMLPlain->complete_buffer)
  {
    return 0;
  }
  nsString& cb = *(textHTMLPlain->complete_buffer);
  nsString asPlaintext;
  PRUint32 flags = nsIDocumentEncoder::OutputFormatted
    | nsIDocumentEncoder::OutputWrap
    | nsIDocumentEncoder::OutputFormatFlowed
    | nsIDocumentEncoder::OutputLFLineBreak
    | nsIDocumentEncoder::OutputNoScriptContent
    | nsIDocumentEncoder::OutputNoFramesContent
    | nsIDocumentEncoder::OutputBodyOnly;
  HTML2Plaintext(cb, asPlaintext, flags, 80);

  NS_ConvertUTF16toUTF8 resultCStr(asPlaintext);
  // TODO parse each line independently
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_line(
                             resultCStr.BeginWriting(),
                             resultCStr.Length(),
                             obj);

  cb.Truncate();

  if (status < 0)
    return status;

  // Second part of the flush hack. Pretend obj wasn't closed yet, so that our super class 
  // gets a chance to write the closing.
  PRBool save_closed_p = obj->closed_p;
  obj->closed_p = PR_FALSE;
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  // Restore closed_p.
  obj->closed_p = save_closed_p;
  return status;
}

void
MimeInlineTextHTMLAsPlaintext_finalize (MimeObject *obj)
{
  MimeInlineTextHTMLAsPlaintext *textHTMLPlain =
                                        (MimeInlineTextHTMLAsPlaintext *) obj;
  if (textHTMLPlain && textHTMLPlain->complete_buffer)
  {
    // If there's content in the buffer, make sure that we output it.
    // don't care about return codes
    obj->clazz->parse_eof(obj, PR_FALSE);

    delete textHTMLPlain->complete_buffer;
    textHTMLPlain->complete_buffer = NULL;
      /* It is important to zero the pointer, so we can reliably check for
         the validity of it in the other functions. See above. */
  }
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize (obj);
}

static int
MimeInlineTextHTMLAsPlaintext_parse_line (char *line, PRInt32 length,
                                          MimeObject *obj)
{
  MimeInlineTextHTMLAsPlaintext *textHTMLPlain =
                                       (MimeInlineTextHTMLAsPlaintext *) obj;

  if (!textHTMLPlain || !(textHTMLPlain->complete_buffer))
  {
#if DEBUG
printf("Can't output: %s\n", line);
#endif
    return -1;
  }

  /*
    To convert HTML->TXT syncronously, I need the full source at once,
    not line by line (how do you convert "<li>foo\n" to plaintext?).
    parse_decoded_buffer claims to give me that, but in fact also gives
    me single lines.
    It might be theoretically possible to drive this asyncronously, but
    I don't know, which odd circumstances might arise and how libmime
    will behave then. It's not worth the trouble for me to figure this all out.
   */
  nsCString linestr(line, length);
  NS_ConvertUTF8toUCS2 line_ucs2(linestr.get());
  if (length && line_ucs2.IsEmpty())
    line_ucs2.AssignWithConversion(linestr.get());
  (textHTMLPlain->complete_buffer)->Append(line_ucs2);

  return 0;
}
