/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 sts=2 ts=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconURI.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Sprintf.h"

#include "nsIClassInfoImpl.h"
#include "nsIIOService.h"
#include "nsISerializable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsCRT.h"
#include <stdlib.h>

using namespace mozilla;
using namespace mozilla::ipc;

#define DEFAULT_IMAGE_SIZE 16

#if defined(MAX_PATH)
#  define SANE_FILE_NAME_LEN MAX_PATH
#elif defined(PATH_MAX)
#  define SANE_FILE_NAME_LEN PATH_MAX
#else
#  define SANE_FILE_NAME_LEN 1024
#endif

static NS_DEFINE_CID(kThisIconURIImplementationCID,
                     NS_THIS_ICONURI_IMPLEMENTATION_CID);

static const char* const kSizeStrings[] = {"button", "toolbar", "toolbarsmall",
                                           "menu",   "dnd",     "dialog"};

static const char* const kStateStrings[] = {"normal", "disabled"};

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_CLASSINFO(nsMozIconURI, nullptr, nsIClassInfo::THREADSAFE,
                  NS_ICONURI_CID)
// Empty CI getter. We only need nsIClassInfo for Serialization
NS_IMPL_CI_INTERFACE_GETTER0(nsMozIconURI)

nsMozIconURI::nsMozIconURI()
    : mSize(DEFAULT_IMAGE_SIZE), mIconSize(-1), mIconState(-1) {}

nsMozIconURI::~nsMozIconURI() {}

NS_IMPL_ADDREF(nsMozIconURI)
NS_IMPL_RELEASE(nsMozIconURI)

NS_INTERFACE_MAP_BEGIN(nsMozIconURI)
  if (aIID.Equals(kThisIconURIImplementationCID)) {
    foundInterface = static_cast<nsIURI*>(this);
  } else
    NS_INTERFACE_MAP_ENTRY(nsIMozIconURI)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsINestedURI, mIconURL)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  NS_IMPL_QUERY_CLASSINFO(nsMozIconURI)
NS_INTERFACE_MAP_END

