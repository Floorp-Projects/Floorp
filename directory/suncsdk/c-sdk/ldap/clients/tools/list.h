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

/*************************************************************************
    double linked list declaration
**************************************************************************/

typedef struct QElement {       /* double-linked list data type */
    struct QElement *next;      /* pointer to next entry (NULL if none) */
    struct QElement *prev;      /* pointer to previous entry (NULL if none) */
    char *buffer;
} QElement;

typedef struct {                /* generic double-linked list head */
    QElement *first;            /* pointer to first entry (NULL if empty list) */
    QElement *last;             /* pointer to last entry (NULL if empty list) */
    long nnodes;
} QHead;

void  q_init(QHead *list);
void  q_append(QElement *node, QHead  *headNode);
void  q_prepend(QElement *node, QHead *headNode);
void *q_find(QElement *node, QHead *headNode);
void  q_after(QElement *elem1, QElement *elem2, QHead *headNode);
void  q_before(QElement *ele1, QElement *ele2, QHead *headNode);
void  q_remove(QElement *node, QHead *headNode);
void  q_move(QElement *node, QHead *headNode1, QHead *headNode2);

/* The Qhead Node is passed in an allocated entity */
#define QInit(qh)           q_init((QHead *)(qh))
#define QInsert(q, qh)      q_append((QElement *)(q), (QHead *)(qh))
#define QAppend(q, qh)      q_append((QElement *)(q), (QHead *)(qh))
#define QPrepend(q, qh)     q_prepend((QElement *)(q), (QHead *)(qh))
#define QAfter(q1, q2, qh)  q_after((QElement *)(q1), (QElement *)(q2), (QHead *)(qh))
#define QBefore(q1, q2, qh) q_before((QElement *)(q1), (QElement *)(q2), (QHead *)(qh))
#define QRemove(q, qh)      q_remove((QElement *)(q), (QHead *)(qh))
#define QMove(q, from, to)  q_move((QElement *)(q), (QHead *)(from), (QHead *)(to))
