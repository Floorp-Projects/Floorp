/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jspropertytree_h___
#define jspropertytree_h___

#include "jsalloc.h"

#include "js/HashTable.h"
#include "js/RootingAPI.h"

namespace js {

ForwardDeclare(Shape);
struct StackShape;

struct ShapeHasher {
    typedef RawShape Key;
    typedef StackShape Lookup;

    static inline HashNumber hash(const Lookup &l);
    static inline bool match(Key k, const Lookup &l);
};

typedef HashSet<RawShape, ShapeHasher, SystemAllocPolicy> KidsHash;

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
    RawShape toShape() const {
        JS_ASSERT(isShape());
        return reinterpret_cast<RawShape>(w & ~uintptr_t(TAG));
    }
    void setShape(RawShape shape) {
        JS_ASSERT(shape);
        JS_ASSERT((reinterpret_cast<uintptr_t>(static_cast<RawShape>(shape)) & TAG) == 0);
        w = reinterpret_cast<uintptr_t>(static_cast<RawShape>(shape)) | SHAPE;
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
    void checkConsistency(RawShape aKid) const;
#endif
};

class PropertyTree
{
    friend class ::JSFunction;

    JSCompartment *compartment;

    bool insertChild(JSContext *cx, RawShape parent, RawShape child);

    PropertyTree();

  public:
    enum { MAX_HEIGHT = 128 };

    PropertyTree(JSCompartment *comp)
        : compartment(comp)
    {
    }

    RawShape newShape(JSContext *cx);
    RawShape getChild(JSContext *cx, Shape *parent, uint32_t nfixed, const StackShape &child);

#ifdef DEBUG
    static void dumpShapes(JSRuntime *rt);
#endif
};

} /* namespace js */

#endif /* jspropertytree_h___ */
