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
#include "modlmime.h"
#include "libi18n.h"
#include "xp_time.h"
#include "msgcom.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimetric.h"   /* for MIME_RichtextConverter */
#include "mimethtm.h"
#include "mimemsig.h"
#include "mimemrel.h"
#include "mimemalt.h"
#include "xpgetstr.h"
#include "mimebuf.h"
#include "edt.h"
#include "mimerosetta.h"
#include "proto.h"
#include "secrng.h"
#include "prprf.h"
#include "intl_csi.h"
#include "mimei.h"      /* for moved MimeDisplayData struct */
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "prmem.h"
#include "mimemoz2.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

#ifdef MOZ_SECURITY
#include HG01944
#include HG04488
#include HG01999
#endif /* MOZ_SECURITY */

#ifdef HAVE_MIME_DATA_SLOT
#define LOCK_LAST_CACHED_MESSAGE
#endif

// For the new pref API's
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

extern "C" int MK_UNABLE_TO_OPEN_TMP_FILE;
extern "C" int XP_FORWARDED_MESSAGE_ATTACHMENT;

/* Arrgh.  Why isn't this in a reasonable header file somewhere???  ###tw */
extern char * NET_ExplainErrorDetails (int code, ...);

extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset);


/* Interface between netlib and the top-level message/rfc822 parser:
   MIME_MessageConverter()
 */
static MimeHeadersState MIME_HeaderType;
static XP_Bool MIME_NoInlineAttachments;
static XP_Bool MIME_WrapLongLines;
static XP_Bool MIME_VariableWidthPlaintext;

static char *
mime_reformat_date(const char *date, void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  MWContext *context = msd->context;
  const char *s;
  time_t t;
  PR_ASSERT(date);
  if (!date) return 0;
  t = XP_ParseTimeString(date, PR_FALSE);
  if (t <= 0) return 0;
  s = MSG_FormatDateFromContext(context, t);
  if (!s) return 0;
  return PL_strdup(s);
}


static char *
mime_file_type (const char *filename, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_type ((char *) filename);
  if (!cinfo || !cinfo->type)
    return 0;
  else
    return PL_strdup(cinfo->type);
}

static char *
mime_type_desc(const char *type, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_info_by_type((char *) type);
  if (!cinfo || !cinfo->desc || !*cinfo->desc)
    return 0;
  else
    return PL_strdup(cinfo->desc);
}


static char *
mime_type_icon(const char *type, void *stream_closure)
{
  NET_cinfo *cinfo = NET_cinfo_find_info_by_type((char *) type);

  if (cinfo && cinfo->icon && *cinfo->icon)
    return PL_strdup(cinfo->icon);
  else if (!PL_strncasecmp(type, "text/", 5))
    return PL_strdup("resource:/res/network/gopher-text.gif");
  else if (!PL_strncasecmp(type, "image/", 6))
    return PL_strdup("resource:/res/network/gopher-image.gif");
  else if (!PL_strncasecmp(type, "audio/", 6))
    return PL_strdup("resource:/res/network/gopher-sound.gif");
  else if (!PL_strncasecmp(type, "video/", 6))
    return PL_strdup("resource:/res/network/gopher-movie.gif");
  else if (!PL_strncasecmp(type, "application/", 12))
    return PL_strdup("resource:/res/network/gopher-binary.gif");
  else
    return PL_strdup("internal-gopher-unknown.gif");
}

static int
mime_convert_charset (const char *input_line, PRInt32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, PRInt32 *output_size_ret,
                      void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  unsigned char *converted;

  converted = INTL_ConvMailToWinCharCode(msd->context,
                                         (unsigned char *) input_line,
                                         input_length);
  if (converted)
    {
      *output_ret = (char *) converted;
      *output_size_ret = PL_strlen((char *) converted);
    }
  else
    {
      *output_ret = 0;
      *output_size_ret = 0;
    }
  return 0;
}


