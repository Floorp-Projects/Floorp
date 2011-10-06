/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is this file as it was released on May 1 2001.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
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
                PRUint32 aWhatToShow,
                nsIDOMNodeFilter *aFilter,
                bool aExpandEntityReferences);
    virtual ~nsTraversal();

protected:
    nsCOMPtr<nsINode> mRoot;
    PRUint32 mWhatToShow;
    nsCOMPtr<nsIDOMNodeFilter> mFilter;
    bool mExpandEntityReferences;
    bool mInAcceptNode;

    /*
     * Tests if and how a node should be filtered. Uses mWhatToShow and
     * mFilter to test the node.
     * @param aNode     Node to test
     * @param _filtered Returned filtervalue. See nsIDOMNodeFilter.idl
     * @returns         Errorcode
     */
    nsresult TestNode(nsINode* aNode, PRInt16* _filtered);
};

#endif

