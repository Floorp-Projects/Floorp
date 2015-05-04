/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TestCImplementedInterface_h
#define TestCImplementedInterface_h

#include "../TestJSImplGenBinding.h"

namespace mozilla {
namespace dom {

class TestCImplementedInterface : public TestJSImplInterface
{
public:
  TestCImplementedInterface(JS::Handle<JSObject*> aJSImpl,
                            nsIGlobalObject* aParent)
    : TestJSImplInterface(aJSImpl, aParent)
  {}
};

class TestCImplementedInterface2 : public nsISupports,
                                   public nsWrapperCache
{
public:
  explicit TestCImplementedInterface2(nsIGlobalObject* aParent)
  {}
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestCImplementedInterface2)

  // We need a GetParentObject to make binding codegen happy
  nsISupports* GetParentObject();
};



} // namespace dom
} // namespace mozilla

#endif // TestCImplementedInterface_h
