/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* mimemoz.c --- Mozilla interface to libmime.a
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "xp.h"
#include "libi18n.h"
#include "xp_time.h"
#include "msgcom.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimetric.h"   /* for MIME_RichtextConverter */
#include "mimethtm.h"
#include "mimemsig.h"
#include "mimecryp.h"
#ifndef MOZILLA_30
#include "mimemrel.h"
#include "mimemalt.h"
# include "xpgetstr.h"
# include "mimevcrd.h"  /* for MIME_VCardConverter */
#ifdef MOZ_CALENDAR
# include "mimecal.h"   /* for MIME_JulianConverter */
#endif
# include "edt.h"
  extern int XP_FORWARDED_MESSAGE_ATTACHMENT;
#endif /* !MOZILLA_30 */
#include "prefapi.h"

#include "secrng.h"
#include "prprf.h"
#include "intl_csi.h"

#if defined(XP_UNIX) || defined(XP_WIN32)
#if	defined(MOZ_CALENDAR)
    #define JULIAN_EXISTS 1		/* Julian isn't working on mac yet */
#endif
#endif


#ifdef JULIAN_EXISTS
#include "julianform.h"
#endif

#ifdef HAVE_MIME_DATA_SLOT
# define LOCK_LAST_CACHED_MESSAGE
#endif

extern int MK_UNABLE_TO_OPEN_TMP_FILE;

/* Arrgh.  Why isn't this in a reasonable header file somewhere???  ###tw */
extern char * NET_ExplainErrorDetails (int code, ...);


#include "mimedisp.h"


#ifndef MOZILLA_30

static MimeHeadersState MIME_HeaderType;
static XP_Bool MIME_NoInlineAttachments;
static XP_Bool MIME_WrapLongLines;
static XP_Bool MIME_VariableWidthPlaintext;
static XP_Bool MIME_PrefDataValid = 0; /* 0: First time. */
                                /* 1: Cache is not valid. */
                                /* 2: Cache is valid. */

#endif

/* #### defined in libmsg/msgutils.c */
extern NET_StreamClass *
msg_MakeRebufferingStream (NET_StreamClass *next_stream,
                           URL_Struct *url,
                           MWContext *context);


static char *
mime_reformat_date(const char *date, void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  MWContext *context = msd->context;
  const char *s;
  time_t t;
  XP_ASSERT(date);
  if (!date) return 0;
  t = XP_ParseTimeString(date, FALSE);
  if (t <= 0) return 0;
  s = MSG_FormatDateFromContext(context, t);
  if (!s) return 0;
  return XP_STRDUP(s);
}


static char *
mime_file_type (const char *filename, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_type ((char *) filename);
  if (!cinfo || !cinfo->type)
    return 0;
  else
    return XP_STRDUP(cinfo->type);
}

static char *
mime_type_desc(const char *type, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_info_by_type((char *) type);
  if (!cinfo || !cinfo->desc || !*cinfo->desc)
    return 0;
  else
    return XP_STRDUP(cinfo->desc);
}


static char *
mime_type_icon(const char *type, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_info_by_type((char *) type);
  if (cinfo && cinfo->icon && *cinfo->icon)
    return XP_STRDUP(cinfo->icon);
  else if (!strncasecomp(type, "text/", 5))
    return XP_STRDUP("internal-gopher-text");
  else if (!strncasecomp(type, "image/", 6))
    return XP_STRDUP("internal-gopher-image");
  else if (!strncasecomp(type, "audio/", 6))
    return XP_STRDUP("internal-gopher-sound");
  else if (!strncasecomp(type, "video/", 6))
    return XP_STRDUP("internal-gopher-movie");
  else if (!strncasecomp(type, "application/", 12))
    return XP_STRDUP("internal-gopher-binary");
  else
    return XP_STRDUP("internal-gopher-unknown");
}


static int
mime_convert_charset (const char *input_line, int32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, int32 *output_size_ret,
                      void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  unsigned char *converted;

  /* #### */
  extern unsigned char *INTL_ConvMailToWinCharCode(MWContext *context,
                                                   unsigned char *pSrc,
                                                   uint32 block_size);

  converted = INTL_ConvMailToWinCharCode(msd->context,
                                         (unsigned char *) input_line,
                                         input_length);
  if (converted)
    {
      *output_ret = (char *) converted;
      *output_size_ret = XP_STRLEN((char *) converted);
    }
  else
    {
      *output_ret = 0;
      *output_size_ret = 0;
    }
  return 0;
}


static int
mime_convert_rfc1522 (const char *input_line, int32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, int32 *output_size_ret,
                      void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  char *converted;
  char *line;

  if (input_line[input_length] == 0)  /* oh good, it's null-terminated */
    line = (char *) input_line;
  else
    {
      line = (char *) XP_ALLOC(input_length+1);
      if (!line) return MK_OUT_OF_MEMORY;
      XP_MEMCPY(line, input_line, input_length);
      line[input_length] = 0;
    }

  converted = IntlDecodeMimePartIIStr(line,
      INTL_DocToWinCharSetID(INTL_DefaultDocCharSetID(msd->context)), FALSE);

  if (line != input_line)
    XP_FREE(line);

  if (converted)
    {
      *output_ret = converted;
      *output_size_ret = XP_STRLEN(converted);
    }
  else
    {
      *output_ret = 0;
      *output_size_ret = 0;
    }
  return 0;
}


static int
mime_output_fn(char *buf, int32 size, void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  XP_ASSERT(msd->stream);
  if (!msd->stream) return -1;
#ifdef DEBUG_terry
  if (msd->logit) {
      XP_FileWrite(buf, size, msd->logit);
  }
#endif
  return msd->stream->put_block (msd->stream, buf, size);
}

static int
compose_only_output_fn(char *buf, int32 size, void *stream_closure)
{
    return 0;
}

extern XP_Bool msg_LoadingForComposeOnly(const MSG_Pane* pane); /* in msgmpane.cpp */

static int
mime_set_html_state_fn (void *stream_closure,
                        XP_Bool layer_encapsulate_p,
                        XP_Bool start_p,
                        XP_Bool abort_p)
{
  int status = 0;
  char *buf;

  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;

#if 1
  char random_close_tags[] = "</SCRIPT><NSCP_CLOSE>";
#else /* 0 */
  char random_close_tags[] =
        "</TABLE></TABLE></TABLE></TABLE></TABLE></TABLE>"
        "</DL></DL></DL></DL></DL></DL></DL></DL></DL></DL>"
        "</DL></DL></DL></DL></DL></DL></DL></DL></DL></DL>"
        "</B></B></B></B></B></B></B></B></B></B></B></B>"
        "</PRE></PRE></PRE></PRE></PRE></PRE></PRE></PRE>"
        "<BASEFONT SIZE=3></SCRIPT>";
#endif /* 0 */
  if (start_p) {
#ifndef MOZILLA_30
    if (layer_encapsulate_p && msd->options && !msd->options->nice_html_only_p){
      uint8 *rand_buf = msd->rand_buf;
      RNG_GenerateGlobalRandomBytes(rand_buf, sizeof msd->rand_buf);

      buf = PR_smprintf("<ILAYER LOCKED CLIP=0,0,AUTO,AUTO "
                        "MATCH=%02x%02x%02x%02x%02x%02x>",
                        rand_buf[0], rand_buf[1], rand_buf[2],
                        rand_buf[3], rand_buf[4], rand_buf[5]);
      if (!buf)
        return MK_OUT_OF_MEMORY;
      status = MimeOptions_write(msd->options, buf, XP_STRLEN(buf), TRUE);
      XP_FREE(buf);
    }
#endif /* MOZILLA_30 */
  } else {
    status = MimeOptions_write(msd->options, random_close_tags,
                               XP_STRLEN(random_close_tags), FALSE);
    if (status < 0)
      return status;

#ifndef MOZILLA_30
    if (layer_encapsulate_p && msd->options && !msd->options->nice_html_only_p){
      uint8 *rand_buf = msd->rand_buf;
      buf = PR_smprintf("</ILAYER MATCH=%02x%02x%02x%02x%02x%02x><BR>",
                        rand_buf[0], rand_buf[1], rand_buf[2],
                        rand_buf[3], rand_buf[4], rand_buf[5]);
      if (!buf)
        return MK_OUT_OF_MEMORY;
      status = MimeOptions_write(msd->options, buf, XP_STRLEN(buf), TRUE);
      XP_FREE(buf);
    }
#endif /* MOZILLA_30 */

  }
  return status;
}


static int
mime_display_stream_write (NET_StreamClass *stream,
                           const char* buf,
                           int32 size)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream->data_object;
  MimeObject *obj = (msd ? msd->obj : 0);
  if (!obj) return -1;
  return obj->class->parse_buffer((char *) buf, size, obj);
}


static unsigned int
mime_display_stream_write_ready (NET_StreamClass *stream)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream->data_object;
  if (msd->istream) {
      return msd->istream->is_write_ready (msd->istream);
  } else if (msd->stream)
    return msd->stream->is_write_ready (msd->stream);
  else
    return (MAX_WRITE_READY);
}


extern void MSG_MimeNotifyCryptoAttachmentKludge(NET_StreamClass *);

static void
mime_display_stream_complete (NET_StreamClass *stream)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream->data_object;
  MimeObject *obj = (msd ? msd->obj : 0);
  if (obj)
    {
      int status;
      status = obj->class->parse_eof(obj, FALSE);
      obj->class->parse_end(obj, (status < 0 ? TRUE : FALSE));

#ifdef HAVE_MIME_DATA_SLOT
      if (msd &&
          msd->context &&
          msd->context->mime_data &&
          obj == msd->context->mime_data->last_parsed_object)
        {
          /* do nothing -- we still have another pointer to this object. */
        }
      else
#endif /* HAVE_MIME_DATA_SLOT */
        {
          /* Somehow there's no pointer in context->mime_data to this
             object, so destroy it now.  (This can happen for any number
             of normal reasons; see comment in mime_output_init_fn().)
           */
          XP_ASSERT(msd->options == obj->options);
          mime_free(obj);
          obj = NULL;
          if (msd->options)
            {
              FREEIF(msd->options->part_to_load);
			  FREEIF(msd->options->default_charset);
			  FREEIF(msd->options->override_charset);
              XP_FREE(msd->options);
              msd->options = 0;
            }
        }
    }

