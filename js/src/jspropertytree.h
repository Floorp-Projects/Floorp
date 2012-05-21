/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jspropertytree_h___
#define jspropertytree_h___

#include "jsprvtd.h"

#include "js/HashTable.h"

namespace js {

struct ShapeHasher {
    typedef js::Shape *Key;
    typedef js::StackShape Lookup;

    static inline HashNumber hash(const Lookup &l);
    static inline bool match(Key k, const Lookup &l);
};

typedef HashSet<js::Shape *, ShapeHasher, SystemAllocPolicy> KidsHash;

class KidsPointer {
  private:
    enum {
        SHAPE = 0,
        HASH  = 1,
        TAG   = 1
    };

    uintptr_t w;

  public:
    bool isNull() const { return !w; }
    void setNull() { w = 0; }

    bool isShape() const { return (w & TAG) == SHAPE && !isNull(); }
    js::Shape *toShape() const {
        JS_ASSERT(isShape());
        return reinterpret_cast<js::Shape *>(w & ~uintptr_t(TAG));
    }
    void setShape(js::Shape *shape) {
        JS_ASSERT(shape);
        JS_ASSERT((reinterpret_cast<uintptr_t>(shape) & TAG) == 0);
        w = reinterpret_cast<uintptr_t>(shape) | SHAPE;
    }

    bool isHash() const { return (w & TAG) == HASH; }
    KidsHash *toHash() const {
        JS_ASSERT(isHash());
        return reinterpret_cast<KidsHash *>(w & ~uintptr_t(TAG));
    }
    void setHash(KidsHash *hash) {
        JS_ASSERT(hash);
        JS_ASSERT((reinterpret_cast<uintptr_t>(hash) & TAG) == 0);
        w = reinterpret_cast<uintptr_t>(hash) | HASH;
    }

#ifdef DEBUG
    void checkConsistency(const js::Shape *aKid) const;
#endif
};

class PropertyTree
{
    friend struct ::JSFunction;

    JSCompartment *compartment;

    bool insertChild(JSContext *cx, js::Shape *parent, js::Shape *child);

    PropertyTree();

  public:
    enum { MAX_HEIGHT = 128 };

    PropertyTree(JSCompartment *comp)
        : compartment(comp)
    {
    }

    js::Shape *newShape(JSContext *cx);
    js::Shape *getChild(JSContext *cx, Shape *parent, uint32_t nfixed, const StackShape &child);

#ifdef DEBUG
    static void dumpShapes(JSRuntime *rt);
    static void meter(JSBasicStats *bs, js::Shape *node);
#endif
};

} /* namespace js */

#endif /* jspropertytree_h___ */
