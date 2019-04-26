/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileCreatorChild_h
#define mozilla_dom_FileCreatorChild_h

#include "mozilla/dom/PFileCreatorChild.h"

namespace mozilla {
namespace dom {

class FileCreatorChild final : public mozilla::dom::PFileCreatorChild {
  friend class mozilla::dom::PFileCreatorChild;

 public:
  FileCreatorChild();
  ~FileCreatorChild();

  void SetPromise(Promise* aPromise);

 private:
  mozilla::ipc::IPCResult Recv__delete__(
      const FileCreationResult& aResult) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<Promise> mPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FileCreatorChild_h
