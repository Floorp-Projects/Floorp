/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeySystemAccess_h
#define mozilla_dom_MediaKeySystemAccess_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/MediaKeySystemAccessBinding.h"
#include "mozilla/dom/MediaKeysRequestStatusBinding.h"

#include "js/TypeDecls.h"

namespace mozilla {

class DecoderDoctorDiagnostics;

namespace dom {

class MediaKeySystemAccess final : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaKeySystemAccess)

public:
  explicit MediaKeySystemAccess(nsPIDOMWindowInner* aParent,
                                const nsAString& aKeySystem,
                                const MediaKeySystemConfiguration& aConfig);

protected:
  ~MediaKeySystemAccess();

public:
  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetKeySystem(nsString& aRetVal) const;

  void GetConfiguration(MediaKeySystemConfiguration& aConfig);

  already_AddRefed<Promise> CreateMediaKeys(ErrorResult& aRv);

  static MediaKeySystemStatus GetKeySystemStatus(const nsAString& aKeySystem,
                                                 int32_t aMinCdmVersion,
                                                 nsACString& aOutExceptionMessage,
                                                 nsACString& aOutCdmVersion);

  static bool IsSupported(const nsAString& aKeySystem,
                          const Sequence<MediaKeySystemConfiguration>& aConfigs,
                          DecoderDoctorDiagnostics* aDiagnostics);

  static void NotifyObservers(nsPIDOMWindowInner* aWindow,
                              const nsAString& aKeySystem,
                              MediaKeySystemStatus aStatus);

  static bool GetSupportedConfig(const nsAString& aKeySystem,
                                 const Sequence<MediaKeySystemConfiguration>& aConfigs,
                                 MediaKeySystemConfiguration& aOutConfig,
                                 DecoderDoctorDiagnostics* aDiagnostics);

  static bool KeySystemSupportsInitDataType(const nsAString& aKeySystem,
                                            const nsAString& aInitDataType);

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  const nsString mKeySystem;
  const MediaKeySystemConfiguration mConfig;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MediaKeySystemAccess_h
