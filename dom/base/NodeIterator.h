/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Traversal's NodeIterator
 */

#ifndef mozilla_dom_NodeIterator_h
#define mozilla_dom_NodeIterator_h

#include "nsTraversal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

class nsINode;

namespace mozilla {
namespace dom {

class NodeIterator final : public nsStubMutationObserver,
                           public nsTraversal
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NodeIterator(nsINode *aRoot,
                 uint32_t aWhatToShow,
                 NodeFilter* aFilter);

    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

    NS_DECL_CYCLE_COLLECTION_CLASS(NodeIterator)

    // WebIDL API
    nsINode* Root() const
    {
        return mRoot;
    }
    nsINode* GetReferenceNode() const
    {
        return mPointer.mNode;
    }
    bool PointerBeforeReferenceNode() const
    {
        return mPointer.mBeforeNode;
    }
    uint32_t WhatToShow() const
    {
        return mWhatToShow;
    }
    NodeFilter* GetFilter()
    {
        return mFilter;
    }
    already_AddRefed<nsINode> NextNode(ErrorResult& aResult)
    {
        return NextOrPrevNode(&NodePointer::MoveToNext, aResult);
    }
    already_AddRefed<nsINode> PreviousNode(ErrorResult& aResult)
    {
        return NextOrPrevNode(&NodePointer::MoveToPrevious, aResult);
    }
    void Detach();

    bool WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

private:
    virtual ~NodeIterator();

    struct NodePointer {
        NodePointer()
          : mNode(nullptr)
          , mBeforeNode(false)
        {
        }
        NodePointer(nsINode *aNode, bool aBeforeNode);

        typedef bool (NodePointer::*MoveToMethodType)(nsINode*);
        bool MoveToNext(nsINode *aRoot);
        bool MoveToPrevious(nsINode *aRoot);

        bool MoveForward(nsINode *aRoot, nsINode *aNode);
        void MoveBackward(nsINode *aParent, nsINode *aNode);

        void AdjustAfterRemoval(nsINode *aRoot, nsINode *aContainer, nsIContent *aChild, nsIContent *aPreviousSibling);

        void Clear() { mNode = nullptr; }

        nsINode *mNode;
        bool mBeforeNode;
    };

    // Have to return a strong ref, because the act of testing the node can
    // remove it from the DOM so we're holding the only ref to it.
    already_AddRefed<nsINode>
    NextOrPrevNode(NodePointer::MoveToMethodType aMove, ErrorResult& aResult);

    NodePointer mPointer;
    NodePointer mWorkingPointer;
};

} // namespace dom

} // namespace mozilla

#endif // mozilla_dom_NodeIterator_h
