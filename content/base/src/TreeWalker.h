/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/*
 * Implementation of DOM Traversal's nsIDOMTreeWalker
 */

#ifndef mozilla_dom_TreeWalker_h
#define mozilla_dom_TreeWalker_h

#include "nsIDOMTreeWalker.h"
#include "nsTraversal.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

class nsINode;
class nsIDOMNode;

namespace mozilla {
namespace dom {

class TreeWalker MOZ_FINAL : public nsIDOMTreeWalker, public nsTraversal
{
    virtual ~TreeWalker();

public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIDOMTREEWALKER

    TreeWalker(nsINode *aRoot,
               uint32_t aWhatToShow,
               const NodeFilterHolder &aFilter);

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
    already_AddRefed<NodeFilter> GetFilter()
    {
        return mFilter.ToWebIDLCallback();
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

    JSObject* WrapObject(JSContext *cx);

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

    // Implementation for our various XPCOM getters
    typedef already_AddRefed<nsINode> (TreeWalker::*NodeGetter)(ErrorResult&);
    inline nsresult ImplNodeGetter(NodeGetter aGetter, nsIDOMNode** aRetval)
    {
        mozilla::ErrorResult rv;
        nsCOMPtr<nsINode> node = (this->*aGetter)(rv);
        if (rv.Failed()) {
            return rv.ErrorCode();
        }
        *aRetval = node ? node.forget().take()->AsDOMNode() : nullptr;
        return NS_OK;
    }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TreeWalker_h

