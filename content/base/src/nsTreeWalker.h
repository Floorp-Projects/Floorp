/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/*
 * Implementation of DOM Traversal's nsIDOMTreeWalker
 */

#ifndef nsTreeWalker_h___
#define nsTreeWalker_h___

#include "nsIDOMTreeWalker.h"
#include "nsTraversal.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

class nsINode;
class nsIDOMNode;
class nsIDOMNodeFilter;

class nsTreeWalker : public nsIDOMTreeWalker, public nsTraversal
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIDOMTREEWALKER

    nsTreeWalker(nsINode *aRoot,
                 PRUint32 aWhatToShow,
                 nsIDOMNodeFilter *aFilter);
    virtual ~nsTreeWalker();

    NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeWalker)

private:
    nsCOMPtr<nsINode> mCurrentNode;

    /*
     * Implements FirstChild and LastChild which only vary in which direction
     * they search.
     * @param aReversed Controls whether we search forwards or backwards
     * @param _retval   Returned node. Null if no child is found
     * @returns         Errorcode
     */
    nsresult FirstChildInternal(bool aReversed, nsIDOMNode **_retval);

    /*
     * Implements NextSibling and PreviousSibling which only vary in which
     * direction they search.
     * @param aReversed Controls whether we search forwards or backwards
     * @param _retval   Returned node. Null if no child is found
     * @returns         Errorcode
     */
    nsresult NextSiblingInternal(bool aReversed, nsIDOMNode **_retval);
};

#endif

