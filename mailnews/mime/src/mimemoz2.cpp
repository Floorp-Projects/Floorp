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
#include "nsCOMPtr.h"
#include "modlmime.h"
#include "libi18n.h"
#include "nsCRT.h"
#include "msgcom.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimetric.h"   /* for MIME_RichtextConverter */
#include "mimethtm.h"
#include "mimemsig.h"
#include "mimemrel.h"
#include "mimemalt.h"
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
#include "nsFileSpec.h"
#include "nsMimeTransition.h"
#include "comi18n.h"
#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsMimeStringResources.h"
#include "nsStreamConverter.h"
#include "nsIMsgSend.h"

#include "nsIIOService.h"
#include "nsIURI.h"

// Define CIDs...
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

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

/* Arrgh.  Why isn't this in a reasonable header file somewhere???  ###tw */
// RICHIE SHERRYextern char * NET_ExplainErrorDetails (int code, ...);

extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset);


/* Interface between netlib and the top-level message/rfc822 parser:
   MIME_MessageConverter()
 */
static MimeHeadersState MIME_HeaderType;
static PRBool MIME_NoInlineAttachments;
static PRBool MIME_WrapLongLines;
static PRBool MIME_VariableWidthPlaintext;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Attachment handling routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
MimeObject    *mime_get_main_object(MimeObject* obj);

extern "C" nsresult
MimeGetAttachmentList(MimeObject *tobj, const char *aMessageURL, nsMsgAttachmentData **data)
{
  extern nsresult       nsMimeNewURI(nsIURI** aInstancePtrResult, const char *aSpec);
  MimeContainer         *cobj;
  nsMsgAttachmentData   *tmp;
  PRInt32               n;
  PRInt32               i;
  char                  *disp;
  PRBool                isMsgBody = PR_FALSE, isAlternativeOrRelated = PR_FALSE;
  MimeObject            *obj;

  if (!data) 
    return 0;
  *data = NULL;

  obj = mime_get_main_object(tobj);
  if ( (obj) && (!mime_subclass_p(obj->clazz, (MimeObjectClass*) &mimeContainerClass)) )
    return 0;

  isMsgBody = MimeObjectChildIsMessageBody(obj, &isAlternativeOrRelated);
  if (isAlternativeOrRelated)
    return 0;
  
  cobj = (MimeContainer*) obj;
  n = cobj->nchildren;          /* This is often too big, but that's OK. */
  if (n <= 0) 
    return n;

  *data = (nsMsgAttachmentData *)PR_Malloc( (n + 1) * sizeof(MSG_AttachmentData));
  if (!*data) 
    return NS_ERROR_OUT_OF_MEMORY;

  nsCRT::memset(*data, 0, (n + 1) * sizeof(MSG_AttachmentData));

  tmp = *data;
  
  // let's figure out where to start..
  if (isMsgBody)
    i = 1;
  else
    i = 0;
  
  for (; i<cobj->nchildren ; i++, tmp++) 
  {
    MimeObject    *child = cobj->children[i];
    char          *part = mime_part_address(child);
    char          *imappart = NULL;

    if (!part) 
      return NS_ERROR_OUT_OF_MEMORY;

    if (obj->options->missing_parts)
      imappart = mime_imap_part_address(child);

    char *urlSpec = nsnull;
    if (imappart)
    {
      urlSpec = mime_set_url_imap_part(aMessageURL, imappart, part);
    }
    else
    {
      urlSpec = mime_set_url_part(aMessageURL, part, PR_TRUE);
    }
  
    if (!urlSpec)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = nsMimeNewURI(&(tmp->url), urlSpec);

    if ( (NS_FAILED(rv)) || (!tmp->url) )
      return NS_ERROR_OUT_OF_MEMORY;

    tmp->real_type = child->content_type ? PL_strdup(child->content_type) : NULL;
    tmp->real_encoding = child->encoding ? PL_strdup(child->encoding) : NULL;
    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);

    if (disp) 
    {
      tmp->real_name = MimeHeaders_get_parameter(disp, "filename", NULL, NULL);
      if (tmp->real_name)
      {
        char *fname = NULL;
        fname = mime_decode_filename(tmp->real_name);
        if (fname && fname != tmp->real_name)
        {
          PR_FREEIF(tmp->real_name);
          tmp->real_name = fname;
        }
      }

      PR_FREEIF(disp);
    }

    disp = MimeHeaders_get(child->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
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
      PR_FREEIF(disp);
    }

    tmp->description = MimeHeaders_get(child->headers, HEADER_CONTENT_DESCRIPTION, PR_FALSE, PR_FALSE);
    if (tmp->real_type && !PL_strcasecmp(tmp->real_type, MESSAGE_RFC822) && 
       (!tmp->real_name || *tmp->real_name == 0))
    {
      mime_SACopy(&(tmp->real_name), "forward.msg");
    }
  }

  return NS_OK;
}