#define MOZICON_SCHEME "moz-icon:"
#define MOZICON_SCHEME_LEN (sizeof(MOZICON_SCHEME) - 1)

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsMozIconURI::GetSpec(nsACString& aSpec) {
  aSpec = MOZICON_SCHEME;

  if (mIconURL) {
    nsAutoCString fileIconSpec;
    nsresult rv = mIconURL->GetSpec(fileIconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    aSpec += fileIconSpec;
  } else if (!mStockIcon.IsEmpty()) {
    aSpec += "//stock/";
    aSpec += mStockIcon;
  } else {
    aSpec += "//";
    aSpec += mFileName;
  }

  aSpec += "?size=";
  if (mIconSize >= 0) {
    aSpec += kSizeStrings[mIconSize];
  } else {
    char buf[20];
    SprintfLiteral(buf, "%d", mSize);
    aSpec.Append(buf);
  }

  if (mIconState >= 0) {
    aSpec += "&state=";
    aSpec += kStateStrings[mIconState];
  }

  if (!mContentType.IsEmpty()) {
    aSpec += "&contentType=";
    aSpec += mContentType.get();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetSpecIgnoringRef(nsACString& result) { return GetSpec(result); }

NS_IMETHODIMP
nsMozIconURI::GetDisplaySpec(nsACString& aUnicodeSpec) {
  return GetSpec(aUnicodeSpec);
}

NS_IMETHODIMP
nsMozIconURI::GetDisplayHostPort(nsACString& aUnicodeHostPort) {
  return GetHostPort(aUnicodeHostPort);
}

NS_IMETHODIMP
nsMozIconURI::GetDisplayHost(nsACString& aUnicodeHost) {
  return GetHost(aUnicodeHost);
}

NS_IMETHODIMP
nsMozIconURI::GetDisplayPrePath(nsACString& aPrePath) {
  return GetPrePath(aPrePath);
}

NS_IMETHODIMP
nsMozIconURI::GetHasRef(bool* result) {
  *result = false;
  return NS_OK;
}

NS_IMPL_NSIURIMUTATOR_ISUPPORTS(nsMozIconURI::Mutator, nsIURISetters,
                                nsIURIMutator, nsISerializable)

NS_IMETHODIMP
nsMozIconURI::Mutate(nsIURIMutator** aMutator) {
  RefPtr<nsMozIconURI::Mutator> mutator = new nsMozIconURI::Mutator();
  nsresult rv = mutator->InitFromURI(this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mutator.forget(aMutator);
  return NS_OK;
}

// helper function for parsing out attributes like size, and contentType
// from the icon url.
// takes a string like ?size=32&contentType=text/html and returns a new string
// containing just the attribute value. i.e you could pass in this string with
// an attribute name of 'size=', this will return 32
// Assumption: attribute pairs in the string are separated by '&'.
static void extractAttributeValue(const char* aSearchString,
                                  const char* aAttributeName,
                                  nsCString& aResult) {
  aResult.Truncate();

  if (aSearchString && aAttributeName) {
    // search the string for attributeName
    uint32_t attributeNameSize = strlen(aAttributeName);
    const char* startOfAttribute = PL_strcasestr(aSearchString, aAttributeName);
    if (startOfAttribute &&
        (*(startOfAttribute - 1) == '?' || *(startOfAttribute - 1) == '&')) {
      startOfAttribute += attributeNameSize;  // skip over the attributeName
      // is there something after the attribute name
      if (*startOfAttribute) {
        const char* endofAttribute = strchr(startOfAttribute, '&');
        if (endofAttribute) {
          aResult.Assign(Substring(startOfAttribute, endofAttribute));
        } else {
          aResult.Assign(startOfAttribute);
        }
      }  // if we have a attribute value
    }    // if we have a attribute name
  }      // if we got non-null search string and attribute name values
}

nsresult nsMozIconURI::SetSpecInternal(const nsACString& aSpec) {
  // Reset everything to default values.
  mIconURL = nullptr;
  mSize = DEFAULT_IMAGE_SIZE;
  mContentType.Truncate();
  mFileName.Truncate();
  mStockIcon.Truncate();
  mIconSize = -1;
  mIconState = -1;

  nsAutoCString iconSpec(aSpec);
  if (!Substring(iconSpec, 0, MOZICON_SCHEME_LEN)
           .EqualsLiteral(MOZICON_SCHEME) ||
      (!Substring(iconSpec, MOZICON_SCHEME_LEN, 7).EqualsLiteral("file://") &&
       // Checking for the leading '//' will match both the '//stock/' and
       // '//.foo' cases:
       !Substring(iconSpec, MOZICON_SCHEME_LEN, 2).EqualsLiteral("//"))) {
    return NS_ERROR_MALFORMED_URI;
  }

  int32_t questionMarkPos = iconSpec.Find("?");
  if (questionMarkPos != -1 &&
      static_cast<int32_t>(iconSpec.Length()) > (questionMarkPos + 1)) {
    extractAttributeValue(iconSpec.get(), "contentType=", mContentType);

    nsAutoCString sizeString;
    extractAttributeValue(iconSpec.get(), "size=", sizeString);
    if (!sizeString.IsEmpty()) {
      const char* sizeStr = sizeString.get();
      for (uint32_t i = 0; i < ArrayLength(kSizeStrings); i++) {
        if (nsCRT::strcasecmp(sizeStr, kSizeStrings[i]) == 0) {
          mIconSize = i;
          break;
        }
      }

      int32_t sizeValue = atoi(sizeString.get());
      if (sizeValue > 0) {
        mSize = sizeValue;
      }
    }

    nsAutoCString stateString;
    extractAttributeValue(iconSpec.get(), "state=", stateString);
    if (!stateString.IsEmpty()) {
      const char* stateStr = stateString.get();
      for (uint32_t i = 0; i < ArrayLength(kStateStrings); i++) {
        if (nsCRT::strcasecmp(stateStr, kStateStrings[i]) == 0) {
          mIconState = i;
          break;
        }
      }
    }
  }

  int32_t pathLength = iconSpec.Length() - MOZICON_SCHEME_LEN;
  if (questionMarkPos != -1) {
    pathLength = questionMarkPos - MOZICON_SCHEME_LEN;
  }
  if (pathLength < 3) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString iconPath(Substring(iconSpec, MOZICON_SCHEME_LEN, pathLength));

  // Icon URI path can have three forms:
  // (1) //stock/<icon-identifier>
  // (2) //<some dummy file with an extension>
  // (3) a valid URL

  if (!strncmp("//stock/", iconPath.get(), 8)) {
    mStockIcon.Assign(Substring(iconPath, 8));
    // An icon identifier must always be specified.
    if (mStockIcon.IsEmpty()) {
      return NS_ERROR_MALFORMED_URI;
    }
    return NS_OK;
  }

  if (StringBeginsWith(iconPath, "//"_ns)) {
    // Sanity check this supposed dummy file name.
    if (iconPath.Length() > SANE_FILE_NAME_LEN) {
      return NS_ERROR_MALFORMED_URI;
    }
    iconPath.Cut(0, 2);
    mFileName.Assign(iconPath);
  }

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  ioService->NewURI(iconPath, nullptr, nullptr, getter_AddRefs(uri));
  mIconURL = do_QueryInterface(uri);
  if (mIconURL) {
    // The inner URI should be a 'file:' one. If not, bail.
    if (!mIconURL->SchemeIs("file")) {
      return NS_ERROR_MALFORMED_URI;
    }
    mFileName.Truncate();
  } else if (mFileName.IsEmpty()) {
    return NS_ERROR_MALFORMED_URI;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetPrePath(nsACString& prePath) {
  prePath = MOZICON_SCHEME;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetScheme(nsACString& aScheme) {
  aScheme = "moz-icon";
  return NS_OK;
}

nsresult nsMozIconURI::SetScheme(const nsACString& aScheme) {
  // doesn't make sense to set the scheme of a moz-icon URL
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetUsername(nsACString& aUsername) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetUsername(const nsACString& aUsername) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPassword(nsACString& aPassword) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetPassword(const nsACString& aPassword) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetUserPass(nsACString& aUserPass) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetUserPass(const nsACString& aUserPass) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetHostPort(nsACString& aHostPort) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetHostPort(const nsACString& aHostPort) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetHost(nsACString& aHost) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetHost(const nsACString& aHost) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPort(int32_t* aPort) { return NS_ERROR_FAILURE; }

nsresult nsMozIconURI::SetPort(int32_t aPort) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
nsMozIconURI::GetPathQueryRef(nsACString& aPath) {
  aPath.Truncate();
  return NS_OK;
}

nsresult nsMozIconURI::SetPathQueryRef(const nsACString& aPath) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetFilePath(nsACString& aFilePath) {
  aFilePath.Truncate();
  return NS_OK;
}

nsresult nsMozIconURI::SetFilePath(const nsACString& aFilePath) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetQuery(nsACString& aQuery) {
  aQuery.Truncate();
  return NS_OK;
}

nsresult nsMozIconURI::SetQuery(const nsACString& aQuery) {
  return NS_ERROR_FAILURE;
}

nsresult nsMozIconURI::SetQueryWithEncoding(const nsACString& aQuery,
                                            const Encoding* aEncoding) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetRef(nsACString& aRef) {
  aRef.Truncate();
  return NS_OK;
}

nsresult nsMozIconURI::SetRef(const nsACString& aRef) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::Equals(nsIURI* other, bool* result) {
  *result = false;
  NS_ENSURE_ARG_POINTER(other);
  MOZ_ASSERT(result, "null pointer");

  nsAutoCString spec1;
  nsAutoCString spec2;

  nsresult rv = GetSpec(spec1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = other->GetSpec(spec2);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsCRT::strcasecmp(spec1.get(), spec2.get())) {
    *result = true;
  } else {
    *result = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::EqualsExceptRef(nsIURI* other, bool* result) {
  // GetRef/SetRef not supported by nsMozIconURI, so
  // EqualsExceptRef() is the same as Equals().
  return Equals(other, result);
}

NS_IMETHODIMP
nsMozIconURI::SchemeIs(const char* aScheme, bool* aEquals) {
  MOZ_ASSERT(aEquals, "null pointer");
  if (!aScheme) {
    *aEquals = false;
    return NS_OK;
  }

  *aEquals = nsCRT::strcasecmp("moz-icon", aScheme) == 0;
  return NS_OK;
}

nsresult nsMozIconURI::Clone(nsIURI** result) {
  nsCOMPtr<nsIURL> newIconURL;
  if (mIconURL) {
    newIconURL = mIconURL;
  }

  RefPtr<nsMozIconURI> uri = new nsMozIconURI();
  newIconURL.swap(uri->mIconURL);
  uri->mSize = mSize;
  uri->mContentType = mContentType;
  uri->mFileName = mFileName;
  uri->mStockIcon = mStockIcon;
  uri->mIconSize = mIconSize;
  uri->mIconState = mIconState;
  uri.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::Resolve(const nsACString& relativePath, nsACString& result) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMozIconURI::GetAsciiSpec(nsACString& aSpecA) { return GetSpec(aSpecA); }

NS_IMETHODIMP
nsMozIconURI::GetAsciiHostPort(nsACString& aHostPortA) {
  return GetHostPort(aHostPortA);
}

NS_IMETHODIMP
nsMozIconURI::GetAsciiHost(nsACString& aHostA) { return GetHost(aHostA); }

////////////////////////////////////////////////////////////////////////////////
// nsIIconUri methods:

NS_IMETHODIMP
nsMozIconURI::GetIconURL(nsIURL** aFileUrl) {
  *aFileUrl = mIconURL;
  NS_IF_ADDREF(*aFileUrl);
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetImageSize(uint32_t* aImageSize)
// measured by # of pixels in a row. defaults to 16.
{
  *aImageSize = mSize;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetContentType(nsACString& aContentType) {
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetFileExtension(nsACString& aFileExtension) {
  // First, try to get the extension from mIconURL if we have one
  if (mIconURL) {
    nsAutoCString fileExt;
    if (NS_SUCCEEDED(mIconURL->GetFileExtension(fileExt))) {
      if (!fileExt.IsEmpty()) {
        // unfortunately, this code doesn't give us the required '.' in
        // front of the extension so we have to do it ourselves.
        aFileExtension.Assign('.');
        aFileExtension.Append(fileExt);
      }
    }
    return NS_OK;
  }

  if (!mFileName.IsEmpty()) {
    // truncate the extension out of the file path...
    const char* chFileName = mFileName.get();  // get the underlying buffer
    const char* fileExt = strrchr(chFileName, '.');
    if (!fileExt) {
      return NS_OK;
    }
    aFileExtension = fileExt;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetStockIcon(nsACString& aStockIcon) {
  aStockIcon = mStockIcon;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetIconSize(nsACString& aSize) {
  if (mIconSize >= 0) {
    aSize = kSizeStrings[mIconSize];
  } else {
    aSize.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetIconState(nsACString& aState) {
  if (mIconState >= 0) {
    aState = kStateStrings[mIconState];
  } else {
    aState.Truncate();
  }
  return NS_OK;
}

void nsMozIconURI::Serialize(URIParams& aParams) {
  IconURIParams params;

  if (mIconURL) {
    URIParams iconURLParams;
    SerializeURI(mIconURL, iconURLParams);
    if (iconURLParams.type() == URIParams::T__None) {
      // Serialization failed, bail.
      return;
    }

    params.uri() = Some(std::move(iconURLParams));
  } else {
    params.uri() = Nothing();
  }

  params.size() = mSize;
  params.fileName() = mFileName;
  params.stockIcon() = mStockIcon;
  params.iconSize() = mIconSize;
  params.iconState() = mIconState;

  aParams = params;
}

bool nsMozIconURI::Deserialize(const URIParams& aParams) {
  if (aParams.type() != URIParams::TIconURIParams) {
    MOZ_ASSERT_UNREACHABLE("Received unknown URI from other process!");
    return false;
  }

  const IconURIParams& params = aParams.get_IconURIParams();
  if (params.uri().isSome()) {
    nsCOMPtr<nsIURI> uri = DeserializeURI(params.uri().ref());
    mIconURL = do_QueryInterface(uri);
    if (!mIconURL) {
      MOZ_ASSERT_UNREACHABLE("bad nsIURI passed");
      return false;
    }
  }

  mSize = params.size();
  mContentType = params.contentType();
  mFileName = params.fileName();
  mStockIcon = params.stockIcon();

  if (params.iconSize() < -1 ||
      params.iconSize() >= (int32_t)ArrayLength(kSizeStrings)) {
    return false;
  }
  mIconSize = params.iconSize();

  if (params.iconState() < -1 ||
      params.iconState() >= (int32_t)ArrayLength(kStateStrings)) {
    return false;
  }
  mIconState = params.iconState();

  return true;
}

NS_IMETHODIMP
nsMozIconURI::GetInnerURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> iconURL = mIconURL;
  if (!iconURL) {
    *aURI = nullptr;
    return NS_ERROR_FAILURE;
  }

  iconURL.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetInnermostURI(nsIURI** aURI) {
  return NS_ImplGetInnermostURI(this, aURI);
}

NS_IMETHODIMP
nsMozIconURI::Read(nsIObjectInputStream* aStream) {
  MOZ_ASSERT_UNREACHABLE("Use nsIURIMutator.read() instead");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMozIconURI::ReadPrivate(nsIObjectInputStream* aStream) {
  nsAutoCString spec;
  nsresult rv = aStream->ReadCString(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  return SetSpecInternal(spec);
}

NS_IMETHODIMP
nsMozIconURI::Write(nsIObjectOutputStream* aStream) {
  nsAutoCString spec;
  nsresult rv = GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  return aStream->WriteStringZ(spec.get());
}
