/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIBaseDOMException.h"
#include "nsIException.h"

class nsBaseDOMException : public nsIException,
                           public nsIBaseDOMException
{
public:
  nsBaseDOMException();
  virtual ~nsBaseDOMException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTION
  NS_IMETHOD Init(nsresult aNSResult, const char* aName,
                  const char* aMessage, PRUint16 aCode,
                  nsIException* aDefaultException);

protected:
  nsresult mResult;
  const char* mName;
  const char* mMessage;
  PRUint16 mCode;                                                            \
  nsCOMPtr<nsIException> mInner;
};

nsresult
NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult, const char** aName,
                                   const char** aMessage,
                                   PRUint16* aCode = nsnull);

#define DECL_INTERNAL_DOM_EXCEPTION(domname)                                 \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                nsIException** aException);


DECL_INTERNAL_DOM_EXCEPTION(DOMException)
DECL_INTERNAL_DOM_EXCEPTION(SVGException)
DECL_INTERNAL_DOM_EXCEPTION(XPathException)