static int
mime_convert_rfc1522 (const char *input_line, PRInt32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, PRInt32 *output_size_ret,
                      void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  char *converted;
  char *line;
  char charset[128];
  
  charset[0] = '\0';

  if (input_line[input_length] == 0)  /* oh good, it's null-terminated */
    line = (char *) input_line;
  else
    {
      line = (char *) PR_MALLOC(input_length+1);
      if (!line) return MK_OUT_OF_MEMORY;
      XP_MEMCPY(line, input_line, input_length);
      line[input_length] = 0;
    }

  converted = MIME_DecodeMimePartIIStr(line, charset);

  if (line != input_line)
    PR_Free(line);

  if (converted)
    {
      *output_ret = converted;
      *output_size_ret = PL_strlen(converted);
    }
  else
    {
      *output_ret = 0;
      *output_size_ret = 0;
    }
  return 0;
}


static int
mime_output_fn(char *buf, PRInt32 size, void *stream_closure)
{
  PRUint32  written;
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  PR_ASSERT(msd->pluginObj);
  if ( (!msd->pluginObj) || (!msd->output_emitter) )
    return -1;
  
  // Now, write to the WriteBody method if this is a message body and not
  // a part retrevial
  if (!msd->options->part_to_load)
  {
    msd->output_emitter->WriteBody(buf, (PRUint32) size, &written);
  }
  else
    msd->output_emitter->Write(buf, (PRUint32) size, &written);
  return written;
}

static int
compose_only_output_fn(char *buf, PRInt32 size, void *stream_closure)
{
    return 0;
}

extern PRBool msg_LoadingForComposeOnly(const MSG_Pane* pane); /* in msgmpane.cpp */

static int
mime_set_html_state_fn (void *stream_closure,
                        PRBool layer_encapsulate_p,
                        PRBool start_p,
                        PRBool abort_p)
{
  int status = 0;

  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  
  if (start_p) 
  {
  } 
  else 
  {
  }

  return status;
}

extern "C" int
mime_display_stream_write (nsMIMESession *stream,
                           const char* buf,
                           PRInt32 size)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;

  MimeObject *obj = (msd ? msd->obj : 0);  
  if (!obj) return -1;

  return obj->clazz->parse_buffer((char *) buf, size, obj);
}

extern "C" void 
mime_display_stream_complete (nsMIMESession *stream)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;
  MimeObject *obj = (msd ? msd->obj : 0);  
  if (obj)
  {
    int status;
    status = obj->clazz->parse_eof(obj, PR_FALSE);
    obj->clazz->parse_end(obj, (status < 0 ? PR_TRUE : PR_FALSE));
    
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
      PR_ASSERT(msd->options == obj->options);
      mime_free(obj);
      obj = NULL;
      if (msd->options)
      {
        PR_FREEIF(msd->options->part_to_load);
        PR_FREEIF(msd->options->default_charset);
        PR_FREEIF(msd->options->override_charset);
        PR_Free(msd->options);
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
      PL_strdup(msd->url->address);
    
#ifdef RICHIE
    /* Lock this URL in the cache. */
    if (msd->context->mime_data->previous_locked_url)
      NET_ChangeCacheFileLock(msd->url, PR_TRUE);
#endif
  }
#endif /* LOCK_LAST_CACHED_MESSAGE */
  
  // Release the prefs service
  if (msd->prefs)
    nsServiceManager::ReleaseService(kPrefCID, msd->prefs);

  PR_Free(msd);
}

extern "C" void
mime_display_stream_abort (nsMIMESession *stream, int status)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) ((nsMIMESession *)stream)->data_object;
  
  MimeObject *obj = (msd ? msd->obj : 0);  
  if (obj)
  {
    if (!obj->closed_p)
      obj->clazz->parse_eof(obj, PR_TRUE);
    if (!obj->parsed_p)
      obj->clazz->parse_end(obj, PR_TRUE);
    
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
      PR_ASSERT(msd->options == obj->options);
      mime_free(obj);
      if (msd->options)
      {
        PR_FREEIF(msd->options->part_to_load);
        PR_Free(msd->options);
        msd->options = 0;
      }
    }
  }
  PR_ASSERT(msd); /* Crash was happening here - jrm */
  
  if (msd)
  {
    // RICHIE - context stuff
    if (msd->pluginObj)
    {
      if (msd->context && msd->context->mime_data && msd->context->mime_data->last_parsed_object)
        msd->context->mime_data->last_parsed_object->showAttachmentIcon = PR_FALSE;      
    }
    PR_Free(msd);
  }
}

