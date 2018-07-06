/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULFrameElement_h__
#define XULFrameElement_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "js/TypeDecls.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsXULElement.h"

class nsIWebNavigation;

namespace mozilla {
namespace dom {

class XULFrameElement final : public nsXULElement
{
public:
  explicit XULFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsXULElement(aNodeInfo)
  {
  }

  nsIDocShell* GetDocShell();
  already_AddRefed<nsIWebNavigation> GetWebNavigation();
  already_AddRefed<nsPIDOMWindowOuter> GetContentWindow();
  nsIDocument* GetContentDocument();

protected:
  virtual ~XULFrameElement()
  {
  }

  JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // XULFrameElement_h
