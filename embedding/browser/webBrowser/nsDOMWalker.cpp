/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 */

#include "nsVector.h"

#include "nsCOMPtr.h"
#include "nsDOMWalker.h"

struct DOMTreePos
{
    nsCOMPtr<nsIDOMNode> current;
};

nsresult nsDOMWalker::WalkDOM(nsIDOMNode *aRootNode, nsDOMWalkerCallback *aCallback)
{
    NS_ENSURE_ARG_POINTER(aRootNode);
    NS_ENSURE_ARG_POINTER(aCallback);

    nsVector stack;
    nsresult rv = NS_OK;

    // Push the top most node onto the stack
    DOMTreePos *rootPos = new DOMTreePos;
    if (rootPos == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    rootPos->current = aRootNode;
    stack.Add(rootPos);

    while (stack.size > 0)
    {
        nsCOMPtr<nsIDOMNode> current;
        nsCOMPtr<nsIDOMNode> next;

        // Pop the last position off the stack
        DOMTreePos *pos = (DOMTreePos *) stack.Get(stack.size - 1);
        stack.Remove(stack.size - 1);
        current = do_QueryInterface(pos->current);
        delete pos;
            
        // Iterate through each sibling
        while (current)
        {
            PRBool hasChildNodes = PR_FALSE;
            current->GetNextSibling(getter_AddRefs(next));
            current->HasChildNodes(&hasChildNodes);

            // Call the callback to let them do what they want to do
            if (aCallback)
            {
              PRBool abort = PR_FALSE;
              aCallback->OnWalkDOMNode(current, &abort);
              if (abort)
              {
                goto cleanup;
              }
            }

            if (hasChildNodes)
            {
                // Push the current node back onto the stack
                if (next)
                {
                    DOMTreePos *nextPos = new DOMTreePos;
                    if (nextPos == nsnull)
                    {
                        rv = NS_ERROR_OUT_OF_MEMORY;
                        goto cleanup;
                    }
                    nextPos->current = next;
                    stack.Add(nextPos);
                }
                // Push the first child onto the stack
                DOMTreePos *childPos = new DOMTreePos;
                if (childPos == nsnull)
                {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                    goto cleanup;
                }
                current->GetFirstChild(getter_AddRefs(childPos->current));
                stack.Add(childPos);
                break;
            }
            current = next;
        }
    }

cleanup:
    // Clean the tree in case we came out early
    while (stack.size > 0)
    {
        DOMTreePos *pos = (DOMTreePos *) stack.Get(stack.size - 1);
        stack.Remove(stack.size - 1);
        delete pos;
    }

    return rv;
}