static int
mime_insert_html_put_block(nsMIMESession *stream, const char* str, PRInt32 length)
{
  struct mime_stream_data* msd = (struct mime_stream_data*) stream;
  char* s = (char*) str;
  char c = s[length];  
  PR_ASSERT(msd);
  if (!msd) return -1;
  if (c) {
    s[length] = '\0';
  }
  /* s is in the outcsid encoding at this point. That was done in
   * mime_insert_html_convert_charset */
#ifdef RICHIE
  EDT_PasteQuoteINTL(msd->context, s, msd->outcsid); 
#endif
  if (c) {
    s[length] = c;
  }
  return 0;
}

static void
mime_insert_html_complete(nsMIMESession *stream)
{
  struct mime_stream_data* msd = (struct mime_stream_data*) stream;  
  PR_ASSERT(msd);
  if (!msd) return;
#ifdef RICHIE
  EDT_PasteQuote(msd->context, "</BLOCKQUOTE>");
#endif
  if (msd->format_out == FO_QUOTE_HTML_MESSAGE) {
      XP_Bool eReplyOnTop = PR_TRUE, nReplyWithExtraLines = PR_FALSE;
      if (msd->prefs)
      {
        msd->prefs->GetBoolPref("mailnews.reply_on_top", &eReplyOnTop);
        msd->prefs->GetBoolPref("mailnews.reply_with_extra_lines", &nReplyWithExtraLines);
      }
      if (0 == eReplyOnTop && nReplyWithExtraLines) {
        for (; nReplyWithExtraLines > 0; nReplyWithExtraLines--)
		;
#ifdef RICHIE
          EDT_PasteQuote(msd->context, "<BR>");
#endif
      }

  }
#ifdef RICHIE
  EDT_PasteQuoteEnd(msd->context);
#endif
}

static void
mime_insert_html_abort(nsMIMESession *stream, int status)
{	
  mime_insert_html_complete(stream);
}


