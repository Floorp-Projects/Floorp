/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVChannel_h
#define mozilla_dom_TVChannel_h

#include "mozilla/DOMEventTargetHelper.h"
// Include TVChannelBinding.h since enum TVChannelType can't be forward declared.
#include "mozilla/dom/TVChannelBinding.h"

class nsITVChannelData;
class nsITVService;

namespace mozilla {
namespace dom {

class Promise;
class TVSource;

class TVChannel final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVChannel, DOMEventTargetHelper)

  static already_AddRefed<TVChannel>
  Create(nsPIDOMWindowInner* aWindow,
         TVSource* aSource,
         nsITVChannelData* aData);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsresult DispatchTVEvent(nsIDOMEvent* aEvent);

  // WebIDL (public APIs)

  already_AddRefed<Promise> GetPrograms(const TVGetProgramsOptions& aOptions,
                                        ErrorResult& aRv);

  already_AddRefed<Promise> GetCurrentProgram(ErrorResult& aRv);

  void GetNetworkId(nsAString& aNetworkId) const;

  void GetTransportStreamId(nsAString& aTransportStreamId) const;

  void GetServiceId(nsAString& aServiceId) const;

  already_AddRefed<TVSource> Source() const;

  TVChannelType Type() const;

  void GetName(nsAString& aName) const;

  void GetNumber(nsAString& aNumber) const;

  bool IsEmergency() const;

  bool IsFree() const;

private:
  TVChannel(nsPIDOMWindowInner* aWindow,
            TVSource* aSource);

  ~TVChannel();

  bool Init(nsITVChannelData* aData);

  nsCOMPtr<nsITVService> mTVService;
  RefPtr<TVSource> mSource;
  nsString mNetworkId;
  nsString mTransportStreamId;
  nsString mServiceId;
  TVChannelType mType;
  nsString mNumber;
  nsString mName;
  bool mIsEmergency;
  bool mIsFree;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVChannel_h
