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
#ifndef __PlugletList_h__
#define __PlugletList_h__
#include "Pluglet.h"
class PlugletListIterator;

class PlugletList {
    friend class PlugletListIterator;
 public:
    PlugletList();
    ~PlugletList();
    void Add(Pluglet *pluglet);
    PlugletListIterator * GetIterator(void);
 private:
    struct PlugletListNode {
	PlugletListNode(Pluglet *v,PlugletListNode *n = NULL);
	Pluglet * value;
	PlugletListNode * next;
    };
    PlugletListNode * head;
    PlugletListNode * tail;
};
class PlugletListIterator {
 public:
    PlugletListIterator(PlugletList *list);
    Pluglet * Get(void);
    Pluglet * Next(void);
 private:
    PlugletList::PlugletListNode *current;
};
    
#endif /* __PlugletList_h__ */
