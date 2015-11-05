/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserElementAudioChannels_h
#define mozilla_dom_BrowserElementAudioChannels_h

#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsIFrameLoader.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

class nsIBrowserElementAPI;
class nsITabParent;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class DOMRequest;

class BrowserElementAudioChannel final : public DOMEventTargetHelper
                                       , public nsSupportsWeakReference
                                       , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BrowserElementAudioChannel,
                                           DOMEventTargetHelper)

  static already_AddRefed<BrowserElementAudioChannel>
  Create(nsPIDOMWindow* aWindow,
         nsIFrameLoader* aFrameLoader,
         nsIBrowserElementAPI* aAPI,
         AudioChannel aAudioChannel,
         ErrorResult& aRv);

  // WebIDL methods

  virtual JSObject* WrapObject(JSContext *aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  AudioChannel Name() const;

  already_AddRefed<dom::DOMRequest> GetVolume(ErrorResult& aRv);
  already_AddRefed<dom::DOMRequest> SetVolume(float aVolume, ErrorResult& aRv);

  already_AddRefed<dom::DOMRequest> GetMuted(ErrorResult& aRv);
  already_AddRefed<dom::DOMRequest> SetMuted(bool aMuted, ErrorResult& aRv);

  already_AddRefed<dom::DOMRequest> IsActive(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(activestatechanged);

private:
  BrowserElementAudioChannel(nsPIDOMWindow* aWindow,
                             nsIFrameLoader* aFrameLoader,
                             nsIBrowserElementAPI* aAPI,
                             AudioChannel aAudioChannel);

  ~BrowserElementAudioChannel();

  nsresult Initialize();

  void ProcessStateChanged(const char16_t* aData);

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
  nsCOMPtr<nsIBrowserElementAPI> mBrowserElementAPI;
  nsCOMPtr<nsITabParent> mTabParent;
  nsCOMPtr<nsPIDOMWindow> mFrameWindow;
  AudioChannel mAudioChannel;

  enum {
    eStateActive,
    eStateInactive,
    eStateUnknown
  } mState;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_BrowserElementAudioChannels_h
