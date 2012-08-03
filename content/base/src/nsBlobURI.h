/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBlobURI_h
#define nsBlobURI_h

#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsIPrincipal.h"
#include "nsISerializable.h"
#include "nsIURIWithPrincipal.h"
#include "nsSimpleURI.h"

class nsBlobURI : public nsSimpleURI,
                  public nsIURIWithPrincipal
{
public:
  nsBlobURI(nsIPrincipal* aPrincipal) :
      nsSimpleURI(), mPrincipal(aPrincipal)
  {}
  virtual ~nsBlobURI() {}

  // For use only from deserialization
  nsBlobURI() : nsSimpleURI() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIURIWITHPRINCIPAL
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  // Override CloneInternal() and EqualsInternal()
  virtual nsresult CloneInternal(RefHandlingEnum aRefHandlingMode,
                                 nsIURI** aClone);
  virtual nsresult EqualsInternal(nsIURI* aOther,
                                  RefHandlingEnum aRefHandlingMode,
                                  bool* aResult);

  // Override StartClone to hand back a nsBlobURI
  virtual nsSimpleURI* StartClone(RefHandlingEnum /* unused */)
  { return new nsBlobURI(); }

  nsCOMPtr<nsIPrincipal> mPrincipal;
};

#define NS_BLOBURI_CID \
{ 0xf5475c51, 0x59a7, 0x4757, \
  { 0xb3, 0xd9, 0xe2, 0x11, 0xa9, 0x41, 0x08, 0x72 } }

#endif /* nsBlobURI_h */
