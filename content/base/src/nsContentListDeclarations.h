/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentListDeclarations_h
#define nsContentListDeclarations_h

#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsStringFwd.h"

class nsContentList;
class nsIAtom;
class nsIContent;
class nsINode;

// Magic namespace id that means "match all namespaces".  This is
// negative so it won't collide with actual namespace constants.
#define kNameSpaceID_Wildcard INT32_MIN

// This is a callback function type that can be used to implement an
// arbitrary matching algorithm.  aContent is the content that may
// match the list, while aNamespaceID, aAtom, and aData are whatever
// was passed to the list's constructor.
typedef bool (*nsContentListMatchFunc)(nsIContent* aContent,
                                       int32_t aNamespaceID,
                                       nsIAtom* aAtom,
                                       void* aData);

typedef void (*nsContentListDestroyFunc)(void* aData);

/**
 * A function that allocates the matching data for this
 * FuncStringContentList.  Returning aString is perfectly fine; in
 * that case the destructor function should be a no-op.
 */
typedef void* (*nsFuncStringContentListDataAllocator)(nsINode* aRootNode,
                                                      const nsString* aString);

// If aMatchNameSpaceId is kNameSpaceID_Unknown, this will return a
// content list which matches ASCIIToLower(aTagname) against HTML
// elements in HTML documents and aTagname against everything else.
// For any other value of aMatchNameSpaceId, the list will match
// aTagname against all elements.
already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode,
                  int32_t aMatchNameSpaceId,
                  const nsAString& aTagname);

already_AddRefed<nsContentList>
NS_GetFuncStringNodeList(nsINode* aRootNode,
                         nsContentListMatchFunc aFunc,
                         nsContentListDestroyFunc aDestroyFunc,
                         nsFuncStringContentListDataAllocator aDataAllocator,
                         const nsAString& aString);
already_AddRefed<nsContentList>
NS_GetFuncStringHTMLCollection(nsINode* aRootNode,
                               nsContentListMatchFunc aFunc,
                               nsContentListDestroyFunc aDestroyFunc,
                               nsFuncStringContentListDataAllocator aDataAllocator,
                               const nsAString& aString);

#endif // nsContentListDeclarations_h
