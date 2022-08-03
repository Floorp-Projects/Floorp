/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSetWorkerImpl_h
#define mozilla_dom_FontFaceSetWorkerImpl_h

#include "mozilla/dom/FontFaceSetImpl.h"

namespace mozilla::dom {
class ThreadSafeWorkerRef;
class WorkerPrivate;

class FontFaceSetWorkerImpl final : public FontFaceSetImpl {
  NS_DECL_ISUPPORTS_INHERITED

 public:
  explicit FontFaceSetWorkerImpl(FontFaceSet* aOwner);

  bool Initialize(WorkerPrivate* aWorkerPrivate);
  void Destroy() override;

  bool IsOnOwningThread() override;
  void DispatchToOwningThread(const char* aName,
                              std::function<void()>&& aFunc) override;

  already_AddRefed<URLExtraData> GetURLExtraData() override;

  void FlushUserFontSet() override;

  // gfxUserFontSet

  nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                     uint32_t aSrcIndex) override;

  bool IsFontLoadAllowed(const gfxFontFaceSrc&) override;

  nsPresContext* GetPresContext() const override;

 private:
  ~FontFaceSetWorkerImpl() override;

  void InitializeOnMainThread();

  uint64_t GetInnerWindowID() override;

  nsresult CreateChannelForSyncLoadFontData(
      nsIChannel** aOutChannel, gfxUserFontEntry* aFontToLoad,
      const gfxFontFaceSrc* aFontFaceSrc) override;

  TimeStamp GetNavigationStartTimeStamp() override;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef MOZ_GUARDED_BY(mMutex);

  RefPtr<URLExtraData> mURLExtraData MOZ_GUARDED_BY(mMutex);
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFaceSetWorkerImpl_h)
