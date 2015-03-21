/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContainerBoxObject_h
#define mozilla_dom_ContainerBoxObject_h

#include "mozilla/dom/BoxObject.h"

namespace mozilla {
namespace dom {

class ContainerBoxObject final : public BoxObject
{
public:
  ContainerBoxObject();

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<nsIDocShell> GetDocShell();

private:
  ~ContainerBoxObject();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContainerBoxObject_h
