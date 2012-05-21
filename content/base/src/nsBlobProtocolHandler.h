/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBlobProtocolHandler_h
#define nsBlobProtocolHandler_h

#include "nsIProtocolHandler.h"

#define BLOBURI_SCHEME "blob"

class nsIDOMBlob;
class nsIPrincipal;

class nsBlobProtocolHandler : public nsIProtocolHandler
{
public:
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // nsBlobProtocolHandler methods:
  nsBlobProtocolHandler() {}
  virtual ~nsBlobProtocolHandler() {}

  // Methods for managing uri->file mapping
  static void AddFileDataEntry(nsACString& aUri,
			       nsIDOMBlob* aFile,
                               nsIPrincipal* aPrincipal);
  static void RemoveFileDataEntry(nsACString& aUri);
  static nsIPrincipal* GetFileDataEntryPrincipal(nsACString& aUri);
  
};

#define NS_BLOBPROTOCOLHANDLER_CID \
{ 0xb43964aa, 0xa078, 0x44b2, \
  { 0xb0, 0x6b, 0xfd, 0x4d, 0x1b, 0x17, 0x2e, 0x66 } }

#endif /* nsBlobProtocolHandler_h */
