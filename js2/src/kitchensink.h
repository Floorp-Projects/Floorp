/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef kitchensink_h___
#define kitchensink_h___

#include <memory>
#include <new>
#include <string>
#include <iterator>
#include <algorithm>
#include <cstdio>
#include <cstdarg>

#include "stlcfg.h"
#include "systemtypes.h"
#include "strings.h"
#include "mem.h"


namespace JavaScript {
//
// Numerics
//

        template<class N> N min(N v1, N v2) {return v1 <= v2 ? v1 : v2;}
        template<class N> N max(N v1, N v2) {return v1 >= v2 ? v1 : v2;}


//
// Alignment
//

        template<typename T>
        struct AlignmentHelper {
                char ch;
                T t;
        };
        #define ALIGNMENT_OF(T) offsetof(JavaScript::AlignmentHelper<T>, t)



        // Assign zero to n elements starting at first.
        // This is equivalent ot fill_n(first, n, 0) but may be more efficient.
        template<class ForwardIterator, class Size>
        inline void zero_n(ForwardIterator first, Size n)
        {
                while (n) {
                        *first = 0;
                        ++first;
                        --n;
                }
        }

//
// Linked Lists
//

        // In some cases it is desirable to manipulate ordinary C-style linked lists as though
        // they were STL-like sequences.  These classes define STL forward iterators that walk
        // through singly-linked lists of objects threaded through fields named 'next'.  The type
        // parameter E must be a class that has a member named 'next' whose type is E* or const E*.

        template <class E>
        class ListIterator: public std::iterator<std::forward_iterator_tag, E> {
                E *element;

          public:
                ListIterator() {}
                explicit ListIterator(E *e): element(e) {}

                E &operator*() const {return *element;}
                E *operator->() const {return element;}
                ListIterator &operator++() {element = element->next; return *this;}
                ListIterator operator++(int) {ListIterator i(*this); element = element->next; return i;}
                friend bool operator==(const ListIterator &i, const ListIterator &j) {return i.element == j.element;}
                friend bool operator!=(const ListIterator &i, const ListIterator &j) {return i.element != j.element;}
        };

        
        template <class E>
  #ifndef _WIN32 // Microsoft VC6 bug: std::iterator should support five template arguments
        class ConstListIterator: public std::iterator<std::forward_iterator_tag, E, ptrdiff_t, const E*, const E&> {
  #else
        class ConstListIterator: public std::iterator<std::forward_iterator_tag, E, ptrdiff_t> {
  #endif
                const E *element;

          public:
                ConstListIterator() {}
                ConstListIterator(const ListIterator<E> &i): element(&*i) {}
                explicit ConstListIterator(const E *e): element(e) {}

                const E &operator*() const {return *element;}
                const E *operator->() const {return element;}
                ConstListIterator &operator++() {element = element->next; return *this;}
                ConstListIterator operator++(int) {ConstListIterator i(*this); element = element->next; return i;}
                friend bool operator==(const ConstListIterator &i, const ConstListIterator &j) {return i.element == j.element;}
                friend bool operator!=(const ConstListIterator &i, const ConstListIterator &j) {return i.element != j.element;}
        };


//
// Input
//


}

#endif /* kitchensink_h___ */