#ifdef LOCK_LAST_CACHED_MESSAGE
  /* The code in this ifdef is to ensure that the most-recently-loaded news
     message is locked down in the memory cache.  (There may be more than one
     message cached, but only the most-recently-loaded is *locked*.)

     When loading a message, we unlock the previously-locked URL, if any.
     (This happens in mime_make_output_stream().)

     When we're done loading a news message, we lock it to prevent it from
     going away (this happens here, in mime_display_stream_complete().  We
     need to do this at the end instead of the beginning so that the document
     is *in* the cache at the time we try to lock it.)

     This implementation probably assumes that news messages go into the
     memory cache and never go into the disk cache.  (But maybe that's
     actually not an issue, since if news messages were to go into the
     disk cache, they would be marked as non-session-persistent anyway?)
   */
  if (msd &&
      msd->context &&
      msd->context->mime_data &&
      !msd->context->mime_data->previous_locked_url)
    {
      /* Save a copy of this URL so that we can unlock it next time. */
      msd->context->mime_data->previous_locked_url =
        XP_STRDUP(msd->url->address);

      /* Lock this URL in the cache. */
      if (msd->context->mime_data->previous_locked_url)
        NET_ChangeCacheFileLock(msd->url, TRUE);
    }
#endif /* LOCK_LAST_CACHED_MESSAGE */

  if (msd->stream)
    {

      /* Hold your breath.  This is how we notify compose.c
         that the message was decrypted -- it will set a flag in the
         mime_delivery_state structure, which we don't have access to from
         here. */
      if (obj &&
          obj->options &&
          obj->options->state &&
          obj->options->state->decrypted_p)
        MSG_MimeNotifyCryptoAttachmentKludge(msd->stream);

      /* Close the output stream. */
      msd->stream->complete (msd->stream);
      XP_FREE (msd->stream);
    }
#ifdef DEBUG_terry
  if (msd->logit) XP_FileClose(msd->logit);
#endif
  XP_FREE(msd);
}

static void
mime_display_stream_abort (NET_StreamClass *stream, int status)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream->data_object;
  MimeObject *obj = (msd ? msd->obj : 0);
  if (obj)
    {
      if (!obj->closed_p)
        obj->class->parse_eof(obj, TRUE);
      if (!obj->parsed_p)
        obj->class->parse_end(obj, TRUE);

#ifdef HAVE_MIME_DATA_SLOT
      if (msd &&
          msd->context &&
          msd->context->mime_data &&
          obj == msd->context->mime_data->last_parsed_object)
        {
          /* do nothing -- we still have another pointer to this object. */
        }
      else
#endif /* HAVE_MIME_DATA_SLOT */
        {
          /* Somehow there's no pointer in context->mime_data to this
             object, so destroy it now.  (This can happen for any number
             of normal reasons; see comment in mime_output_init_fn().)
           */
          XP_ASSERT(msd->options == obj->options);
          mime_free(obj);
          if (msd->options)
            {
              FREEIF(msd->options->part_to_load);
              XP_FREE(msd->options);
              msd->options = 0;
            }
        }
    }

  XP_ASSERT(msd); /* Crash was happening here - jrm */
  if (msd)
  {
      if (msd->stream)
      {
          msd->stream->abort (msd->stream, status);
          XP_FREE (msd->stream);
      }
      XP_FREE(msd);
  }
}



#ifndef MOZILLA_30

static unsigned int
mime_insert_html_write_ready(NET_StreamClass *stream)
{
  return MAX_WRITE_READY;
}

static int
mime_insert_html_put_block(NET_StreamClass *stream, const char* str, int32 length)
{
  struct mime_stream_data* msd = (struct mime_stream_data*) stream->data_object;
  char* s = (char*) str;
  char c = s[length];
  XP_ASSERT(msd);
  if (!msd) return -1;
  if (c) {
    s[length] = '\0';
  }
  /* s is in the outcsid encoding at this point. That was done in
   * mime_insert_html_convert_charset */
  EDT_PasteQuoteINTL(msd->context, s, msd->outcsid);
  if (c) {
    s[length] = c;
  }
  return 0;
}


static void
mime_insert_html_complete(NET_StreamClass *stream)
{
  struct mime_stream_data* msd = (struct mime_stream_data*) stream->data_object;
  XP_ASSERT(msd);
  if (!msd) return;
  EDT_PasteQuote(msd->context, "</BLOCKQUOTE>");
  if (msd->format_out == FO_QUOTE_HTML_MESSAGE) {
      int32 eReplyOnTop = 1, nReplyWithExtraLines = 0;
      PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop);
      PREF_GetIntPref("mailnews.reply_with_extra_lines", &nReplyWithExtraLines);
      if (0 == eReplyOnTop && nReplyWithExtraLines) {
        for (; nReplyWithExtraLines > 0; nReplyWithExtraLines--)
          EDT_PasteQuote(msd->context, "<BR>");
      }

  }
  EDT_PasteQuoteEnd(msd->context);
}

static void
mime_insert_html_abort(NET_StreamClass *stream, int status)
{
  mime_insert_html_complete(stream);
}


static int
mime_insert_html_convert_charset (const char *input_line, int32 input_length,
                                  const char *input_charset,
                                  const char *output_charset,
                                  char **output_ret, int32 *output_size_ret,
                                  void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  int status;
  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(msd->context);
  uint16 old_csid = INTL_GetCSIDocCSID(csi);

  if (input_charset) {
    msd->lastcsid = INTL_CharSetNameToID((char*) input_charset);
  } else {
    msd->lastcsid = 0;
  }
  if (output_charset) {
    msd->outcsid = INTL_CharSetNameToID((char*) output_charset);
  } else {
    msd->outcsid = 0;
  }
  INTL_SetCSIDocCSID(csi, msd->lastcsid);
  status = mime_convert_charset (input_line, input_length,
                                 input_charset, output_charset,
                                 output_ret, output_size_ret,
                                 stream_closure);
  INTL_SetCSIDocCSID(csi, old_csid);
  return status;
}

#endif /* !MOZILLA_30 */


static NET_StreamClass *
mime_make_output_stream(const char *content_type,
                        const char *charset,
                        const char *content_name,
                        const char *x_mac_type,
                        const char *x_mac_creator,
                        int format_out, URL_Struct *url,
                        MWContext *context,
                        struct mime_stream_data* msd)
{
  /* To make the next stream, fill in the URL with the provided attributes,
     and call NET_StreamBuilder.

     But After we've gotten the stream we want, change the URL's type and
     encoding back to what they were before, since things down the line might
     need to know the *original* type.
   */
  NET_StreamClass *stream;
  char *orig_content_type;
  char *orig_encoding;
  char *old_part = 0;
  char *old_part2 = 0;

#ifndef MOZILLA_30
  if (format_out == FO_QUOTE_HTML_MESSAGE) {
    /* Special case here.  Make a stream that just jams data directly
       into our editor context.  No calling of NET_StreamBuilder for me;
       I don't really understand it anyway... */

    XP_ASSERT(msd);
    if (msd) {
      stream = XP_NEW_ZAP(NET_StreamClass);
      if (!stream) return NULL;
      stream->window_id = context;
      stream->data_object = msd;
      stream->is_write_ready = mime_insert_html_write_ready;
      stream->put_block = mime_insert_html_put_block;
      stream->complete = mime_insert_html_complete;
      stream->abort = mime_insert_html_abort;
      return stream;
    }
  }
#endif /* !MOZILLA_30 */


  XP_ASSERT(content_type && url);
  if (!content_type || !url)
    return 0;

  /* If we're saving a message to disk (raw), then treat the output as unknown
     type.  If we didn't do this, then saving a message would recurse back into
     the parser (because the output has content-type message/rfc822), and we'd
     be back here again in short order...

     We don't do this for FO_SAVE_AS_TEXT and FO_SAVE_AS_POSTSCRIPT because
     those work by generating HTML, and then converting that.  Whereas
     FO_SAVE_AS goes directly to disk without another format_out filter
     catching it.

     We only fake out the content-type when we're saving a message (mail or
     news) and not when saving random other types.  The reason for this is that
     we only *need* to do this for messages (because those are the only types
     which are registered to come back in here for FO_SAVE_AS); we don't do it
     for other types, because the Mac needs to know the real type to map a
     MIME type to a Mac Creator when writing the file to disk (so that the
     right application will get launched when the file is clicked on, etc.)

     [mwelch: I'm not adding FO_EDT_SAVE_IMAGE here, because the editor, to
     the best of my knowledge, never spools out a message per se; such a message
     would be filed as an attachment (FO_CACHE_AND_MAIL_TO), independent of the
     editor. In addition, we want to drill down as far as we can within a quoted
     message, in order to identify whatever part has been requested (usually an
     image, sound, applet, or other inline data).]

     In 3.0b7 and earlier, we did this for *all* types.  On 9-Aug-96 jwz and
     aleks changed this to only do it for message types, which seems to be the
     right thing.  However, since it's very late in the release cycle, this
     change is being done as #ifdef XP_MAC, since the Mac is the only platform
     where it was a problem that we were using octet-stream for all
     attachments.  After 3.0 ships, this should be done on all platforms, not
     just Mac.
   */
  if ((format_out == FO_SAVE_AS || format_out == FO_CACHE_AND_SAVE_AS)
      && (!strcasecomp(content_type, MESSAGE_RFC822) ||
          !strcasecomp(content_type, MESSAGE_NEWS)))
    content_type = APPLICATION_OCTET_STREAM;

  orig_content_type = url->content_type;
  orig_encoding     = url->content_encoding;

  url->content_type = XP_STRDUP(content_type);
  if (!url->content_type) return 0;
  url->content_encoding = 0;

  if (charset)       FREEIF(url->charset);
  if (content_name)  FREEIF(url->content_name);
  if (x_mac_type)    FREEIF(url->x_mac_type);
  if (x_mac_creator) FREEIF(url->x_mac_creator);
  if (charset)       url->charset       = XP_STRDUP(charset);
  if (content_name)  url->content_name  = XP_STRDUP(content_name);
  if (x_mac_type)    url->x_mac_type    = XP_STRDUP(x_mac_type);
  if (x_mac_creator) url->x_mac_creator = XP_STRDUP(x_mac_creator);

