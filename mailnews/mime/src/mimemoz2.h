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

#ifndef _MIMEMOZ_H_
#define _MIMEMOZ_H_

#include "prtypes.h"
#include "nsStreamConverter.h"
#include "nsIMimeEmitter.h"
#include "nsIURI.h"
#include "mozITXTToHTMLConv.h"
#include "nsIMsgSend.h"
#include "nsIMimeConverter.h"

class nsOutputFileStream;

// SHERRY - Need to get these out of here eventually

#ifdef XP_UNIX
#undef Bool
#endif


  
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
  char                *orig_url_name; /* original url name */
  nsIChannel          *channel;
  nsMimeOutputType    format_out;
  void                *pluginObj2;  /* The new XP-COM stream converter object */
  nsMIMESession       *istream;     /* Holdover - new stream we're writing out image data-if any. */
  MimeObject          *obj;         /* The root parser object */
  MimeDisplayOptions  *options;     /* Data for communicating with libmime.a */
  MimeHeaders         *headers;     /* Copy of outer most mime header */

  nsIMimeEmitter      *output_emitter;  /* Output emitter engine for libmime */
  PRBool              firstCheck;   /* Is this the first look at the stream data */
};

// 
// This struct is the state we use for loading drafts and templates...
//
struct mime_draft_data 
{
  char                *url_name;           // original url name */
  nsMimeOutputType    format_out;          // intended output format; should be FO_OPEN_DRAFT */
  nsMIMESession       *stream;             // not used for now 
  MimeObject          *obj;                // The root 
  MimeDisplayOptions  *options;            // data for communicating with libmime
  MimeHeaders         *headers;            // Copy of outer most mime header 
  PRInt32             attachments_count;   // how many attachments we have 
  nsMsgAttachedFile   *attachments;        // attachments 
  nsMsgAttachedFile   *messageBody;        // message body 
  nsMsgAttachedFile   *curAttachment;		   // temp 

  nsIFileSpec         *tmpFileSpec;
  nsOutputFileStream  *tmpFileStream;      // output file handle 

  MimeDecoderData     *decoder_data;
  char                *mailcharset;        // get it from CHARSET of Content-Type 
  PRBool              forwardInline;
  nsCOMPtr<nsIMsgIdentity>      identity;
};

////////////////////////////////////////////////////////////////
// Bridge routines for legacy mime code
////////////////////////////////////////////////////////////////

// Create bridge stream for libmime
extern "C"
void         *mime_bridge_create_display_stream(nsIMimeEmitter      *newEmitter,
                                                nsStreamConverter   *newPluginObj2,
                                                nsIURI              *uri,
                                                nsMimeOutputType    format_out,
						                                    PRUint32	          whattodo,
                                                nsIChannel          *aChannel);

// To get the mime emitter...
extern "C" nsIMimeEmitter   *GetMimeEmitter(MimeDisplayOptions *opt);

// To support 2 types of emitters...we need these routines :-(
extern "C" nsresult     mimeSetNewURL(nsMIMESession *stream, char *url);
extern "C" nsresult     mimeEmitterAddAttachmentField(MimeDisplayOptions *opt, const char *field, const char *value); 
extern "C" nsresult     mimeEmitterAddHeaderField(MimeDisplayOptions *opt, const char *field, const char *value);
extern "C" nsresult     mimeEmitterStartAttachment(MimeDisplayOptions *opt, const char *name, const char *contentType, const char *url,
                                                   PRBool aNotDownloaded);
extern "C" nsresult     mimeEmitterEndAttachment(MimeDisplayOptions *opt);
extern "C" nsresult		mimeEmitterEndAllAttachments(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterStartBody(MimeDisplayOptions *opt, PRBool bodyOnly, const char *msgID, const char *outCharset);
extern "C" nsresult     mimeEmitterEndBody(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterEndHeader(MimeDisplayOptions *opt);
extern "C" nsresult     mimeEmitterStartHeader(MimeDisplayOptions *opt, PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                                               const char *outCharset);
extern "C" nsresult     mimeEmitterUpdateCharacterSet(MimeDisplayOptions *opt, const char *aCharset);

/* To Get the connnection to prefs service manager */
extern "C" nsIPref          *GetPrefServiceManager(MimeDisplayOptions *opt);

// Get the text converter...
mozITXTToHTMLConv           *GetTextConverter(MimeDisplayOptions *opt);

/* This is the next generation string retrieval call */
extern "C" char             *MimeGetStringByID(PRInt32 stringID);

// Utility to create a nsIURI object...
extern "C" nsresult         nsMimeNewURI(nsIURI** aInstancePtrResult, const char *aSpec, nsIURI *aBase);

extern "C" nsresult SetMailCharacterSetToMsgWindow(MimeObject *obj, const PRUnichar *aCharacterSet); 

extern "C" nsresult GetMailNewsFont(MimeObject *obj, PRBool styleFixed, char *fontName, PRUint32 nameBuffSize, PRInt32 *fontPixelSize, PRInt32 *fontSizePercentage);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MIMEMOZ_H_ */