static int
mime_insert_html_convert_charset (const char *input_line, PRInt32 input_length,
                                  const char *input_charset,
                                  const char *output_charset,
                                  char **output_ret, PRInt32 *output_size_ret,
                                  void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  int status;
  INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(msd->context);
  PRUint16 old_csid = INTL_GetCSIDocCSID(csi);

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

static int
mime_output_init_fn (const char *type,
                     const char *charset,
                     const char *name,
                     const char *x_mac_type,
                     const char *x_mac_creator,
                     void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  
  // Now, all of this stream creation is done outside of libmime, so this
  // is just a check of the pluginObj member and returning accordingly.
  if (msd->pluginObj) 
    return 0;
  else
    return -1;

  /* If we've converted to HTML, then we've already done charset conversion,
     so label this data as "internal/parser" to prevent it from being passed
     through the charset converters again. */
#ifdef RICHIE
  if (msd->options->write_html_p &&
    !PL_strcasecmp(type, TEXT_HTML))
    type = INTERNAL_PARSER;
#endif
}

static void   *mime_image_begin(const char *image_url, const char *content_type,
                              void *stream_closure);
static void   mime_image_end(void *image_closure, int status);
static char   *mime_image_make_image_html(void *image_data);
static int    mime_image_write_buffer(char *buf, PRInt32 size, void *image_closure);

/* Interface between libmime and inline display of images: the abomination
   that is known as "internal-external-reconnect".
 */
struct mime_image_stream_data {
  struct mime_stream_data *msd;
  URL_Struct *url_struct;
  nsMIMESession *istream;
};

static void *
mime_image_begin(const char *image_url, const char *content_type,
                 void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  struct mime_image_stream_data *mid;

  mid = PR_NEW(struct mime_image_stream_data);
  if (!mid) return 0;

  memset(mid, 0, sizeof(*mid));
  mid->msd = msd;

  /* Internal-external-reconnect only works when going to the screen.
     In that case, return the mid, but leave it empty (returning 0
     here is interpreted as out-of-memory.)
   */
  if (msd->format_out != FO_NGLAYOUT &&
      msd->format_out != FO_CACHE_AND_NGLAYOUT &&
      msd->format_out != FO_PRINT &&
      msd->format_out != FO_CACHE_AND_PRINT &&
      msd->format_out != FO_SAVE_AS_POSTSCRIPT &&
      msd->format_out != FO_CACHE_AND_SAVE_AS_POSTSCRIPT)
    return mid;

  if ( (msd->context) && (!msd->context->img_cx) )
      /* If there is no image context, e.g. if this is a Text context or a
         Mail context on the Mac, then we won't be loading images in the
         image viewer. */
      return mid;

  mid->url_struct = NET_CreateURLStruct (image_url, NET_DONT_RELOAD);
  if (!mid->url_struct)
    {
      PR_Free(mid);
      return 0;
    }

  mid->url_struct->content_encoding = 0;
  mid->url_struct->content_type = PL_strdup(content_type);
  if (!mid->url_struct->content_type)
    {
      NET_FreeURLStruct (mid->url_struct);
      PR_Free(mid);
      return 0;
    }

  mid->istream = (nsMIMESession *) msd->pluginObj;
  return mid;
}


static void
mime_image_end(void *image_closure, int status)
{
  struct mime_image_stream_data *mid =
    (struct mime_image_stream_data *) image_closure;
  
  PR_ASSERT(mid);
  if (!mid) return;
  if (mid->istream)
  {
    /*
      if (status < 0)
      mid->istream->abort(mid->istream, status);
      else
      mid->istream->complete(mid->istream);
      PR_ASSERT(mid->msd->istream == mid->istream);
      mid->msd->istream = NULL;
      PR_Free (mid->istream);
    */
  }

  if (mid->url_struct)
    NET_FreeURLStruct (mid->url_struct);
  PR_Free(mid);
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
  PR_ASSERT(mid);
  if (!mid) return 0;

  /* Internal-external-reconnect only works when going to the screen. */
  if (!mid->istream)
    return PL_strdup("<P><CENTER><IMG SRC=\"resource:/res/network/gopher-image.gif\" ALT=\"[Image]\"></CENTER><P>");

  url = ((mid->url_struct && mid->url_struct->address)
         ? mid->url_struct->address
         : "");
  buf = (char *) PR_MALLOC (PL_strlen (prefix) + PL_strlen (suffix) +
                           PL_strlen (url) + 20);
  if (!buf) return 0;
  *buf = 0;

  PL_strcat (buf, prefix);
  PL_strcat (buf, url);
  PL_strcat (buf, suffix);
  return buf;
}


static int
mime_image_write_buffer(char *buf, PRInt32 size, void *image_closure)
{
  struct mime_image_stream_data *mid =
                (struct mime_image_stream_data *) image_closure;
  struct mime_stream_data *msd = mid->msd;

  if ( (!msd->pluginObj) || (!msd->output_emitter) )
    return -1;

  //
  // If we get here, we are just eating the data this time around
  // and the returned URL will deal with writing the data to the viewer.
  // Just return the size value to the caller.
  //
  return size;
}

// 
// Utility for finding HTML part.
//
static MimeObject*
mime_find_text_html_part_1(MimeObject* obj)
{
  if (mime_subclass_p(obj->clazz,
                      (MimeObjectClass*) &mimeInlineTextHTMLClass)) {
    return obj;
  }
  if (mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeContainerClass)) {
    MimeContainer* cobj = (MimeContainer*) obj;
    PRInt32 i;
    for (i=0 ; i<cobj->nchildren ; i++) {
      MimeObject* result = mime_find_text_html_part_1(cobj->children[i]);
      if (result) return result;
    }
  }
  return NULL;
}

/* Finds the main object of the message -- generally a multipart/mixed,
   text/plain, or text/html. */
static MimeObject*
mime_get_main_object(MimeObject* obj)
{
  MimeContainer* cobj;
  if (!(mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeMessageClass))) {
    return obj;
  }
  cobj = (MimeContainer*) obj;
  if (cobj->nchildren != 1) return obj;
  obj = cobj->children[0];
  for (;;) {
#ifdef MOZ_SECURITY
    HG99001
#else
    if (!mime_subclass_p(obj->clazz,
                         (MimeObjectClass*) &mimeMultipartSignedClass)) {
#endif /* MOZ_SECURITY */
      return obj;
    }
  /* Our main thing is a signed or xlated object.
     We don't care about that; go on inside to the thing that we signed or
     xlated. */
    cobj = (MimeContainer*) obj;
    if (cobj->nchildren != 1) return obj;
    obj = cobj->children[0];
  }
}

