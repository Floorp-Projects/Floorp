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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nsMimeBaseEmitter_h_
#define _nsMimeBaseEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsIMimeEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIPref.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgHeaderParser.h"
#include "nsIPipe.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIMimeConverter.h"

//
// The base emitter will serve as the place to do all of the caching,
// sorting, etc... of mail headers and bodies for this internally developed
// emitter library. The other emitter classes in this file (nsMimeHTMLEmitter, etc.)
// will only be concerned with doing output processing ONLY.
//

//
// Used for keeping track of the attachment information...
//
typedef struct {
  char      *displayName;
  char      *urlSpec;
  char      *contentType;
  PRBool    notDownloaded;
} attachmentInfoType;

//
// For header info...
//
typedef struct {
  char      *name;
  char      *value;
} headerInfoType;

class nsMimeBaseEmitter : public nsIMimeEmitter, 
                          public nsIInputStreamObserver,
                          public nsIOutputStreamObserver
{
public: 
  nsMimeBaseEmitter ();
  virtual             ~nsMimeBaseEmitter (void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIMIMEEMITTER
  NS_DECL_NSIINPUTSTREAMOBSERVER
  NS_DECL_NSIOUTPUTSTREAMOBSERVER

  // Utility output functions...
  NS_IMETHOD          UtilityWriteCRLF(const char *buf);

  // For string bundle usage...
  char                *MimeGetStringByName(const char *aHeaderName);
  char                *LocalizeHeaderName(const char *aHeaderName, const char *aDefaultName);

  // For header processing...
  char                *GetHeaderValue(const char  *aHeaderName,
                                      nsVoidArray *aArray);

  // To write out a stored header array as HTML
  virtual nsresult            WriteHeaderFieldHTMLPrefix();
  virtual nsresult            WriteHeaderFieldHTML(const char *field, const char *value);
  virtual nsresult            WriteHeaderFieldHTMLPostfix();
  virtual nsresult            WriteHTMLHeaders();

protected:
  // Internal methods...
  void                CleanupHeaderArray(nsVoidArray *aArray);

  // For header output...
  nsresult            DumpSubjectFromDate();
  nsresult            DumpToCC();
  nsresult            DumpRestOfHeaders();
  nsresult            OutputGenericHeader(const char *aHeaderVal);

  // For string bundle usage...
  nsCOMPtr<nsIStringBundle>	m_stringBundle;     // For string bundle usage...

  // For buffer management on output
  MimeRebuffer        *mBufferMgr;

	// mscott
  // dont ref count the streams....the emitter is owned by the converter
	// which owns these streams...
  //
  nsIOutputStream     *mOutStream;
	nsIInputStream	    *mInputStream;
  nsIStreamListener   *mOutListener;
	nsIChannel			    *mChannel;

  // For gathering statistics on processing...
  PRUint32            mTotalWritten;
  PRUint32            mTotalRead;

  // Output control and info...
  nsIPref             *mPrefs;            // Connnection to prefs service manager
  PRBool              mDocHeader;         // For header determination...
  nsIURI              *mURL;              // the url for the data being processed...
  PRInt32             mHeaderDisplayType; // The setting for header output...
  nsCString           mHTMLHeaders;       // HTML Header Data...

  // For attachment processing...
  PRInt32             mAttachCount;
  nsVoidArray         *mAttachArray;
  attachmentInfoType  *mCurrentAttachment;

  // For header caching...
  nsVoidArray         *mHeaderArray;
  nsVoidArray         *mEmbeddedHeaderArray;
  nsCOMPtr<nsIMsgHeaderParser>  mHeaderParser;

  // For body caching...
  PRBool              mBodyStarted;
  nsCString           mBody;
  PRBool              mFirstHeaders;

  // For the format being used...
  PRInt32             mFormat;

  // For I18N Conversion...
  nsCOMPtr<nsIMimeConverter> mUnicodeConverter;
  nsString            mCharset;
};

#endif /* _nsMimeBaseEmitter_h_ */
