/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Sun LDAP C SDK.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 *
 * Portions created by Sun Microsystems, Inc are Copyright (C) 2005
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "ldaptool.h"
#include "list.h"

/* Initialize the pointer to the Head of the list
 * The QHead is not allocated internally
 */

void q_init(QHead *list)
{
    if(list)
    {
        list->first = NULL;
        list->last = NULL;
        list->nnodes = 0;
    }
}

/* Append the node given to the end of the queue */
/* The node is NOT internally malloced */

void q_append(QElement *node, QHead *headNode)
{
    if (!node || !headNode)
        return;

    node->next = NULL;

    if (headNode->first == NULL)
    {
        node->prev= NULL;
        headNode->last = headNode->first = node;
    }
    else
    {
        node->prev = headNode->last;
        headNode->last = node->prev->next = node;
    }
    headNode->nnodes++;
}


/* Pre-pend the node given to the start of the queue */
/* The node is NOT internally malloced */
void q_prepend(QElement *node, QHead *headNode)
{
    if(!node || !headNode )
        return;

    node->prev = NULL;

    if((node->next = headNode->first) == NULL)
    {
        headNode->first = headNode->last = node;
    }
    else
    {
        headNode->first = node->next->prev = node;
    }
    headNode->nnodes++;
}
/* Find the node in the list - helper function */

void *q_find(QElement *node, QHead *headNode)
{
    QElement *walker=NULL;
    if(!node || !headNode)
        return NULL;

    for(walker = headNode->first; walker; walker = walker->next)
    {
        if(node == walker)
        {
            return walker;
        }
    }
    return NULL;
}

/* Insert the element 1 (elem1) BEFORE element 2 (elem2) in the list */

/* It is assumed though there is no proof of this in any application code
 * that the elem2 is already in the list and that ele1 is being inserted,
 * positioning it before elem2
 * If the elem1 already exits in the list, it is removed from its old position
 * and then repositioned as follows:
 * elem1 gets inserted before elem2
 * elem1 gets prepended before elem2
 */
void q_before(QElement *elem1, QElement *elem2, QHead *headNode)
{
    /* insert element 1 before element 2 now ----prepend it  */
    QElement *walker=NULL;
    QElement *insertnode=NULL;
    QElement *newnode = elem1;

    if(!elem1 || !elem2 || !headNode )
     return;

    if((walker = (QElement *)q_find(elem2, headNode)) != NULL)
    {

        if((insertnode = (QElement *)q_find(elem1, headNode)) != NULL)
        {
            /* the node already exists in the list
               remove the node and the proceed to add it
               to the right spot in the list */
            q_remove(insertnode, headNode);
        }

        if(headNode->first == walker)
        {
        /* elem 2 is the first node in the list
           update the HeadNode pointers to point to the new node */
            q_prepend(newnode, headNode );
            return;
        }
        newnode->next = walker;
        newnode->prev = walker->prev;
        walker->prev->next = newnode;
        walker->prev = newnode;
        headNode->nnodes++;
    }
}
/* Insert the element 1(elem1) after element 2 (elem2) in the list */

/* The semantics of these routines is determined from their use in
 * the Universal Connector application
 * It is assumed that element 2 is already in the list and that
 * element 1 gets inserted after element 2
* If elem1 already exists in the list then the elem1 is removed and
 * repostioned as follows:
 * element 1 after element 2
 * elem1 appened to  elem2
 */

void q_after(QElement *elem1, QElement *elem2, QHead *headNode)
{
    /* insert element 1 after element 2 now ----append after it  */
    QElement *walker = NULL;
    QElement *insertnode = NULL;
    QElement *newnode = elem1;

    if(!elem1 || !elem2 || !headNode )
     return;

    if((walker = (QElement *)q_find(elem2, headNode)) != NULL)
    {
        if((insertnode = (QElement *)q_find(elem1, headNode)) != NULL)
        {
            /* the node already exists in the list
               remove the node and the proceed to add it
               to the right spot in the list */
            q_remove(insertnode, headNode);
        }

        if(headNode->last == walker)
        {
        /* elem 2 is the last node in the list
           update the headNode pointers */
           q_append(newnode, headNode);
           return;
        }
        newnode->next = walker->next;
        walker->next->prev = newnode;
        walker->next = newnode;
        newnode->prev = walker;
        headNode->nnodes++;

    }

}
/* Remove the node from the list */

void q_remove(QElement *node, QHead *headNode)
{
    QElement *walker = NULL;
    QElement *prevnode = NULL;

    if(!node || !headNode)
        return;

    for(walker = headNode->first; walker; walker = walker->next)
    {
        if(walker == node)
        {
            if(headNode->first == walker)
            {
                headNode->first = walker->next;
            }
            if(headNode->last == walker)
            {
                headNode->last = prevnode;
            }
            walker = walker->next;
            if(prevnode != NULL)
            {
                prevnode->next = walker;
            }
            if(walker != NULL)
            {
                walker->prev = prevnode;
            }
            headNode->nnodes--;
            return;
        }
        else /* walker != node */
        {
            prevnode = walker;
        }
    }
}
/* Move the node from list with HeadNode1 to list with HeadNode2 */

void q_move(QElement *node, QHead *headNode1, QHead *headNode2)
{
    QElement *walker=NULL;

    if((walker = (QElement *)q_find(node, headNode1)) != NULL)
    {
        q_remove(walker, headNode1);
        q_append(walker, headNode2);
    }
}
