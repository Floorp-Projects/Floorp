/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(BackgroundVideoDecodingPermissionObserver_h_)
#define BackgroundVideoDecodingPermissionObserver_h_

#include "nsIObserver.h"
#include "nsISupportsImpl.h"

class nsIDocument;
class nsISupports;
class nsPIDOMWindowOuter;

namespace mozilla {

class MediaDecoder;

class BackgroundVideoDecodingPermissionObserver final
  : public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS

    explicit BackgroundVideoDecodingPermissionObserver(MediaDecoder* aDecoder);

    NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) override;
    void RegisterEvent();
    void UnregisterEvent();
  private:
    ~BackgroundVideoDecodingPermissionObserver();
    void EnableEvent() const;
    void DisableEvent() const;
    already_AddRefed<nsPIDOMWindowOuter> GetOwnerWindow() const;
    nsIDocument* GetOwnerDoc() const;
    bool IsValidEventSender(nsISupports* aSubject) const;

    // The life cycle of observer would always be shorter than decoder, so we
    // use raw pointer here.
    MediaDecoder* mDecoder;
    bool mIsRegisteredForEvent;
};

} // namespace mozilla

#endif // BackgroundVideoDecodingPermissionObserver_h_
