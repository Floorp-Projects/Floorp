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

#include "nsVoidArray.h"

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

    nsAutoVoidArray stack;
    PRInt32 stackSize = 0;
    nsresult rv = NS_OK;

    // Push the top most node onto the stack
    DOMTreePos *rootPos = new DOMTreePos;
    if (rootPos == nsnull)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    rootPos->current = aRootNode;
    stack.AppendElement(rootPos);
    stackSize++;

    while (stackSize > 0)
    {
        nsCOMPtr<nsIDOMNode> current;
        nsCOMPtr<nsIDOMNode> next;

        // Pop the last position off the stack
        stackSize--;
        DOMTreePos *currentPos = NS_STATIC_CAST(DOMTreePos *, stack[stackSize]);
        current = do_QueryInterface(currentPos->current);
            
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
                    if (stackSize < stack.Count())
                    {
                        DOMTreePos *nextPos = NS_STATIC_CAST(DOMTreePos *, stack[stackSize]);
                        nextPos->current = next;
                    }
                    else
                    {
                        DOMTreePos *nextPos = new DOMTreePos;
                        if (nextPos == nsnull)
                        {
                            rv = NS_ERROR_OUT_OF_MEMORY;
                            goto cleanup;
                        }
                        nextPos->current = next;
                        stack.AppendElement(nextPos);
                    }
                    stackSize++;
                }

                // Push the first child onto the stack
                if (stackSize < stack.Count())
                {
                    DOMTreePos *childPos = NS_STATIC_CAST(DOMTreePos *, stack[stackSize]);
                    current->GetFirstChild(getter_AddRefs(childPos->current));
                }
                else
                {
                    DOMTreePos *childPos = new DOMTreePos;
                    if (childPos == nsnull)
                    {
                        rv = NS_ERROR_OUT_OF_MEMORY;
                        goto cleanup;
                    }
                    current->GetFirstChild(getter_AddRefs(childPos->current));
                    stack.AppendElement(childPos);
                }
                stackSize++;

                break;
            }
            current = next;
        }
    }

cleanup:
    // Clean the tree in case we came out early
    PRInt32 i;
    for (i = 0; i < stack.Count(); i++)
    {
        DOMTreePos *pos = NS_STATIC_CAST(DOMTreePos *, stack.ElementAt(i));
        delete pos;
    }
    stack.Clear();

    return rv;
}