int
MimeGetAttachmentCount(MWContext* context)
{
  MimeObject* obj;
  MimeContainer* cobj;
  PRBool isMsgBody = PR_FALSE, isAlternativeOrRelated = PR_FALSE;

  PR_ASSERT(context);
  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object) {
    return 0;
  }
  obj = mime_get_main_object(context->mime_data->last_parsed_object);
  if (!mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeContainerClass))
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

#ifdef MOZ_SECURITY
HG56025
#endif

/* Guessing the filename to use in "Save As", given a URL which may point
   to a MIME part that we've recently displayed.  (Kloooooge!)
 */
#ifdef HAVE_MIME_DATA_SLOT

//
// Leaving this here for now for possible code reuse!
//
int
MimeGetAttachmentList(MWContext* context, MSG_AttachmentData** data)
{
  MimeObject* obj;
  MimeContainer* cobj;
  MSG_AttachmentData* tmp;
  PRInt32 n;
  PRInt32 i;
  char* disp;
  char c;
  PRBool isMsgBody = PR_FALSE, isAlternativeOrRelated = PR_FALSE;

  if (!data) return 0;
  *data = NULL;
  PR_ASSERT(context);
  if (!context ||
      !context->mime_data ||
      !context->mime_data->last_parsed_object) {
    return 0;
  }
  obj = mime_get_main_object(context->mime_data->last_parsed_object);
  if (!mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeContainerClass)) {
    return 0;
  }
  isMsgBody = MimeObjectChildIsMessageBody(obj,
										   &isAlternativeOrRelated);
  if (isAlternativeOrRelated)
	  return 0;

  cobj = (MimeContainer*) obj;
  n = cobj->nchildren;          /* This is often too big, but that's OK. */
  if (n <= 0) return n;
  *data = (MSG_AttachmentData *) XP_CALLOC(n + 1, sizeof(MSG_AttachmentData));
  if (!*data) return MK_OUT_OF_MEMORY;
  tmp = *data;
  c = '?';
  if (PL_strchr(context->mime_data->last_parsed_url, '?')) {
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
		tmp->url = mime_set_url_part(context->mime_data->last_parsed_url, part, PR_TRUE);
	}
	/*
	tmp->url = PR_smprintf("%s%cpart=%s", context->mime_data->last_parsed_url,
                           c, part);
	*/
    if (!tmp->url) return MK_OUT_OF_MEMORY;
    tmp->real_type = child->content_type ?
      PL_strdup(child->content_type) : NULL;
    tmp->real_encoding = child->encoding ? PL_strdup(child->encoding) : NULL;
    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_DISPOSITION,
                           PR_FALSE, PR_FALSE);
    if (disp) {
      tmp->real_name = MimeHeaders_get_parameter(disp, "filename", NULL, NULL);
	  if (tmp->real_name)
	  {
		char *fname = NULL;
		fname = mime_decode_filename(tmp->real_name);
		if (fname && fname != tmp->real_name)
		{
			PR_Free(tmp->real_name);
			tmp->real_name = fname;
		}
	  }
      PR_Free(disp);
    }
    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_TYPE,
                       PR_FALSE, PR_FALSE);
    if (disp)
    {
      tmp->x_mac_type   = MimeHeaders_get_parameter(disp, PARAM_X_MAC_TYPE, NULL, NULL);
      tmp->x_mac_creator= MimeHeaders_get_parameter(disp, PARAM_X_MAC_CREATOR, NULL, NULL);
	  if (!tmp->real_name || *tmp->real_name == 0)
	  {
		PR_FREEIF(tmp->real_name);
		tmp->real_name = MimeHeaders_get_parameter(disp, "name", NULL, NULL);
		if (tmp->real_name)
		{
			char *fname = NULL;
			fname = mime_decode_filename(tmp->real_name);
			if (fname && fname != tmp->real_name)
			{
				PR_Free(tmp->real_name);
				tmp->real_name = fname;
			}
		}
	  }
      PR_Free(disp);
    }
    tmp->description = MimeHeaders_get(child->headers,
                                       HEADER_CONTENT_DESCRIPTION,
                                       PR_FALSE, PR_FALSE);
    if (tmp->real_type && !PL_strcasecmp(tmp->real_type, MESSAGE_RFC822) &&
        (!tmp->real_name || *tmp->real_name == 0))
    {
        StrAllocCopy(tmp->real_name, XP_GetString(XP_FORWARDED_MESSAGE_ATTACHMENT));
    }
  }
  return 0;
}

