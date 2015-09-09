/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileRequest_h
#define mozilla_dom_FileRequest_h

#include "FileHandleCommon.h"
#include "js/TypeDecls.h"
#include "nsString.h"

struct PRThread;

namespace mozilla {
namespace dom {

class FileHandleBase;

/**
 * This class provides a base for FileRequest implementations.
 */
class FileRequestBase
  : public RefCountedThreadObject
{
  nsString mEncoding;

  bool mHasEncoding;

public:
  class ResultCallback;

  void
  SetEncoding(const nsAString& aEncoding)
  {
    mEncoding = aEncoding;
    mHasEncoding = true;
  }

  const nsAString&
  GetEncoding() const
  {
    return mEncoding;
  }

  bool
  HasEncoding() const
  {
    return mHasEncoding;
  }

  virtual FileHandleBase*
  FileHandle() const = 0;

  virtual void
  OnProgress(uint64_t aProgress, uint64_t aProgressMax) = 0;

  virtual void
  SetResultCallback(ResultCallback* aCallback) = 0;

  virtual void
  SetError(nsresult aError) = 0;

protected:
  FileRequestBase(DEBUGONLY(PRThread* aOwningThread))
    : RefCountedThreadObject(DEBUGONLY(aOwningThread))
    , mHasEncoding(false)
  {
    AssertIsOnOwningThread();
  }

  virtual ~FileRequestBase()
  {
    AssertIsOnOwningThread();
  }
};

class NS_NO_VTABLE FileRequestBase::ResultCallback
{
public:
  virtual nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) = 0;

protected:
  ResultCallback()
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileRequest_h
