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

#ifndef _MIMEMOZ_H_
#define _MIMEMOZ_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "prtypes.h"
#include "nsStreamConverter.h"
#include "nsIMimeEmitter.h"
#include "nsIURI.h"

// SHERRY - Need to get these out of here eventually

#ifdef XP_UNIX
#undef Bool
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

  
#include "net.h"
#include "mimei.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "nsIPref.h"

typedef struct _nsMIMESession nsMIMESession;

/* stream functions */
typedef unsigned int
(*MKSessionWriteReadyFunc) (nsMIMESession *stream);

#define MAX_WRITE_READY (((unsigned) (~0) << 1) >> 1)   /* must be <= than MAXINT!!!!! */

typedef int 
(*MKSessionWriteFunc) (nsMIMESession *stream, const char *str, PRInt32 len);

typedef void 
(*MKSessionCompleteFunc) (nsMIMESession *stream);

typedef void 
(*MKSessionAbortFunc) (nsMIMESession *stream, int status);

/* streamclass function */
struct _nsMIMESession {

    char      * name;          /* Just for diagnostics */

    void      * window_id;     /* used for progress messages, etc. */

    void      * data_object;   /* a pointer to whatever
                                * structure you wish to have
                                * passed to the routines below
                                * during writes, etc...
                                * 
                                * this data object should hold
                                * the document, document
                                * structure or a pointer to the
                                * document.
                                */

    MKSessionWriteReadyFunc  is_write_ready;   /* checks to see if the stream is ready
                                               * for writing.  Returns 0 if not ready
                                               * or the number of bytes that it can
                                               * accept for write
                                               */
    MKSessionWriteFunc       put_block;        /* writes a block of data to the stream */
    MKSessionCompleteFunc    complete;         /* normal end */
    MKSessionAbortFunc       abort;            /* abnormal end */

    PRBool                  is_multipart;    /* is the stream part of a multipart sequence */
};

/*
 * This is for the reworked mime parser.
 */
struct mime_stream_data {           /* This struct is the state we pass around
                                       amongst the various stream functions
                                       used by MIME_MessageConverter(). */
  char                *url_name;
  nsMimeOutputType    format_out;
  MWContext           *context;     /* Must REMOVE this entry. */
  void                *pluginObj2;  /* The new XP-COM stream converter object */
  nsMIMESession       *istream;     /* Holdover - new stream we're writing out image data-if any. */
  MimeObject          *obj;         /* The root parser object */
  MimeDisplayOptions  *options;     /* Data for communicating with libmime.a */

  /* These are used by FO_QUOTE_HTML_MESSAGE stuff only: */
  PRInt16             lastcsid;     /* csid corresponding to above. */
  PRInt16             outcsid;      /* csid passed to EDT_PasteQuoteINTL */
  nsIMimeEmitter      *output_emitter;  /* Output emitter engine for libmime */
};

////////////////////////////////////////////////////////////////
// Bridge routines for legacy mime code
////////////////////////////////////////////////////////////////

// Create bridge stream for libmime
extern "C"
void         *mime_bridge_create_display_stream(nsIMimeEmitter      *newEmitter,
                                                nsStreamConverter   *newPluginObj2,
                                                nsIURI              *uri,
                                                nsMimeOutputType    format_out);

// To get the mime emitter...
extern "C" nsIMimeEmitter   *GetMimeEmitter(MimeDisplayOptions *opt);

// To support 2 types of emitters...we need these routines :-(
extern "C" nsresult     mimeSetNewURL(nsMIMESession *stream, char *url);
extern "C" nsresult     mimeEmitterAddAttachmentField(MimeDisplayOptions *opt, const char *field, const char *value); 
extern "C" nsresult     mimeEmitterAddHeaderField(MimeDisplayOptions *opt, const char *field, const char *value);
extern "C" nsresult     mimeEmitterStartAttachment(MimeDisplayOptions *opt, const char *name, const char *contentType, const char *url);
extern "C" nsresult     mimeEmitterEndAttachment(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterStartBody(MimeDisplayOptions *opt, PRBool bodyOnly, const char *msgID, const char *outCharset);
extern "C" nsresult     mimeEmitterEndBody(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterEndHeader(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterStartHeader(MimeDisplayOptions *opt, PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                                               const char *outCharset);

/* To Get the connnection to prefs service manager */
extern "C" nsIPref          *GetPrefServiceManager(MimeDisplayOptions *opt);

extern "C" char             *GetOSTempFile(const char *name);

/* This is the next generation string retrieval call */
extern "C" char             *MimeGetStringByID(PRInt32 stringID);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MIMEMOZ_H_ */