  /* If we're going back down into the message/rfc822 parser (that is, if
     we're displaying a sub-part which is itself a message part) then remove
     any part specifier from the URL.  This is to prevent that same part
     from being retreived a second time, which to the user, would have no
     effect.

     Don't do this for all other types, because that might affect image
     cacheing and so on (causing it to cache on the wrong key.)
   */
  if (!strcasecomp(content_type, MESSAGE_RFC822) ||
      !strcasecomp(content_type, MESSAGE_NEWS))
    {
      old_part = XP_STRSTR(url->address, "?part=");
      if (!old_part)
        old_part2 = XP_STRSTR(url->address, "&part=");

      if (old_part) *old_part = 0;
      else if (old_part2) *old_part2 = 0;
    }

  if(msd && msd->options && msd->options->default_charset)
  {
	  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(context);
	  INTL_SetCSIMimeCharset(csi, msd->options->default_charset);
  }

  stream = NET_StreamBuilder (format_out, url, context);
  if (stream && (context->type != MWContextMessageComposition))
  {
    /* Bug #110565: do not change the stream to a rebuffering stream
       when we have a Mail Compose context */
    NET_StreamClass * buffer = msg_MakeRebufferingStream(stream, url, context);
    if (buffer)
      stream = buffer;
  }

  /* Put it back -- note that this string is now also pointed to by
     obj->options->url, so we're modifying data reachable by the
     internals of the library (and that is the goal of this hack.) */
  if (old_part) *old_part = '?';
  else if (old_part2) *old_part2 = '&';

  FREEIF(url->content_type);
  url->content_type     = orig_content_type;
  url->content_encoding = orig_encoding;

#ifdef LOCK_LAST_CACHED_MESSAGE
  /* Always cache this one. */
  url->must_cache = TRUE;

  /* Un-cache the last one. */
  if (context->mime_data &&
      context->mime_data->previous_locked_url)
    {
      URL_Struct *url_s =
        NET_CreateURLStruct(context->mime_data->previous_locked_url,
                            NET_NORMAL_RELOAD);
      if (url_s)
        {
          /* Note: if post data was involved here, we'd lose.  We're assuming
             that all we need to save is the url->address. */
          NET_ChangeCacheFileLock(url_s, FALSE);
          NET_FreeURLStruct(url_s);
        }
      XP_FREE(context->mime_data->previous_locked_url);
      context->mime_data->previous_locked_url = 0;
    }
#endif /* LOCK_LAST_CACHED_MESSAGE */

  return stream;
}



static int
mime_output_init_fn (const char *type,
                     const char *charset,
                     const char *name,
                     const char *x_mac_type,
                     const char *x_mac_creator,
                     void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  int format_out;
  XP_ASSERT(!msd->stream);
  if (msd->stream) return -1;

  XP_ASSERT(type && *type);
  if (!type || !*type) return -1;

  format_out = msd->format_out;

  /* If we've converted to HTML, then we've already done charset conversion,
     so label this data as "internal/parser" to prevent it from being passed
     through the charset converters again. */
  if (msd->options->write_html_p &&
      !strcasecomp(type, TEXT_HTML))
    type = INTERNAL_PARSER;


  /* If this stream converter was created for FO_MAIL_TO, then the input type
     and output type will be identical (message/rfc822) so we need to change
     the format_out to break out of the loop.  libmsg/compose.c knows to treat
     FO_MAIL_MESSAGE_TO roughly the same as FO_MAIL_TO.
   */
#ifdef FO_MAIL_MESSAGE_TO
  if (format_out == FO_MAIL_TO)
    format_out = FO_MAIL_MESSAGE_TO;
  else if (format_out == FO_CACHE_AND_MAIL_TO)
    format_out = FO_CACHE_AND_MAIL_MESSAGE_TO;
#endif /* FO_MAIL_MESSAGE_TO */


  msd->stream = mime_make_output_stream(type, charset, name,
                                        x_mac_type, x_mac_creator,
                                        format_out, msd->url,
                                        msd->context, msd);

  if (!msd->stream)
    /* #### We can't return MK_OUT_OF_MEMORY here because sometimes
       NET_StreamBuilder() returns 0 because it ran out of memory; and
       sometimes it returns 0 because the user hit Cancel on the file
       save dialog box.  Wonderful condition handling we've got... */
    return -1;


#ifdef HAVE_MIME_DATA_SLOT
  /* At this point, the window has been cleared; so discard the cached
     data relating to the previously-parsed MIME object. */

  XP_ASSERT(msd && msd->obj && msd->context);
  if (msd && msd->obj && msd->context)
    {
      MWContext *context = msd->context;

      if (msd->context->mime_data &&
          msd->context->mime_data->last_parsed_object)
        {
          XP_ASSERT(msd->options !=
                    context->mime_data->last_parsed_object->options);
          XP_ASSERT(msd->obj != context->mime_data->last_parsed_object);

          /* There are two interesting cases:

             Look at message-1, then look at message-2; and
             Look at message-1, then look at message-1-part-A
              (where part-A is itself a message.)

            In the first case, by the time we've begun message-2, we're done
            with message-1.  Its object is still around for reference (for
            examining the part names) but the parser has run to completion
            on it.

            In the second case, we begin parsing part-A before the parser of
            message-1 has run to completion -- in fact, the parser that is
            operating on message-1 is still on the stack above us.

            So if there is a last_parsed_object, but that object does not have
            its `parsed_p' slot set, then that means that that object has not
            yet hit the `parse_eof' state; therefore, it is still *being*
            parsed: it is a parent object of the one we're currently looking
            at.  (When presenting a part of type message/rfc822, the
            MIME_MessageConverter stream is entered recursively, the first
            time to extract the sub-message, and the second time to convert
            that sub-message to HTML.)

            So if we're in this nested call, we can simply replace the pointer
            in mime_data.  When current MessageConverter stream reaches its
            `complete' or `abort' methods, it will skip freeing the current
            object (since there is a pointer to it in mime_data); and then when
            the outer MessageConverter stream reaches its `complete' or `abort'
            methods, it will free that outer object (since it will see that
            there is no pointer to it in mime_data.)

            More precisely:

            Look at message-1, then look at message-2:
              In this case, the flow of control is like this:

              = make object-1 (message/rfc822)
              = parse it
              = save it in context->mime_data
              = done with object-1; free nothing.

              = make object-2 (message/rfc822)
              = parse it
              = note that object-1 is still in context->mime_data
              = free object-1
              = save object-2 in context->mime_data
              = done with object-2; free nothing.

            Look at message-1, then look at message-1-part-A:
              The flow of control in this case is somewhat different:

              = make object-1 (message/rfc822)
              = parse it
              = save it in context->mime_data
              = done with object-1; free nothing.

              = make object-1 (message/rfc822)
              = parse it
              = note that previous-object-1 is still in context->mime_data
              = free previous-object-1
              = save object-1 in context->mime_data
              = enter the parser recursively:
                 = make part-A (message/rfc822)
                 = parse it
                 = note that object1 is still in context->mime_data,
                   and has not yet been fully parsed (in other words,
                   it's still on the stack.)  Don't touch it.
                 = save part-A in context->mime_data
                   (cutting the pointer to object-1)
                 = done with part-A; free nothing.
              = done with object-1;
                note that object-1 is *not* in the context->mime_data
              = free object-1
              = result: part-A remains in context->mime_data
            */

          if (context->mime_data->last_parsed_object->parsed_p)
            {
              /* Free it if it's parsed.

                 Note that we have to call mime_free() before we free the
                 options struct, not after -- so since mime_free() frees
                 the object, we have to pull `options' out first to avoid
                 reaching into freed memory.
               */
              MimeDisplayOptions *options =
                context->mime_data->last_parsed_object->options;

              mime_free(context->mime_data->last_parsed_object);
              if (options)
                {
                  FREEIF(options->part_to_load);
                  XP_FREE(options);
                }
            }

          /* Cut the old saved pointer, whether its parsed or not. */
          context->mime_data->last_parsed_object = 0;
          FREEIF(context->mime_data->last_parsed_url);
        }

      /* And now save away the current object, for consultation the first
         time a link is clicked upon. */
      if (!context->mime_data)
        {
          context->mime_data = XP_NEW(struct MimeDisplayData);
          if (!context->mime_data)
            return MK_OUT_OF_MEMORY;
          XP_MEMSET(context->mime_data, 0, sizeof(*context->mime_data));
        }
      context->mime_data->last_parsed_object = msd->obj;
      context->mime_data->last_parsed_url = XP_STRDUP(msd->url->address);
#ifndef MOZILLA_30
      context->mime_data->last_pane = msd->url->msg_pane;
#endif /* MOZILLA_30 */

      XP_ASSERT(!msd->options ||
                msd->options == msd->obj->options);
    }
#endif /* HAVE_MIME_DATA_SLOT */

  return 0;
}


static void *mime_image_begin(const char *image_url, const char *content_type,
                              void *stream_closure);
static void mime_image_end(void *image_closure, int status);
static char *mime_image_make_image_html(void *image_data);
static int mime_image_write_buffer(char *buf, int32 size, void *image_closure);

#ifdef MOZILLA_30
extern XP_Bool MSG_VariableWidthPlaintext;
#endif


#ifndef MOZILLA_30
int PR_CALLBACK
mime_PrefsChangeCallback(const char* prefname, void* data)
{
  MIME_PrefDataValid = 1;       /* Invalidates our cached stuff. */
  return PREF_NOERROR;
}
#endif /* !MOZILLA_30 */


