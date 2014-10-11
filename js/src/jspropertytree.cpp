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
using namespace js::gc;

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

static KidsHash *
HashChildren(Shape *kid1, Shape *kid2)
{
    KidsHash *hash = js_new<KidsHash>();
    if (!hash || !hash->init(2)) {
        js_delete(hash);
        return nullptr;
    }

    JS_ALWAYS_TRUE(hash->putNew(StackShape(kid1), kid1));
    JS_ALWAYS_TRUE(hash->putNew(StackShape(kid2), kid2));
    return hash;
}

bool
PropertyTree::insertChild(ExclusiveContext *cx, Shape *parent, Shape *child)
{
    MOZ_ASSERT(!parent->inDictionary());
    MOZ_ASSERT(!child->parent);
    MOZ_ASSERT(!child->inDictionary());
    MOZ_ASSERT(child->compartment() == parent->compartment());
    MOZ_ASSERT(cx->isInsideCurrentCompartment(this));

    KidsPointer *kidp = &parent->kids;

    if (kidp->isNull()) {
        child->setParent(parent);
        kidp->setShape(child);
        return true;
    }

    if (kidp->isShape()) {
        Shape *shape = kidp->toShape();
        MOZ_ASSERT(shape != child);
        MOZ_ASSERT(!shape->matches(child));

        KidsHash *hash = HashChildren(shape, child);
        if (!hash) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        kidp->setHash(hash);
        child->setParent(parent);
        return true;
    }

    if (!kidp->toHash()->putNew(StackShape(child), child)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    child->setParent(parent);
    return true;
}

void
Shape::removeChild(Shape *child)
{
    MOZ_ASSERT(!child->inDictionary());
    MOZ_ASSERT(child->parent == this);

    KidsPointer *kidp = &kids;

    if (kidp->isShape()) {
        MOZ_ASSERT(kidp->toShape() == child);
        kidp->setNull();
        child->parent = nullptr;
        return;
    }

    KidsHash *hash = kidp->toHash();
    MOZ_ASSERT(hash->count() >= 2);      /* otherwise kidp->isShape() should be true */

#ifdef DEBUG
    size_t oldCount = hash->count();
#endif

    hash->remove(StackShape(child));
    child->parent = nullptr;

    MOZ_ASSERT(hash->count() == oldCount - 1);

    if (hash->count() == 1) {
        /* Convert from HASH form back to SHAPE form. */
        KidsHash::Range r = hash->all();
        Shape *otherChild = r.front();
        MOZ_ASSERT((r.popFront(), r.empty()));    /* No more elements! */
        kidp->setShape(otherChild);
        js_delete(hash);
    }
}

Shape *
PropertyTree::getChild(ExclusiveContext *cx, Shape *parentArg, StackShape &unrootedChild)
{
    RootedShape parent(cx, parentArg);
    MOZ_ASSERT(parent);

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
        if (zone->needsIncrementalBarrier()) {
            /*
             * We need a read barrier for the shape tree, since these are weak
             * pointers.
             */
            Shape *tmp = existingShape;
            MarkShapeUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
            MOZ_ASSERT(tmp == existingShape);
        } else if (zone->isGCSweeping() && !existingShape->isMarked() &&
                   !existingShape->arenaHeader()->allocatedDuringIncremental)
        {
            /*
             * The shape we've found is unreachable and due to be finalized, so
             * remove our weak reference to it and don't use it.
             */
            MOZ_ASSERT(parent->isMarked());
            parent->removeChild(existingShape);
            existingShape = nullptr;
        } else if (existingShape->isMarked(gc::GRAY)) {
            JS::UnmarkGrayGCThingRecursively(existingShape, JSTRACE_SHAPE);
        }
    }
#endif

    if (existingShape)
        return existingShape;

    Shape *shape = Shape::new_(cx, unrootedChild, parent->numFixedSlots());
    if (!shape)
        return nullptr;

    if (!insertChild(cx, parent, shape))
        return nullptr;

    return shape;
}

