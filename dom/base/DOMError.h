/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domerror_h__
#define mozilla_dom_domerror_h__

#include "nsIDOMDOMError.h"

#include "nsCOMPtr.h"
#include "nsStringGlue.h"

namespace mozilla {
namespace dom {

class DOMError : public nsIDOMDOMError
{
  nsString mName;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMERROR

  static already_AddRefed<nsIDOMDOMError>
  CreateForNSResult(nsresult rv);

  static already_AddRefed<nsIDOMDOMError>
  CreateForDOMExceptionCode(PRUint16 aDOMExceptionCode);

  static already_AddRefed<nsIDOMDOMError>
  CreateWithName(const nsAString& aName)
  {
    nsCOMPtr<nsIDOMDOMError> error = new DOMError(aName);
    return error.forget();
  }

protected:
  DOMError(const nsAString& aName)
  : mName(aName)
  { }

  virtual ~DOMError()
  { }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_domerror_h__