NET_StreamClass *
MIME_MessageConverter (int format_out, void *closure,
                       URL_Struct *url, MWContext *context)
{
  int status = 0;
  MimeObject *obj;
  struct mime_stream_data *msd;
  NET_StreamClass *stream = 0;

#ifdef FO_MAIL_MESSAGE_TO
  if (format_out == FO_MAIL_MESSAGE_TO ||
      format_out == FO_CACHE_AND_MAIL_MESSAGE_TO)
    {
      /* Bad news -- this would cause an endless loop. */
      XP_ASSERT(0);
      return 0;
    }
#else  /* !FO_MAIL_MESSAGE_TO */
  /* Otherwise, we oughtn't be getting in here at all. */
  XP_ASSERT(format_out != FO_MAIL_TO && format_out != FO_CACHE_AND_MAIL_TO);
#endif /* !FO_MAIL_MESSAGE_TO */

  msd = XP_NEW(struct mime_stream_data);
  if (!msd) return 0;
  XP_MEMSET(msd, 0, sizeof(*msd));
#ifdef DEBUG_terry
#if defined(XP_WIN) || defined(XP_OS2)
  msd->logit = XP_FileOpen("C:\\temp\\twtemp.html", xpTemporary, XP_FILE_WRITE);
#endif
#if defined(XP_UNIX)
  msd->logit = XP_FileOpen("/tmp/twtemp.html", xpTemporary, XP_FILE_WRITE);
#endif
#endif /* DEBUG_terry */
  msd->url = url;
  msd->context = context;
  msd->format_out = format_out;

  msd->options = XP_NEW(MimeDisplayOptions);
  if (!msd->options)
    {
      XP_FREE(msd);
      return 0;
    }
  XP_MEMSET(msd->options, 0, sizeof(*msd->options));
#ifndef MOZILLA_30
  msd->options->pane = url->msg_pane;
#endif /* !MOZILLA_30 */

  if ((format_out == FO_PRESENT || format_out == FO_CACHE_AND_PRESENT) &&
      url->fe_data)
    {
      /* If we're going to the screen, and the URL has fe_data, then it is
         an options structure (that is how the news code hands us its callback
         functions.)  We copy it and free the passed-in data right away.
         (If we're not going to the screen, the fe_data might be some random
         object intended for someone further down the line; for example, it
         could be the XP_File that FO_SAVE_TO_DISK needs to pass around.)
       */
      MimeDisplayOptions *opt2 = (MimeDisplayOptions *) url->fe_data;
      *msd->options = *opt2;  /* copies */
      XP_FREE (opt2);
      url->fe_data = 0;
      msd->options->attachment_icon_layer_id = 0; /* Sigh... */
  }

  /* Set the defaults, based on the context, and the output-type.
   */
  if (format_out == FO_PRESENT ||
      format_out == FO_CACHE_AND_PRESENT ||
      format_out == FO_PRINT ||
      format_out == FO_CACHE_AND_PRINT ||
      format_out == FO_SAVE_AS_POSTSCRIPT ||
      format_out == FO_CACHE_AND_SAVE_AS_POSTSCRIPT)
    msd->options->fancy_headers_p = TRUE;

#ifndef MOZILLA_30
  if (format_out == FO_PRESENT ||
      format_out == FO_CACHE_AND_PRESENT)
    msd->options->output_vcard_buttons_p = TRUE;
#endif /* !MOZILLA_30 */

  if (format_out == FO_PRESENT ||
      format_out == FO_CACHE_AND_PRESENT) {
    msd->options->fancy_links_p = TRUE;
  }

  msd->options->headers = MimeHeadersSome;

#ifndef MOZILLA_30
  if (MIME_PrefDataValid < 2) {
    int32 headertype;
    if (MIME_PrefDataValid == 0) {
      PREF_RegisterCallback("mail.", &mime_PrefsChangeCallback, NULL);
    }
    headertype = 1;
    PREF_GetIntPref("mail.show_headers", &headertype);
    switch (headertype) {
      case 0: MIME_HeaderType = MimeHeadersMicro; break;
      case 1: MIME_HeaderType = MimeHeadersSome; break;
      case 2: MIME_HeaderType = MimeHeadersAll; break;
      default:
        XP_ASSERT(FALSE);
        break;
    }
    MIME_NoInlineAttachments = TRUE;
    PREF_GetBoolPref("mail.inline_attachments", &MIME_NoInlineAttachments);
    MIME_NoInlineAttachments = !MIME_NoInlineAttachments;
                                /* This pref is written down in with the
                                   opposite sense of what we like to use... */
    MIME_WrapLongLines = FALSE;
    PREF_GetBoolPref("mail.wrap_long_lines", &MIME_WrapLongLines);
    MIME_VariableWidthPlaintext = TRUE;
    PREF_GetBoolPref("mail.fixed_width_messages",
                     &MIME_VariableWidthPlaintext);
    MIME_VariableWidthPlaintext = !MIME_VariableWidthPlaintext;
                                /* This pref is written down in with the
                                   opposite sense of what we like to use... */
    MIME_PrefDataValid = 2;
  }
  msd->options->no_inline_p = MIME_NoInlineAttachments;
  msd->options->wrap_long_lines_p = MIME_WrapLongLines;
  msd->options->headers = MIME_HeaderType;
#endif /* !MOZILLA_30 */

  if (context->type == MWContextMail ||
      context->type == MWContextNews
#ifndef MOZILLA_30
      || context->type == MWContextMailMsg
      || context->type == MWContextNewsMsg
#endif /* !MOZILLA_30 */
      )
    {
#ifndef MOZILLA_30
      MSG_Pane* pane = MSG_FindPane(context, MSG_MESSAGEPANE);
      msd->options->rot13_p = FALSE;
      if (pane) {
        msd->options->rot13_p = MSG_ShouldRot13Message(pane);
      }
#else  /* MOZILLA_30 */

      XP_Bool all_headers_p = FALSE;
      XP_Bool micro_headers_p = FALSE;
      MSG_GetContextPrefs(context,
                          &all_headers_p,
                          &micro_headers_p,
                          &msd->options->no_inline_p,
                          &msd->options->rot13_p,
                          &msd->options->wrap_long_lines_p);
      if (all_headers_p)
        msd->options->headers = MimeHeadersAll;
      else if (micro_headers_p)
        msd->options->headers = MimeHeadersMicro;
      else
        msd->options->headers = MimeHeadersSome;

#endif /* MOZILLA_30 */
    }

  status = mime_parse_url_options(url->address, msd->options);
  if (status < 0)
    {
      FREEIF(msd->options->part_to_load);
      XP_FREE(msd->options);
      XP_FREE(msd);
      return 0;
    }

  if (msd->options->headers == MimeHeadersMicro &&
#ifndef MOZILLA_30
      (url->address == NULL || (XP_STRNCMP(url->address, "news:", 5) != 0 &&
                                XP_STRNCMP(url->address, "snews:", 6) != 0))
#else  /* MOZILLA_30 */
      context->type == MWContextMail
#endif /* MOZILLA_30 */
        )
    msd->options->headers = MimeHeadersMicroPlus;

  if (format_out == FO_QUOTE_MESSAGE ||
           format_out == FO_CACHE_AND_QUOTE_MESSAGE
#ifndef MOZILLA_30
           || format_out == FO_QUOTE_HTML_MESSAGE
#endif /* !MOZILLA_30 */
           )
    {
      msd->options->headers = MimeHeadersCitation;
      msd->options->fancy_headers_p = FALSE;
#ifndef MOZILLA_30
      if (format_out == FO_QUOTE_HTML_MESSAGE) {
        msd->options->nice_html_only_p = TRUE;
      }
#endif /* !MOZILLA_30 */
    }

  else if (msd->options->headers == MimeHeadersSome &&
           (format_out == FO_PRINT ||
            format_out == FO_CACHE_AND_PRINT ||
            format_out == FO_SAVE_AS_POSTSCRIPT ||
            format_out == FO_CACHE_AND_SAVE_AS_POSTSCRIPT ||
            format_out == FO_SAVE_AS_TEXT ||
            format_out == FO_CACHE_AND_SAVE_AS_TEXT))
    msd->options->headers = MimeHeadersSomeNoRef;

#ifdef FO_MAIL_MESSAGE_TO
  /* If we're attaching a message (for forwarding) then we must eradicate all
     traces of encryption from it, since forwarding someone else a message
     that wasn't encrypted for them doesn't work.  We have to decrypt it
     before sending it.
   */
  if ((format_out == FO_MAIL_TO || format_out == FO_CACHE_AND_MAIL_TO) &&
      msd->options->write_html_p == FALSE)
    msd->options->decrypt_p = TRUE;
#endif /* FO_MAIL_MESSAGE_TO */

  msd->options->url                   = url->address;
  msd->options->write_html_p          = TRUE;
  msd->options->output_init_fn        = mime_output_init_fn;

#if !defined(MOZILLA_30) && defined(XP_MAC)
  /* If it's a thread context, don't output all the mime stuff (hangs on Macintosh for
  ** unexpanded threadpane, because HTML is generated that needs images and layers).
  */
  if (context->type == MWContextMail)
    msd->options->output_fn           = compose_only_output_fn;
  else
#endif /* XP_MAC */
    msd->options->output_fn           = mime_output_fn;

  msd->options->set_html_state_fn     = mime_set_html_state_fn;
#ifndef MOZILLA_30
  if (format_out == FO_QUOTE_HTML_MESSAGE) {
    msd->options->charset_conversion_fn = mime_insert_html_convert_charset;
    msd->options->dont_touch_citations_p = TRUE;
  } else
#endif
    msd->options->charset_conversion_fn = mime_convert_charset;
  msd->options->rfc1522_conversion_fn = mime_convert_rfc1522;
  msd->options->reformat_date_fn      = mime_reformat_date;
  msd->options->file_type_fn          = mime_file_type;
  msd->options->type_description_fn   = mime_type_desc;
  msd->options->type_icon_name_fn     = mime_type_icon;
  msd->options->stream_closure        = msd;
  msd->options->passwd_prompt_fn      = 0;
  msd->options->passwd_prompt_fn_arg  = context;

  msd->options->image_begin           = mime_image_begin;
  msd->options->image_end             = mime_image_end;
  msd->options->make_image_html       = mime_image_make_image_html;
  msd->options->image_write_buffer    = mime_image_write_buffer;

#ifndef MOZILLA_30
  msd->options->variable_width_plaintext_p = MIME_VariableWidthPlaintext;
#else  /* MOZILLA_30 */
  msd->options->variable_width_plaintext_p = MSG_VariableWidthPlaintext;
#endif /* MOZILLA_30 */

    /* ### mwelch We want FO_EDT_SAVE_IMAGE to behave like *_SAVE_AS here
                  because we're spooling untranslated raw data. */
  if (format_out == FO_SAVE_AS ||
      format_out == FO_CACHE_AND_SAVE_AS ||
      format_out == FO_MAIL_TO ||
      format_out == FO_CACHE_AND_MAIL_TO ||
#ifdef FO_EDT_SAVE_IMAGE
      format_out == FO_EDT_SAVE_IMAGE ||
#endif
      msd->options->part_to_load)
    msd->options->write_html_p = FALSE;

  XP_ASSERT(!msd->stream);

  obj = mime_new ((MimeObjectClass *)&mimeMessageClass,
                  (MimeHeaders *) NULL,
                  MESSAGE_RFC822);
  if (!obj)
    {
      FREEIF(msd->options->part_to_load);
      XP_FREE(msd->options);
      XP_FREE(msd);
      return 0;
    }
  obj->options = msd->options;
  msd->obj = obj;

  /* Both of these better not be true at the same time. */
  XP_ASSERT(! (obj->options->decrypt_p && obj->options->write_html_p));

  stream = XP_NEW (NET_StreamClass);
  if (!stream)
    {
      FREEIF(msd->options->part_to_load);
      XP_FREE(msd->options);
      XP_FREE(msd);
      XP_FREE(obj);
      return 0;
    }
  XP_MEMSET (stream, 0, sizeof (*stream));

  stream->name           = "MIME Conversion Stream";
  stream->complete       = mime_display_stream_complete;
  stream->abort          = mime_display_stream_abort;
  stream->put_block      = mime_display_stream_write;
  stream->is_write_ready = mime_display_stream_write_ready;
  stream->data_object    = msd;
  stream->window_id      = context;

  status = obj->class->initialize(obj);
  if (status >= 0)
    status = obj->class->parse_begin(obj);
  if (status < 0)
    {
      XP_FREE(stream);
      FREEIF(msd->options->part_to_load);
      XP_FREE(msd->options);
      XP_FREE(msd);
      XP_FREE(obj);
      return 0;
    }

  TRACEMSG(("Returning stream from MIME_MessageConverter"));

  return stream;
}


