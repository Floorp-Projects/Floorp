/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileCreatorParent_h
#define mozilla_dom_FileCreatorParent_h

#include "mozilla/dom/PFileCreatorParent.h"

class nsIFile;

namespace mozilla {
namespace dom {

class BlobImpl;

class FileCreatorParent final : public mozilla::dom::PFileCreatorParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileCreatorParent)

  FileCreatorParent();

  mozilla::ipc::IPCResult CreateAndShareFile(
      const nsString& aFullPath, const nsString& aType, const nsString& aName,
      const Maybe<int64_t>& aLastModified, const bool& aExistenceCheck,
      const bool& aIsFromNsIFile);

 private:
  ~FileCreatorParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult CreateBlobImpl(const nsAString& aPath, const nsAString& aType,
                          const nsAString& aName, bool aLastModifiedPassed,
                          int64_t aLastModified, bool aExistenceCheck,
                          bool aIsFromNsIFile, BlobImpl** aBlobImpl);

  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
  bool mIPCActive;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileCreatorParent_h
