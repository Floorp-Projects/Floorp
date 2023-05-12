/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for representation of media lists */

#ifndef mozilla_dom_MediaList_h
#define mozilla_dom_MediaList_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoUtils.h"

#include "nsWrapperCache.h"

class nsMediaQueryResultCacheKey;

namespace mozilla {
class ErrorResult;
class StyleSheet;

namespace dom {

class Document;

class MediaList final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MediaList)

  // Needed for CSSOM, but please don't use it outside of that :)
  explicit MediaList(already_AddRefed<RawServoMediaList> aRawList)
      : mRawList(aRawList) {}

  static already_AddRefed<MediaList> Create(
      const nsACString& aMedia, CallerType aCallerType = CallerType::NonSystem);

  already_AddRefed<MediaList> Clone();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
  nsISupports* GetParentObject() const;

  void GetText(nsACString& aMediaText);
  void SetText(const nsACString& aMediaText);
  bool Matches(const Document&) const;

  void SetStyleSheet(StyleSheet* aSheet);
  void SetRawAfterClone(RefPtr<RawServoMediaList> aRaw) {
    mRawList = std::move(aRaw);
  }

  // WebIDL
  void GetMediaText(nsACString& aMediaText);
  void SetMediaText(const nsACString& aMediaText);
  uint32_t Length();
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsACString& aReturn);
  void Item(uint32_t aIndex, nsACString& aResult);
  void DeleteMedium(const nsACString& aMedium, ErrorResult& aRv);
  void AppendMedium(const nsACString& aMedium, ErrorResult& aRv);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 protected:
  MediaList(const nsACString& aMedia, CallerType);
  MediaList();

  void SetTextInternal(const nsACString& aMediaText, CallerType);

  void Delete(const nsACString& aOldMedium, ErrorResult& aRv);
  void Append(const nsACString& aNewMedium, ErrorResult& aRv);

  ~MediaList() {
    MOZ_ASSERT(!mStyleSheet, "Backpointer should have been cleared");
  }

  bool IsReadOnly() const;

  // not refcounted; sheet will let us know when it goes away
  // mStyleSheet is the sheet that needs to be dirtied when this
  // medialist changes
  StyleSheet* mStyleSheet = nullptr;

 private:
  template <typename Func>
  inline void DoMediaChange(Func aCallback, ErrorResult& aRv);
  RefPtr<RawServoMediaList> mRawList;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaList_h
