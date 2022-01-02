/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/dom/TextTrackCueListBinding.h"
#include "mozilla/dom/TextTrackCue.h"

namespace mozilla::dom {

class CompareCuesByTime {
 public:
  bool Equals(TextTrackCue* aOne, TextTrackCue* aTwo) const { return false; }
  bool LessThan(TextTrackCue* aOne, TextTrackCue* aTwo) const {
    return aOne->StartTime() < aTwo->StartTime() ||
           (aOne->StartTime() == aTwo->StartTime() &&
            aOne->EndTime() >= aTwo->EndTime());
  }
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TextTrackCueList, mParent, mList)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackCueList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackCueList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackCueList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TextTrackCueList::TextTrackCueList(nsISupports* aParent) : mParent(aParent) {}

TextTrackCueList::~TextTrackCueList() = default;

JSObject* TextTrackCueList::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return TextTrackCueList_Binding::Wrap(aCx, this, aGivenProto);
}

TextTrackCue* TextTrackCueList::IndexedGetter(uint32_t aIndex, bool& aFound) {
  aFound = aIndex < mList.Length();
  if (!aFound) {
    return nullptr;
  }
  return mList[aIndex];
}

TextTrackCue* TextTrackCueList::operator[](uint32_t aIndex) {
  return mList.SafeElementAt(aIndex, nullptr);
}

TextTrackCueList& TextTrackCueList::operator=(const TextTrackCueList& aOther) {
  mList = aOther.mList.Clone();
  return *this;
}

TextTrackCue* TextTrackCueList::GetCueById(const nsAString& aId) {
  if (aId.IsEmpty()) {
    return nullptr;
  }

  for (uint32_t i = 0; i < mList.Length(); i++) {
    if (aId.Equals(mList[i]->Id())) {
      return mList[i];
    }
  }
  return nullptr;
}

void TextTrackCueList::AddCue(TextTrackCue& aCue) {
  if (mList.Contains(&aCue)) {
    return;
  }
  mList.InsertElementSorted(&aCue, CompareCuesByTime());
}

void TextTrackCueList::RemoveCue(TextTrackCue& aCue, ErrorResult& aRv) {
  if (!mList.Contains(&aCue)) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  mList.RemoveElement(&aCue);
}

void TextTrackCueList::RemoveCue(TextTrackCue& aCue) {
  mList.RemoveElement(&aCue);
}

void TextTrackCueList::RemoveCueAt(uint32_t aIndex) {
  if (aIndex < mList.Length()) {
    mList.RemoveElementAt(aIndex);
  }
}

void TextTrackCueList::RemoveAll() { mList.Clear(); }

void TextTrackCueList::GetArray(nsTArray<RefPtr<TextTrackCue>>& aCues) {
  aCues = mList.Clone();
}

void TextTrackCueList::SetCuesInactive() {
  for (uint32_t i = 0; i < mList.Length(); ++i) {
    mList[i]->SetActive(false);
  }
}

void TextTrackCueList::NotifyCueUpdated(TextTrackCue* aCue) {
  if (aCue) {
    mList.RemoveElement(aCue);
    mList.InsertElementSorted(aCue, CompareCuesByTime());
  }
}

bool TextTrackCueList::IsCueExist(TextTrackCue* aCue) {
  if (aCue && mList.Contains(aCue)) {
    return true;
  }
  return false;
}

nsTArray<RefPtr<TextTrackCue>>& TextTrackCueList::GetCuesArray() {
  return mList;
}

}  // namespace mozilla::dom