/* Interface between libmime and inline display of images: the abomination
   that is known as "internal-external-reconnect".
 */

struct mime_image_stream_data {
  struct mime_stream_data *msd;
  URL_Struct *url_struct;
  NET_StreamClass *istream;
};


static void *
mime_image_begin(const char *image_url, const char *content_type,
                 void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  struct mime_image_stream_data *mid;

  mid = XP_NEW(struct mime_image_stream_data);
  if (!mid) return 0;

  XP_MEMSET(mid, 0, sizeof(*mid));
  mid->msd = msd;

  /* Internal-external-reconnect only works when going to the screen.
     In that case, return the mid, but leave it empty (returning 0
     here is interpreted as out-of-memory.)
   */
  if (msd->format_out != FO_PRESENT &&
      msd->format_out != FO_CACHE_AND_PRESENT &&
      msd->format_out != FO_PRINT &&
      msd->format_out != FO_CACHE_AND_PRINT &&
      msd->format_out != FO_SAVE_AS_POSTSCRIPT &&
      msd->format_out != FO_CACHE_AND_SAVE_AS_POSTSCRIPT)
    return mid;
#ifndef MOZILLA_30
  if (!msd->context->img_cx)
      /* If there is no image context, e.g. if this is a Text context or a
         Mail context on the Mac, then we won't be loading images in the
         image viewer. */
      return mid;
#endif /* !MOZILLA_30 */

  mid->url_struct = NET_CreateURLStruct (image_url, NET_DONT_RELOAD);
  if (!mid->url_struct)
    {
      XP_FREE(mid);
      return 0;
    }

  mid->url_struct->content_encoding = 0;
  mid->url_struct->content_type = XP_STRDUP(content_type);
  if (!mid->url_struct->content_type)
    {
      NET_FreeURLStruct (mid->url_struct);
      XP_FREE(mid);
      return 0;
    }

  mid->istream = NET_StreamBuilder (FO_MULTIPART_IMAGE, mid->url_struct,
                                    msd->context);
  if (!mid->istream)
    {
      NET_FreeURLStruct (mid->url_struct);
      XP_FREE(mid);
      return 0;
    }

  /* When uudecoding, we tend to come up with tiny chunks of data
     at a time.  Make a stream to put them back together, so that
     we hand bigger pieces to the image library.
   */
  {
    NET_StreamClass *buffer =
      msg_MakeRebufferingStream (mid->istream, mid->url_struct, msd->context);
    if (buffer)
      mid->istream = buffer;
  }
  XP_ASSERT(msd->istream == NULL);
  msd->istream = mid->istream;
  return mid;
}


static void
mime_image_end(void *image_closure, int status)
{
  struct mime_image_stream_data *mid =
    (struct mime_image_stream_data *) image_closure;

  XP_ASSERT(mid);
  if (!mid) return;
  if (mid->istream)
    {
      if (status < 0)
        mid->istream->abort(mid->istream, status);
      else
        mid->istream->complete(mid->istream);
      XP_ASSERT(mid->msd->istream == mid->istream);
      mid->msd->istream = NULL;
      XP_FREE (mid->istream);
    }
  if (mid->url_struct)
    NET_FreeURLStruct (mid->url_struct);
  XP_FREE(mid);
}


static char *
mime_image_make_image_html(void *image_closure)
{
  struct mime_image_stream_data *mid =
    (struct mime_image_stream_data *) image_closure;

  const char *prefix = "<P><CENTER><IMG SRC=\"";
  const char *suffix = "\"></CENTER><P>";
  const char *url;
  char *buf;
  XP_ASSERT(mid);
  if (!mid) return 0;

  /* Internal-external-reconnect only works when going to the screen. */
  if (!mid->istream)
    return XP_STRDUP("<IMG SRC=\"internal-gopher-image\" ALT=\"[Image]\">");

  url = ((mid->url_struct && mid->url_struct->address)
         ? mid->url_struct->address
         : "");
  buf = (char *) XP_ALLOC (XP_STRLEN (prefix) + XP_STRLEN (suffix) +
                           XP_STRLEN (url) + 20);
  if (!buf) return 0;
  *buf = 0;

  XP_STRCAT (buf, prefix);
  XP_STRCAT (buf, url);
  XP_STRCAT (buf, suffix);
  return buf;
}


static int
mime_image_write_buffer(char *buf, int32 size, void *image_closure)
{
  struct mime_image_stream_data *mid =
    (struct mime_image_stream_data *) image_closure;
  if (!mid) return -1;
  if (!mid->istream) return 0;
  return mid->istream->put_block(mid->istream, buf, size);
}


/* Guessing the filename to use in "Save As", given a URL which may point
   to a MIME part that we've recently displayed.  (Kloooooge!)
 */


#ifdef HAVE_MIME_DATA_SLOT

/* If the given URL points to the MimeObject currently displayed on MWContext,
   or to a sub-part of it, then a freshly-allocated part-address-string will
   be returned.  (This string will be relative to the URL in the window.)
   Else, returns NULL.
 */
char *
mime_extract_relative_part_address(MWContext *context, const char *url)
{
  char *url1 = 0, *url2 = 0, *part = 0, *result = 0;    /* free these */
  char *base_part = 0, *sub_part = 0, *s = 0;           /* don't free these */

  XP_ASSERT(context && url);

  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object ||
      !context->mime_data->last_parsed_url)
    goto FAIL;

  url1 = XP_STRDUP(url);
  if (!url1) goto FAIL;  /* MK_OUT_OF_MEMORY */
  url2 = XP_STRDUP(context->mime_data->last_parsed_url);
  if (!url2) goto FAIL;  /* MK_OUT_OF_MEMORY */

  s = XP_STRCHR(url1, '?');
  if (s) *s = 0;
  s = XP_STRCHR(url2, '?');
  if (s) *s = 0;

  if (s) base_part = s+1;

  /* If the two URLs, minus their '?' parts, don't match, give up.
   */
  if (!!XP_STRCMP(url1, url2))
    goto FAIL;


  /* Otherwise, the URLs match, so now we can go search for the part.
   */

  /* First we need to extract `base_part' from url2 -- this is the common
     prefix between the two URLs. */
  if (base_part)
    {
      s = XP_STRSTR(base_part, "?part=");
      if (!s)
        s = XP_STRSTR(base_part, "&part=");
      base_part = s;
      if (base_part) /* found it */
        {
          base_part += 6;
          /* truncate `base_part' after part spec. */
          for (s = base_part; *s != 0 && *s != '?' && *s != '&'; s++)
            ;
          *s = 0;
        }
    }

  /* Find the part we're looking for.
   */
  s = XP_STRSTR(url, "?part=");
  if (!s)
    s = XP_STRSTR(url, "&part=");
  if (!s)
    goto FAIL;

  s += 6;
  part = XP_STRDUP(s);
  if (!part) goto FAIL;   /* MK_OUT_OF_MEMORY */

  /* truncate `part' after part spec. */
  for (s = part; *s != 0 && *s != '?' && *s != '&'; s++)
    ;
  *s = 0;

  /* Now remove the common prefix, if any. */
  sub_part = part;
  if (base_part)
    {
      int L = XP_STRLEN(base_part);
      if (!strncasecomp(sub_part, base_part, L) &&
          sub_part[L] == '.')
        sub_part += L + 1;
    }

  result = XP_STRDUP(sub_part);

FAIL:

  FREEIF(part);
  FREEIF(url1);
  FREEIF(url2);
  return result;
}

static char *
mime_guess_url_content_name_1 (MWContext *context, const char *url)
{
  char *name = 0;
  char *addr = mime_extract_relative_part_address(context, url);
  if (!addr) return 0;
  name = mime_find_suggested_name_of_part(addr,
                                      context->mime_data->last_parsed_object);
  XP_FREE(addr);
  return name;
}


char *
MimeGuessURLContentName(MWContext *context, const char *url)
{
  char *result = mime_guess_url_content_name_1 (context, url);
  if (result)
    return result;
  else
    {
      /* Ok, that didn't work, let's go look in all the other contexts! */
      XP_List *list = XP_GetGlobalContextList();
      int32 i, j = XP_ListCount(list);
      for (i = 1; i <= j; i++)
        {
          MWContext *cx2 = (MWContext *) XP_ListGetObjectNum(list, i);
          if (cx2 != context)
            {
              result = mime_guess_url_content_name_1 (cx2, url);
              if (result) return result;
            }
        }
    }
  return 0;
}

static char *
mime_get_url_content_type_1 (MWContext *context, const char *url)
{
  char *name = 0;
  char *addr = mime_extract_relative_part_address(context, url);
  if (!addr) return 0;
  name = mime_find_content_type_of_part(addr,
                                      context->mime_data->last_parsed_object);
  XP_FREE(addr);
  return name;
}

