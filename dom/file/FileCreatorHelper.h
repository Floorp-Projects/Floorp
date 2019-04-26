/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileCreatorHelper_h
#define mozilla_dom_FileCreatorHelper_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

// Undefine the macro of CreateFile to avoid FileCreatorHelper#CreateFile being
// replaced by FileCreatorHelper#CreateFileW.
#ifdef CreateFile
#  undef CreateFile
#endif

class nsIFile;
class nsIGlobalObject;

namespace mozilla {
namespace dom {

struct ChromeFilePropertyBag;
class Promise;

class FileCreatorHelper final {
 public:
  static already_AddRefed<Promise> CreateFile(nsIGlobalObject* aGlobalObject,
                                              nsIFile* aFile,
                                              const ChromeFilePropertyBag& aBag,
                                              bool aIsFromNsIFile,
                                              ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileCreatorHelper_h
