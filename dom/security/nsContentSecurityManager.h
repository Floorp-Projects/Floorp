/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentSecurityManager_h___
#define nsContentSecurityManager_h___

#include "nsIChannel.h"
class nsIStreamListener;

class nsContentSecurityManager
{
private:
  nsContentSecurityManager() {}
  virtual ~nsContentSecurityManager() {}

public:
  static nsresult doContentSecurityCheck(nsIChannel* aChannel,
                                         nsCOMPtr<nsIStreamListener>& aInAndOutListener);
};

#endif /* nsContentSecurityManager_h___ */
