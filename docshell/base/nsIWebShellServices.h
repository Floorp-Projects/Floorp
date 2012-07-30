/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIWebShellServices_h___
#define nsIWebShellServices_h___

#include "nsISupports.h"
#include "nsCharsetSource.h"

// Interface ID for nsIWebShellServices

/* 0c628af0-5638-4703-8f99-ed6134c9de18 */
#define NS_IWEB_SHELL_SERVICES_IID \
{ 0x0c628af0, 0x5638, 0x4703, {0x8f, 0x99, 0xed, 0x61, 0x34, 0xc9, 0xde, 0x18} }

//----------------------------------------------------------------------

class nsIWebShellServices : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWEB_SHELL_SERVICES_IID)

  NS_IMETHOD ReloadDocument(const char* aCharset = nullptr , 
                            PRInt32 aSource = kCharsetUninitialized) = 0;
  NS_IMETHOD StopDocumentLoad(void) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWebShellServices, NS_IWEB_SHELL_SERVICES_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIWEBSHELLSERVICES \
  NS_IMETHOD ReloadDocument(const char *aCharset=nullptr, PRInt32 aSource=kCharsetUninitialized); \
  NS_IMETHOD StopDocumentLoad(void); \

#endif /* nsIWebShellServices_h___ */
