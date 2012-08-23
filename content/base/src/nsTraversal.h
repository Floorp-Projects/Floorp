/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/*
 * Implementation of DOM Traversal's nsIDOMTreeWalker
 */

#ifndef nsTraversal_h___
#define nsTraversal_h___

#include "nsCOMPtr.h"

class nsINode;
class nsIDOMNodeFilter;

class nsTraversal
{
public:
    nsTraversal(nsINode *aRoot,
                uint32_t aWhatToShow,
                nsIDOMNodeFilter *aFilter);
    virtual ~nsTraversal();

protected:
    nsCOMPtr<nsINode> mRoot;
    uint32_t mWhatToShow;
    nsCOMPtr<nsIDOMNodeFilter> mFilter;
    bool mInAcceptNode;

    /*
     * Tests if and how a node should be filtered. Uses mWhatToShow and
     * mFilter to test the node.
     * @param aNode     Node to test
     * @param _filtered Returned filtervalue. See nsIDOMNodeFilter.idl
     * @returns         Errorcode
     */
    nsresult TestNode(nsINode* aNode, int16_t* _filtered);
};

#endif

