/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileMessageThread_h
#define mozilla_dom_MobileMessageThread_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

namespace mobilemessage {
class MobileMessageThreadInternal;
} // namespace mobilemessage

/**
 * Each instance of this class provides the DOM-level representation of
 * a MobileMessageThread object to bind it to a window being exposed to.
 */
class MobileMessageThread final : public nsISupports,
                                  public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MobileMessageThread)

  MobileMessageThread(nsPIDOMWindowInner* aWindow,
                      mobilemessage::MobileMessageThreadInternal* aThread);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx,
             JS::Handle<JSObject*> aGivenProto) override;

  uint64_t
  Id() const;

  void
  GetLastMessageSubject(nsString& aRetVal) const;

  void
  GetBody(nsString& aRetVal) const;

  uint64_t
  UnreadCount() const;

  void
  GetParticipants(nsTArray<nsString>& aRetVal) const;

  DOMTimeStamp
  Timestamp() const;

  void
  GetLastMessageType(nsString& aRetVal) const;

private:
  // Don't try to use the default constructor.
  MobileMessageThread() = delete;

  ~MobileMessageThread();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<mobilemessage::MobileMessageThreadInternal> mThread;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MobileMessageThread_h
