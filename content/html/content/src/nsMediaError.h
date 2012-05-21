/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDOMMediaError.h"
#include "nsISupports.h"
#include "mozilla/Attributes.h"

class nsMediaError MOZ_FINAL : public nsIDOMMediaError
{
public:
  nsMediaError(PRUint16 aCode);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMMediaError
  NS_DECL_NSIDOMMEDIAERROR

private:
  // Error code
  PRUint16 mCode;
};
