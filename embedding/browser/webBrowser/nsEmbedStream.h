/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <nsISupports.h>
#include <nsCOMPtr.h>
#include <nsIOutputStream.h>
#include <nsIInputStream.h>
#include <nsILoadGroup.h>
#include <nsIChannel.h>
#include <nsIStreamListener.h>
#include <nsIWebBrowser.h>
  
class nsEmbedStream : public nsIInputStream 
{
 public:

  nsEmbedStream();
  virtual ~nsEmbedStream();

  void      InitOwner      (nsIWebBrowser *aOwner);
  NS_METHOD Init           (void);

  NS_METHOD OpenStream     (const char *aBaseURI, const char *aContentType);
  NS_METHOD AppendToStream (const char *aData, PRInt32 aLen);
  NS_METHOD CloseStream    (void);

  NS_METHOD Append         (const char *aData, PRUint32 aLen);

  // nsISupports
  NS_DECL_ISUPPORTS
  // nsIInputStream
  NS_DECL_NSIINPUTSTREAM

 private:
  nsCOMPtr<nsIOutputStream>   mOutputStream;
  nsCOMPtr<nsIInputStream>    mInputStream;

  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsCOMPtr<nsIChannel>        mChannel;
  nsCOMPtr<nsIStreamListener> mStreamListener;

  PRUint32                    mOffset;
  PRBool                      mDoingStream;

  nsIWebBrowser              *mOwner;

};