Shape *
PropertyTree::lookupChild(ThreadSafeContext *cx, Shape *parent, const StackShape &child)
{
    /* Keep this in sync with the logic of getChild above. */
    Shape *shape = nullptr;

    MOZ_ASSERT(parent);

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

#if defined(JSGC_INCREMENTAL) && defined(DEBUG)
    if (shape) {
        JS::Zone *zone = shape->arenaHeader()->zone;
        MOZ_ASSERT(!zone->needsIncrementalBarrier());
        MOZ_ASSERT(!(zone->isGCSweeping() && !shape->isMarked() &&
                     !shape->arenaHeader()->allocatedDuringIncremental));
    }
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

#ifdef JSGC_COMPACTING

void
Shape::fixupDictionaryShapeAfterMovingGC()
{
    if (!listp)
        return;

    // It's possible that this shape is unreachable and that listp points to the
    // location of a dead object in the nursery, in which case we should never
    // touch it again.
    if (IsInsideNursery(reinterpret_cast<Cell *>(listp))) {
        listp = nullptr;
        return;
    }

    MOZ_ASSERT(!IsInsideNursery(reinterpret_cast<Cell *>(listp)));
    AllocKind kind = TenuredCell::fromPointer(listp)->getAllocKind();
    MOZ_ASSERT(kind == FINALIZE_SHAPE ||
               kind == FINALIZE_ACCESSOR_SHAPE ||
               kind <= FINALIZE_OBJECT_LAST);
    if (kind == FINALIZE_SHAPE || kind == FINALIZE_ACCESSOR_SHAPE) {
        // listp points to the parent field of the next shape.
        Shape *next = reinterpret_cast<Shape *>(uintptr_t(listp) -
                                                offsetof(Shape, parent));
        listp = &gc::MaybeForwarded(next)->parent;
    } else {
        // listp points to the shape_ field of an object.
        JSObject *last = reinterpret_cast<JSObject *>(uintptr_t(listp) -
                                                      offsetof(JSObject, shape_));
        listp = &gc::MaybeForwarded(last)->shape_;
    }
}

void
Shape::fixupShapeTreeAfterMovingGC()
{
    if (kids.isNull())
        return;

    if (kids.isShape()) {
        if (gc::IsForwarded(kids.toShape()))
            kids.setShape(gc::Forwarded(kids.toShape()));
        return;
    }

    MOZ_ASSERT(kids.isHash());
    KidsHash *kh = kids.toHash();
    for (KidsHash::Enum e(*kh); !e.empty(); e.popFront()) {
        Shape *key = e.front();
        if (IsForwarded(key))
            key = Forwarded(key);

        BaseShape *base = key->base();
        if (IsForwarded(base))
            base = Forwarded(base);
        UnownedBaseShape *unowned = base->unowned();
        if (IsForwarded(unowned))
            unowned = Forwarded(unowned);

        PropertyOp getter = key->getter();
        if (key->hasGetterObject() && IsForwarded(key->getterObject()))
            getter = PropertyOp(Forwarded(key->getterObject()));

        StrictPropertyOp setter = key->setter();
        if (key->hasSetterObject() && IsForwarded(key->setterObject()))
            setter = StrictPropertyOp(Forwarded(key->setterObject()));

        StackShape lookup(unowned,
                          const_cast<Shape *>(key)->propidRef(),
                          key->slotInfo & Shape::SLOT_MASK,
                          key->attrs,
                          key->flags);
        lookup.updateGetterSetter(getter, setter);
        e.rekeyFront(lookup, key);
    }
}

void
Shape::fixupAfterMovingGC()
{
    if (inDictionary())
        fixupDictionaryShapeAfterMovingGC();
    else
        fixupShapeTreeAfterMovingGC();
}

#endif // JSGC_COMPACTING

#ifdef JSGC_GENERATIONAL
void
ShapeGetterSetterRef::mark(JSTracer *trc)
{
    // Update the current shape's entry in the parent KidsHash table if needed.
    // This is necessary as the computed hash includes the getter/setter
    // pointers.

    JSObject *obj = *objp;
    JSObject *prior = obj;
    trc->setTracingLocation(&*prior);
    gc::Mark(trc, &obj, "AccessorShape getter or setter");
    if (obj == *objp)
        return;

    Shape *parent = shape->parent;
    if (shape->inDictionary() || !parent->kids.isHash()) {
        *objp = obj;
        return;
    }

    KidsHash *kh = parent->kids.toHash();
    kh->remove(StackShape(shape));
    *objp = obj;
    MOZ_ALWAYS_TRUE(kh->putNew(StackShape(shape), shape));
}
#endif

#ifdef DEBUG

void
KidsPointer::checkConsistency(Shape *aKid) const
{
    if (isShape()) {
        MOZ_ASSERT(toShape() == aKid);
    } else {
        MOZ_ASSERT(isHash());
        KidsHash *hash = toHash();
        KidsHash::Ptr ptr = hash->lookup(StackShape(aKid));
        MOZ_ASSERT(*ptr == aKid);
    }
}

void
Shape::dump(JSContext *cx, FILE *fp) const
{
    /* This is only used from gdb, so allowing GC here would just be confusing. */
    gc::AutoSuppressGC suppress(cx);

    jsid propid = this->propid();

    MOZ_ASSERT(!JSID_IS_VOID(propid));

    if (JSID_IS_INT(propid)) {
        fprintf(fp, "[%ld]", (long) JSID_TO_INT(propid));
    } else if (JSID_IS_ATOM(propid)) {
        if (JSLinearString *str = JSID_TO_ATOM(propid))
            FileEscapedString(fp, str, '"');
        else
            fputs("<error>", fp);
    } else {
        MOZ_ASSERT(JSID_IS_SYMBOL(propid));
        JSID_TO_SYMBOL(propid)->dump(fp);
    }

    fprintf(fp, " g/s %p/%p slot %d attrs %x ",
            JS_FUNC_TO_DATA_PTR(void *, getter()),
            JS_FUNC_TO_DATA_PTR(void *, setter()),
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
        DUMP_FLAG(IN_DICTIONARY, in_dictionary);
#undef  DUMP_FLAG
        fputs(") ", fp);
    }
}

void
Shape::dumpSubtree(JSContext *cx, int level, FILE *fp) const
{
    if (!parent) {
        MOZ_ASSERT(level == 0);
        MOZ_ASSERT(JSID_IS_EMPTY(propid_));
        fprintf(fp, "class %s emptyShape\n", getObjectClass()->name);
    } else {
        fprintf(fp, "%*sid ", level, "");
        dump(cx, fp);
    }

    if (!kids.isNull()) {
        ++level;
        if (kids.isShape()) {
            Shape *kid = kids.toShape();
            MOZ_ASSERT(kid->parent == this);
            kid->dumpSubtree(cx, level, fp);
        } else {
            const KidsHash &hash = *kids.toHash();
            for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
                Shape *kid = range.front();

                MOZ_ASSERT(kid->parent == this);
                kid->dumpSubtree(cx, level, fp);
            }
        }
    }
}

#endif
