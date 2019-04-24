/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * ParentSHistory is the ultimate source of truth for a browsing session's
 * history. It lives in the parent process and stores the complete view of
 * session history.
 *
 * NOTE: Currently session history is in transition, meaning that we're still
 * using the legacy nsSHistory class internally. This means that currently the
 * session history state lives in the content process, rather than in this
 * class.
 */

#ifndef mozilla_dom_ParentSHistory_h
#define mozilla_dom_ParentSHistory_h

#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "nsISHistory.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

class nsDocShell;

namespace mozilla {
namespace dom {

class BrowserParent;
class ChildSHistory;

class ParentSHistory : public nsISupports, public nsWrapperCache {
 public:
  friend class ChildSHistory;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ParentSHistory)
  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  explicit ParentSHistory(nsFrameLoader* aFrameLoader);

  // XXX(nika): Implement
  int32_t Count() { return 0; }
  int32_t Index() { return 0; }

 private:
  nsDocShell* GetDocShell();
  BrowserParent* GetBrowserParent();

  already_AddRefed<ChildSHistory> GetChildIfSameProcess();

  virtual ~ParentSHistory();

  RefPtr<nsFrameLoader> mFrameLoader;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_ParentSHistory_h */
