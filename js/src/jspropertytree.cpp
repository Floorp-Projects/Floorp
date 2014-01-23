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
        return nullptr;
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
        child->parent = nullptr;
        return;
    }

    KidsHash *hash = kidp->toHash();
    JS_ASSERT(hash->count() >= 2);      /* otherwise kidp->isShape() should be true */

    hash->remove(child);
    child->parent = nullptr;

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
PropertyTree::getChild(ExclusiveContext *cx, Shape *parentArg, StackShape &unrootedChild)
{
    RootedShape parent(cx, parentArg);
    JS_ASSERT(parent);

    Shape *existingShape = nullptr;

    /*
     * The property tree has extremely low fan-out below its root in
     * popular embeddings with real-world workloads. Patterns such as
     * defining closures that capture a constructor's environment as
     * getters or setters on the new object that is passed in as
     * |this| can significantly increase fan-out below the property
     * tree root -- see bug 335700 for details.
     */
    KidsPointer *kidp = &parent->kids;
    if (kidp->isShape()) {
        Shape *kid = kidp->toShape();
        if (kid->matches(unrootedChild))
        existingShape = kid;
    } else if (kidp->isHash()) {
        if (KidsHash::Ptr p = kidp->toHash()->lookup(unrootedChild))
        existingShape = *p;
    } else {
        /* If kidp->isNull(), we always insert. */
    }

#ifdef JSGC_INCREMENTAL
    if (existingShape) {
        JS::Zone *zone = existingShape->zone();
        if (zone->needsBarrier()) {
            /*
             * We need a read barrier for the shape tree, since these are weak
             * pointers.
             */
            Shape *tmp = existingShape;
            MarkShapeUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
            JS_ASSERT(tmp == existingShape);
        } else if (zone->isGCSweeping() && !existingShape->isMarked() &&
                   !existingShape->arenaHeader()->allocatedDuringIncremental)
        {
            /*
             * The shape we've found is unreachable and due to be finalized, so
             * remove our weak reference to it and don't use it.
             */
            JS_ASSERT(parent->isMarked());
            parent->removeChild(existingShape);
            existingShape = nullptr;
        }
    }
#endif

    if (existingShape)
        return existingShape;

    RootedGeneric<StackShape*> child(cx, &unrootedChild);

    Shape *shape = newShape(cx);
    if (!shape)
        return nullptr;

    new (shape) Shape(*child, parent->numFixedSlots());

    if (!insertChild(cx, parent, shape))
        return nullptr;

    return shape;
}

Shape *
PropertyTree::lookupChild(ThreadSafeContext *cx, Shape *parent, const StackShape &child)
{
    /* Keep this in sync with the logic of getChild above. */
    Shape *shape = nullptr;

    JS_ASSERT(parent);

    KidsPointer *kidp = &parent->kids;
    if (kidp->isShape()) {
        Shape *kid = kidp->toShape();
        if (kid->matches(child))
            shape = kid;
    } else if (kidp->isHash()) {
        if (KidsHash::Ptr p = kidp->toHash()->readonlyThreadsafeLookup(child))
            shape = *p;
    } else {
        return nullptr;
    }

#ifdef JSGC_INCREMENTAL
    mozilla::DebugOnly<JS::Zone *> zone = shape->arenaHeader()->zone;
    JS_ASSERT(!zone->needsBarrier());
    JS_ASSERT(!(zone->isGCSweeping() && !shape->isMarked() &&
		!shape->arenaHeader()->allocatedDuringIncremental));
#endif

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
    /* This is only used from gdb, so allowing GC here would just be confusing. */
    gc::AutoSuppressGC suppress(cx);

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
            Value v = IdToValue(propid);
            JSString *s = ToStringSlow<NoGC>(cx, v);
            fputs("object ", fp);
            str = s ? s->ensureLinear(cx) : nullptr;
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

#endif
