/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Here is the list, from beppe and glazman:
    href >> A, AREA, BASE, LINK
    src >> FRAME, IFRAME, IMG, INPUT, SCRIPT
    <META http-equiv="refresh" content="3,http://www.acme.com/intro.html">
    longdesc >> FRAME, IFRAME, IMG
    usemap >> IMG, INPUT, OBJECT
    action >> FORM
    background >> BODY
    codebase >> OBJECT, APPLET
    classid >> OBJECT
    data >> OBJECT
    cite >> BLOCKQUOTE, DEL, INS, Q
    profile >> HEAD
    ARCHIVE attribute on APPLET ; warning, it contains a list of URIs.

    Easier way of organizing the list:
    a:      href
    area:   href
    base:   href
    body:   background
    blockquote: cite (not normally rewritable)
    link:   href
    frame:  src, longdesc
    iframe: src, longdesc
    input:  src, usemap
    form:   action
    img:    src, longdesc, usemap
    script: src
    applet: codebase, archive <list>
    object: codebase, data, classid, usemap
    head:   profile
    del:    cite
    ins:    cite
    q:      cite
 */

#include "HTMLURIRefObject.h"

#include "mozilla/mozalloc.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsDOMAttributeMap.h"
#include "nsError.h"
#include "nsID.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsISupportsUtils.h"
#include "nsString.h"

namespace mozilla {

// String classes change too often and I can't keep up.
// Set this macro to this week's approved case-insensitive compare routine.
#define MATCHES(tagName, str) tagName.EqualsIgnoreCase(str)

HTMLURIRefObject::HTMLURIRefObject()
  : mCurAttrIndex(0)
  , mAttributeCnt(0)
{
}

HTMLURIRefObject::~HTMLURIRefObject()
{
}

//Interfaces for addref and release and queryinterface
NS_IMPL_ISUPPORTS(HTMLURIRefObject, nsIURIRefObject)

NS_IMETHODIMP
HTMLURIRefObject::Reset()
{
  mCurAttrIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLURIRefObject::GetNextURI(nsAString& aURI)
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);

  // XXX Why don't you use nsAtom for comparing the tag name a lot?
  nsAutoString tagName;
  nsresult rv = mNode->GetNodeName(tagName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop over attribute list:
  if (!mAttributes) {
    nsCOMPtr<dom::Element> element(do_QueryInterface(mNode));
    NS_ENSURE_TRUE(element, NS_ERROR_INVALID_ARG);

    mAttributes = element->Attributes();
    mAttributeCnt = mAttributes->Length();
    NS_ENSURE_TRUE(mAttributeCnt, NS_ERROR_FAILURE);
    mCurAttrIndex = 0;
  }

  while (mCurAttrIndex < mAttributeCnt) {
    RefPtr<dom::Attr> attrNode = mAttributes->Item(mCurAttrIndex++);
    NS_ENSURE_ARG_POINTER(attrNode);
    nsString curAttr;
    rv = attrNode->GetName(curAttr);
    NS_ENSURE_SUCCESS(rv, rv);

    // href >> A, AREA, BASE, LINK
    if (MATCHES(curAttr, "href")) {
      if (!MATCHES(tagName, "a") && !MATCHES(tagName, "area") &&
          !MATCHES(tagName, "base") && !MATCHES(tagName, "link")) {
        continue;
      }
      rv = attrNode->GetValue(aURI);
      NS_ENSURE_SUCCESS(rv, rv);
      nsString uri (aURI);
      // href pointing to a named anchor doesn't count
      if (aURI.First() != char16_t('#')) {
        return NS_OK;
      }
      aURI.Truncate();
      return NS_ERROR_INVALID_ARG;
    }
    // src >> FRAME, IFRAME, IMG, INPUT, SCRIPT
    else if (MATCHES(curAttr, "src")) {
      if (!MATCHES(tagName, "img") &&
          !MATCHES(tagName, "frame") && !MATCHES(tagName, "iframe") &&
          !MATCHES(tagName, "input") && !MATCHES(tagName, "script")) {
        continue;
      }
      return attrNode->GetValue(aURI);
    }
    //<META http-equiv="refresh" content="3,http://www.acme.com/intro.html">
    else if (MATCHES(curAttr, "content")) {
      if (!MATCHES(tagName, "meta")) {
        continue;
      }
    }
    // longdesc >> FRAME, IFRAME, IMG
    else if (MATCHES(curAttr, "longdesc")) {
      if (!MATCHES(tagName, "img") &&
          !MATCHES(tagName, "frame") && !MATCHES(tagName, "iframe")) {
        continue;
      }
    }
    // usemap >> IMG, INPUT, OBJECT
    else if (MATCHES(curAttr, "usemap")) {
      if (!MATCHES(tagName, "img") &&
          !MATCHES(tagName, "input") && !MATCHES(tagName, "object")) {
        continue;
      }
    }
    // action >> FORM
    else if (MATCHES(curAttr, "action")) {
      if (!MATCHES(tagName, "form")) {
        continue;
      }
    }
    // background >> BODY
    else if (MATCHES(curAttr, "background")) {
      if (!MATCHES(tagName, "body")) {
        continue;
      }
    }
    // codebase >> OBJECT, APPLET
    else if (MATCHES(curAttr, "codebase")) {
      if (!MATCHES(tagName, "meta")) {
        continue;
      }
    }
    // classid >> OBJECT
    else if (MATCHES(curAttr, "classid")) {
      if (!MATCHES(tagName, "object")) {
        continue;
      }
    }
    // data >> OBJECT
    else if (MATCHES(curAttr, "data")) {
      if (!MATCHES(tagName, "object")) {
        continue;
      }
    }
    // cite >> BLOCKQUOTE, DEL, INS, Q
    else if (MATCHES(curAttr, "cite")) {
      if (!MATCHES(tagName, "blockquote") && !MATCHES(tagName, "q") &&
          !MATCHES(tagName, "del") && !MATCHES(tagName, "ins")) {
        continue;
      }
    }
    // profile >> HEAD
    else if (MATCHES(curAttr, "profile")) {
      if (!MATCHES(tagName, "head")) {
        continue;
      }
    }
    // archive attribute on APPLET; warning, it contains a list of URIs.
    else if (MATCHES(curAttr, "archive")) {
      if (!MATCHES(tagName, "applet")) {
        continue;
      }
    }
  }
  // Return a code to indicate that there are no more,
  // to distinguish that case from real errors.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
HTMLURIRefObject::RewriteAllURIs(const nsAString& aOldPat,
                                 const nsAString& aNewPat,
                                 bool aMakeRel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HTMLURIRefObject::GetNode(nsIDOMNode** aNode)
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  *aNode = mNode.get();
  NS_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
HTMLURIRefObject::SetNode(nsIDOMNode* aNode)
{
  mNode = aNode;
  nsAutoString dummyURI;
  if (NS_SUCCEEDED(GetNextURI(dummyURI))) {
    mCurAttrIndex = 0;    // Reset so we'll get the first node next time
    return NS_OK;
  }

  // If there weren't any URIs in the attributes,
  // then don't accept this node.
  mNode = nullptr;
  return NS_ERROR_INVALID_ARG;
}

} // namespace mozilla

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsIDOMNode* aNode)
{
  RefPtr<mozilla::HTMLURIRefObject> refObject = new mozilla::HTMLURIRefObject();
  nsresult rv = refObject->SetNode(aNode);
  if (NS_FAILED(rv)) {
    *aResult = 0;
    return rv;
  }
  refObject.forget(aResult);
  return NS_OK;
}