#endif /* !HAVE_MIME_DATA_SLOT */


PRBool MimeObjectChildIsMessageBody(MimeObject *obj, 
									 PRBool *isAlternativeOrRelated)
{
	char *disp = 0;
	PRBool bRet = PR_FALSE;
	MimeObject *firstChild = 0;
	MimeContainer *container = (MimeContainer*) obj;

	if (isAlternativeOrRelated)
		*isAlternativeOrRelated = PR_FALSE;

	if (!container ||
		!mime_subclass_p(obj->clazz, 
						 (MimeObjectClass*) &mimeContainerClass))
	{
		return bRet;
	}
	else if (mime_subclass_p(obj->clazz, (MimeObjectClass*)
							 &mimeMultipartRelatedClass)) 
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = PR_TRUE;
		return bRet;
	}
	else if (mime_subclass_p(obj->clazz, (MimeObjectClass*)
							 &mimeMultipartAlternativeClass))
	{
		if (isAlternativeOrRelated)
			*isAlternativeOrRelated = PR_TRUE;
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
							PR_TRUE,
							PR_FALSE);
	if (disp /* && !PL_strcasecmp (disp, "attachment") */)
		bRet = PR_FALSE;
	else if (!PL_strcasecmp (firstChild->content_type, TEXT_PLAIN) ||
			 !PL_strcasecmp (firstChild->content_type, TEXT_HTML) ||
			 !PL_strcasecmp (firstChild->content_type, TEXT_MDL) ||
			 !PL_strcasecmp (firstChild->content_type, MULTIPART_ALTERNATIVE) ||
			 !PL_strcasecmp (firstChild->content_type, MULTIPART_RELATED) ||
			 !PL_strcasecmp (firstChild->content_type, MESSAGE_NEWS) ||
			 !PL_strcasecmp (firstChild->content_type, MESSAGE_RFC822))
		bRet = PR_TRUE;
	else
		bRet = PR_FALSE;
	PR_FREEIF(disp);
	return bRet;
}

#ifdef MOZ_SECURITY
HG99007
#endif

extern int MIME_HasAttachments(MWContext *context)
{
	return (context->mime_data && context->mime_data->last_parsed_object->showAttachmentIcon);
}

/* RICHIE
 * HOW WILL THIS BE HANDLED GOING FORWARD????
 */
PRBool 
ValidateDocData(MWContext *window_id) 
{ 
  printf("ValidateDocData not implemented, stubbed in webshell/tests/viewer/nsStubs.cpp\n"); 
  return PR_TRUE; 
}

/**************************************************************
 **************************************************************
 **************************************************************
 **************************************************************
                 NEW WORK FOR STREAM CONVERSION!
 **************************************************************
 **************************************************************
 **************************************************************
 **************************************************************
 **************************************************************/

nsIMimeEmitter *
GetMimeEmitter(MimeDisplayOptions *opt)
{
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  if (!msd) 
    return NULL;

  nsIMimeEmitter     *ptr = (nsIMimeEmitter *)(msd->output_emitter);
  return ptr;
}

/* Get the connnection to prefs service manager */
nsIPref *
GetPrefServiceManager(MimeDisplayOptions *opt)
{
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  if (!msd) 
    return NULL;

  nsIPref     *prefs = (nsIPref *)(msd->prefs);
  return prefs;
}

////////////////////////////////////////////////////////////////
// Bridge routines for new stream converter XP-COM interface 
////////////////////////////////////////////////////////////////
void
mime_bridge_destroy_stream(void *newStream)
{
  mime_stream_data  *msd = (mime_stream_data *)((nsMIMESession *)newStream)->data_object;
  nsMIMESession     *stream = (nsMIMESession *)newStream;
  if (!stream)
    return;
  
  PR_FREEIF(stream);
}

