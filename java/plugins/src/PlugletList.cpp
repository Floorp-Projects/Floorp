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
#include"PlugletList.h"

PlugletList::PlugletList() {
    head = tail = NULL;
}

PlugletList::~PlugletList() {
    for(PlugletListNode * current=head;!current;delete current,current = current->next)
	;
}

PlugletList::PlugletListNode::PlugletListNode(Pluglet *v,PlugletList::PlugletListNode *n) {
    value = v; next = n;
}

void PlugletList::Add(Pluglet *pluglet) {
    if (!pluglet) {
	return;
    }
    if (!tail) {
	head = tail = new PlugletListNode(pluglet);
    } else {
	tail->next = new PlugletListNode(pluglet);
	if (tail->next) {
	    tail = tail->next;
	}
    }
}

PlugletListIterator * PlugletList::GetIterator(void) {
    return new PlugletListIterator(this);
}

PlugletListIterator::PlugletListIterator(PlugletList *list) {
    current = list->head;
}
Pluglet * PlugletListIterator::Get(void) {
    Pluglet * result = NULL;
    if (current) {
	result = current->value;
    }
    return result;
}

Pluglet * PlugletListIterator::Next(void) {
    Pluglet * result = NULL;
    if (current) {
	current = current->next;
	result = Get();
    }
    return result;
}
