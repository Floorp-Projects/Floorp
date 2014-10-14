/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MenuBoxObject_h
#define mozilla_dom_MenuBoxObject_h

#include "mozilla/dom/BoxObject.h"

namespace mozilla {
namespace dom {

class KeyboardEvent;

class MenuBoxObject MOZ_FINAL : public BoxObject
{
public:

  MenuBoxObject();

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void OpenMenu(bool aOpenFlag);
  already_AddRefed<Element> GetActiveChild();
  void SetActiveChild(Element* arg);
  bool HandleKeyPress(KeyboardEvent& keyEvent);
  bool OpenedWithKey();

private:
  ~MenuBoxObject();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MenuBoxObject_h