void  *
mime_bridge_create_stream(MimePluginInstance  *newPluginObj, 
                          nsIMimeEmitter      *newEmitter,
                          const char          *urlString,
                          int                 format_out)
{
  int                       status = 0;
  MimeObject                *obj;
  struct mime_stream_data   *msd;
  nsMIMESession             *stream = 0;
  
  /***
  * SHERRY - MAKE THESE GO AWAY!
  ***/
  MWContext   *context = NULL;
  /***
  * SHERRY - MAKE THESE GO AWAY!
  ***/
  
  if (format_out == FO_MAIL_MESSAGE_TO || format_out == FO_CACHE_AND_MAIL_MESSAGE_TO)
  {
    /* Bad news -- this would cause an endless loop. */
    PR_ASSERT(0);
    return NULL;
  }
  
  msd = PR_NEWZAP(struct mime_stream_data);
  if (!msd) 
    return NULL;
  
  msd->url = PR_NEWZAP( URL_Struct );
  if (!msd->url)
  {
    PR_FREEIF(msd);
    return NULL;
  }

  nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&(msd->prefs));
  if (! (msd->prefs && NS_SUCCEEDED(rv)))
	{
    PR_FREEIF(msd);
    return NULL;
  }

  msd->prefs->Startup(MIME_PREFS_FILE);

  // Assign the new mime emitter - will handle output operations
  msd->output_emitter = newEmitter;
  
  (msd->url)->address = PL_strdup(urlString);
  if (!((msd->url)->address))
  {
    delete msd->output_emitter;
    PR_FREEIF(msd->url);
    PR_FREEIF(msd);
    return NULL;
  }
  
  msd->context = context;           // SHERRY - need to wax this soon
  msd->format_out = format_out;     // output format
  msd->pluginObj = newPluginObj;    // the plugin object pointer 
  
  msd->options = PR_NEW(MimeDisplayOptions);
  if (!msd->options)
  {
    PR_Free(msd);
    return 0;
  }
  memset(msd->options, 0, sizeof(*msd->options));

  /* handle the case where extracting attachments from nested messages */
#ifdef RICHIE
  if (url->content_modified != IMAP_CONTENT_NOT_MODIFIED)
    msd->options->missing_parts = PR_TRUE;
#endif /* RICHIE */
  
#ifdef RICHIE
    /*	fe_data now seems to hold information that is relative to the 
    XP-COM information		
  */
  if ((format_out == FO_NGLAYOUT || format_out == FO_CACHE_AND_NGLAYOUT) &&
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
    PR_Free (opt2);
    url->fe_data = 0;
    msd->options->attachment_icon_layer_id = 0; /* Sigh... */
  }
