/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIFileStorage_h__
#define nsIFileStorage_h__

#include "nsISupports.h"

#define NS_FILESTORAGE_IID \
  {0xa0801944, 0x2f1c, 0x4203, \
  { 0x9c, 0xaa, 0xaa, 0x47, 0xe0, 0x0c, 0x67, 0x92 } }

class nsIFileStorage : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FILESTORAGE_IID)

  virtual const nsACString&
  StorageOrigin() = 0;

  virtual nsISupports*
  StorageId() = 0;

  virtual bool
  IsStorageInvalidated() = 0;

  virtual bool
  IsStorageShuttingDown() = 0;

  virtual void
  SetThreadLocals() = 0;

  virtual void
  UnsetThreadLocals() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFileStorage, NS_FILESTORAGE_IID)

#define NS_DECL_NSIFILESTORAGE                                                 \
  virtual const nsACString&                                                    \
  StorageOrigin() MOZ_OVERRIDE;                                                \
                                                                               \
  virtual nsISupports*                                                         \
  StorageId() MOZ_OVERRIDE;                                                    \
                                                                               \
  virtual bool                                                                 \
  IsStorageInvalidated() MOZ_OVERRIDE;                                         \
                                                                               \
  virtual bool                                                                 \
  IsStorageShuttingDown() MOZ_OVERRIDE;                                        \
                                                                               \
  virtual void                                                                 \
  SetThreadLocals() MOZ_OVERRIDE;                                              \
                                                                               \
  virtual void                                                                 \
  UnsetThreadLocals() MOZ_OVERRIDE;

#endif // nsIFileStorage_h__