extern "C" void
MimeFreeAttachmentList(nsMsgAttachmentData *data)
{
  if (data) 
  {
    nsMsgAttachmentData   *tmp;
    for (tmp = data ; tmp->url ; tmp++) 
    {
      /* Can't do PR_FREEIF on `const' values... */
      NS_IF_RELEASE(tmp->url);
      if (tmp->real_type) PR_Free((char *) tmp->real_type);
      if (tmp->real_encoding) PR_Free((char *) tmp->real_encoding);
      if (tmp->real_name) PR_Free((char *) tmp->real_name);
      if (tmp->x_mac_type) PR_Free((char *) tmp->x_mac_type);
      if (tmp->x_mac_creator) PR_Free((char *) tmp->x_mac_creator);
      if (tmp->description) PR_Free((char *) tmp->description);
      tmp->url = 0;
      tmp->real_type = 0;
      tmp->real_name = 0;
      tmp->description = 0;
    }
    PR_Free(data);
  }
}

extern "C" void
NotifyEmittersOfAttachmentList(MimeDisplayOptions     *opt,
                               nsMsgAttachmentData    *data)
{
  PRInt32     i = 0;
  struct      nsMsgAttachmentData  *tmp = data;

  // RICHIE - this will go away once we cutover...
  nsIPref *pref = GetPrefServiceManager(opt);   // Pref service manager
  PRBool      mimeXULOutput = PR_TRUE;
  
  if (pref)
    pref->GetBoolPref("mail.mime_xul_output", &mimeXULOutput);
  if (!mimeXULOutput)
    return;

  if ( (!tmp) || (!tmp->real_name) )
    return;

  while ( (tmp) && (tmp->real_name) )
  {
    char  *spec;

    spec = nsnull;
    if ( tmp->url ) 
      tmp->url->GetSpec(&spec);

    mimeEmitterStartAttachment(opt, tmp->real_name, tmp->real_type, spec);
    mimeEmitterAddAttachmentField(opt, HEADER_X_MOZILLA_PART_URL, spec);

    /* rhp - for now, just leave these here, but they are really
             not necessary
    printf("URL for Part      : %s\n", spec);
    printf("Real Name         : %s\n", tmp->real_name);
	  printf("Desired Type      : %s\n", tmp->desired_type);
    printf("Real Type         : %s\n", tmp->real_type);
	  printf("Real Encoding     : %s\n", tmp->real_encoding); 
    printf("Description       : %s\n", tmp->description);
    printf("Mac Type          : %s\n", tmp->x_mac_type);
    printf("Mac Creator       : %s\n", tmp->x_mac_creator);
    */

    mimeEmitterEndAttachment(opt);
    ++i;
    ++tmp;
  }
}

extern "C" char *
GetOSTempFile(const char *name)
{
char  *retName; 

  nsFileSpec  *fs = new nsFileSpec();
  if (!fs)
    return NULL;
  
  fs->MakeUnique(name);
  retName = (char *)PL_strdup(fs->GetCString());
  delete fs;
  return retName;
}
  
