/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef XPLIST_H
#define XPLIST_H

#include "xp_core.h"

/* generic list structure
 */
struct _XP_List {
  void            * object;
  struct _XP_List * next;
  struct _XP_List * prev;
};
 
XP_BEGIN_PROTOS

extern XP_List * XP_ListNew (void);
extern void XP_ListDestroy (XP_List *list);

extern void     XP_ListAddObject (XP_List *list, void *newObject);
extern void     XP_ListAddObjectToEnd (XP_List *list, void *newObject);
extern void     XP_ListInsertObject (XP_List *list, void *insert_before, void *newObject);
extern void XP_ListInsertObjectAfter (XP_List *list, void *insert_after, void *newObject);


/* returns the list node of the specified object if it was
 * in the list
 */
extern XP_List * XP_ListFindObject (XP_List *list, void * obj);


extern Bool     XP_ListRemoveObject (XP_List *list, void *oldObject);
extern void *   XP_ListRemoveTopObject (XP_List *list);
extern void *   XP_ListPeekTopObject (XP_List *list);
extern void *   XP_ListRemoveEndObject (XP_List *list);
#define         XP_ListIsEmpty(list) (list ? list->next == NULL : TRUE)
extern int      XP_ListCount (XP_List *list);
#define         XP_ListTopObject(list) (list && list->next ? list->next->object : NULL)

extern void *   XP_ListGetEndObject (XP_List *list);
extern void * XP_ListGetObjectNum (XP_List *list, int num);
extern int    XP_ListGetNumFromObject (XP_List *list, void * object);

/* move the top object to the bottom of the list
 * this function is useful for reordering the list
 * so that a round robin ordering can occur
 */
extern void XP_ListMoveTopToBottom (XP_List *list);


XP_END_PROTOS

/* traverse the list in order
 *
 * make a copy of the list pointer and point it at the list object head.
 * the first call the XP_ListNextObject will return the first object
 * and increment the copy of the list pointer.  Subsequent calls
 * will continue to increment the copy of the list pointer and return
 * objects
 */
#define XP_ListNextObject(list) \
  (list && ((list = list->next)!=0) ? list->object : NULL)

#endif /* XPLIST_H */
