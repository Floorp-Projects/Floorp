/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaCapabilities_h_
#define mozilla_dom_MediaCapabilities_h_

#include "MediaContainerType.h"
#include "js/TypeDecls.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {

namespace dom {

struct MediaDecodingConfiguration;
struct MediaEncodingConfiguration;
struct AudioConfiguration;
struct VideoConfiguration;
class Promise;

class MediaCapabilities final
  : public nsISupports
  , public nsWrapperCache
{
public:
  // Ref counting and cycle collection
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaCapabilities)

  // WebIDL Methods
  already_AddRefed<Promise> DecodingInfo(
    const MediaDecodingConfiguration& aConfiguration,
    ErrorResult& aRv);
  already_AddRefed<Promise> EncodingInfo(
    const MediaEncodingConfiguration& aConfiguration,
    ErrorResult& aRv);
  // End WebIDL Methods

  explicit MediaCapabilities(nsIGlobalObject* aParent)
    : mParent(aParent)
  {
  }

  nsIGlobalObject* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static bool Enabled(JSContext* aCx, JSObject* aGlobal);

private:
  virtual ~MediaCapabilities() = default;
  bool CheckContentType(const nsAString& aMIMEType,
                        Maybe<MediaContainerType>& aContainer) const;
  bool CheckVideoConfiguration(const VideoConfiguration& aConfig) const;
  bool CheckAudioConfiguration(const AudioConfiguration& aConfig) const;
  bool CheckTypeForMediaSource(const nsAString& aType);
  bool CheckTypeForFile(const nsAString& aType);
  bool CheckTypeForEncoder(const nsAString& aType);
  nsCOMPtr<nsIGlobalObject> mParent;
};

class MediaCapabilitiesInfo final : public NonRefcountedDOMObject
{
public:
  // WebIDL methods
  bool Supported() const { return mSupported; }
  bool Smooth() const { return mSmooth; }
  bool PowerEfficient() const { return mPowerEfficient; }
  // End WebIDL methods

  MediaCapabilitiesInfo(bool aSupported, bool aSmooth, bool aPowerEfficient)
    : mSupported(aSupported)
    , mSmooth(aSmooth)
    , mPowerEfficient(aPowerEfficient)
  {
  }

  bool WrapObject(JSContext* aCx,
                  JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

private:
  bool mSupported;
  bool mSmooth;
  bool mPowerEfficient;
};

} // namespace dom

} // namespace mozilla

#endif /* mozilla_dom_MediaCapabilities_h_ */
