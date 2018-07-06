/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/dom/XULFrameElementBinding.h"

namespace mozilla {
namespace dom {

JSObject*
XULFrameElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XULFrameElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsIDocShell*
XULFrameElement::GetDocShell()
{
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  return frameLoader ? frameLoader->GetDocShell(IgnoreErrors()) : nullptr;
}

already_AddRefed<nsIWebNavigation>
XULFrameElement::GetWebNavigation()
{
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  nsCOMPtr<nsIWebNavigation> webnav = do_QueryInterface(docShell);
  return webnav.forget();
}

already_AddRefed<nsPIDOMWindowOuter>
XULFrameElement::GetContentWindow()
{
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (docShell) {
    nsCOMPtr<nsPIDOMWindowOuter> win = docShell->GetWindow();
    return win.forget();
  }

  return nullptr;
}

nsIDocument*
XULFrameElement::GetContentDocument()
{
  nsCOMPtr<nsPIDOMWindowOuter> win = GetContentWindow();
  return win ? win->GetDoc() : nullptr;
}

} // namespace dom
} // namespace mozilla
