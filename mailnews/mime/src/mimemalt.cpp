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
 *    Ben Bucksch <mozilla@bucksch.org>
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

#include "mimemalt.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsMimeTypes.h"
#include "nsMimeStringResources.h"
#include "nsIPref.h"
#include "mimemoz2.h" // for prefs
#include "nsCRT.h"

extern "C" MimeObjectClass mimeMultipartRelatedClass;

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeMultipartAlternative, MimeMultipartAlternativeClass,
       mimeMultipartAlternativeClass, &MIME_SUPERCLASS);

static int MimeMultipartAlternative_initialize (MimeObject *);
static void MimeMultipartAlternative_finalize (MimeObject *);
static int MimeMultipartAlternative_parse_eof (MimeObject *, PRBool);
static int MimeMultipartAlternative_create_child(MimeObject *);
static int MimeMultipartAlternative_parse_child_line (MimeObject *, char *,
                            PRInt32, PRBool);
static int MimeMultipartAlternative_close_child(MimeObject *);

static PRBool MimeMultipartAlternative_display_part_p(MimeObject *self,
                             MimeHeaders *sub_hdrs);
static int MimeMultipartAlternative_discard_cached_part(MimeObject *);
static int MimeMultipartAlternative_display_cached_part(MimeObject *);

static int
MimeMultipartAlternativeClassInitialize(MimeMultipartAlternativeClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize       = MimeMultipartAlternative_initialize;
  oclass->finalize         = MimeMultipartAlternative_finalize;
  oclass->parse_eof        = MimeMultipartAlternative_parse_eof;
  mclass->create_child     = MimeMultipartAlternative_create_child;
  mclass->parse_child_line = MimeMultipartAlternative_parse_child_line;
  mclass->close_child      = MimeMultipartAlternative_close_child;
  return 0;
}


static int
MimeMultipartAlternative_initialize (MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  PR_ASSERT(!malt->part_buffer);
  malt->part_buffer = MimePartBufferCreate();
  if (!malt->part_buffer)
  return MIME_OUT_OF_MEMORY;

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(obj);
}

static void
MimeMultipartAlternative_cleanup(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  if (malt->buffered_hdrs)
  {
    MimeHeaders_free(malt->buffered_hdrs);
    malt->buffered_hdrs = 0;
  }
  if (malt->part_buffer)
  {
    MimePartBufferDestroy(malt->part_buffer);
    malt->part_buffer = 0;
  }
}


static void
MimeMultipartAlternative_finalize (MimeObject *obj)
{
  MimeMultipartAlternative_cleanup(obj);
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(obj);
}


static int
MimeMultipartAlternative_parse_eof (MimeObject *obj, PRBool abort_p)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  int status = 0;

  if (obj->closed_p) return 0;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  /* If there's a cached part we haven't written out yet, do it now.
   */
  if (malt->buffered_hdrs && !abort_p)
  {
    status = MimeMultipartAlternative_display_cached_part(obj);
    if (status < 0) return status;
  }

  MimeMultipartAlternative_cleanup(obj);

  return status;
}


static int
MimeMultipartAlternative_create_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  if (MimeMultipartAlternative_display_part_p (obj, mult->hdrs))
  {
    /* If this part is potentially displayable, begin populating the cache
     with it.  If there's something in the cache already, discard it
     first.  (Just because this part is displayable doesn't mean we will
     display it -- of two consecutive displayable parts, it is the second
     one that gets displayed.)
     */
    int status;
    mult->state = MimeMultipartPartFirstLine;

    status = MimeMultipartAlternative_discard_cached_part(obj);
    if (status < 0) return status;

    PR_ASSERT(!malt->buffered_hdrs);
    malt->buffered_hdrs = MimeHeaders_copy(mult->hdrs);
    if (!malt->buffered_hdrs) return MIME_OUT_OF_MEMORY;
    return 0;
  }
  else
  {
    /* If this part is not displayable, then skip it. maybe the next one will be...
     */
    mult->state = MimeMultipartSkipPartLine;
    return 0;
  }
}


static int
MimeMultipartAlternative_parse_child_line (MimeObject *obj,
                       char *line, PRInt32 length,
                       PRBool first_line_p)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  PR_ASSERT(malt->part_buffer);
  if (!malt->part_buffer) return -1;

  /* Push this line into the buffer for later retrieval. */
  return MimePartBufferWrite (malt->part_buffer, line, length);
}


static int
MimeMultipartAlternative_close_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  /* PR_ASSERT(malt->part_buffer);      Some Mac brokenness trips this...
  if (!malt->part_buffer) return -1; */

  if (malt->part_buffer)
  MimePartBufferClose(malt->part_buffer);

  /* PR_ASSERT(mult->hdrs);         I expect the Mac trips this too */
  if (mult->hdrs)
  MimeHeaders_free(mult->hdrs);
  mult->hdrs = 0;

  return 0;
}


