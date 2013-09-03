/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jspropertytree.h"

#include "jscntxt.h"
#include "jsgc.h"
#include "jstypes.h"

#include "vm/Shape.h"

#include "jsgcinlines.h"

#include "vm/Shape-inl.h"

using namespace js;

inline HashNumber
ShapeHasher::hash(const Lookup &l)
{
    return l.hash();
}

inline bool
ShapeHasher::match(const Key k, const Lookup &l)
{
    return k->matches(l);
}

Shape *
PropertyTree::newShape(ExclusiveContext *cx)
{
    Shape *shape = js_NewGCShape(cx);
    if (!shape)
        js_ReportOutOfMemory(cx);
    return shape;
}

static KidsHash *
HashChildren(Shape *kid1, Shape *kid2)
{
    KidsHash *hash = js_new<KidsHash>();
    if (!hash || !hash->init(2)) {
        js_delete(hash);
        return NULL;
    }

    JS_ALWAYS_TRUE(hash->putNew(kid1, kid1));
    JS_ALWAYS_TRUE(hash->putNew(kid2, kid2));
    return hash;
}

bool
PropertyTree::insertChild(ExclusiveContext *cx, Shape *parent, Shape *child)
{
    JS_ASSERT(!parent->inDictionary());
    JS_ASSERT(!child->parent);
    JS_ASSERT(!child->inDictionary());
    JS_ASSERT(child->compartment() == parent->compartment());
    JS_ASSERT(cx->isInsideCurrentCompartment(this));

    KidsPointer *kidp = &parent->kids;

    if (kidp->isNull()) {
        child->setParent(parent);
        kidp->setShape(child);
        return true;
    }

    if (kidp->isShape()) {
        Shape *shape = kidp->toShape();
        JS_ASSERT(shape != child);
        JS_ASSERT(!shape->matches(child));

        KidsHash *hash = HashChildren(shape, child);
        if (!hash) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        kidp->setHash(hash);
        child->setParent(parent);
        return true;
    }

    if (!kidp->toHash()->putNew(child, child)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    child->setParent(parent);
    return true;
}

void
Shape::removeChild(Shape *child)
{
    JS_ASSERT(!child->inDictionary());
    JS_ASSERT(child->parent == this);

    KidsPointer *kidp = &kids;

    if (kidp->isShape()) {
        JS_ASSERT(kidp->toShape() == child);
        kidp->setNull();
        child->parent = NULL;
        return;
    }

    KidsHash *hash = kidp->toHash();
    JS_ASSERT(hash->count() >= 2);      /* otherwise kidp->isShape() should be true */

    hash->remove(child);
    child->parent = NULL;

    if (hash->count() == 1) {
        /* Convert from HASH form back to SHAPE form. */
        KidsHash::Range r = hash->all();
        Shape *otherChild = r.front();
        JS_ASSERT((r.popFront(), r.empty()));    /* No more elements! */
        kidp->setShape(otherChild);
        js_delete(hash);
    }
}

Shape *
PropertyTree::getChild(ExclusiveContext *cx, Shape *parent_, uint32_t nfixed, const StackShape &child)
{
    {
        Shape *shape = NULL;

        JS_ASSERT(parent_);

        /*
         * The property tree has extremely low fan-out below its root in
         * popular embeddings with real-world workloads. Patterns such as
         * defining closures that capture a constructor's environment as
         * getters or setters on the new object that is passed in as
         * |this| can significantly increase fan-out below the property
         * tree root -- see bug 335700 for details.
         */
        KidsPointer *kidp = &parent_->kids;
        if (kidp->isShape()) {
            Shape *kid = kidp->toShape();
            if (kid->matches(child))
                shape = kid;
        } else if (kidp->isHash()) {
            if (KidsHash::Ptr p = kidp->toHash()->lookup(child))
                shape = *p;
        } else {
            /* If kidp->isNull(), we always insert. */
        }

#ifdef JSGC_INCREMENTAL
        if (shape) {
            JS::Zone *zone = shape->zone();
            if (zone->needsBarrier()) {
                /*
                 * We need a read barrier for the shape tree, since these are weak
                 * pointers.
                 */
                Shape *tmp = shape;
                MarkShapeUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
                JS_ASSERT(tmp == shape);
            } else if (zone->isGCSweeping() && !shape->isMarked() &&
                       !shape->arenaHeader()->allocatedDuringIncremental)
            {
                /*
                 * The shape we've found is unreachable and due to be finalized, so
                 * remove our weak reference to it and don't use it.
                 */
                JS_ASSERT(parent_->isMarked());
                parent_->removeChild(shape);
                shape = NULL;
            }
        }
#endif

        if (shape)
            return shape;
    }

    StackShape::AutoRooter childRoot(cx, &child);
    RootedShape parent(cx, parent_);

    Shape *shape = newShape(cx);
    if (!shape)
        return NULL;

    new (shape) Shape(child, nfixed);

    if (!insertChild(cx, parent, shape))
        return NULL;

    return shape;
}

void
Shape::sweep()
{
    if (inDictionary())
        return;

    /*
     * We detach the child from the parent if the parent is reachable.
     *
     * Note that due to incremental sweeping, the parent pointer may point
     * to the original reachable parent, or it may point to a new live
     * object allocated in the same cell that used to hold the parent.
     *
     * There are three cases:
     *
     * Case 1: parent is not marked - parent is unreachable, may have been
     *         finalized, and the cell may subsequently have been
     *         reallocated to a compartment that is not being marked (cells
     *         are marked when allocated in a compartment that is currenly
     *         being marked by the collector).
     *
     * Case 2: parent is marked and is in a different compartment - parent
     *         has been freed and reallocated to compartment that was being
     *         marked.
     *
     * Case 3: parent is marked and is in the same compartment - parent is
     *         stil reachable and we need to detach from it.
     */
    if (parent && parent->isMarked() && parent->compartment() == compartment())
        parent->removeChild(this);
}

void
Shape::finalize(FreeOp *fop)
{
    if (!inDictionary() && kids.isHash())
        fop->delete_(kids.toHash());
}

#ifdef DEBUG

void
KidsPointer::checkConsistency(Shape *aKid) const
{
    if (isShape()) {
        JS_ASSERT(toShape() == aKid);
    } else {
        JS_ASSERT(isHash());
        KidsHash *hash = toHash();
        KidsHash::Ptr ptr = hash->lookup(aKid);
        JS_ASSERT(*ptr == aKid);
    }
}

void
Shape::dump(JSContext *cx, FILE *fp) const
{
    jsid propid = this->propid();

    JS_ASSERT(!JSID_IS_VOID(propid));

    if (JSID_IS_INT(propid)) {
        fprintf(fp, "[%ld]", (long) JSID_TO_INT(propid));
    } else {
        JSLinearString *str;
        if (JSID_IS_ATOM(propid)) {
            str = JSID_TO_ATOM(propid);
        } else {
            JS_ASSERT(JSID_IS_OBJECT(propid));
            RootedValue v(cx, IdToValue(propid));
            JSString *s = ToStringSlow<CanGC>(cx, v);
            fputs("object ", fp);
            str = s ? s->ensureLinear(cx) : NULL;
        }
        if (!str)
            fputs("<error>", fp);
        else
            FileEscapedString(fp, str, '"');
    }

    fprintf(fp, " g/s %p/%p slot %d attrs %x ",
            JS_FUNC_TO_DATA_PTR(void *, base()->rawGetter),
            JS_FUNC_TO_DATA_PTR(void *, base()->rawSetter),
            hasSlot() ? slot() : -1, attrs);

    if (attrs) {
        int first = 1;
        fputs("(", fp);
#define DUMP_ATTR(name, display) if (attrs & JSPROP_##name) fputs(&(" " #display)[first], fp), first = 0
        DUMP_ATTR(ENUMERATE, enumerate);
        DUMP_ATTR(READONLY, readonly);
        DUMP_ATTR(PERMANENT, permanent);
        DUMP_ATTR(GETTER, getter);
        DUMP_ATTR(SETTER, setter);
        DUMP_ATTR(SHARED, shared);
#undef  DUMP_ATTR
        fputs(") ", fp);
    }

    fprintf(fp, "flags %x ", flags);
    if (flags) {
        int first = 1;
        fputs("(", fp);
#define DUMP_FLAG(name, display) if (flags & name) fputs(&(" " #display)[first], fp), first = 0
        DUMP_FLAG(HAS_SHORTID, has_shortid);
        DUMP_FLAG(IN_DICTIONARY, in_dictionary);
#undef  DUMP_FLAG
        fputs(") ", fp);
    }

    fprintf(fp, "shortid %d\n", maybeShortid());
}

void
Shape::dumpSubtree(JSContext *cx, int level, FILE *fp) const
{
    if (!parent) {
        JS_ASSERT(level == 0);
        JS_ASSERT(JSID_IS_EMPTY(propid_));
        fprintf(fp, "class %s emptyShape\n", getObjectClass()->name);
    } else {
        fprintf(fp, "%*sid ", level, "");
        dump(cx, fp);
    }

    if (!kids.isNull()) {
        ++level;
        if (kids.isShape()) {
            Shape *kid = kids.toShape();
            JS_ASSERT(kid->parent == this);
            kid->dumpSubtree(cx, level, fp);
        } else {
            const KidsHash &hash = *kids.toHash();
            for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
                Shape *kid = range.front();

                JS_ASSERT(kid->parent == this);
                kid->dumpSubtree(cx, level, fp);
            }
        }
    }
}

void
js::PropertyTree::dumpShapes(JSRuntime *rt)
{
    static bool init = false;
    static FILE *dumpfp = NULL;
    if (!init) {
        init = true;
        const char *name = getenv("JS_DUMP_SHAPES_FILE");
        if (!name)
            return;
        dumpfp = fopen(name, "a");
    }

    if (!dumpfp)
        return;

    fprintf(dumpfp, "rt->gcNumber = %lu", (unsigned long)rt->gcNumber);

    for (gc::GCCompartmentsIter c(rt); !c.done(); c.next()) {
        fprintf(dumpfp, "*** Compartment %p ***\n", (void *)c.get());

        /*
        typedef JSCompartment::EmptyShapeSet HS;
        HS &h = c->emptyShapes;
        for (HS::Range r = h.all(); !r.empty(); r.popFront()) {
            Shape *empty = r.front();
            empty->dumpSubtree(rt, 0, dumpfp);
            putc('\n', dumpfp);
        }
        */
    }
}
#endif
