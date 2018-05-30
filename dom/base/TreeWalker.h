/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Traversal's TreeWalker
 */

#ifndef mozilla_dom_TreeWalker_h
#define mozilla_dom_TreeWalker_h

#include "nsISupports.h"
#include "nsTraversal.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

class nsINode;

namespace mozilla {
namespace dom {

class TreeWalker final : public nsISupports, public nsTraversal
{
    virtual ~TreeWalker();

public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    TreeWalker(nsINode *aRoot,
               uint32_t aWhatToShow,
               NodeFilter* aFilter);

    NS_DECL_CYCLE_COLLECTION_CLASS(TreeWalker)

    // WebIDL API
    nsINode* Root() const
    {
        return mRoot;
    }
    uint32_t WhatToShow() const
    {
        return mWhatToShow;
    }
    NodeFilter* GetFilter()
    {
        return mFilter;
    }
    nsINode* CurrentNode() const
    {
        return mCurrentNode;
    }
    void SetCurrentNode(nsINode& aNode, ErrorResult& aResult);
    // All our traversal methods return strong refs because filtering can
    // remove nodes from the tree.
    already_AddRefed<nsINode> ParentNode(ErrorResult& aResult);
    already_AddRefed<nsINode> FirstChild(ErrorResult& aResult);
    already_AddRefed<nsINode> LastChild(ErrorResult& aResult);
    already_AddRefed<nsINode> PreviousSibling(ErrorResult& aResult);
    already_AddRefed<nsINode> NextSibling(ErrorResult& aResult);
    already_AddRefed<nsINode> PreviousNode(ErrorResult& aResult);
    already_AddRefed<nsINode> NextNode(ErrorResult& aResult);

    bool WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

private:
    nsCOMPtr<nsINode> mCurrentNode;

    /*
     * Implements FirstChild and LastChild which only vary in which direction
     * they search.
     * @param aReversed Controls whether we search forwards or backwards
     * @param aResult   Whether we threw or not.
     * @returns         The desired node. Null if no child is found
     */
    already_AddRefed<nsINode> FirstChildInternal(bool aReversed,
                                                 ErrorResult& aResult);

    /*
     * Implements NextSibling and PreviousSibling which only vary in which
     * direction they search.
     * @param aReversed Controls whether we search forwards or backwards
     * @param aResult   Whether we threw or not.
     * @returns         The desired node. Null if no child is found
     */
    already_AddRefed<nsINode> NextSiblingInternal(bool aReversed,
                                                  ErrorResult& aResult);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TreeWalker_h

