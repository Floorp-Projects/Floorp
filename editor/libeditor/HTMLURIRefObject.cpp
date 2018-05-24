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
#include "nsINode.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsGkAtoms.h"

namespace mozilla {

// String classes change too often and I can't keep up.
// Set this macro to this week's approved case-insensitive compare routine.
#define MATCHES(tagName, str) tagName.EqualsIgnoreCase(str)

HTMLURIRefObject::HTMLURIRefObject()
  : mCurAttrIndex(0)
  , mAttributeCnt(0)
  , mAttrsInited(false)
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

  if (NS_WARN_IF(!mNode->IsElement())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<dom::Element> element = mNode->AsElement();

  // Loop over attribute list:
  if (!mAttrsInited) {
    mAttrsInited = true;
    mAttributeCnt = element->GetAttrCount();
    NS_ENSURE_TRUE(mAttributeCnt, NS_ERROR_FAILURE);
    mCurAttrIndex = 0;
  }

  while (mCurAttrIndex < mAttributeCnt) {
    BorrowedAttrInfo attrInfo = element->GetAttrInfoAt(mCurAttrIndex++);
    NS_ENSURE_ARG_POINTER(attrInfo.mName);

    // href >> A, AREA, BASE, LINK
    if (attrInfo.mName->Equals(nsGkAtoms::href)) {
      if (!element->IsAnyOfHTMLElements(nsGkAtoms::a,
                                        nsGkAtoms::area,
                                        nsGkAtoms::base,
                                        nsGkAtoms::link)) {
        continue;
      }

      attrInfo.mValue->ToString(aURI);
      // href pointing to a named anchor doesn't count
      if (StringBeginsWith(aURI, NS_LITERAL_STRING("#"))) {
        aURI.Truncate();
        return NS_ERROR_INVALID_ARG;
      }

      return NS_OK;
    }
    // src >> FRAME, IFRAME, IMG, INPUT, SCRIPT
    else if (attrInfo.mName->Equals(nsGkAtoms::src)) {
      if (!element->IsAnyOfHTMLElements(nsGkAtoms::img,
                                        nsGkAtoms::frame,
                                        nsGkAtoms::iframe,
                                        nsGkAtoms::input,
                                        nsGkAtoms::script)) {
        continue;
      }
      attrInfo.mValue->ToString(aURI);
      return NS_OK;
    }
    //<META http-equiv="refresh" content="3,http://www.acme.com/intro.html">
    else if (attrInfo.mName->Equals(nsGkAtoms::content)) {
      if (!element->IsHTMLElement(nsGkAtoms::meta)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // longdesc >> FRAME, IFRAME, IMG
    else if (attrInfo.mName->Equals(nsGkAtoms::longdesc)) {
      if (!element->IsAnyOfHTMLElements(nsGkAtoms::img,
                                        nsGkAtoms::frame,
                                        nsGkAtoms::iframe)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // usemap >> IMG, INPUT, OBJECT
    else if (attrInfo.mName->Equals(nsGkAtoms::usemap)) {
      if (!element->IsAnyOfHTMLElements(nsGkAtoms::img,
                                        nsGkAtoms::input,
                                        nsGkAtoms::object)) {
        continue;
      }
    }
    // action >> FORM
    else if (attrInfo.mName->Equals(nsGkAtoms::action)) {
      if (!element->IsHTMLElement(nsGkAtoms::form)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // background >> BODY
    else if (attrInfo.mName->Equals(nsGkAtoms::background)) {
      if (!element->IsHTMLElement(nsGkAtoms::body)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // codebase >> OBJECT
    else if (attrInfo.mName->Equals(nsGkAtoms::codebase)) {
      if (!element->IsHTMLElement(nsGkAtoms::object)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // classid >> OBJECT
    else if (attrInfo.mName->Equals(nsGkAtoms::classid)) {
      if (!element->IsHTMLElement(nsGkAtoms::object)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // data >> OBJECT
    else if (attrInfo.mName->Equals(nsGkAtoms::data)) {
      if (!element->IsHTMLElement(nsGkAtoms::object)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // cite >> BLOCKQUOTE, DEL, INS, Q
    else if (attrInfo.mName->Equals(nsGkAtoms::cite)) {
      if (!element->IsAnyOfHTMLElements(nsGkAtoms::blockquote,
                                        nsGkAtoms::q,
                                        nsGkAtoms::del,
                                        nsGkAtoms::ins)) {
        continue;
      }

      // XXXbz And if it is?
    }
    // profile >> HEAD
    else if (attrInfo.mName->Equals(nsGkAtoms::profile)) {
      if (!element->IsHTMLElement(nsGkAtoms::head)) {
        continue;
      }

      // XXXbz And if it is?
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
HTMLURIRefObject::GetNode(nsINode** aNode)
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  *aNode = do_AddRef(mNode).take();
  return NS_OK;
}

NS_IMETHODIMP
HTMLURIRefObject::SetNode(nsINode* aNode)
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

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsINode* aNode)
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
