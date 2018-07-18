/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This wraps nsSimpleURI so that all calls to it are done on the main thread.
 */

#ifndef mozilla_NullPrincipalURI_h
#define mozilla_NullPrincipalURI_h

#include "nsIURI.h"
#include "nsISizeOf.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "nsIIPCSerializableURI.h"
#include "mozilla/MemoryReporting.h"
#include "NullPrincipal.h"
#include "nsID.h"
#include "nsIURIMutator.h"

// {51fcd543-3b52-41f7-b91b-6b54102236e6}
#define NS_NULLPRINCIPALURI_IMPLEMENTATION_CID \
  {0x51fcd543, 0x3b52, 0x41f7, \
    {0xb9, 0x1b, 0x6b, 0x54, 0x10, 0x22, 0x36, 0xe6} }

namespace mozilla {

class Encoding;

class NullPrincipalURI final : public nsIURI
                             , public nsISizeOf
                             , public nsIIPCSerializableURI
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIIPCSERIALIZABLEURI

  // nsISizeOf
  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // Returns null on failure.
  static already_AddRefed<NullPrincipalURI> Create();

private:
  NullPrincipalURI();
  NullPrincipalURI(const NullPrincipalURI& aOther);

  ~NullPrincipalURI() {}

  nsresult Init();

  nsAutoCStringN<NSID_LENGTH> mPath;

  nsresult Clone(nsIURI** aURI);
  nsresult SetSpecInternal(const nsACString &input);
  nsresult SetScheme(const nsACString &input);
  nsresult SetUserPass(const nsACString &input);
  nsresult SetUsername(const nsACString &input);
  nsresult SetPassword(const nsACString &input);
  nsresult SetHostPort(const nsACString &aValue);
  nsresult SetHost(const nsACString &input);
  nsresult SetPort(int32_t port);
  nsresult SetPathQueryRef(const nsACString &input);
  nsresult SetRef(const nsACString &input);
  nsresult SetFilePath(const nsACString &input);
  nsresult SetQuery(const nsACString &input);
  nsresult SetQueryWithEncoding(const nsACString &input, const Encoding* encoding);
  bool Deserialize(const mozilla::ipc::URIParams&);

public:
  class Mutator final
      : public nsIURIMutator
      , public BaseURIMutator<NullPrincipalURI>
  {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)

    NS_IMETHOD Deserialize(const mozilla::ipc::URIParams& aParams) override
    {
      return InitFromIPCParams(aParams);
    }

    NS_IMETHOD Finalize(nsIURI** aURI) override
    {
      mURI.forget(aURI);
      return NS_OK;
    }

    NS_IMETHOD SetSpec(const nsACString & aSpec, nsIURIMutator** aMutator) override
    {
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    explicit Mutator() { }
  private:
    virtual ~Mutator() { }

    friend class NullPrincipalURI;
  };

  friend class BaseURIMutator<NullPrincipalURI>;
};

} // mozilla namespace

#endif // mozilla_NullPrincipalURI_h