static char *
mime_reformat_date(const char *date, void *stream_closure)
{
  /*  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure; */
  return PL_strdup(date);
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
mime_convert_charset (const PRBool input_autodetect, const char *input_line, PRInt32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, PRInt32 *output_size_ret,
                      void *stream_closure)
{
  /*  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure; */

  // Now do conversion to UTF-8 for output
  char  *convertedString = NULL;
  PRInt32 convertedStringLen;
  PRInt32 res = MIME_ConvertCharset(input_autodetect, input_charset, "UTF-8", input_line, input_length, 
                                    &convertedString, &convertedStringLen);
  if (res != 0)
  {
      *output_ret = 0;
      *output_size_ret = 0;
  }
  else
  {
    *output_ret = (char *) convertedString;
    *output_size_ret = convertedStringLen;
  }  

  return 0;
}


static int
mime_convert_rfc1522 (const char *input_line, PRInt32 input_length,
                      const char *input_charset, const char *output_charset,
                      char **output_ret, PRInt32 *output_size_ret,
                      void *stream_closure)
{
  /*  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure; */
  char *converted;
  char *line;
  char charset[128];
  
  charset[0] = '\0';

  if (input_line[input_length] == 0)  /* oh good, it's null-terminated */
    line = (char *) input_line;
  else
    {
      line = (char *) PR_MALLOC(input_length+1);
      if (!line) return MIME_OUT_OF_MEMORY;
      nsCRT::memcpy(line, input_line, input_length);
      line[input_length] = 0;
    }

  converted = MIME_DecodeMimePartIIStr(line, charset);

  if (line != input_line)
    PR_Free(line);

  if (converted)
    {
      char  *convertedString = NULL; 

      PRInt32 res = MIME_ConvertString(charset, "UTF-8", converted, &convertedString); 
      if ( (res != 0) || (!convertedString) )
      {
        *output_ret = converted;
        *output_size_ret = PL_strlen(converted);
      }
      else
      {
        PR_Free(converted); 
        *output_ret = convertedString;
        *output_size_ret = PL_strlen(convertedString);
      }
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
  PRUint32  written = 0;
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  if ( (!msd->pluginObj2) && (!msd->output_emitter) )
    return -1;
  
  // Now, write to the WriteBody method if this is a message body and not
  // a part retrevial
  if (!msd->options->part_to_load)
  {
    if (msd->output_emitter)
    {
      msd->output_emitter->WriteBody(buf, (PRUint32) size, &written);
    }
  }
  else
  {
    if (msd->output_emitter)
    {
      msd->output_emitter->Write(buf, (PRUint32) size, &written);
    }
  }
  return written;
}

#ifdef XP_MAC
static int
compose_only_output_fn(char *buf, PRInt32 size, void *stream_closure)
{
    return 0;
}
#endif

static int
mime_set_html_state_fn (void *stream_closure,
                        PRBool layer_encapsulate_p,
                        PRBool start_p,
                        PRBool abort_p)
{
  int status = 0;

  /*  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure; */  
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
    int       status;
    PRBool    abortNow = PR_FALSE;

    if ((obj->options) && (obj->options->headers == MimeHeadersOnly))
      abortNow = PR_TRUE;

    status = obj->clazz->parse_eof(obj, abortNow);
    obj->clazz->parse_end(obj, (status < 0 ? PR_TRUE : PR_FALSE));
  
    //
    // Ok, now we are going to process the attachment data by getting all
    // of the attachment info and then driving the emitter with this data.
    //
    if (!msd->options->part_to_load)
    {
      extern void  mime_dump_attachments ( nsMsgAttachmentData *attachData );

      nsMsgAttachmentData *attachments;
      nsresult rv = MimeGetAttachmentList(obj, msd->url_name, &attachments);
      if (NS_SUCCEEDED(rv))
      {
        NotifyEmittersOfAttachmentList(msd->options, attachments);
        MimeFreeAttachmentList(attachments);
      }
    }

    // Destroy the object now.
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
    
  // Release the prefs service
  if ( (obj) && (obj->options) && (obj->options->prefs) )
    nsServiceManager::ReleaseService(kPrefCID, obj->options->prefs);

  PR_FREEIF(msd);
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
    
    // Destroy code....
    PR_ASSERT(msd->options == obj->options);
    mime_free(obj);
    if (msd->options)
    {
      PR_FREEIF(msd->options->part_to_load);
      PR_FREEIF(msd->options);
      msd->options = 0;
    }
  }

  PR_FREEIF(msd);
}

static int
mime_insert_html_convert_charset (const PRBool input_autodetect, const char *input_line, 
                                  PRInt32 input_length, const char *input_charset, 
                                  const char *output_charset,
                                  char **output_ret, PRInt32 *output_size_ret,
                                  void *stream_closure)
{
  struct mime_stream_data *msd = (struct mime_stream_data *) stream_closure;
  int                     status;

  // RICHIE SHERRY - I think this should go away, but I don't want to remove it without
  // being around for the I18N folks
  INTL_CharSetInfo csi = nsnull;
  PRUint16         old_csid = INTL_GetCSIDocCSID(csi);

  if (input_charset) 
  {
    msd->lastcsid = INTL_CharSetNameToID((char*) input_charset);
  } 
  else 
  {
    msd->lastcsid = 0;
  }
  if (output_charset) 
  {
    msd->outcsid = INTL_CharSetNameToID((char*) output_charset);
  } 
  else 
  {
    msd->outcsid = 0;
  }
  INTL_SetCSIDocCSID(csi, msd->lastcsid);

  status = mime_convert_charset (input_autodetect, input_line, input_length,
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
  if (!msd->pluginObj2)
    return -1;
  else
    return 0;
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
  char                    *url;
  nsMIMESession           *istream;
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

  mid->url = (char *) PL_strdup(image_url);
  if (!mid->url)
  {
    PR_Free(mid);
    return 0;
  }

  mid->istream = (nsMIMESession *) msd->pluginObj2;
  return mid;
}

static void
mime_image_end(void *image_closure, int status)
{
  struct mime_image_stream_data *mid =
    (struct mime_image_stream_data *) image_closure;
  
  PR_ASSERT(mid);
  if (!mid) 
    return;
  PR_FREEIF(mid->url);
  PR_FREEIF(mid);
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

  static PRInt32  makeUniqueHackID = 1;
  char            makeUniqueHackString[128] = "";
  
  PR_ASSERT(mid);
  if (!mid) return 0;

  PR_snprintf(makeUniqueHackString, sizeof(makeUniqueHackString), "&hackID=%d", makeUniqueHackID++);

  /* Internal-external-reconnect only works when going to the screen. */
  if (!mid->istream)
    return PL_strdup("<P><CENTER><IMG SRC=\"resource:/res/network/gopher-image.gif\" ALT=\"[Image]\"></CENTER><P>");

  if ( (!mid->url) || (!(*mid->url)) )
    url = "";
  else
    url = mid->url;

  buf = (char *) PR_MALLOC (PL_strlen(prefix) + PL_strlen(suffix) +
                           PL_strlen(url) + PL_strlen(makeUniqueHackString) + 20) ;
  if (!buf) 
    return 0;
  *buf = 0;

  PL_strcat (buf, prefix);
  PL_strcat (buf, url);
  PL_strcat (buf, makeUniqueHackString);
  PL_strcat (buf, suffix);
  return buf;
}

static int
mime_image_write_buffer(char *buf, PRInt32 size, void *image_closure)
{
  struct mime_image_stream_data *mid =
                (struct mime_image_stream_data *) image_closure;
  struct mime_stream_data *msd = mid->msd;

  if ( ( (!msd->output_emitter) ) &&
       ( (!msd->pluginObj2)     ) )
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
MimeObject*
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
  return NULL;
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

//
// New Stream Converter Interface
//

// Get the connnection to prefs service manager 
nsIPref *
GetPrefServiceManager(MimeDisplayOptions *opt)
{
  if (!opt) 
    return nsnull;

  return opt->prefs;
}

////////////////////////////////////////////////////////////////
// Bridge routines for new stream converter XP-COM interface 
////////////////////////////////////////////////////////////////
extern "C" void  *
mime_bridge_create_display_stream(
                          nsIMimeEmitter      *newEmitter,
                          nsStreamConverter   *newPluginObj2,
                          nsIURI              *uri,
                          nsMimeOutputType    format_out)
{
  int                       status = 0;
  MimeObject                *obj;
  struct mime_stream_data   *msd;
  nsMIMESession             *stream = 0;
  
  if (!uri)
    return nsnull;

  msd = PR_NEWZAP(struct mime_stream_data);
  if (!msd) 
    return NULL;

  // Assign the new mime emitter - will handle output operations
  msd->output_emitter = newEmitter;

  // Store the URL string for this decode operation
  char *urlString;
  if (NS_SUCCEEDED(uri->GetSpec(&urlString)))
  {
    if ((urlString) && (*urlString))
    {
      msd->url_name = PL_strdup(urlString);
      if (!(msd->url_name))
      {
        // RICHIE_URL PR_FREEIF(msd->url);
        PR_FREEIF(msd);
        return NULL;
      }

      PR_FREEIF(urlString);
    }
  }
  
  msd->format_out = format_out;       // output format
  msd->pluginObj2 = newPluginObj2;    // the plugin object pointer 
  
  msd->options = PR_NEW(MimeDisplayOptions);
  if (!msd->options)
  {
    PR_Free(msd);
    return 0;
  }
  memset(msd->options, 0, sizeof(*msd->options));
  msd->options->format_out = format_out;     // output format

  nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&(msd->options->prefs));
  if (! (msd->options->prefs && NS_SUCCEEDED(rv)))
	{
    PR_FREEIF(msd);
    return nsnull;
  }

  //
  // Set the defaults, based on the context, and the output-type.
  //
  MIME_HeaderType = MimeHeadersAll;
  switch (format_out) 
  {
		case nsMimeOutput::nsMimeMessageSplitDisplay:   // the wrapper HTML output to produce the split header/body display
		case nsMimeOutput::nsMimeMessageHeaderDisplay:  // the split header/body display
		case nsMimeOutput::nsMimeMessageBodyDisplay:    // the split header/body display
    case nsMimeOutput::nsMimeMessageXULDisplay:
      msd->options->fancy_headers_p = PR_TRUE;
      msd->options->output_vcard_buttons_p = PR_TRUE;
      msd->options->fancy_links_p = PR_TRUE;
      break;

	case nsMimeOutput::nsMimeMessageQuoting:        // all HTML quoted output
      msd->options->fancy_headers_p = PR_TRUE;
      msd->options->fancy_links_p = PR_TRUE;
      break;

	case nsMimeOutput::nsMimeMessageBodyQuoting:        // only HTML body quoted output
      MIME_HeaderType = MimeHeadersNone;
      break;

    case nsMimeOutput::nsMimeMessageRaw:              // the raw RFC822 data (view source) and attachments
		case nsMimeOutput::nsMimeMessageDraftOrTemplate:  // Loading drafts & templates
		case nsMimeOutput::nsMimeMessageEditorTemplate:   // Loading templates into editor
      break;
  }

  ////////////////////////////////////////////////////////////
  // Now, get the libmime prefs...
  ////////////////////////////////////////////////////////////
//  msd->options->headers = MimeHeadersAll;
  
  MIME_NoInlineAttachments = PR_TRUE;   // false - display as links 
                                        // true - display attachment

  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.inline_attachments", &MIME_NoInlineAttachments);
  MIME_NoInlineAttachments = !MIME_NoInlineAttachments;

  /* This pref is written down in with the
  opposite sense of what we like to use... */
  MIME_WrapLongLines = PR_TRUE;
  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.wrap_long_lines", &MIME_WrapLongLines);

  MIME_VariableWidthPlaintext = PR_TRUE;
  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.fixed_width_messages", &MIME_VariableWidthPlaintext);
  MIME_VariableWidthPlaintext = !MIME_VariableWidthPlaintext;

  msd->options->no_inline_p = MIME_NoInlineAttachments;
  msd->options->wrap_long_lines_p = MIME_WrapLongLines;
  msd->options->headers = MIME_HeaderType;
  
  // We need to have the URL to be able to support the various 
  // arguments
  status = mime_parse_url_options(msd->url_name, msd->options);
  if (status < 0)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    return 0;
  }
 
  if (msd->options->headers == MimeHeadersMicro &&
     (msd->url_name == NULL || (PL_strncmp(msd->url_name, "news:", 5) != 0 &&
              PL_strncmp(msd->url_name, "snews:", 6) != 0)) )
    msd->options->headers = MimeHeadersMicroPlus;

  msd->options->url = msd->url_name;
  msd->options->write_html_p          = PR_TRUE;
  msd->options->output_init_fn        = mime_output_init_fn;
  
  msd->options->output_fn             = mime_output_fn;
  msd->options->set_html_state_fn     = mime_set_html_state_fn;

  //
  // For quoting, don't mess with citatation...
  if (format_out == nsMimeOutput::nsMimeMessageQuoting || format_out == nsMimeOutput::nsMimeMessageBodyQuoting)
  {
    msd->options->charset_conversion_fn = mime_insert_html_convert_charset;
    msd->options->dont_touch_citations_p = PR_TRUE;
  }

  msd->options->charset_conversion_fn = mime_convert_charset;
  msd->options->rfc1522_conversion_fn = mime_convert_rfc1522;
  msd->options->reformat_date_fn      = mime_reformat_date;
  msd->options->file_type_fn          = mime_file_type;
  msd->options->type_description_fn   = mime_type_desc;
  msd->options->type_icon_name_fn     = mime_type_icon;
  msd->options->stream_closure        = msd;
  msd->options->passwd_prompt_fn      = 0;
  
  msd->options->image_begin           = mime_image_begin;
  msd->options->image_end             = mime_image_end;
  msd->options->make_image_html       = mime_image_make_image_html;
  msd->options->image_write_buffer    = mime_image_write_buffer;
  
  msd->options->variable_width_plaintext_p = MIME_VariableWidthPlaintext;

  // 
  // Charset overrides takes place here
  //
  // We have a bool pref (mail.force_user_charset) to deal with attachments.
  // 1) If true - libmime does NO conversion and just passes it through to raptor
  // 2) If false, then we try to use the charset of the part and if not available, 
  //    the charset of the root message 
  //
  msd->options->force_user_charset = PR_FALSE;

  if (msd->options->prefs)
    msd->options->prefs->GetBoolPref("mail.force_user_charset", &(msd->options->force_user_charset));
  if (msd->options->force_user_charset)
  {
    /* rhp:
        For now, we are not going to do this, but I am leaving the code here just in case
        we do want a pref charset override capability.
    char    charset[256];
    int     length = sizeof(charset);

    msd->prefs->GetCharPref("mail.charset", charset, &length); 
    msd->options->override_charset = PL_strdup(charset);
    ****/
  }

  // If this is a part, then we should emit the HTML to render the data
  // (i.e. embedded images)
  if (msd->options->part_to_load)
    msd->options->write_html_p = PR_FALSE;

  obj = mime_new ((MimeObjectClass *)&mimeMessageClass, (MimeHeaders *) NULL, MESSAGE_RFC822);
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
  
  stream = PR_NEW(nsMIMESession);
  if (!stream)
  {
    PR_FREEIF(msd->options->part_to_load);
    PR_Free(msd->options);
    PR_Free(msd);
    PR_Free(obj);
    return 0;
  }
  
  nsCRT::memset (stream, 0, sizeof (*stream));  
  stream->name           = "MIME Conversion Stream";
  stream->complete       = mime_display_stream_complete;
  stream->abort          = mime_display_stream_abort;
  stream->put_block      = mime_display_stream_write;
  stream->data_object    = msd;
  
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

/* This is the next generation string retrieval call */
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

#define MIME_URL "resource:/res/mailnews/messenger/mime.properties"

extern "C" 
char *
MimeGetStringByIDREAL(PRInt32 stringID)
{
  nsresult    res = NS_OK;
  char*       propertyURL;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->CopyCharPref("mail.strings.mime", &propertyURL);

  if (!NS_SUCCEEDED(res) || !prefs)
    propertyURL = MIME_URL;

  nsCOMPtr<nsIURI> pURI;
  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
  {
    if (propertyURL != MIME_URL)
      PR_FREEIF(propertyURL);
    return PL_strdup("???");   // Don't I18N this string...failsafe return value
  }

  res = pService->NewURI(propertyURL, nsnull, getter_AddRefs(pURI));

  NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
  if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
  {
    nsILocale   *locale = nsnull;
    nsIStringBundle* sBundle = nsnull;
    res = sBundleService->CreateBundle(propertyURL, locale, &sBundle);

    if (NS_FAILED(res)) 
    {
      return PL_strdup("???");   // Don't I18N this string...failsafe return value
    }

    nsAutoString v("");
#if 1
    PRUnichar *ptrv = nsnull;
    res = sBundle->GetStringFromID(stringID, &ptrv);
    v = ptrv;
#else
    res = sBundle->GetStringFromID(stringID, v);
#endif
 
    if (NS_FAILED(res)) 
    {
      char    buf[128];

      PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
      return PL_strdup(buf);
    }

    // Here we need to return a new copy of the string
    char      *returnBuffer = NULL;
    PRInt32   bufferLen = v.Length() + 1;

    returnBuffer = (char *)PR_MALLOC(bufferLen);
    if (returnBuffer)
    {
      v.ToCString(returnBuffer, bufferLen);
      return returnBuffer;
    }
  }

  return PL_strdup("???");   // Don't I18N this string...failsafe return value
}

extern "C" 
char *
MimeGetStringByID(PRInt32 stringID)
{
  if (-1000 == stringID) return PL_strdup("Application is out of memory.");
  if (-1001 == stringID) return PL_strdup("Unable to open the temporary file\n.\n%s\nCheck your `Temporary Directory' setting and try again.");
  if (-1002 == stringID) return PL_strdup("Error writing temporary file.");
  if (1000 == stringID) return PL_strdup("Subject");
  if (1001 == stringID) return PL_strdup("Resent-Comments");
  if (1002 == stringID) return PL_strdup("Resent-Date");
  if (1003 == stringID) return PL_strdup("Resent-Sender");
  if (1004 == stringID) return PL_strdup("Resent-From");
  if (1005 == stringID) return PL_strdup("Resent-To");
  if (1006 == stringID) return PL_strdup("Resent-CC");
  if (1007 == stringID) return PL_strdup("Date");
  if (1008 == stringID) return PL_strdup("Sender");
  if (1009 == stringID) return PL_strdup("From");
  if (1010 == stringID) return PL_strdup("Reply-To");
  if (1011 == stringID) return PL_strdup("Organization");
  if (1012 == stringID) return PL_strdup("To");
  if (1013 == stringID) return PL_strdup("CC");
  if (1014 == stringID) return PL_strdup("Newsgroups");
  if (1015 == stringID) return PL_strdup("Followup-To");
  if (1016 == stringID) return PL_strdup("References");
  if (1017 == stringID) return PL_strdup("Name");
  if (1018 == stringID) return PL_strdup("Type");
  if (1019 == stringID) return PL_strdup("Encoding");
  if (1020 == stringID) return PL_strdup("Description");
  if (1021 == stringID) return PL_strdup("Message-ID");
  if (1022 == stringID) return PL_strdup("Resent-Message-ID");
  if (1023 == stringID) return PL_strdup("BCC");
  if (1024 == stringID) return PL_strdup("Download Status");
  if (1025 == stringID) return PL_strdup("Not Downloaded Inline");
  if (1026 == stringID) return PL_strdup("Link to Document");
  if (1027 == stringID) return PL_strdup("<B>Document Info:</B>");
  if (1028 == stringID) return PL_strdup("Attachment");
  if (1029 == stringID) return PL_strdup("forward.msg");
  if (1030 == stringID) return PL_strdup("Add %s to your Address Book");
  if (1031 == stringID) return PL_strdup("<B><FONT COLOR=\042#808080\042>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Internal</FONT></B>");
  if (1032 == stringID) return PL_strdup("In message   wrote:<P>");
  if (1033 == stringID) return PL_strdup(" wrote:<P>");
  if (1034 == stringID) return PL_strdup("(no headers)");
  if (1035 == stringID) return PL_strdup("Toggle Attachment Pane");

  char    buf[128];
  
  PR_snprintf(buf, sizeof(buf), "[StringID %d?]", stringID);
  return PL_strdup(buf);
}


// To support 2 types of emitters...we need these routines :-(
nsIMimeEmitter *
GetMimeEmitter(MimeDisplayOptions *opt)
{
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  if (!msd) 
    return NULL;

  nsIMimeEmitter     *ptr = (nsIMimeEmitter *)(msd->output_emitter);
  return ptr;
}

mime_stream_data *
GetMSD(MimeDisplayOptions *opt)
{
  if (!opt)
    return nsnull;
  mime_stream_data  *msd = (mime_stream_data *)opt->stream_closure;
  return msd;
}

PRBool
NoEmitterProcessing(nsMimeOutputType    format_out)
{
  if ( (format_out == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
       (format_out == nsMimeOutput::nsMimeMessageEditorTemplate) )
    return PR_TRUE;
  else
    return PR_FALSE;
}

extern "C" nsresult
mimeEmitterAddAttachmentField(MimeDisplayOptions *opt, const char *field, const char *value)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->AddAttachmentField(field, value);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterAddHeaderField(MimeDisplayOptions *opt, const char *field, const char *value)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->AddHeaderField(field, value);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartAttachment(MimeDisplayOptions *opt, const char *name, const char *contentType, const char *url)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartAttachment(name, contentType, url);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndAttachment(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    if (emitter)
      return emitter->EndAttachment();
    else
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartBody(MimeDisplayOptions *opt, PRBool bodyOnly, const char *msgID, const char *outCharset)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartBody(bodyOnly, msgID, outCharset);
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndBody(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->EndBody();
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterEndHeader(MimeDisplayOptions *opt)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->EndHeader();
  }

  return NS_ERROR_FAILURE;
}

extern "C" nsresult     
mimeEmitterStartHeader(MimeDisplayOptions *opt, PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                       const char *outCharset)
{
  // Check for draft processing...
  if (NoEmitterProcessing(opt->format_out))
    return NS_OK;

  mime_stream_data  *msd = GetMSD(opt);
  if (!msd) 
    return NS_ERROR_FAILURE;

  if (msd->output_emitter)
  {
    nsIMimeEmitter *emitter = (nsIMimeEmitter *)msd->output_emitter;
    return emitter->StartHeader(rootMailHeader, headerOnly, msgID, outCharset);
  }

  return NS_ERROR_FAILURE;
}


extern "C" nsresult
mimeSetNewURL(nsMIMESession *stream, char *url)
{
  if ( (!stream) || (!url) || (!*url) )
    return NS_ERROR_FAILURE;

  mime_stream_data  *msd = (mime_stream_data *)stream->data_object;
  if (!msd)
    return NS_ERROR_FAILURE;

  char *tmpPtr = PL_strdup(url);
  if (!tmpPtr)
    return NS_ERROR_FAILURE;

  PR_FREEIF(msd->url_name);
  msd->url_name = PL_strdup(tmpPtr);
  return NS_OK;
}





