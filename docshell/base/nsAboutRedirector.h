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

/* 56ebedd4-6ccf-48e8-bdae-adc77f044567 */
#define NS_ABOUT_REDIRECTOR_MODULE_CID \
{ 0x56ebedd4, 0x6ccf, 0x48e8, \
  { 0xbd, 0xae, 0xad, 0xc7, 0x7f, 0x04, 0x45, 0x67 } }

#endif // nsAboutRedirector_h__
