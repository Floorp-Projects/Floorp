/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TestBindingHeader_h
#define TestBindingHeader_h

#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class TestInterface : public nsISupports,
		      public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS

  virtual nsISupports* GetParentObject();

private:
};

} // namespace dom
} // namespace mozilla

#endif /* TestBindingHeader_h */
