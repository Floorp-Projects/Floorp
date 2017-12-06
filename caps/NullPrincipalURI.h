/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This wraps nsSimpleURI so that all calls to it are done on the main thread.
 */

#ifndef __NullPrincipalURI_h__
#define __NullPrincipalURI_h__

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

class NullPrincipalURI final : public nsIURI
                             , public nsISizeOf
                             , public nsIIPCSerializableURI
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIIPCSERIALIZABLEURI

  // nsISizeOf
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  // Returns null on failure.
  static already_AddRefed<NullPrincipalURI> Create();

private:
  NullPrincipalURI();
  NullPrincipalURI(const NullPrincipalURI& aOther);

  ~NullPrincipalURI() {}

  nsresult Init();

  nsAutoCStringN<NSID_LENGTH> mPath;

public:
  class Mutator
      : public nsIURIMutator
      , public BaseURIMutator<NullPrincipalURI>
  {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)

    NS_IMETHOD Deserialize(const mozilla::ipc::URIParams& aParams) override
    {
      return InitFromIPCParams(aParams);
    }

    NS_IMETHOD Read(nsIObjectInputStream* aStream) override
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Finalize(nsIURI** aURI) override
    {
      mURI.forget(aURI);
      return NS_OK;
    }

    NS_IMETHOD SetSpec(const nsACString & aSpec, nsIURIMutator** aMutator) override
    {
      NS_ADDREF(*aMutator = this);
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    explicit Mutator() { }
  private:
    virtual ~Mutator() { }

    friend class NullPrincipalURI;
  };

  friend class BaseURIMutator<NullPrincipalURI>;
};

#endif // __NullPrincipalURI_h__
