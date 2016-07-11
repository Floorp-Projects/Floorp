/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLURIRefObject_h
#define HTMLURIRefObject_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsIURIRefObject.h"
#include "nscore.h"

#define NS_URI_REF_OBJECT_CID                          \
{ /* {bdd79df6-1dd1-11b2-b29c-c3d63a58f1d2} */         \
    0xbdd79df6, 0x1dd1, 0x11b2,                        \
    { 0xb2, 0x9c, 0xc3, 0xd6, 0x3a, 0x58, 0xf1, 0xd2 } \
}

class nsIDOMMozNamedAttrMap;
class nsIDOMNode;

namespace mozilla {

class HTMLURIRefObject final : public nsIURIRefObject
{
public:
  HTMLURIRefObject();

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIURIREFOBJECT

protected:
  virtual ~HTMLURIRefObject();

  nsCOMPtr<nsIDOMNode> mNode;
  nsCOMPtr<nsIDOMMozNamedAttrMap> mAttributes;
  uint32_t mCurAttrIndex;
  uint32_t mAttributeCnt;
};

} // namespace mozilla

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsIDOMNode* aNode);

#endif // #ifndef HTMLURIRefObject_h
