/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDocumentActivity_h__
#define nsIDocumentActivity_h__

#include "nsISupports.h"

#define NS_IDOCUMENTACTIVITY_IID \
{ 0x9b9f584e, 0xefa8, 0x11e3, \
  { 0xbb, 0x74, 0x5e, 0xdd, 0x1d, 0x5d, 0x46, 0xb0 } }

class nsIDocumentActivity : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENTACTIVITY_IID)

  virtual void NotifyOwnerDocumentActivityChanged() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentActivity, NS_IDOCUMENTACTIVITY_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIDOCUMENTACTIVITY \
  virtual void NotifyOwnerDocumentActivityChanged() MOZ_OVERRIDE;

#endif /* nsIDocumentActivity_h__ */
