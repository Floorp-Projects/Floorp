/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey property tree implementation
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2002-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
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

#ifndef jspropertytree_h___
#define jspropertytree_h___

#include "jsarena.h"
#include "jshashtable.h"
#include "jsprvtd.h"

namespace js {

enum {
    MAX_KIDS_PER_CHUNK   = 10U,
    CHUNK_HASH_THRESHOLD = 30U
};

struct KidsChunk {
    js::Shape   *kids[MAX_KIDS_PER_CHUNK];
    KidsChunk   *next;

    static KidsChunk *create(JSContext *cx);
    static KidsChunk *destroy(JSContext *cx, KidsChunk *chunk);
};

struct ShapeHasher {
    typedef js::Shape *Key;
    typedef const js::Shape *Lookup;

    static inline HashNumber hash(const Lookup l);
    static inline bool match(Key k, Lookup l);
};

typedef HashSet<js::Shape *, ShapeHasher, SystemAllocPolicy> KidsHash;

class KidsPointer {
  private:
    enum {
        SHAPE = 0,
        CHUNK = 1,
        HASH  = 2,
        TAG   = 3
    };

    jsuword w;

  public:
    bool isNull() const { return !w; }
    void setNull() { w = 0; }

    bool isShapeOrNull() const { return (w & TAG) == SHAPE; }
    bool isShape() const { return (w & TAG) == SHAPE && !isNull(); }
    js::Shape *toShape() const {
        JS_ASSERT(isShape());
        return reinterpret_cast<js::Shape *>(w & ~jsuword(TAG));
    }
    void setShape(js::Shape *shape) {
        JS_ASSERT(shape);
        JS_ASSERT((reinterpret_cast<jsuword>(shape) & TAG) == 0);
        w = reinterpret_cast<jsuword>(shape) | SHAPE;
    }

    bool isChunk() const { return (w & TAG) == CHUNK; }
    KidsChunk *toChunk() const {
        JS_ASSERT(isChunk());
        return reinterpret_cast<KidsChunk *>(w & ~jsuword(TAG));
    }
    void setChunk(KidsChunk *chunk) {
        JS_ASSERT(chunk);
        JS_ASSERT((reinterpret_cast<jsuword>(chunk) & TAG) == 0);
        w = reinterpret_cast<jsuword>(chunk) | CHUNK;
    }

    bool isHash() const { return (w & TAG) == HASH; }
    KidsHash *toHash() const {
        JS_ASSERT(isHash());
        return reinterpret_cast<KidsHash *>(w & ~jsuword(TAG));
    }
    void setHash(KidsHash *hash) {
        JS_ASSERT(hash);
        JS_ASSERT((reinterpret_cast<jsuword>(hash) & TAG) == 0);
        w = reinterpret_cast<jsuword>(hash) | HASH;
    }

#ifdef DEBUG
    void checkConsistency(const js::Shape *aKid) const;
#endif
};

class PropertyTree
{
    friend struct ::JSFunction;

    JSArenaPool arenaPool;
    js::Shape   *freeList;

    bool insertChild(JSContext *cx, js::Shape *parent, js::Shape *child);
    void removeChild(JSContext *cx, js::Shape *child);

  public:
    bool init();
    void finish();

    js::Shape *newShape(JSContext *cx, bool gcLocked = false);
    js::Shape *getChild(JSContext *cx, js::Shape *parent, const js::Shape &child);

    static void orphanKids(JSContext *cx, js::Shape *shape);
    static void sweepShapes(JSContext *cx);
#ifdef DEBUG
    static void meter(JSBasicStats *bs, js::Shape *node);
#endif
};

} /* namespace js */

#endif /* jspropertytree_h___ */
