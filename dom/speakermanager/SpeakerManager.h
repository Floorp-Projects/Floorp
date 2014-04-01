/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeakerManager_h
#define mozilla_dom_SpeakerManager_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MozSpeakerManagerBinding.h"

namespace mozilla {
namespace dom {
/* This class is used for UA to control devices's speaker status.
 * After UA set the speaker status, the UA should handle the
 * forcespeakerchange event and change the speaker status in UI.
 * The device's speaker status would set back to normal when UA close the application.
 */
class SpeakerManager MOZ_FINAL
  : public DOMEventTargetHelper
  , public nsIDOMEventListener
{
  friend class SpeakerManagerService;
  friend class SpeakerManagerServiceChild;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER

public:
  void Init(nsPIDOMWindow* aWindow);

  nsPIDOMWindow* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
  /**
   * WebIDL Interface
   */
  // Get this api's force speaker setting.
  bool Forcespeaker();
  // Force acoustic sound go through speaker. Don't force to speaker if application
  // stay in the background and re-force when application
  // go to foreground
  void SetForcespeaker(bool aEnable);
  // Get the device's speaker forced setting.
  bool Speakerforced();

  void SetAudioChannelActive(bool aIsActive);
  IMPL_EVENT_HANDLER(speakerforcedchange)

  static already_AddRefed<SpeakerManager>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

protected:
  SpeakerManager();
  ~SpeakerManager();
  void DispatchSimpleEvent(const nsAString& aStr);
  // This api's force speaker setting
  bool mForcespeaker;
  bool mVisible;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SpeakerManager_h
