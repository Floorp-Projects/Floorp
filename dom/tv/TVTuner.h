/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVTuner_h
#define mozilla_dom_TVTuner_h

#include "mozilla/DOMEventTargetHelper.h"
// Include TVTunerBinding.h since enum TVSourceType can't be forward declared.
#include "mozilla/dom/TVTunerBinding.h"

class nsITVService;
class nsITVTunerData;

namespace mozilla {

class DOMMediaStream;

namespace dom {

class Promise;
class TVSource;

class TVTuner final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVTuner, DOMEventTargetHelper)

  static already_AddRefed<TVTuner> Create(nsPIDOMWindow* aWindow,
                                          nsITVTunerData* aData);
  nsresult NotifyImageSizeChanged(uint32_t aWidth, uint32_t aHeight);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsresult SetCurrentSource(TVSourceType aSourceType);

  nsresult DispatchTVEvent(nsIDOMEvent* aEvent);

  // WebIDL (public APIs)

  void GetSupportedSourceTypes(nsTArray<TVSourceType>& aSourceTypes,
                               ErrorResult& aRv) const;

  already_AddRefed<Promise> GetSources(ErrorResult& aRv);

  already_AddRefed<Promise> SetCurrentSource(const TVSourceType aSourceType,
                                             ErrorResult& aRv);

  void GetId(nsAString& aId) const;

  already_AddRefed<TVSource> GetCurrentSource() const;

  already_AddRefed<DOMMediaStream> GetStream() const;

  IMPL_EVENT_HANDLER(currentsourcechanged);

private:
  explicit TVTuner(nsPIDOMWindow* aWindow);

  ~TVTuner();

  bool Init(nsITVTunerData* aData);

  nsresult InitMediaStream();

  already_AddRefed<DOMMediaStream> CreateSimulatedMediaStream();

  nsresult DispatchCurrentSourceChangedEvent(TVSource* aSource);

  nsCOMPtr<nsITVService> mTVService;
  nsRefPtr<DOMMediaStream> mStream;
  uint16_t mStreamType;
  nsRefPtr<TVSource> mCurrentSource;
  nsTArray<nsRefPtr<TVSource>> mSources;
  nsString mId;
  nsTArray<TVSourceType> mSupportedSourceTypes;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVTuner_h
