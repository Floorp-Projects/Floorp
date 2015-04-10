/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputPortServiceFactory_h
#define mozilla_dom_InputPortServiceFactory_h

#include "nsCOMPtr.h"

class nsIInputPortService;

namespace mozilla {
namespace dom {

class FakeInputPortService;

class InputPortServiceFactory final
{
public:
  static already_AddRefed<FakeInputPortService> CreateFakeInputPortService();

  static already_AddRefed<nsIInputPortService> AutoCreateInputPortService();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputPortServiceFactory_h
