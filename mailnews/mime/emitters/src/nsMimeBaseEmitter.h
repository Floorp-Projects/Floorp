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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

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
                          public nsIInterfaceRequestor,
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
  NS_DECL_NSIINTERFACEREQUESTOR

  // Utility output functions...
  NS_IMETHOD          UtilityWriteCRLF(const char *buf);

  // For string bundle usage...
  char                *MimeGetStringByName(const char *aHeaderName);
  char                *LocalizeHeaderName(const char *aHeaderName, const char *aDefaultName);

  // For header processing...
  char                *GetHeaderValue(const char  *aHeaderName, nsVoidArray *aArray);

  // To write out a stored header array as HTML
  virtual nsresult            WriteHeaderFieldHTMLPrefix();
  virtual nsresult            WriteHeaderFieldHTML(const char *field, const char *value);
  virtual nsresult            WriteHeaderFieldHTMLPostfix();

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
