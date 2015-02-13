/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutRedirector_h__
#define nsAboutRedirector_h__

#include "nsIAboutModule.h"

class nsAboutRedirector : public nsIAboutModule
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIABOUTMODULE

  nsAboutRedirector() {}

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

protected:
  virtual ~nsAboutRedirector() {}
};

#define NS_ABOUT_REDIRECTOR_MODULE_CID               \
{ /*  f0acde16-1dd1-11b2-9e35-f5786fff5a66*/         \
    0xf0acde16,                                      \
    0x1dd1,                                          \
    0x11b2,                                          \
    {0x9e, 0x35, 0xf5, 0x78, 0x6f, 0xff, 0x5a, 0x66} \
}

#endif // nsAboutRedirector_h__
