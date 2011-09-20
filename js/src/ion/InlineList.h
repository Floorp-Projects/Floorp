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

template <typename T> class InlineForwardList;
template <typename T> class InlineForwardListIterator;

template <typename T>
class InlineForwardListNode
{
  public:
    InlineForwardListNode() : next(NULL)
    { }
    InlineForwardListNode(InlineForwardListNode<T> *n) : next(n)
    { }

  protected:
    friend class InlineForwardList<T>;
    friend class InlineForwardListIterator<T>;

    InlineForwardListNode<T> *next;
};

template <typename T>
class InlineForwardList : protected InlineForwardListNode<T>
{
    friend class InlineForwardListIterator<T>;

    typedef InlineForwardListNode<T> Node;

    Node *tail_;
#ifdef DEBUG
    uintptr_t modifyCount_;
#endif

    InlineForwardList<T> *thisFromConstructor() {
        return this;
    }

  public:
    InlineForwardList()
      : tail_(thisFromConstructor())
#ifdef DEBUG
      ,  modifyCount_(0)
#endif
    { }

  public:
    typedef InlineForwardListIterator<T> iterator;

  public:
    iterator begin() const {
        return iterator(this);
    }
    iterator end() const {
        return iterator(NULL);
    }
    iterator removeAt(iterator &where) {
        iterator iter(where);
        iter++;
        iter.prev = where.prev;
#ifdef DEBUG
        iter.modifyCount++;
#endif

        // Once the element 'where' points at has been removed, it is no longer
        // safe to do any operations that would touch 'iter', as the element
        // may be added to another list, etc. This NULL ensures that any
        // improper uses of this function will fail quickly and loudly.
        removeAfter(where.prev, where.iter);
        where.prev = where.iter = NULL;

        return iter;
    }
    void pushFront(Node *t) {
        insertAfter(this, t);
    }
    void pushBack(Node *t) {
#ifdef DEBUG
        modifyCount_++;
#endif
        tail_->next = t;
        t->next = NULL;
        tail_ = t;
    }
    T *popFront() {
        JS_ASSERT(!empty());
        T* result = static_cast<T *>(this->next);
        removeAfter(this, result);
        return result;
    }
    void insertAfter(Node *at, Node *item) {
#ifdef DEBUG
        modifyCount_++;
#endif
        if (at == tail_)
            tail_ = item;
        item->next = at->next;
        at->next = item;
    }
    void removeAfter(Node *at, Node *item) {
#ifdef DEBUG
        modifyCount_++;
#endif
        if (item == tail_)
            tail_ = at;
        JS_ASSERT(at->next == item);
        at->next = item->next;
    }
    bool empty() const {
        return tail_ == this;
    }
};

template <typename T>
class InlineForwardListIterator
{
private:
    friend class InlineForwardList<T>;

    typedef InlineForwardListNode<T> Node;

    InlineForwardListIterator<T>(const InlineForwardList<T> *owner)
      : prev(const_cast<Node *>(static_cast<const Node *>(owner))),
        iter(owner ? owner->next : NULL)
#ifdef DEBUG
      , owner(owner),
        modifyCount(owner ? owner->modifyCount_ : 0)
#endif
    { }

public:
    InlineForwardListIterator<T> & operator ++() {
        JS_ASSERT(modifyCount == owner->modifyCount_);
        prev = iter;
        iter = iter->next;
        return *this;
    }
    InlineForwardListIterator<T> operator ++(int) {
        JS_ASSERT(modifyCount == owner->modifyCount_);
        InlineForwardListIterator<T> old(*this);
        prev = iter;
        iter = iter->next;
        return old;
    }
    T * operator *() const {
        JS_ASSERT(modifyCount == owner->modifyCount_);
        return static_cast<T *>(iter);
    }
    T * operator ->() const {
        JS_ASSERT(modifyCount == owner->modifyCount_);
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineForwardListIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineForwardListIterator<T> &where) const {
        return iter == where.iter;
    }

private:
    Node *prev;
    Node *iter;
#ifdef DEBUG
    const InlineForwardList<T> *owner;
    uintptr_t modifyCount;
#endif
};

template <typename T> class InlineList;
template <typename T> class InlineListIterator;
template <typename T> class InlineListReverseIterator;

template <typename T>
class InlineListNode : public InlineForwardListNode<T>
{
  public:
    InlineListNode() : InlineForwardListNode<T>(NULL), prev(NULL)
    { }
    InlineListNode(InlineListNode<T> *n, InlineListNode<T> *p)
      : InlineForwardListNode<T>(n),
        prev(p)
    { }

