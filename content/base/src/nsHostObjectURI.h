/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHostObjectURI_h
#define nsHostObjectURI_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsIPrincipal.h"
#include "nsISerializable.h"
#include "nsIURIWithPrincipal.h"
#include "nsSimpleURI.h"

/**
 * These URIs refer to host objects: Blobs, with scheme "blob", and
 * MediaStreams, with scheme "mediastream".
 */
class nsHostObjectURI : public nsSimpleURI,
                        public nsIURIWithPrincipal
{
public:
  nsHostObjectURI(nsIPrincipal* aPrincipal) :
      nsSimpleURI(), mPrincipal(aPrincipal)
  {}
  virtual ~nsHostObjectURI() {}

  // For use only from deserialization
  nsHostObjectURI() : nsSimpleURI() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  // Override CloneInternal() and EqualsInternal()
  virtual nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                                 nsIURI** aClone) MOZ_OVERRIDE;
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult) MOZ_OVERRIDE;

  // Override StartClone to hand back a nsHostObjectURI
  virtual nsSimpleURI* StartClone(RefHandlingEnum /* unused */) MOZ_OVERRIDE
  { return new nsHostObjectURI(); }

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

#define NS_HOSTOBJECTURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

#endif /* nsHostObjectURI_h */
