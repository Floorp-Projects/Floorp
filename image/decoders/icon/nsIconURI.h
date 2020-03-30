/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_icon_nsIconURI_h
#define mozilla_image_decoders_icon_nsIconURI_h

#include "nsIIconURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIClassInfo.h"
#include "nsINestedURI.h"
#include "nsIURIMutator.h"

#define NS_THIS_ICONURI_IMPLEMENTATION_CID           \
  { /* 0b9bb0c2-fee6-470b-b9b9-9fd9462b5e19 */       \
    0x5c3e417f, 0xb686, 0x4105, {                    \
      0x86, 0xe7, 0xf9, 0x1b, 0xac, 0x97, 0x4d, 0x5c \
    }                                                \
  }

namespace mozilla {
class Encoding;
}

class nsMozIconURI final : public nsIMozIconURI,
                           public nsINestedURI,
                           public nsISerializable,
                           public nsIClassInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIMOZICONURI
  NS_DECL_NSINESTEDURI
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

 protected:
  nsMozIconURI();
  virtual ~nsMozIconURI();
  nsCOMPtr<nsIURL> mIconURL;  // a URL that we want the icon for
  uint32_t mSize;  // the # of pixels in a row that we want for this image.
                   // Typically 16, 32, 128, etc.
  nsCString mContentType;  // optional field explicitly specifying the content
                           // type
  nsCString mFileName;  // for if we don't have an actual file path, we're just
                        // given a filename with an extension
  nsCString mStockIcon;
  int32_t mIconSize;   // -1 if not specified, otherwise index into
                       // kSizeStrings
  int32_t mIconState;  // -1 if not specified, otherwise index into
                       // kStateStrings

 private:
  nsresult Clone(nsIURI** aURI);
  nsresult SetSpecInternal(const nsACString& input);
  nsresult SetScheme(const nsACString& input);
  nsresult SetUserPass(const nsACString& input);
  nsresult SetUsername(const nsACString& input);
  nsresult SetPassword(const nsACString& input);
  nsresult SetHostPort(const nsACString& aValue);
  nsresult SetHost(const nsACString& input);
  nsresult SetPort(int32_t port);
  nsresult SetPathQueryRef(const nsACString& input);
  nsresult SetRef(const nsACString& input);
  nsresult SetFilePath(const nsACString& input);
  nsresult SetQuery(const nsACString& input);
  nsresult SetQueryWithEncoding(const nsACString& input,
                                const mozilla::Encoding* encoding);
  nsresult ReadPrivate(nsIObjectInputStream* stream);
  bool Deserialize(const mozilla::ipc::URIParams&);

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<nsMozIconURI>,
                        public nsISerializable {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    [[nodiscard]] NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    NS_IMETHOD Deserialize(const mozilla::ipc::URIParams& aParams) override {
      return InitFromIPCParams(aParams);
    }

    NS_IMETHOD Finalize(nsIURI** aURI) override {
      mURI.forget(aURI);
      return NS_OK;
    }

    NS_IMETHOD SetSpec(const nsACString& aSpec,
                       nsIURIMutator** aMutator) override {
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return InitFromSpec(aSpec);
    }

    explicit Mutator() {}

   private:
    virtual ~Mutator() {}

    friend class nsMozIconURI;
  };

  friend BaseURIMutator<nsMozIconURI>;
};

#endif  // mozilla_image_decoders_icon_nsIconURI_h
