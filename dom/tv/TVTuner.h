/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVTuner_h__
#define mozilla_dom_TVTuner_h__

#include "mozilla/DOMEventTargetHelper.h"
// Include TVTunerBinding.h since enum TVSourceType can't be forward declared.
#include "mozilla/dom/TVTunerBinding.h"

namespace mozilla {

class DOMMediaStream;

namespace dom {

class Promise;
class TVSource;

class TVTuner MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVTuner, DOMEventTargetHelper)

  explicit TVTuner(nsPIDOMWindow* aWindow);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

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
  ~TVTuner();

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVTuner_h__