static PRBool
MimeMultipartAlternative_display_part_p(MimeObject *self,
                    MimeHeaders *sub_hdrs)
{
  char *ct = MimeHeaders_get (sub_hdrs, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);
  if (!ct)
    return PR_FALSE;

  /* RFC 1521 says:
     Receiving user agents should pick and display the last format
     they are capable of displaying.  In the case where one of the
     alternatives is itself of type "multipart" and contains unrecognized
     sub-parts, the user agent may choose either to show that alternative,
     an earlier alternative, or both.

   Ugh.  If there is a multipart subtype of alternative, we simply show
   that, without descending into it to determine if any of its sub-parts
   are themselves unknown.
   */

  // prefer_plaintext pref
  nsIPref *pref = GetPrefServiceManager(self->options); 
  PRBool prefer_plaintext = PR_FALSE;
  if (pref)
    (void)pref->GetBoolPref("mailnews.display.prefer_plaintext",
                            &prefer_plaintext);
  if (prefer_plaintext
      && self->options->format_out != nsMimeOutput::nsMimeMessageSaveAs
      && (!nsCRT::strncasecmp(ct, "text/html", 9) ||
          !nsCRT::strncasecmp(ct, "text/enriched", 13) ||
          !nsCRT::strncasecmp(ct, "text/richtext", 13))
     )
    // if the user prefers plaintext and this is the "rich" (e.g. HTML) part...
  {
#if DEBUG
    printf ("Ignoring %s alternative\n", ct);
#endif
    return PR_FALSE;
  }
#if DEBUG
  printf ("Considering %s alternative\n", ct);
#endif

  MimeObjectClass *clazz = mime_find_class (ct, sub_hdrs, self->options, PR_TRUE);
  PRBool result = (clazz
          ? clazz->displayable_inline_p(clazz, sub_hdrs)
          : PR_FALSE);
  PR_FREEIF(ct);
  return result;
}

static int 
MimeMultipartAlternative_discard_cached_part(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  if (malt->buffered_hdrs)
  {
    MimeHeaders_free(malt->buffered_hdrs);
    malt->buffered_hdrs = 0;
  }
  if (malt->part_buffer)
  MimePartBufferReset (malt->part_buffer);

  return 0;
}

static int 
MimeMultipartAlternative_display_cached_part(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  int status;

  char *ct = (malt->buffered_hdrs
        ? MimeHeaders_get (malt->buffered_hdrs, HEADER_CONTENT_TYPE,
                 PR_TRUE, PR_FALSE)
        : 0);
  const char *dct = (((MimeMultipartClass *) obj->clazz)->default_part_type);
  MimeObject *body;

  /* Don't pass in NULL as the content-type (this means that the
   auto-uudecode-hack won't ever be done for subparts of a
   multipart, but only for untyped children of message/rfc822.
   */
  body = mime_create(((ct && *ct) ? ct : (dct ? dct: TEXT_PLAIN)),
           malt->buffered_hdrs, obj->options);

  PR_FREEIF(ct);
  if (!body) return MIME_OUT_OF_MEMORY;

  status = ((MimeContainerClass *) obj->clazz)->add_child(obj, body);
  if (status < 0)
  {
    mime_free(body);
    return status;
  }

#ifdef MIME_DRAFTS
  /* if this object is a child of a multipart/related object, the parent is
     taking care of decomposing the whole part, don't need to do it at this level.
     However, we still have to call decompose_file_init_fn and decompose_file_close_fn
     in order to set the correct content-type. But don't call MimePartBufferRead
  */
  PRBool multipartRelatedChild = mime_typep(obj->parent,(MimeObjectClass*)&mimeMultipartRelatedClass);
  PRBool decomposeFile = obj->options &&
                  obj->options->decompose_file_p &&
                  obj->options->decompose_file_init_fn &&
                  !mime_typep(body, (MimeObjectClass *) &mimeMultipartClass);

  if (decomposeFile)
  {
    status = obj->options->decompose_file_init_fn (
                        obj->options->stream_closure,
                        malt->buffered_hdrs);
    if (status < 0) return status;
  }
#endif /* MIME_DRAFTS */


  /* Now that we've added this new object to our list of children,
   start its parser going. */
  status = body->clazz->parse_begin(body);
  if (status < 0) return status;

#ifdef MIME_DRAFTS
  if (decomposeFile && !multipartRelatedChild)
    status = MimePartBufferRead (malt->part_buffer,
                  obj->options->decompose_file_output_fn,
                  obj->options->stream_closure);
  else
#endif /* MIME_DRAFTS */

  status = MimePartBufferRead (malt->part_buffer,
                  /* The (nsresult (*) ...) cast is to turn the
                   `void' argument into `MimeObject'. */
                  ((nsresult (*) (const char *, PRInt32, void *))
                  body->clazz->parse_buffer),
                  body);

  if (status < 0) return status;

  MimeMultipartAlternative_cleanup(obj);

  /* Done parsing. */
  status = body->clazz->parse_eof(body, PR_FALSE);
  if (status < 0) return status;
  status = body->clazz->parse_end(body, PR_FALSE);
  if (status < 0) return status;

#ifdef MIME_DRAFTS
  if (decomposeFile)
  {
    status = obj->options->decompose_file_close_fn ( obj->options->stream_closure );
    if (status < 0) return status;
  }
#endif /* MIME_DRAFTS */

  return 0;
}
