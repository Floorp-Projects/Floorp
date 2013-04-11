/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPIDOMStorage_h_
#define __nsPIDOMStorage_h_

#include "nsISupports.h"
#include "nsTArray.h"

class nsIDOMStorageObsolete;
class nsIURI;
class nsIPrincipal;

#define NS_PIDOMSTORAGE_IID \
{ 0x9c292365, 0x6ae4, 0x461b,                           \
  { 0x86, 0x98, 0xf5, 0x23, 0x6f, 0xfa, 0xd2, 0x30 } }

class nsPIDOMStorage : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMSTORAGE_IID)

  typedef enum {
    Unknown = 0,
    LocalStorage = 1,
    SessionStorage = 2
  } nsDOMStorageType;

  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                        bool aPrivate) = 0;
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                      bool aPrivate) = 0;

  virtual already_AddRefed<nsIDOMStorage> Clone() = 0;
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI) = 0;
  virtual bool IsForkOf(nsIDOMStorage* aThat) = 0;

  virtual nsTArray<nsString> *GetKeys() = 0;

  virtual nsIPrincipal* Principal() = 0;
  virtual bool CanAccess(nsIPrincipal *aPrincipal) = 0;

  virtual nsDOMStorageType StorageType() = 0;

  virtual bool IsPrivate() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMStorage, NS_PIDOMSTORAGE_IID)

#endif // __nsPIDOMStorage_h_
