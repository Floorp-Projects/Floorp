/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentListDeclarations_h
#define nsContentListDeclarations_h

#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsStringFwd.h"

class nsContentList;
class nsAtom;
class nsIContent;
class nsINode;

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

// Magic namespace id that means "match all namespaces".  This is
// negative so it won't collide with actual namespace constants.
#define kNameSpaceID_Wildcard INT32_MIN

// This is a callback function type that can be used to implement an
// arbitrary matching algorithm.  aContent is the content that may
// match the list, while aNamespaceID, aAtom, and aData are whatever
// was passed to the list's constructor.
using nsContentListMatchFunc = bool (*)(mozilla::dom::Element* aElement,
                                        int32_t aNamespaceID, nsAtom* aAtom,
                                        void* aData);

using nsContentListDestroyFunc = void (*)(void* aData);

/**
 * A function that allocates the matching data for this
 * FuncStringContentList.  Returning aString is perfectly fine; in
 * that case the destructor function should be a no-op.
 */
using nsFuncStringContentListDataAllocator = void* (*)(nsINode* aRootNode,
                                                       const nsString* aString);

// If aMatchNameSpaceId is kNameSpaceID_Unknown, this will return a
// content list which matches ASCIIToLower(aTagname) against HTML
// elements in HTML documents and aTagname against everything else.
// For any other value of aMatchNameSpaceId, the list will match
// aTagname against all elements.
already_AddRefed<nsContentList> NS_GetContentList(nsINode* aRootNode,
                                                  int32_t aMatchNameSpaceId,
                                                  const nsAString& aTagname);

template <class ListType>
already_AddRefed<nsContentList> GetFuncStringContentList(
    nsINode* aRootNode, nsContentListMatchFunc aFunc,
    nsContentListDestroyFunc aDestroyFunc,
    nsFuncStringContentListDataAllocator aDataAllocator,
    const nsAString& aString);

#endif  // nsContentListDeclarations_h
