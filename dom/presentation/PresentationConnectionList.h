/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationConnectionList_h
#define mozilla_dom_PresentationConnectionList_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class PresentationConnection;
class Promise;

class PresentationConnectionList final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationConnectionList,
                                           DOMEventTargetHelper)

  PresentationConnectionList(nsPIDOMWindowInner* aWindow,
                             Promise* aPromise);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetConnections(nsTArray<RefPtr<PresentationConnection>>& aConnections) const;

  void NotifyStateChange(const nsAString& aSessionId, PresentationConnection* aConnection);

  IMPL_EVENT_HANDLER(connectionavailable);

private:
  virtual ~PresentationConnectionList() = default;

  nsresult DispatchConnectionAvailableEvent(PresentationConnection* aConnection);

  typedef nsTArray<RefPtr<PresentationConnection>> ConnectionArray;
  typedef ConnectionArray::index_type ConnectionArrayIndex;

  ConnectionArrayIndex FindConnectionById(const nsAString& aId);

  RefPtr<Promise> mGetConnectionListPromise;

  // This array stores only non-terminsted connections.
  ConnectionArray mConnections;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationConnectionList_h