/* This routine is currently used to get the content type for a mime part url
   so the fe's can decide to open a new browser window or just open the save as dialog.
   It only works on a context that is displaying the message containing the part.
*/
char *
MimeGetURLContentType(MWContext *context, const char *url)
{
  char *result = mime_get_url_content_type_1 (context, url);
  /* I don't think we need to look at any other contexts for our purposes,
   * but I bow to Jamie's suscpicion that there's a good reason to.
  */
  if (result)
    return result;
  else
    {
      /* Ok, that didn't work, let's go look in all the other contexts! */
      XP_List *list = XP_GetGlobalContextList();
      int32 i, j = XP_ListCount(list);
      for (i = 1; i <= j; i++)
        {
          MWContext *cx2 = (MWContext *) XP_ListGetObjectNum(list, i);
          if (cx2 != context)
            {
              result = mime_get_url_content_type_1 (cx2, url);
              if (result) return result;
            }
        }
    }
  return 0;
}


static MimeObject*
mime_find_text_html_part_1(MimeObject* obj)
{
  if (mime_subclass_p(obj->class,
                      (MimeObjectClass*) &mimeInlineTextHTMLClass)) {
    return obj;
  }
  if (mime_subclass_p(obj->class, (MimeObjectClass*) &mimeContainerClass)) {
    MimeContainer* cobj = (MimeContainer*) obj;
    int32 i;
    for (i=0 ; i<cobj->nchildren ; i++) {
      MimeObject* result = mime_find_text_html_part_1(cobj->children[i]);
      if (result) return result;
    }
  }
  return NULL;
}


static MimeObject*
mime_find_text_html_part(MWContext* context)
{
  XP_ASSERT(context);
  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object)
    return NULL;

  return mime_find_text_html_part_1(context->mime_data->last_parsed_object);
}


XP_Bool
MimeShowingTextHtml(MWContext* context)
{
  return mime_find_text_html_part(context) != NULL;
}


extern char*
MimeGetHtmlPartURL(MWContext* context)
{
  MimeObject* obj = mime_find_text_html_part(context);
  if (obj == NULL) return NULL;
  return mime_part_address(obj);
}


/* Finds the main object of the message -- generally a multipart/mixed,
   text/plain, or text/html. */
static MimeObject*
mime_get_main_object(MimeObject* obj)
{
  MimeContainer* cobj;
  if (!(mime_subclass_p(obj->class, (MimeObjectClass*) &mimeMessageClass))) {
    return obj;
  }
  cobj = (MimeContainer*) obj;
  if (cobj->nchildren != 1) return obj;
  obj = cobj->children[0];
  for (;;) {
    if (!mime_subclass_p(obj->class,
                         (MimeObjectClass*) &mimeMultipartSignedClass) &&
        !mime_subclass_p(obj->class,
                         (MimeObjectClass*) &mimeFilterClass)) {
      return obj;
    }
  /* Our main thing is a signed or encrypted object.
     We don't care about that; go on inside to the thing that we signed or
     encrypted. */
    cobj = (MimeContainer*) obj;
    if (cobj->nchildren != 1) return obj;
    obj = cobj->children[0];
  }
}


XP_Bool MimeObjectChildIsMessageBody(MimeObject *obj,
									 XP_Bool *isAlternativeOrRelated)
{
	char *disp = 0;
	XP_Bool bRet = FALSE;
	MimeObject *firstChild = 0;
	MimeContainer *container = (MimeContainer*) obj;

	if (isAlternativeOrRelated)
		*isAlternativeOrRelated = FALSE;

	if (!container ||
		!mime_subclass_p(obj->class,
						 (MimeObjectClass*) &mimeContainerClass))
	{
		return bRet;
	}
	else if (mime_subclass_p(obj->class, (MimeObjectClass*)
							 &mimeMultipartRelatedClass))
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = TRUE;
		return bRet;
	}
	else if (mime_subclass_p(obj->class, (MimeObjectClass*)
							 &mimeMultipartAlternativeClass))
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = TRUE;
		return bRet;
	}

	if (container->children)
		firstChild = container->children[0];

	if (!firstChild ||
		!firstChild->content_type ||
		!firstChild->headers)
		return bRet;

	disp = MimeHeaders_get (firstChild->headers,
							HEADER_CONTENT_DISPOSITION,
							TRUE,
							FALSE);
	if (disp /* && !strcasecomp (disp, "attachment") */)
		bRet = FALSE;
	else if (!strcasecomp (firstChild->content_type, TEXT_PLAIN) ||
			 !strcasecomp (firstChild->content_type, TEXT_HTML) ||
			 !strcasecomp (firstChild->content_type, TEXT_MDL) ||
			 !strcasecomp (firstChild->content_type, MULTIPART_ALTERNATIVE) ||
			 !strcasecomp (firstChild->content_type, MULTIPART_RELATED) ||
			 !strcasecomp (firstChild->content_type, MESSAGE_NEWS) ||
			 !strcasecomp (firstChild->content_type, MESSAGE_RFC822))
		bRet = TRUE;
	else
		bRet = FALSE;
	FREEIF(disp);
	return bRet;
}


int
MimeGetAttachmentCount(MWContext* context)
{
  MimeObject* obj;
  MimeContainer* cobj;
  XP_Bool isMsgBody = FALSE, isAlternativeOrRelated = FALSE;

  XP_ASSERT(context);
  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object) {
    return 0;
  }
  obj = mime_get_main_object(context->mime_data->last_parsed_object);
  if (!mime_subclass_p(obj->class, (MimeObjectClass*) &mimeContainerClass))
    return 0;

  cobj = (MimeContainer*) obj;

  isMsgBody = MimeObjectChildIsMessageBody(obj,
										   &isAlternativeOrRelated);

  if (isAlternativeOrRelated)
	  return 0;
  else if (isMsgBody)
	  return cobj->nchildren - 1;
  else
	  return cobj->nchildren;
}


int
MimeGetAttachmentList(MWContext* context, MSG_AttachmentData** data)
{
  MimeObject* obj;
  MimeContainer* cobj;
  MSG_AttachmentData* tmp;
  int32 n;
  int32 i;
  char* disp;
  char c;
  XP_Bool isMsgBody = FALSE, isAlternativeOrRelated = FALSE;

  if (!data) return 0;
  *data = NULL;
  XP_ASSERT(context);
  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object) {
    return 0;
  }
  obj = mime_get_main_object(context->mime_data->last_parsed_object);
  if (!mime_subclass_p(obj->class, (MimeObjectClass*) &mimeContainerClass)) {
    return 0;
  }
  isMsgBody = MimeObjectChildIsMessageBody(obj,
										   &isAlternativeOrRelated);
  if (isAlternativeOrRelated)
	  return 0;

  cobj = (MimeContainer*) obj;
  n = cobj->nchildren;          /* This is often too big, but that's OK. */
  if (n <= 0) return n;
  *data = XP_CALLOC(n + 1, sizeof(MSG_AttachmentData));
  if (!*data) return MK_OUT_OF_MEMORY;
  tmp = *data;
  c = '?';
  if (XP_STRCHR(context->mime_data->last_parsed_url, '?')) {
    c = '&';
  }

  /* let's figure out where to start */
  if (isMsgBody)
	  i = 1;
  else
	  i = 0;

  for ( ; i<cobj->nchildren ; i++, tmp++) {
    MimeObject* child = cobj->children[i];
    char* part = mime_part_address(child);
	char* imappart = NULL;
    if (!part) return MK_OUT_OF_MEMORY;
	if (obj->options->missing_parts)
		imappart = mime_imap_part_address (child);
	if (imappart)
	{
		tmp->url = mime_set_url_imap_part(context->mime_data->last_parsed_url, imappart, part);
	}
	else
	{
		tmp->url = mime_set_url_part(context->mime_data->last_parsed_url, part, TRUE);
	}
	/*
	tmp->url = PR_smprintf("%s%cpart=%s", context->mime_data->last_parsed_url,
                           c, part);
	*/
    if (!tmp->url) return MK_OUT_OF_MEMORY;
    tmp->real_type = child->content_type ?
      XP_STRDUP(child->content_type) : NULL;
    tmp->real_encoding = child->encoding ? XP_STRDUP(child->encoding) : NULL;
    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_DISPOSITION,
                           FALSE, FALSE);
    if (disp) {
      tmp->real_name = MimeHeaders_get_parameter(disp, "filename", NULL, NULL);
	  if (tmp->real_name)
	  {
		char *fname = NULL;
		fname = mime_decode_filename(tmp->real_name);
		if (fname && fname != tmp->real_name)
		{
			XP_FREE(tmp->real_name);
			tmp->real_name = fname;
		}
	  }
      XP_FREE(disp);
    }
    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_TYPE,
                       FALSE, FALSE);
    if (disp)
    {
      tmp->x_mac_type   = MimeHeaders_get_parameter(disp, PARAM_X_MAC_TYPE, NULL, NULL);
      tmp->x_mac_creator= MimeHeaders_get_parameter(disp, PARAM_X_MAC_CREATOR, NULL, NULL);
	  if (!tmp->real_name || *tmp->real_name == 0)
	  {
		XP_FREEIF(tmp->real_name);
		tmp->real_name = MimeHeaders_get_parameter(disp, "name", NULL, NULL);
		if (tmp->real_name)
		{
			char *fname = NULL;
			fname = mime_decode_filename(tmp->real_name);
			if (fname && fname != tmp->real_name)
			{
				XP_FREE(tmp->real_name);
				tmp->real_name = fname;
			}
		}
	  }
      XP_FREE(disp);
    }
    tmp->description = MimeHeaders_get(child->headers,
                                       HEADER_CONTENT_DESCRIPTION,
                                       FALSE, FALSE);
#ifndef MOZILLA_30
    if (tmp->real_type && !strcasecomp(tmp->real_type, MESSAGE_RFC822) &&
        (!tmp->real_name || *tmp->real_name == 0))
    {
        StrAllocCopy(tmp->real_name, XP_GetString(XP_FORWARDED_MESSAGE_ATTACHMENT));
    }
#endif /* !MOZILLA_30 */
  }
  return 0;
}


