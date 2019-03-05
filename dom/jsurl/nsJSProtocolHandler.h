/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSProtocolHandler_h___
#define nsJSProtocolHandler_h___

#include "mozilla/Attributes.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsIMutable.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsSimpleURI.h"
#include "nsINestedURI.h"

#define NS_JSPROTOCOLHANDLER_CID                     \
  { /* bfc310d2-38a0-11d3-8cd3-0060b0fc14a3 */       \
    0xbfc310d2, 0x38a0, 0x11d3, {                    \
      0x8c, 0xd3, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 \
    }                                                \
  }

#define NS_JSURI_CID                                 \
  { /* 58f089ee-512a-42d2-a935-d0c874128930 */       \
    0x58f089ee, 0x512a, 0x42d2, {                    \
      0xa9, 0x35, 0xd0, 0xc8, 0x74, 0x12, 0x89, 0x30 \
    }                                                \
  }

#define NS_JSURIMUTATOR_CID                          \
  { /* 574ce83e-fe9f-4095-b85c-7909abbf7c37 */       \
    0x574ce83e, 0xfe9f, 0x4095, {                    \
      0xb8, 0x5c, 0x79, 0x09, 0xab, 0xbf, 0x7c, 0x37 \
    }                                                \
  }

#define NS_JSPROTOCOLHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "javascript"

class nsJSProtocolHandler : public nsIProtocolHandler {
 public:
  NS_DECL_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // nsJSProtocolHandler methods:
  nsJSProtocolHandler();

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  nsresult Init();

  static nsresult CreateNewURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, nsIURI** result);

 protected:
  virtual ~nsJSProtocolHandler();

  static nsresult EnsureUTF8Spec(const nsCString& aSpec, const char* aCharset,
                                 nsACString& aUTF8Spec);
};

class nsJSURI final : public mozilla::net::nsSimpleURI {
 public:
  using mozilla::net::nsSimpleURI::Read;
  using mozilla::net::nsSimpleURI::Write;

  nsIURI* GetBaseURI() const { return mBaseURI; }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIURI overrides
  virtual mozilla::net::nsSimpleURI* StartClone(
      RefHandlingEnum refHandlingMode, const nsACString& newRef) override;
  NS_IMETHOD Mutate(nsIURIMutator** _retval) override;
  NS_IMETHOD_(void) Serialize(mozilla::ipc::URIParams& aParams) override;

  // nsISerializable overrides
  NS_IMETHOD Read(nsIObjectInputStream* aStream) override;
  NS_IMETHOD Write(nsIObjectOutputStream* aStream) override;

  // Override the nsIClassInfo method GetClassIDNoAlloc to make sure our
  // nsISerializable impl works right.
  NS_IMETHOD GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) override;
  // NS_IMETHOD QueryInterface( const nsIID& aIID, void** aInstancePtr )
  // override;

 protected:
  nsJSURI() {}
  explicit nsJSURI(nsIURI* aBaseURI) : mBaseURI(aBaseURI) {}

  virtual ~nsJSURI() {}

  virtual nsresult EqualsInternal(nsIURI* other,
                                  RefHandlingEnum refHandlingMode,
                                  bool* result) override;
  bool Deserialize(const mozilla::ipc::URIParams&);
  nsresult ReadPrivate(nsIObjectInputStream* aStream);

 private:
  nsCOMPtr<nsIURI> mBaseURI;

 public:
  class Mutator final : public nsIURIMutator,
                        public BaseURIMutator<nsJSURI>,
                        public nsISerializable,
                        public nsIJSURIMutator {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    NS_IMETHOD
    Write(nsIObjectOutputStream* aOutputStream) override {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    MOZ_MUST_USE NS_IMETHOD Read(nsIObjectInputStream* aStream) override {
      return InitFromInputStream(aStream);
    }

    MOZ_MUST_USE NS_IMETHOD SetBase(nsIURI* aBaseURI) override {
      mURI = new nsJSURI(aBaseURI);
      return NS_OK;
    }

    explicit Mutator() {}

   private:
    virtual ~Mutator() {}

    friend class nsJSURI;
  };

  friend BaseURIMutator<nsJSURI>;
};

#endif /* nsJSProtocolHandler_h___ */
