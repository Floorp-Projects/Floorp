/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for representation of media lists */

#include "mozilla/dom/MediaList.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/dom/MediaListBinding.h"
#include "mozilla/ServoMediaList.h"
#include "mozilla/StyleSheetInlines.h"
#include "nsCSSParser.h"
#include "nsMediaList.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaList)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MediaList)

JSObject*
MediaList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaListBinding::Wrap(aCx, this, aGivenProto);
}

void
MediaList::SetStyleSheet(StyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet == mStyleSheet || !aSheet || !mStyleSheet,
             "Multiple style sheets competing for one media list");
  mStyleSheet = aSheet;
}

template<typename Func>
nsresult
MediaList::DoMediaChange(Func aCallback)
{
  nsCOMPtr<nsIDocument> doc;
  if (mStyleSheet) {
    doc = mStyleSheet->GetAssociatedDocument();
  }
  mozAutoDocUpdate updateBatch(doc, UPDATE_STYLE, true);
  if (mStyleSheet) {
    mStyleSheet->WillDirty();
  }

  nsresult rv = aCallback();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mStyleSheet) {
    mStyleSheet->DidDirty();
  }
  /* XXXldb Pass something meaningful? */
  if (doc) {
    doc->StyleRuleChanged(mStyleSheet, nullptr);
  }
  return rv;
}

/* static */ already_AddRefed<MediaList>
MediaList::Create(
    StyleBackendType aBackendType,
    const nsAString& aMedia,
    CallerType aCallerType)
{
  if (aBackendType == StyleBackendType::Servo) {
    RefPtr<ServoMediaList> mediaList = new ServoMediaList(aMedia, aCallerType);
    return mediaList.forget();
  }

  nsCSSParser parser;
  RefPtr<nsMediaList> mediaList = new nsMediaList();
  parser.ParseMediaList(aMedia, nullptr, 0, mediaList, aCallerType);
  return mediaList.forget();
}

NS_IMETHODIMP
MediaList::GetMediaText(nsAString& aMediaText)
{
  GetText(aMediaText);
  return NS_OK;
}

NS_IMETHODIMP
MediaList::SetMediaText(const nsAString& aMediaText)
{
  return DoMediaChange([&]() {
    SetText(aMediaText);
    return NS_OK;
  });
}

NS_IMETHODIMP
MediaList::GetLength(uint32_t* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = Length();
  return NS_OK;
}

NS_IMETHODIMP
MediaList::Item(uint32_t aIndex, nsAString& aReturn)
{
  bool dummy;
  IndexedGetter(aIndex, dummy, aReturn);
  return NS_OK;
}

NS_IMETHODIMP
MediaList::DeleteMedium(const nsAString& aOldMedium)
{
  return DoMediaChange([&]() { return Delete(aOldMedium); });
}

NS_IMETHODIMP
MediaList::AppendMedium(const nsAString& aNewMedium)
{
  return DoMediaChange([&]() { return Append(aNewMedium); });
}

} // namespace dom
} // namespace mozilla
