/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ParentSHistory.h"
#include "mozilla/dom/ParentSHistoryBinding.h"
#include "mozilla/dom/TabParent.h"
#include "nsDocShell.h"
#include "nsFrameLoader.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

ParentSHistory::ParentSHistory(nsFrameLoader* aFrameLoader)
  : mFrameLoader(aFrameLoader)
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

ParentSHistory::~ParentSHistory()
{
}

nsDocShell*
ParentSHistory::GetDocShell()
{
  return nsDocShell::Cast(mFrameLoader->GetExistingDocShell());
}

TabParent*
ParentSHistory::GetTabParent()
{
  return static_cast<TabParent*>(mFrameLoader->GetRemoteBrowser());
}

already_AddRefed<ChildSHistory>
ParentSHistory::GetChildIfSameProcess()
{
  if (GetDocShell()) {
    return GetDocShell()->GetSessionHistory();
  }

  return nullptr;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ParentSHistory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ParentSHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ParentSHistory)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ParentSHistory,
                                      mFrameLoader)

JSObject*
ParentSHistory::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
  return ParentSHistory_Binding::Wrap(cx, this, aGivenProto);
}

nsISupports*
ParentSHistory::GetParentObject() const
{
  return mFrameLoader;
}

} // namespace dom
} // namespace mozilla
