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
#include"List.h"

List::List() {
    head = tail = NULL;
}

List::~List() {
    ListNode *tmp;
    for(ListNode * current=head;!current;tmp = current,current = current->next, delete tmp)
	;
}

List::ListNode::ListNode(void *v,List::ListNode *n) {
    value = v; next = n;
}

void List::Add(void *v) {
    if (!v) {
	return;
    }
    if (!tail) {
	head = tail = new ListNode(v);
    } else {
	tail->next = new ListNode(v);
	if (tail->next) {
	    tail = tail->next;
	}
    }
}

ListIterator * List::GetIterator(void) {
    return new ListIterator(this);
}

ListIterator::ListIterator(List *list) {
    current = list->head;
}
void * ListIterator::Get(void) {
    void * result = NULL;
    if (current) {
	result = current->value;
    }
    return result;
}

void * ListIterator::Next(void) {
    void * result = NULL;
    if (current) {
	current = current->next;
	result = Get();
    }
    return result;
}
