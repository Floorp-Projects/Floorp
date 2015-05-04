/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableFile_h
#define mozilla_dom_MutableFile_h

#include "nsCOMPtr.h"
#include "nsString.h"

class nsIFile;
class nsIOfflineStorage;

namespace mozilla {
namespace dom {

class FileService;

/**
 * This class provides a base for MutableFile implementations.
 * The subclasses can override implementation of IsInvalid and CreateStream.
 * (for example IDBMutableFile provides IndexedDB specific implementation).
 */
class MutableFileBase
{
  friend class FileService;

public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;

  virtual bool
  IsInvalid()
  {
    return false;
  }

  // A temporary method that will be removed along with nsIOfflineStorage
  // interface.
  virtual nsIOfflineStorage*
  Storage() = 0;

  virtual already_AddRefed<nsISupports>
  CreateStream(bool aReadOnly);


protected:
  MutableFileBase();

  virtual ~MutableFileBase();

  nsCOMPtr<nsIFile> mFile;

  nsCString mStorageId;
  nsString mFileName;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MutableFile_h
