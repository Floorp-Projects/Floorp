/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
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

#ifndef js_inline_list_h__
#define js_inline_list_h__

namespace js {

template <typename T> class InlineList;

template <typename T>
class InlineListNode
{
    friend class InlineList<T>;
  public:
    InlineListNode() : next(NULL), prev(NULL)
    { }
    InlineListNode(InlineListNode *n, InlineListNode *p) : next(n), prev(p)
    { }

  protected:
    InlineListNode *next;
    InlineListNode *prev;
};

template <typename T>
class InlineList
{
    typedef InlineListNode<T> Node;

    Node head;

  public:
    InlineList() : head(&head, &head)
    { }

  public:
    class iterator
    {
        friend class InlineList;
        Node *iter;
      public:
        iterator(Node *iter) : iter(iter) { }

        iterator & operator ++() {
            iter = iter->next;
            return *iter;
        }
        iterator operator ++(int) {
            iterator old(*this);
            iter = iter->next;
            return old;
        }
        T * operator *() {
            return static_cast<T *>(iter);
        }
        T * operator ->() {
            return static_cast<T *>(iter);
        }
        bool operator != (const iterator &where) const {
            return iter != where.iter;
        }
        bool operator == (const iterator &where) const {
            return iter == where.iter;
        }
        iterator prev() {
            iterator p(iter->prev);
            return p;
        }
    };

    class reverse_iterator
    {
        friend class InlineList;
        Node *iter;
      public:
        reverse_iterator(Node *iter) : iter(iter) { }

        reverse_iterator & operator ++() {
            iter = iter->prev;
            return *iter;
        }
        reverse_iterator operator ++(int) {
            reverse_iterator old(*this);
            iter = iter->prev;
            return old;
        }
        T * operator *() {
            return static_cast<T *>(iter);
        }
        T * operator ->() {
            return static_cast<T *>(iter);
        }
        bool operator != (const reverse_iterator &where) const {
            return iter != where.iter;
        }
        bool operator == (const reverse_iterator &where) const {
            return iter == where.iter;
        }
    };

    class const_iterator
    {
        friend class InlineList;
        const Node *iter;
      public:
        const_iterator(const Node *iter) : iter(iter) { }

        const_iterator & operator ++() {
            iter = iter->next;
            return *iter;
        }
        const_iterator operator ++(int) {
            const_iterator old(*this);
            iter = iter->next;
            return old;
        }
        T * operator *() const {
            return const_cast<T *>(static_cast<const T *>(iter));
        }
        T * operator ->() {
            return const_cast<T *>(static_cast<const T *>(iter));
        }
        bool operator != (const const_iterator &where) const {
            return iter != where.iter;
        }
        bool operator == (const const_iterator &where) const {
            return iter == where.iter;
        }
    };

  public:
    iterator begin() {
        return iterator(head.next);
    }
    iterator end() {
        return iterator(&head);
    }
    reverse_iterator rbegin() {
        return reverse_iterator(head.prev);
    }
    reverse_iterator rend() {
        return reverse_iterator(&head);
    }
    const_iterator begin() const {
        return const_iterator(head.next);
    }
    const_iterator end() const {
        return const_iterator(&head);
    }

    iterator removeAt(iterator &where) {
        Node *node = where.iter;
        iterator iter(where);
        iter++;

        node->prev->next = node->next;
        node->next->prev = node->prev;

        return iter;
    }

    void insertBefore(Node *at, Node *item) {
        item->next = at;
        item->prev = at->prev;
        at->prev->next = item;
        at->prev = item;
    }

    void insertAfter(Node *at, Node *item) {
        item->next = at->next;
        item->prev = at;
        at->next->prev = item;
        at->next = item;
    }

    void insert(Node *t) {
        t->prev = head.prev;
        t->next = &head;
        head.prev->next = t;
        head.prev = t;
    }
};

} // namespace js

#endif // js_inline_list_h__

