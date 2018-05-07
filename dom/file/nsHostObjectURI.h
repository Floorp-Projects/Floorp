/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostObjectURI_h
#define nsHostObjectURI_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/File.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsIPrincipal.h"
#include "nsISerializable.h"
#include "nsIURIWithPrincipal.h"
#include "nsSimpleURI.h"
#include "nsIIPCSerializableURI.h"
#include "nsProxyRelease.h"

/**
 * These URIs refer to host objects: Blobs, with scheme "blob",
 * MediaStreams, with scheme "mediastream", and MediaSources, with scheme
 * "mediasource".
 */
class nsHostObjectURI final
  : public mozilla::net::nsSimpleURI
  , public nsIURIWithPrincipal
{
private:
  explicit nsHostObjectURI(nsIPrincipal* aPrincipal)
    : mozilla::net::nsSimpleURI()
  {
    mPrincipal = new nsMainThreadPtrHolder<nsIPrincipal>("nsIPrincipal", aPrincipal, false);
  }

  // For use only from deserialization
  explicit nsHostObjectURI()
    : mozilla::net::nsSimpleURI()
  {}

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIIPCSERIALIZABLEURI

  // Override CloneInternal() and EqualsInternal()
  virtual nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                                 const nsACString& newRef,
                                 nsIURI** aClone) override;
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult) override;

  // Override StartClone to hand back a nsHostObjectURI
  virtual mozilla::net::nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode,
                                                const nsACString& newRef) override
  {
    nsHostObjectURI* url = new nsHostObjectURI();
    SetRefOnClone(url, refHandlingMode, newRef);
    return url;
  }

  NS_IMETHOD Mutate(nsIURIMutator * *_retval) override;

  nsMainThreadPtrHandle<nsIPrincipal> mPrincipal;

protected:
  virtual ~nsHostObjectURI() {}

  nsresult SetScheme(const nsACString &aProtocol) override;
  bool Deserialize(const mozilla::ipc::URIParams&);
  nsresult ReadPrivate(nsIObjectInputStream *stream);

public:
  class Mutator final
    : public nsIURIMutator
    , public BaseURIMutator<nsHostObjectURI>
    , public nsIPrincipalURIMutator
    , public nsISerializable
  {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)
    NS_DEFINE_NSIMUTATOR_COMMON

    NS_IMETHOD
    Write(nsIObjectOutputStream *aOutputStream) override
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    MOZ_MUST_USE NS_IMETHOD
    Read(nsIObjectInputStream* aStream) override
    {
        return InitFromInputStream(aStream);
    }

    MOZ_MUST_USE NS_IMETHOD
    SetPrincipal(nsIPrincipal *aPrincipal) override
    {
        if (!mURI) {
            return NS_ERROR_NULL_POINTER;
        }
        MOZ_ASSERT(NS_IsMainThread());
        mURI->mPrincipal = new nsMainThreadPtrHolder<nsIPrincipal>("nsIPrincipal", aPrincipal, false);
        return NS_OK;
    }

    explicit Mutator() { }
  private:
    virtual ~Mutator() { }

    friend class nsHostObjectURI;
  };

  friend BaseURIMutator<nsHostObjectURI>;
};

#define NS_HOSTOBJECTURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

#define NS_HOSTOBJECTURIMUTATOR_CID \
{ 0xbbe50ef2, 0x80eb, 0x469d, \
  { 0xb7, 0x0d, 0x02, 0x85, 0x82, 0x75, 0x38, 0x9f } }

#endif /* nsHostObjectURI_h */
