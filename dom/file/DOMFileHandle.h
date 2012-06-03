/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domfilehandle_h__
#define mozilla_dom_file_domfilehandle_h__

#include "FileCommon.h"

#include "FileHandle.h"

BEGIN_FILE_NAMESPACE

class DOMFileHandle : public FileHandle
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<DOMFileHandle>
  Create(nsPIDOMWindow* aWindow,
         nsIFileStorage* aFileStorage,
         nsIFile* aFile);

  virtual already_AddRefed<nsISupports>
  CreateStream(nsIFile* aFile, bool aReadOnly);

  virtual already_AddRefed<nsIDOMFile>
  CreateFileObject(LockedFile* aLockedFile, PRUint32 aFileSize);

protected:
  DOMFileHandle()
  { }

  ~DOMFileHandle()
  { }
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domfilehandle_h__
