/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileRequest_h
#define mozilla_dom_FileRequest_h

#include "nscore.h"

namespace mozilla {
namespace dom {

class FileHelper;

/**
 * This class provides a base for FileRequest implementations.
 */
class FileRequestBase
{
public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() = 0;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() = 0;

  virtual void
  OnProgress(uint64_t aProgress, uint64_t aProgressMax) = 0;

  virtual nsresult
  NotifyHelperCompleted(FileHelper* aFileHelper) = 0;

protected:
  FileRequestBase();

  virtual ~FileRequestBase();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileRequest_h
