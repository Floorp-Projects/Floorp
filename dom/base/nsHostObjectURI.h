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
#include "nsIURIWithBlobImpl.h"
#include "nsIURIWithPrincipal.h"
#include "nsSimpleURI.h"
#include "nsIIPCSerializableURI.h"

/**
 * These URIs refer to host objects: Blobs, with scheme "blob",
 * MediaStreams, with scheme "mediastream", and MediaSources, with scheme
 * "mediasource".
 */
class nsHostObjectURI : public mozilla::net::nsSimpleURI
                      , public nsIURIWithPrincipal
                      , public nsIURIWithBlobImpl
{
public:
  nsHostObjectURI(nsIPrincipal* aPrincipal,
                  mozilla::dom::BlobImpl* aBlobImpl)
    : mozilla::net::nsSimpleURI()
    , mPrincipal(aPrincipal)
    , mBlobImpl(aBlobImpl)
  {}

  // For use only from deserialization
  nsHostObjectURI() : mozilla::net::nsSimpleURI() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIURIWITHBLOBIMPL
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIIPCSERIALIZABLEURI

  NS_IMETHOD SetScheme(const nsACString &aProtocol) override;

  // Override CloneInternal() and EqualsInternal()
  virtual nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                                 nsIURI** aClone) override;
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult) override;

  // Override StartClone to hand back a nsHostObjectURI
  virtual mozilla::net::nsSimpleURI* StartClone(RefHandlingEnum /* unused */) override
  { return new nsHostObjectURI(); }

  nsCOMPtr<nsIPrincipal> mPrincipal;
  RefPtr<mozilla::dom::BlobImpl> mBlobImpl;

protected:
  virtual ~nsHostObjectURI() {}
};

#define NS_HOSTOBJECTURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

#endif /* nsHostObjectURI_h */