  protected:
    friend class InlineList<T>;
    friend class InlineListIterator<T>;
    friend class InlineListReverseIterator<T>;

    InlineListNode<T> *prev;
};

template <typename T>
class InlineList : protected InlineListNode<T>
{
    typedef InlineListNode<T> Node;

    // Silence MSVC warning C4355
    InlineList<T> *thisFromConstructor() {
        return this;
    }

  public:
    InlineList() : InlineListNode<T>(thisFromConstructor(), thisFromConstructor())
    { }

  public:
    typedef InlineListIterator<T> iterator;
    typedef InlineListReverseIterator<T> reverse_iterator;

  public:
    iterator begin() const {
        return iterator(static_cast<Node *>(this->next));
    }
    iterator end() const {
        return iterator(this);
    }
    reverse_iterator rbegin() {
        return reverse_iterator(this->prev);
    }
    reverse_iterator rend() {
        return reverse_iterator(this);
    }
    template <typename itertype>
    itertype removeAt(itertype &where) {
        itertype iter(where);
        iter++;

        // Once the element 'where' points at has been removed, it is no longer
        // safe to do any operations that would touch 'iter', as the element
        // may be added to another list, etc. This NULL ensures that any
        // improper uses of this function will fail quickly and loudly.
        remove(where.iter);
        where.iter = NULL;

        return iter;
    }
    void pushFront(Node *t) {
        insertAfter(this, t);
    }
    void pushBack(Node *t) {
        insertBefore(this, t);
    }
    T *popFront() {
        JS_ASSERT(!empty());
        T *t = static_cast<T *>(this->next);
        remove(t);
        return t;
    }
    T *popBack() {
        JS_ASSERT(!empty());
        T *t = static_cast<T *>(this->prev);
        remove(t);
        return t;
    }
    T *peekBack() const {
        iterator iter = end();
        iter--;
        return *iter;
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
        static_cast<Node *>(at->next)->prev = item;
        at->next = item;
    }
    void steal(InlineList<T> *from) {
        Node *oldTail = this->prev;
        Node *stealHead = from->next;
        Node *stealTail = from->prev;
        oldTail->next = stealHead;
        stealHead->prev = oldTail;
        stealTail->next = this;
        this->prev = stealTail;
        from->next = from->prev = from;
    }
    void remove(Node *t) {
        t->prev->next = t->next;
        static_cast<Node *>(t->next)->prev = t->prev;
        t->next = t->prev = NULL;
    }
    void clear() {
        this->next = this->prev = this;
    }
    bool empty() const {
        return begin() == end();
    }
};

template <typename T>
class InlineListIterator
{
  private:
    friend class InlineList<T>;

    typedef InlineListNode<T> Node;

    InlineListIterator(const Node *iter)
      : iter(const_cast<Node *>(iter))
    { }

  public:
    InlineListIterator<T> & operator ++() {
        iter = iter->next;
        return *iter;
    }
    InlineListIterator<T> operator ++(int) {
        InlineListIterator<T> old(*this);
        iter = static_cast<Node *>(iter->next);
        return old;
    }
    InlineListIterator<T> operator --(int) {
        InlineListIterator<T> old(*this);
        iter = iter->prev;
        return old;
    }
    T * operator *() const {
        return static_cast<T *>(iter);
    }
    T * operator ->() const {
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineListIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineListIterator<T> &where) const {
        return iter == where.iter;
    }

  private:
    Node *iter;
};

template <typename T>
class InlineListReverseIterator
{
  private:
    friend class InlineList<T>;

    typedef InlineListNode<T> Node;

    InlineListReverseIterator(const Node *iter)
      : iter(const_cast<Node *>(iter))
    { }

  public:
    InlineListReverseIterator<T> & operator ++() {
        iter = iter->prev;
        return *iter;
    }
    InlineListReverseIterator<T> operator ++(int) {
        InlineListReverseIterator<T> old(*this);
        iter = iter->prev;
        return old;
    }
    T * operator *() {
        return static_cast<T *>(iter);
    }
    T * operator ->() {
        return static_cast<T *>(iter);
    }
    bool operator !=(const InlineListReverseIterator<T> &where) const {
        return iter != where.iter;
    }
    bool operator ==(const InlineListReverseIterator<T> &where) const {
        return iter == where.iter;
    }

  private:
    Node *iter;
};

} // namespace js

#endif // js_inline_list_h__
