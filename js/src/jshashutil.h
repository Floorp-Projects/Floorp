/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jshashutil_h
#define jshashutil_h

#include "jscntxt.h"

namespace js {

/*
 * Used to add entries to a js::HashMap or HashSet where the key depends on a GC
 * thing that may be moved by generational collection between the call to
 * lookupForAdd() and relookupOrAdd().
 */
template <class T>
struct DependentAddPtr
{
    typedef typename T::AddPtr AddPtr;
    typedef typename T::Entry Entry;

    template <class Lookup>
    DependentAddPtr(const ExclusiveContext *cx, const T &table, const Lookup &lookup)
      : addPtr(table.lookupForAdd(lookup))
#ifdef JSGC_GENERATIONAL
      , originalGcNumber(cx->zone()->gcNumber())
#endif
        {}

    template <class KeyInput, class ValueInput>
    bool add(const ExclusiveContext *cx, T &table, const KeyInput &key, const ValueInput &value) {
#ifdef JSGC_GENERATIONAL
        bool gcHappened = originalGcNumber != cx->zone()->gcNumber();
        if (gcHappened)
            addPtr = table.lookupForAdd(key);
#endif
        return table.relookupOrAdd(addPtr, key, value);
    }

    typedef void (DependentAddPtr::* ConvertibleToBool)();
    void nonNull() {}

    bool found() const                 { return addPtr.found(); }
    operator ConvertibleToBool() const { return found() ? &DependentAddPtr::nonNull : 0; }
    const Entry &operator*() const     { return *addPtr; }
    const Entry *operator->() const    { return &*addPtr; }

  private:
    AddPtr addPtr ;
#ifdef JSGC_GENERATIONAL
    const uint64_t originalGcNumber;
#endif

    DependentAddPtr() MOZ_DELETE;
    DependentAddPtr(const DependentAddPtr&) MOZ_DELETE;
    DependentAddPtr& operator=(const DependentAddPtr&) MOZ_DELETE;
};

} // namespace js

#endif