void
MimeFreeAttachmentList(MSG_AttachmentData* data)
{
  if (data) {
    MSG_AttachmentData* tmp;
    for (tmp = data ; tmp->url ; tmp++) {
      /* Can't do FREEIF on `const' values... */
      if (tmp->url) XP_FREE((char *) tmp->url);
      if (tmp->real_type) XP_FREE((char *) tmp->real_type);
      if (tmp->real_encoding) XP_FREE((char *) tmp->real_encoding);
      if (tmp->real_name) XP_FREE((char *) tmp->real_name);
      if (tmp->x_mac_type) XP_FREE((char *) tmp->x_mac_type);
      if (tmp->x_mac_creator) XP_FREE((char *) tmp->x_mac_creator);
      if (tmp->description) XP_FREE((char *) tmp->description);
      tmp->url = 0;
      tmp->real_type = 0;
      tmp->real_name = 0;
      tmp->description = 0;
    }
    XP_FREE(data);
  }
}



void
MimeDestroyContextData(MWContext *context)
{
  INTL_CharSetInfo csi = NULL;

  XP_ASSERT(context);
  if (!context) return;

  csi = LO_GetDocumentCharacterSetInfo(context);
  if (csi)
	  INTL_SetCSIMimeCharset(csi, NULL);

  if (!context->mime_data) return;

  if (context->mime_data->last_parsed_object)
    {
      MimeDisplayOptions *options =
        context->mime_data->last_parsed_object->options;

      mime_free(context->mime_data->last_parsed_object);
      if (options)
        {
          FREEIF(options->part_to_load);
          XP_FREE(options);
        }

      context->mime_data->last_parsed_object = 0;
      FREEIF(context->mime_data->last_parsed_url);
    }

#ifdef LOCK_LAST_CACHED_MESSAGE
  if (context->mime_data->previous_locked_url)
    {
      /* duplicated from mime_make_output_stream()... */
      URL_Struct *url_s =
        NET_CreateURLStruct(context->mime_data->previous_locked_url,
                            NET_NORMAL_RELOAD);
      if (url_s)
        {
          /* Note: if post data was involved here, we'd lose.  We're assuming
             that all we need to save is the url->address. */
          NET_ChangeCacheFileLock(url_s, FALSE);
          NET_FreeURLStruct(url_s);
        }
      XP_FREE(context->mime_data->previous_locked_url);
      context->mime_data->previous_locked_url = 0;
    }
#endif /* LOCK_LAST_CACHED_MESSAGE */

  XP_FREE(context->mime_data);
  context->mime_data = 0;
}

#else  /* !HAVE_MIME_DATA_SLOT */

char *
MimeGuessURLContentName(MWContext *context, const char *url)
{
  return 0;
}

void
MimeDestroyContextData(MWContext *context)
{
  if (!context) return;

  INTL_SetCSIMimeCharset(LO_GetDocumentCharacterSetInfo(context), NULL);

}
#endif /* !HAVE_MIME_DATA_SLOT */





/* Interface between netlib and the converters from text/richtext
   and text/enriched to text/html: MIME_RichtextConverter() and
   MIME_EnrichedTextConverter().
 */

struct mime_richtext_data {
  URL_Struct *url;                  /* The URL this is all coming from. */
  MWContext *context;
  int format_out;
  NET_StreamClass *stream;          /* The stream to which we write HTML. */
  char *ibuffer, *obuffer;
  int32 ibuffer_size, obuffer_size;
  int32 ibuffer_fp;
  XP_Bool enriched_p;
};

static int
mime_richtext_stream_fn (char *buf, int32 size, void *closure)
{
  struct mime_richtext_data *mrd = (struct mime_richtext_data *) closure;
  XP_ASSERT(mrd->stream);
  if (!mrd->stream) return -1;
  return mrd->stream->put_block (mrd->stream, buf, size);
}

static int
mime_richtext_write_line (char* line, int32 size, void *closure)
{
  struct mime_richtext_data *mrd = (struct mime_richtext_data *) closure;
  return MimeRichtextConvert (line, size, mime_richtext_stream_fn,
                              mrd, &mrd->obuffer, &mrd->obuffer_size,
                              mrd->enriched_p);
}

static int
mime_richtext_write (NET_StreamClass *stream, const char* buf, int32 size)
{
  struct mime_richtext_data *data = (struct mime_richtext_data *) stream->data_object;
  return msg_LineBuffer (buf, size, &data->ibuffer, &data->ibuffer_size,
                         &data->ibuffer_fp, FALSE, mime_richtext_write_line,
                         data);
}

static unsigned int
mime_richtext_write_ready (NET_StreamClass *stream)
{
  struct mime_richtext_data *data = (struct mime_richtext_data *) stream->data_object;
  if (data->stream)
    return ((*data->stream->is_write_ready)
            (data->stream));
  else
    return (MAX_WRITE_READY);
}

static void
mime_richtext_complete (NET_StreamClass *stream)
{
  struct mime_richtext_data *mrd = (struct mime_richtext_data *) stream->data_object;
  if (!mrd) return;
  FREEIF(mrd->obuffer);
  if (mrd->stream)
    {
      mrd->stream->complete (mrd->stream);
      XP_FREE (mrd->stream);
    }
  XP_FREE(mrd);
}

static void
mime_richtext_abort (NET_StreamClass *stream, int status)
{
  struct mime_richtext_data *mrd = (struct mime_richtext_data *) stream->data_object;
  if (!mrd) return;
  FREEIF(mrd->obuffer);
  if (mrd->stream)
    {
      mrd->stream->abort (mrd->stream, status);
      XP_FREE (mrd->stream);
    }
  XP_FREE(mrd);
}


static NET_StreamClass *
MIME_RichtextConverter_1 (int format_out, void *closure,
                          URL_Struct *url, MWContext *context,
                          XP_Bool enriched_p)
{
  struct mime_richtext_data *data;
  NET_StreamClass *stream, *next_stream;

  next_stream = mime_make_output_stream(TEXT_HTML, 0, 0, 0, 0,
                                        format_out, url, context, NULL);
  if (!next_stream) return 0;

  data = XP_NEW(struct mime_richtext_data);
  if (!data)
    {
      XP_FREE(next_stream);
      return 0;
    }
  XP_MEMSET(data, 0, sizeof(*data));
  data->url = url;
  data->context = context;
  data->format_out = format_out;
  data->stream = next_stream;
  data->enriched_p = enriched_p;

  stream = XP_NEW (NET_StreamClass);
  if (!stream)
    {
      XP_FREE(next_stream);
      XP_FREE(data);
      return 0;
    }
  XP_MEMSET (stream, 0, sizeof (*stream));

  stream->name           = "Richtext Conversion Stream";
  stream->complete       = mime_richtext_complete;
  stream->abort          = mime_richtext_abort;
  stream->put_block      = mime_richtext_write;
  stream->is_write_ready = mime_richtext_write_ready;
  stream->data_object    = data;
  stream->window_id      = context;

  TRACEMSG(("Returning stream from MIME_RichtextConverter"));

  return stream;
}

NET_StreamClass *
MIME_RichtextConverter (int format_out, void *closure,
                        URL_Struct *url, MWContext *context)
{
  return MIME_RichtextConverter_1 (format_out, closure, url, context, FALSE);
}

NET_StreamClass *
MIME_EnrichedTextConverter (int format_out, void *closure,
                            URL_Struct *url, MWContext *context)
{
  return MIME_RichtextConverter_1 (format_out, closure, url, context, TRUE);
}



#ifndef MOZILLA_30

int
MIME_DisplayAttachmentPane(MWContext* context)
{
    if (context && context->mime_data) {
        MSG_Pane* pane = context->mime_data->last_pane;
        if (!pane)
            pane = MSG_FindPane(context, MSG_MESSAGEPANE);
        if (pane) {
            MSG_MessagePaneCallbacks* callbacks;
            void* closure;
            callbacks = MSG_GetMessagePaneCallbacks(pane, &closure);
            if (callbacks && callbacks->UserWantsToSeeAttachments) {
                (*callbacks->UserWantsToSeeAttachments)(pane, closure);
            }
        }
    }
    return 0;
}


/* This struct is the state we used in MIME_VCardConverter() */
struct mime_vcard_data {
    URL_Struct *url;                         /* original url */
    int format_out;                          /* intended output format;
                                              should be TEXT-VCARD */
    MWContext *context;
    NET_StreamClass *stream;                 /* not used for now */
    MimeDisplayOptions *options;             /* data for communicating with libmime.a */
    MimeObject *obj;                         /* The root */
};

static int
mime_vcard_write (NET_StreamClass *stream,
              const char *buf,
              int32 size )
{
  struct mime_vcard_data *vcd = (struct mime_vcard_data *) stream->data_object;
  XP_ASSERT ( vcd );

  if ( !vcd || !vcd->obj ) return -1;

  return vcd->obj->class->parse_line ((char *) buf, size, vcd->obj);
}

static unsigned int
mime_vcard_write_ready (NET_StreamClass *stream)
{
  struct mime_vcard_data *vcd = (struct mime_vcard_data *) stream->data_object;
  XP_ASSERT (vcd);

  if (!vcd) return MAX_WRITE_READY;
  if (vcd->stream)
    return vcd->stream->is_write_ready ( vcd->stream );
  else
    return MAX_WRITE_READY;
}

static void
mime_vcard_complete (NET_StreamClass *stream)
{
  struct mime_vcard_data *vcd = (struct mime_vcard_data *) stream->data_object;

  XP_ASSERT (vcd);

  if (!vcd) return;

  if (vcd->obj) {
    int status;

    status = vcd->obj->class->parse_eof ( vcd->obj, FALSE );
    vcd->obj->class->parse_end( vcd->obj, status < 0 ? TRUE : FALSE );

    mime_free (vcd->obj);
    vcd->obj = 0;

    if (vcd->stream) {
      vcd->stream->complete (vcd->stream);
      XP_FREE( vcd->stream );
      vcd->stream = 0;
    }
  }
}

static void
mime_vcard_abort (NET_StreamClass *stream, int status )
{
  struct mime_vcard_data *vcd = (struct mime_vcard_data *) stream->data_object;

  XP_ASSERT (vcd);
  if (!vcd) return;

  if (vcd->obj) {
      int status;

      if ( !vcd->obj->closed_p )
          status = vcd->obj->class->parse_eof ( vcd->obj, TRUE );
      if ( !vcd->obj->parsed_p )
          vcd->obj->class->parse_end( vcd->obj, TRUE );

      mime_free (vcd->obj);
      vcd->obj = 0;

      if (vcd->stream) {
          vcd->stream->abort (vcd->stream, status);
          XP_FREE( vcd->stream );
          vcd->stream = 0;
      }
  }
  XP_FREE (vcd);
}