#endif
  
  /* Set the defaults, based on the context, and the output-type.
  */
  if (format_out == FO_NGLAYOUT ||
      format_out == FO_CACHE_AND_NGLAYOUT ||
      format_out == FO_PRINT ||
      format_out == FO_CACHE_AND_PRINT ||
      format_out == FO_SAVE_AS_POSTSCRIPT ||
      format_out == FO_CACHE_AND_SAVE_AS_POSTSCRIPT)
    msd->options->fancy_headers_p = PR_TRUE;
  
  if (format_out == FO_NGLAYOUT ||
      format_out == FO_CACHE_AND_NGLAYOUT)
    msd->options->output_vcard_buttons_p = PR_TRUE;
  
  if (format_out == FO_NGLAYOUT ||
      format_out == FO_CACHE_AND_NGLAYOUT) 
  {
    msd->options->fancy_links_p = PR_TRUE;
  }
  
  msd->options->headers = MimeHeadersAll;
  
  // Get the libmime prefs...
  MIME_NoInlineAttachments = PR_TRUE;
  msd->prefs->GetBoolPref("mail.inline_attachments", &MIME_NoInlineAttachments);
  MIME_NoInlineAttachments = !MIME_NoInlineAttachments;

  /* This pref is written down in with the
  opposite sense of what we like to use... */
  MIME_WrapLongLines = PR_FALSE;
  msd->prefs->GetBoolPref("mail.wrap_long_lines", &MIME_WrapLongLines);

  MIME_VariableWidthPlaintext = PR_TRUE;
  msd->prefs->GetBoolPref("mail.fixed_width_messages", &MIME_VariableWidthPlaintext);
  MIME_VariableWidthPlaintext = !MIME_VariableWidthPlaintext;

  msd->options->no_inline_p = MIME_NoInlineAttachments;
  msd->options->wrap_long_lines_p = MIME_WrapLongLines;
  msd->options->headers = MIME_HeaderType;
  
  // We need to have the URL to be able to support the various 
  // arguments
  status = mime_parse_url_options(msd->url->address, msd->options);
  if (status < 0)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    return 0;
  }
  
  if (msd->options->headers == MimeHeadersMicro &&
    (msd->url->address == NULL || (PL_strncmp(msd->url->address, "news:", 5) != 0 &&
    PL_strncmp(msd->url->address, "snews:", 6) != 0))
    )
    msd->options->headers = MimeHeadersMicroPlus;
  
  if (format_out == FO_QUOTE_MESSAGE ||
    format_out == FO_CACHE_AND_QUOTE_MESSAGE
    || format_out == FO_QUOTE_HTML_MESSAGE
    )
  {
    msd->options->headers = MimeHeadersCitation;
    msd->options->fancy_headers_p = PR_FALSE;
    if (format_out == FO_QUOTE_HTML_MESSAGE) {
      msd->options->nice_html_only_p = PR_TRUE;
    }
  }
  
  else if (msd->options->headers == MimeHeadersSome &&
    (format_out == FO_PRINT ||
    format_out == FO_CACHE_AND_PRINT ||
    format_out == FO_SAVE_AS_POSTSCRIPT ||
    format_out == FO_CACHE_AND_SAVE_AS_POSTSCRIPT ||
    format_out == FO_SAVE_AS_TEXT ||
    format_out == FO_CACHE_AND_SAVE_AS_TEXT))
    msd->options->headers = MimeHeadersSomeNoRef;
  
    /* If we're attaching a message (for forwarding) then we must eradicate all
    traces of xlateion from it, since forwarding someone else a message
    that wasn't xlated for them doesn't work.  We have to dexlate it
    before sending it.
  */
  if ((format_out == FO_MAIL_TO || format_out == FO_CACHE_AND_MAIL_TO) &&
    msd->options->write_html_p == PR_FALSE)
    msd->options->dexlate_p = PR_TRUE;
  
  msd->options->url                   = msd->url->address;
  msd->options->write_html_p          = PR_TRUE;
  msd->options->output_init_fn        = mime_output_init_fn;
  
#ifdef XP_MAC
  /* If it's a thread context, don't output all the mime stuff (hangs on Macintosh for
  ** unexpanded threadpane, because HTML is generated that needs images and layers).
  */
  if (context->type == MWContextMail)
    msd->options->output_fn           = compose_only_output_fn;
  else
#endif /* XP_MAC */
    msd->options->output_fn           = mime_output_fn;
  
  msd->options->set_html_state_fn     = mime_set_html_state_fn;
  if (format_out == FO_QUOTE_HTML_MESSAGE) {
    msd->options->charset_conversion_fn = mime_insert_html_convert_charset;
    msd->options->dont_touch_citations_p = PR_TRUE;
  } else 
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
  
  msd->options->variable_width_plaintext_p = MIME_VariableWidthPlaintext;
  
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
    msd->options->write_html_p = PR_FALSE;
  
  //  PR_ASSERT(!msd->stream);
  
  obj = mime_new ((MimeObjectClass *)&mimeMessageClass,
    (MimeHeaders *) NULL,
    MESSAGE_RFC822);
  if (!obj)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    return 0;
  }
  obj->options = msd->options;
  msd->obj = obj;
  
  /* Both of these better not be true at the same time. */
  PR_ASSERT(! (obj->options->dexlate_p && obj->options->write_html_p));
  
  stream = PR_NEW (nsMIMESession);
  if (!stream)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    PR_Free(obj);
    return 0;
  }
  memset (stream, 0, sizeof (*stream));
  
  stream->name           = "MIME Conversion Stream";
  stream->complete       = mime_display_stream_complete;
  stream->abort          = mime_display_stream_abort;
  stream->put_block      = mime_display_stream_write;
  stream->data_object    = msd;
  stream->window_id      = context;
  
  status = obj->clazz->initialize(obj);
  if (status >= 0)
    status = obj->clazz->parse_begin(obj);
  if (status < 0)
  {
    PR_Free(stream);
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    PR_Free(obj);
    return 0;
  }
  
  return stream;
}
