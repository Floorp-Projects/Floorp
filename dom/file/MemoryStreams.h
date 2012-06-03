/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_memorystreams_h__
#define mozilla_dom_file_memorystreams_h__

#include "FileCommon.h"

#include "nsIOutputStream.h"

BEGIN_FILE_NAMESPACE

class MemoryOutputStream : public nsIOutputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  static already_AddRefed<MemoryOutputStream>
  Create(PRUint64 aSize);


  const nsCString&
  Data() const
  {
    return mData;
  }

private:
  MemoryOutputStream()
  : mOffset(0)
  { }

  virtual ~MemoryOutputStream()
  { }

  nsCString mData;
  PRUint64 mOffset;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_memorystreams_h__