extern int MIME_HasAttachments(MWContext *context)
{
	return (context->mime_data && context->mime_data->last_parsed_object->class->showAttachmentIcon);
}



extern NET_StreamClass *
MIME_VCardConverter ( int format_out,
            void *closure,
            URL_Struct *url,
            MWContext *context )
{
    int status = 0;
    NET_StreamClass * stream = NULL;
    NET_StreamClass * next_stream = NULL;
    struct mime_vcard_data *vcd = NULL;
    MimeObject *obj;

    XP_ASSERT (url && context);
    if ( !url || !context ) return NULL;

    next_stream = mime_make_output_stream (TEXT_HTML, 0, 0, 0, 0,
                format_out, url, context, NULL);

    if (!next_stream) return 0;


    vcd = XP_NEW_ZAP (struct mime_vcard_data);
    if (!vcd) {
        XP_FREE (next_stream);
        return 0;
    }

    vcd->url = url;
    vcd->context = context;
    vcd->format_out = format_out;
    vcd->stream = next_stream;

    vcd->options = XP_NEW_ZAP ( MimeDisplayOptions );

    if ( !vcd->options ) {
        XP_FREE (next_stream);
        XP_FREE ( vcd );
        return 0;
    }

    vcd->options->write_html_p        = TRUE;
    vcd->options->output_fn           = mime_output_fn;
    if (format_out == FO_PRESENT ||
        format_out == FO_CACHE_AND_PRESENT)
    vcd->options->output_vcard_buttons_p = TRUE;

#ifdef MIME_DRAFTS
    vcd->options->decompose_file_p = FALSE; /* new field in MimeDisplayOptions */
#endif /* MIME_DRAFTS */

    vcd->options->url = url->address;
    vcd->options->stream_closure = vcd;
    vcd->options->html_closure = vcd;

    obj = mime_new ( (MimeObjectClass *) &mimeInlineTextVCardClass,
        (MimeHeaders *) NULL,
        TEXT_VCARD );

    if ( !obj ) {
        FREEIF( vcd->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( vcd->options );
        XP_FREE ( vcd );
        return 0;
    }

    obj->options = vcd->options;
    vcd->obj = obj;

    stream = XP_NEW_ZAP ( NET_StreamClass );
    if ( !stream ) {
        FREEIF ( vcd->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( vcd->options );
        XP_FREE ( vcd );
        XP_FREE ( obj );
        return 0;
    }

    stream->name = "MIME To VCard Converter Stream";
    stream->complete = mime_vcard_complete;
    stream->abort = mime_vcard_abort;
    stream->put_block = mime_vcard_write;
    stream->is_write_ready = mime_vcard_write_ready;
    stream->data_object = vcd;
    stream->window_id = context;

    status = obj->class->initialize ( obj );
    if ( status >= 0 )
        status = obj->class->parse_begin ( obj );
    if ( status < 0 ) {
        XP_FREE ( stream );
        FREEIF( vcd->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( vcd->options );
        XP_FREE ( vcd );
        XP_FREE ( obj );
        return 0;
    }

    return stream;
}

#endif /* !MOZILLA_30 */



int
MimeSendMessage(MimeDisplayOptions* options, char* to, char* subject,
				char* otherheaders, char* body)
{
	struct mime_stream_data* msd =
		(struct mime_stream_data*) options->stream_closure;
	return NET_SendMessageUnattended(msd->context, to, subject,
									 otherheaders, body);
}


int
mime_TranslateCalendar(char* caldata, char** html)
{
#ifdef JULIAN_EXISTS
    static XP_Bool initialized = FALSE;
    void* closure;
    if (!initialized) {
	Julian_Form_Callback_Struct jcbs;

		jcbs.callbackurl = NET_CallbackURLCreate;
		jcbs.callbackurlfree = NET_CallbackURLFree;
		jcbs.ParseURL = NET_ParseURL;
		jcbs.MakeNewWindow = FE_MakeNewWindow;
		jcbs.CreateURLStruct = NET_CreateURLStruct;
		jcbs.StreamBuilder = NET_StreamBuilder;
		jcbs.SACopy = NET_SACopy;

                /* John Sun added 4-22-98 */
                jcbs.SendMessageUnattended = NET_SendMessageUnattended;
                jcbs.DestroyWindow = FE_DestroyWindow;

                jcbs.GetString = XP_GetString;

		jf_Initialize(&jcbs);
		initialized = TRUE;
    }
    closure = jf_New(caldata);
    *html = jf_getForm(closure);
    jf_Destroy(closure);
    return 0;
#else
    *html = XP_STRDUP("<b>Can't handle calendar data on this platform yet</b>");
    return 0;
#endif /* JULIAN_EXISTS */
}



/*
 *
 * MIME_JulianConverter Stream handler stuff
 *
 */

#ifdef MOZ_CALENDAR
struct mime_calendar_data {
    URL_Struct *url;                         /* original url */
    int format_out;                          /* intended output format; should be TEXT-CALENDAR */
    MWContext *context;
    NET_StreamClass *stream;                 /* not used for now */
    MimeDisplayOptions *options;             /* data for communicating with libmime.a */
    MimeObject *obj;                         /* The root */
};


static int mime_calendar_write (void *stream, const char *buf, int32 size )
{
  struct mime_calendar_data *cald = (struct mime_calendar_data *) stream;
  XP_ASSERT ( cald );

  if ( !cald || !cald->obj ) return -1;

  return cald->obj->class->parse_line ((char *) buf, size, cald->obj);
}

static unsigned int mime_calendar_write_ready (void *stream)
{
  struct mime_calendar_data *cald = (struct mime_calendar_data *) stream;
  XP_ASSERT (cald);

  if (!cald) return MAX_WRITE_READY;
  if (cald->stream)
    return cald->stream->is_write_ready ( cald->stream->data_object );
  else
    return MAX_WRITE_READY;
}

static void mime_calendar_complete (void *stream)
{
  struct mime_calendar_data *cald = (struct mime_calendar_data *) stream;

  XP_ASSERT (cald);

  if (!cald) return;

  if (cald->obj) {
    int status;

    status = cald->obj->class->parse_eof ( cald->obj, FALSE );
    cald->obj->class->parse_end( cald->obj, status < 0 ? TRUE : FALSE );

    mime_free (cald->obj);
    cald->obj = 0;

    if (cald->stream) {
      cald->stream->complete (cald->stream->data_object);
      XP_FREE( cald->stream );
      cald->stream = 0;
    }
  }
}

static void mime_calendar_abort (void *stream, int status )
{
  struct mime_calendar_data *cald = (struct mime_calendar_data *) stream;

  XP_ASSERT (cald);
  if (!cald) return;

  if (cald->obj) {
      int status;

      if ( !cald->obj->closed_p )
          status = cald->obj->class->parse_eof ( cald->obj, TRUE );
      if ( !cald->obj->parsed_p )
          cald->obj->class->parse_end( cald->obj, TRUE );

      mime_free (cald->obj);
      cald->obj = 0;

      if (cald->stream) {
          cald->stream->abort (cald->stream->data_object, status);
          XP_FREE( cald->stream );
          cald->stream = 0;
      }
  }
  XP_FREE (cald);
}

extern NET_StreamClass * MIME_JulianConverter (int format_out, void *closure, URL_Struct *url, MWContext *context )
{
    int status = 0;
    NET_StreamClass * stream = NULL;
    NET_StreamClass * next_stream = NULL;
    struct mime_calendar_data *cald = NULL;
    MimeObject *obj;

    XP_ASSERT (url && context);
    if ( !url || !context ) return NULL;

    next_stream = mime_make_output_stream (TEXT_HTML, 0, 0, 0, 0,
                format_out, url, context, NULL);

    if (!next_stream) return 0;

    cald = XP_NEW_ZAP (struct mime_calendar_data);
    if (!cald) {
        XP_FREE (next_stream);
        return 0;
    }

    cald->url = url;
    cald->context = context;
    cald->format_out = format_out;
    cald->stream = next_stream;

    cald->options = XP_NEW_ZAP ( MimeDisplayOptions );

    if ( !cald->options ) {
        XP_FREE (next_stream);
        XP_FREE ( cald );
        return 0;
    }

    cald->options->write_html_p        = TRUE;
    cald->options->output_fn           = mime_output_fn;
    if (format_out == FO_PRESENT ||
        format_out == FO_CACHE_AND_PRESENT)
    cald->options->output_vcard_buttons_p = FALSE;

#ifdef MIME_DRAFTS
    cald->options->decompose_file_p = FALSE; /* new field in MimeDisplayOptions */
#endif /* MIME_DRAFTS */

    cald->options->url = url->address;
    cald->options->stream_closure = cald;
    cald->options->html_closure = cald;

    obj = mime_new ( (MimeObjectClass *) &mimeInlineTextCalendarClass,
        (MimeHeaders *) NULL,
        TEXT_CALENDAR );

    if ( !obj ) {
        FREEIF( cald->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( cald->options );
        XP_FREE ( cald );
        return 0;
    }

    obj->options = cald->options;
    cald->obj = obj;

    stream = XP_NEW_ZAP ( NET_StreamClass );
    if ( !stream ) {
        FREEIF ( cald->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( cald->options );
        XP_FREE ( cald );
        XP_FREE ( obj );
        return 0;
    }

    stream->name = "MIME To Calendar Converter Stream";
    stream->complete = mime_calendar_complete;
    stream->abort = mime_calendar_abort;
    stream->put_block = mime_calendar_write;
    stream->is_write_ready = mime_calendar_write_ready;
    stream->data_object = cald;
    stream->window_id = context;

    status = obj->class->initialize ( obj );
    if ( status >= 0 )
        status = obj->class->parse_begin ( obj );
    if ( status < 0 ) {
        XP_FREE ( stream );
        FREEIF( cald->options->part_to_load );
        XP_FREE ( next_stream );
        XP_FREE ( cald->options );
        XP_FREE ( cald );
        XP_FREE ( obj );
        return 0;
    }

    return stream;
}
#endif

