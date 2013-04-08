/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  TestCImplementedInterface(JSObject* aJSImpl, nsISupports* aParent)
    : TestJSImplInterface(aJSImpl, aParent)
  {}
};

class TestCImplementedInterface2 : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestCImplementedInterface2)

  // We need a GetParentObject to make binding codegen happy
  nsISupports* GetParentObject();
};



} // namespace dom
} // namespace mozilla

#endif // TestCImplementedInterface_h
