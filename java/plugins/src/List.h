/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
#ifndef __List_h__
#define __List_h__
#include <stdlib.h> //in order to have NULL definition
class ListIterator;

class List {
    friend class ListIterator;
 public:
    List();
    ~List();
    void Add(void *value);
    ListIterator * GetIterator(void);
 private:
    struct ListNode {
	ListNode(void *v,ListNode *n = NULL);
	void * value;
	ListNode * next;
    };
    ListNode * head;
    ListNode * tail;
};
class ListIterator {
 public:
    ListIterator(List *list);
    void * Get(void);
    void * Next(void);
 private:
   List::ListNode *current;
};
    
#endif /* __List_h__ */
