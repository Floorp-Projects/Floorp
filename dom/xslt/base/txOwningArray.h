/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txOwningArray_h__
#define txOwningArray_h__

// Class acting like a nsTArray except that it deletes its objects
// on destruction. It does not however delete its objects on operations
// like RemoveElementsAt or on |array[i] = bar|.

template<class E>
class txOwningArray : public nsTArray<E*>
{
public:
    typedef nsTArray<E*> base_type;
    typedef typename base_type::elem_type elem_type;

    ~txOwningArray()
    {
        elem_type* iter = base_type::Elements();
        elem_type* end = iter + base_type::Length();
        for (; iter < end; ++iter) {
            delete *iter;
        }
    }
  
};

#endif // txOwningArray_h__
