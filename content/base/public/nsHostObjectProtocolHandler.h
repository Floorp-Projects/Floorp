/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostObjectProtocolHandler_h
#define nsHostObjectProtocolHandler_h

#include "mozilla/Attributes.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"

#define BLOBURI_SCHEME "blob"
#define MEDIASTREAMURI_SCHEME "mediastream"
#define MEDIASOURCEURI_SCHEME "mediasource"
#define FONTTABLEURI_SCHEME "moz-fonttable"
#define RTSPURI_SCHEME "rtsp"

class nsIDOMBlob;
class nsIDOMMediaStream;
class nsIPrincipal;

namespace mozilla {
namespace dom {
class MediaSource;
}
}

class nsHostObjectProtocolHandler : public nsIProtocolHandler
{
public:
  nsHostObjectProtocolHandler();
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods, except for GetScheme which is only defined
  // in subclasses.
  NS_IMETHOD GetDefaultPort(int32_t *aDefaultPort) MOZ_OVERRIDE;
  NS_IMETHOD GetProtocolFlags(uint32_t *aProtocolFlags) MOZ_OVERRIDE;
  NS_IMETHOD NewURI(const nsACString & aSpec, const char * aOriginCharset, nsIURI *aBaseURI, nsIURI * *_retval) MOZ_OVERRIDE;
  NS_IMETHOD NewChannel(nsIURI *aURI, nsIChannel * *_retval) MOZ_OVERRIDE;
  NS_IMETHOD AllowPort(int32_t port, const char * scheme, bool *_retval) MOZ_OVERRIDE;

  static nsresult GenerateURIString(const nsACString &aScheme,
                                    nsACString &aUri);

  // Methods for managing uri->object mapping
  // AddDataEntry creates the URI with the given scheme and returns it in aUri
  static nsresult AddDataEntry(const nsACString& aScheme,
                               nsISupports* aObject,
                               nsIPrincipal* aPrincipal,
                               nsACString& aUri);
  static void RemoveDataEntry(const nsACString& aUri);
  static nsIPrincipal* GetDataEntryPrincipal(const nsACString& aUri);
  static void Traverse(const nsACString& aUri, nsCycleCollectionTraversalCallback& aCallback);

protected:
  virtual ~nsHostObjectProtocolHandler() {}

private:
  static void Init(void);
};

class nsBlobProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetScheme(nsACString &result) MOZ_OVERRIDE;
};

class nsMediaStreamProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetScheme(nsACString &result) MOZ_OVERRIDE;
};

class nsMediaSourceProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetScheme(nsACString &result) MOZ_OVERRIDE;
};

class nsFontTableProtocolHandler : public nsHostObjectProtocolHandler
{
public:
  NS_IMETHOD GetScheme(nsACString &result);
  NS_IMETHOD NewURI(const nsACString & aSpec, const char * aOriginCharset, nsIURI *aBaseURI, nsIURI * *_retval);
};

inline bool IsBlobURI(nsIURI* aUri)
{
  bool isBlob;
  return NS_SUCCEEDED(aUri->SchemeIs(BLOBURI_SCHEME, &isBlob)) && isBlob;
}

inline bool IsRtspURI(nsIURI* aUri)
{
  bool isRtsp;
  return NS_SUCCEEDED(aUri->SchemeIs(RTSPURI_SCHEME, &isRtsp)) && isRtsp;
}

inline bool IsMediaStreamURI(nsIURI* aUri)
{
  bool isStream;
  return NS_SUCCEEDED(aUri->SchemeIs(MEDIASTREAMURI_SCHEME, &isStream)) && isStream;
}

inline bool IsMediaSourceURI(nsIURI* aUri)
{
  bool isMediaSource;
  return NS_SUCCEEDED(aUri->SchemeIs(MEDIASOURCEURI_SCHEME, &isMediaSource)) && isMediaSource;
}

inline bool IsFontTableURI(nsIURI* aUri)
{
  bool isFont;
  return NS_SUCCEEDED(aUri->SchemeIs(FONTTABLEURI_SCHEME, &isFont)) && isFont;
}

extern nsresult
NS_GetStreamForBlobURI(nsIURI* aURI, nsIInputStream** aStream);

extern nsresult
NS_GetStreamForMediaStreamURI(nsIURI* aURI, nsIDOMMediaStream** aStream);

extern nsresult
NS_GetSourceForMediaSourceURI(nsIURI* aURI, mozilla::dom::MediaSource** aSource);

#define NS_BLOBPROTOCOLHANDLER_CID \
{ 0xb43964aa, 0xa078, 0x44b2, \
  { 0xb0, 0x6b, 0xfd, 0x4d, 0x1b, 0x17, 0x2e, 0x66 } }

#define NS_MEDIASTREAMPROTOCOLHANDLER_CID \
{ 0x27d1fa24, 0x2b73, 0x4db3, \
	{ 0xab, 0x48, 0xb9, 0x83, 0x83, 0x40, 0xe0, 0x81 } }

#define NS_MEDIASOURCEPROTOCOLHANDLER_CID \
{ 0x12ef31fc, 0xa8fb, 0x4661, \
	{ 0x9a, 0x63, 0xfb, 0x61, 0x04,0x5d, 0xb8, 0x61 } }

#define NS_FONTTABLEPROTOCOLHANDLER_CID \
{ 0x3fc8f04e, 0xd719, 0x43ca, \
  { 0x9a, 0xd0, 0x18, 0xee, 0x32, 0x02, 0x11, 0xf2 } }

#endif /* nsHostObjectProtocolHandler_h */
