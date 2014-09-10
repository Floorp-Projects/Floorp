/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS symbol tables. */

#include "vm/Shape-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PodOperations.h"

#include "jsatom.h"
#include "jscntxt.h"
#include "jshashutil.h"
#include "jsobj.h"

#include "js/HashTable.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

#include "gc/ForkJoinNursery-inl.h"
#include "vm/ObjectImpl-inl.h"
#include "vm/Runtime-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::CeilingLog2Size;
using mozilla::DebugOnly;
using mozilla::PodZero;
using mozilla::RotateLeft;

bool
ShapeTable::init(ThreadSafeContext *cx, Shape *lastProp)
{
    uint32_t sizeLog2 = CeilingLog2Size(entryCount);
    uint32_t size = JS_BIT(sizeLog2);
    if (entryCount >= size - (size >> 2))
        sizeLog2++;
    if (sizeLog2 < MIN_SIZE_LOG2)
        sizeLog2 = MIN_SIZE_LOG2;

    /*
     * Use rt->calloc for memory accounting and overpressure handling
     * without OOM reporting. See ShapeTable::change.
     */
    entries = cx->pod_calloc<Shape *>(JS_BIT(sizeLog2));
    if (!entries)
        return false;

    hashShift = HASH_BITS - sizeLog2;
    for (Shape::Range<NoGC> r(lastProp); !r.empty(); r.popFront()) {
        Shape &shape = r.front();
        JS_ASSERT(cx->isThreadLocal(&shape));
        Shape **spp = search(shape.propid(), true);

        /*
         * Beware duplicate args and arg vs. var conflicts: the youngest shape
         * (nearest to lastProp) must win. See bug 600067.
         */
        if (!SHAPE_FETCH(spp))
            SHAPE_STORE_PRESERVING_COLLISION(spp, &shape);
    }
    return true;
}

void
Shape::removeFromDictionary(ObjectImpl *obj)
{
    JS_ASSERT(inDictionary());
    JS_ASSERT(obj->inDictionaryMode());
    JS_ASSERT(listp);

    JS_ASSERT(obj->shape_->inDictionary());
    JS_ASSERT(obj->shape_->listp == &obj->shape_);

    if (parent)
        parent->listp = listp;
    *listp = parent;
    listp = nullptr;
}

void
Shape::insertIntoDictionary(HeapPtrShape *dictp)
{
    // Don't assert inDictionaryMode() here because we may be called from
    // JSObject::toDictionaryMode via JSObject::newDictionaryShape.
    JS_ASSERT(inDictionary());
    JS_ASSERT(!listp);

    JS_ASSERT_IF(*dictp, (*dictp)->inDictionary());
    JS_ASSERT_IF(*dictp, (*dictp)->listp == dictp);
    JS_ASSERT_IF(*dictp, compartment() == (*dictp)->compartment());

    setParent(dictp->get());
    if (parent)
        parent->listp = &parent;
    listp = (HeapPtrShape *) dictp;
    *dictp = this;
}

bool
Shape::makeOwnBaseShape(ThreadSafeContext *cx)
{
    JS_ASSERT(!base()->isOwned());
    JS_ASSERT(cx->isThreadLocal(this));
    assertSameCompartmentDebugOnly(cx, compartment());

    BaseShape *nbase = js_NewGCBaseShape<NoGC>(cx);
    if (!nbase)
        return false;

    new (nbase) BaseShape(StackBaseShape(this));
    nbase->setOwned(base()->toUnowned());

    this->base_ = nbase;

    return true;
}

void
Shape::handoffTableTo(Shape *shape)
{
    JS_ASSERT(inDictionary() && shape->inDictionary());

    if (this == shape)
        return;

    JS_ASSERT(base()->isOwned() && !shape->base()->isOwned());

    BaseShape *nbase = base();

    JS_ASSERT_IF(shape->hasSlot(), nbase->slotSpan() > shape->slot());

    this->base_ = nbase->baseUnowned();
    nbase->adoptUnowned(shape->base()->toUnowned());

    shape->base_ = nbase;
}

/* static */ bool
Shape::hashify(ThreadSafeContext *cx, Shape *shape)
{
    JS_ASSERT(!shape->hasTable());

    if (!shape->ensureOwnBaseShape(cx))
        return false;

    ShapeTable *table = cx->new_<ShapeTable>(shape->entryCount());
    if (!table)
        return false;

    if (!table->init(cx, shape)) {
        js_free(table);
        return false;
    }

    shape->base()->setTable(table);
    return true;
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
#define HASH1(hash0,shift)      ((hash0) >> (shift))
#define HASH2(hash0,log2,shift) ((((hash0) << (log2)) >> (shift)) | 1)

Shape **
ShapeTable::search(jsid id, bool adding)
{
    js::HashNumber hash0, hash1, hash2;
    int sizeLog2;
    Shape *stored, *shape, **spp, **firstRemoved;
    uint32_t sizeMask;

    JS_ASSERT(entries);
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /* Compute the primary hash address. */
    hash0 = HashId(id);
    hash1 = HASH1(hash0, hashShift);
    spp = entries + hash1;

    /* Miss: return space for a new entry. */
    stored = *spp;
    if (SHAPE_IS_FREE(stored))
        return spp;

    /* Hit: return entry. */
    shape = SHAPE_CLEAR_COLLISION(stored);
    if (shape && shape->propidRaw() == id)
        return spp;

    /* Collision: double hash. */
    sizeLog2 = HASH_BITS - hashShift;
    hash2 = HASH2(hash0, sizeLog2, hashShift);
    sizeMask = JS_BITMASK(sizeLog2);

#ifdef DEBUG
    uintptr_t collision_flag = SHAPE_COLLISION;
#endif

    /* Save the first removed entry pointer so we can recycle it if adding. */
    if (SHAPE_IS_REMOVED(stored)) {
        firstRemoved = spp;
    } else {
        firstRemoved = nullptr;
        if (adding && !SHAPE_HAD_COLLISION(stored))
            SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
        collision_flag &= uintptr_t(*spp) & SHAPE_COLLISION;
#endif
    }

    for (;;) {
        hash1 -= hash2;
        hash1 &= sizeMask;
        spp = entries + hash1;

        stored = *spp;
        if (SHAPE_IS_FREE(stored))
            return (adding && firstRemoved) ? firstRemoved : spp;

        shape = SHAPE_CLEAR_COLLISION(stored);
        if (shape && shape->propidRaw() == id) {
            JS_ASSERT(collision_flag);
            return spp;
        }

        if (SHAPE_IS_REMOVED(stored)) {
            if (!firstRemoved)
                firstRemoved = spp;
        } else {
            if (adding && !SHAPE_HAD_COLLISION(stored))
                SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
            collision_flag &= uintptr_t(*spp) & SHAPE_COLLISION;
#endif
        }
    }

    /* NOTREACHED */
    return nullptr;
}

#ifdef JSGC_COMPACTING
void
ShapeTable::fixupAfterMovingGC()
{
    int log2 = HASH_BITS - hashShift;
    uint32_t size = JS_BIT(log2);
    for (HashNumber i = 0; i < size; i++) {
        Shape *shape = SHAPE_FETCH(&entries[i]);
        if (shape && IsForwarded(shape))
            SHAPE_STORE_PRESERVING_COLLISION(&entries[i], Forwarded(shape));
    }
}
#endif

bool
ShapeTable::change(int log2Delta, ThreadSafeContext *cx)
{
    JS_ASSERT(entries);

    /*
     * Grow, shrink, or compress by changing this->entries.
     */
    int oldlog2 = HASH_BITS - hashShift;
    int newlog2 = oldlog2 + log2Delta;
    uint32_t oldsize = JS_BIT(oldlog2);
    uint32_t newsize = JS_BIT(newlog2);
    Shape **newTable = cx->pod_calloc<Shape *>(newsize);
    if (!newTable)
        return false;

    /* Now that we have newTable allocated, update members. */
    hashShift = HASH_BITS - newlog2;
    removedCount = 0;
    Shape **oldTable = entries;
    entries = newTable;

    /* Copy only live entries, leaving removed and free ones behind. */
    for (Shape **oldspp = oldTable; oldsize != 0; oldspp++) {
        Shape *shape = SHAPE_FETCH(oldspp);
        JS_ASSERT(cx->isThreadLocal(shape));
        if (shape) {
            Shape **spp = search(shape->propid(), true);
            JS_ASSERT(SHAPE_IS_FREE(*spp));
            *spp = shape;
        }
        oldsize--;
    }

    /* Finally, free the old entries storage. */
    js_free(oldTable);
    return true;
}

bool
ShapeTable::grow(ThreadSafeContext *cx)
{
    JS_ASSERT(needsToGrow());

    uint32_t size = capacity();
    int delta = removedCount < size >> 2;

    if (!change(delta, cx) && entryCount + removedCount == size - 1) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

/* static */ Shape *
Shape::replaceLastProperty(ExclusiveContext *cx, StackBaseShape &base,
                           TaggedProto proto, HandleShape shape)
{
    JS_ASSERT(!shape->inDictionary());

    if (!shape->parent) {
        /* Treat as resetting the initial property of the shape hierarchy. */
        AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
        return EmptyShape::getInitialShape(cx, base.clasp, proto,
                                           base.parent, base.metadata, kind,
                                           base.flags & BaseShape::OBJECT_FLAG_MASK);
    }

    UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return nullptr;

    StackShape child(shape);
    child.base = nbase;

    return cx->compartment()->propertyTree.getChild(cx, shape->parent, child);
}

/*
 * Get or create a property-tree or dictionary child property of |parent|,
 * which must be lastProperty() if inDictionaryMode(), else parent must be
 * one of lastProperty() or lastProperty()->parent.
 */
/* static */ Shape *
JSObject::getChildPropertyOnDictionary(ThreadSafeContext *cx, JS::HandleObject obj,
                                       HandleShape parent, js::StackShape &child)
{
    /*
     * Shared properties have no slot, but slot_ will reflect that of parent.
     * Unshared properties allocate a slot here but may lose it due to a
     * JS_ClearScope call.
     */
    if (!child.hasSlot()) {
        child.setSlot(parent->maybeSlot());
    } else {
        if (child.hasMissingSlot()) {
            uint32_t slot;
            if (!allocSlot(cx, obj, &slot))
                return nullptr;
            child.setSlot(slot);
        } else {
            /*
             * Slots can only be allocated out of order on objects in
             * dictionary mode.  Otherwise the child's slot must be after the
             * parent's slot (if it has one), because slot number determines
             * slot span for objects with that shape.  Usually child slot
             * *immediately* follows parent slot, but there may be a slot gap
             * when the object uses some -- but not all -- of its reserved
             * slots to store properties.
             */
            JS_ASSERT(obj->inDictionaryMode() ||
                      parent->hasMissingSlot() ||
                      child.slot() == parent->maybeSlot() + 1 ||
                      (parent->maybeSlot() + 1 < JSSLOT_FREE(obj->getClass()) &&
                       child.slot() == JSSLOT_FREE(obj->getClass())));
        }
    }

    RootedShape shape(cx);

    if (obj->inDictionaryMode()) {
        JS_ASSERT(parent == obj->lastProperty());
        RootedGeneric<StackShape*> childRoot(cx, &child);
        shape = js_NewGCShape(cx);
        if (!shape)
            return nullptr;
        if (childRoot->hasSlot() && childRoot->slot() >= obj->lastProperty()->base()->slotSpan()) {
            if (!JSObject::setSlotSpan(cx, obj, childRoot->slot() + 1))
                return nullptr;
        }
        shape->initDictionaryShape(*childRoot, obj->numFixedSlots(), &obj->shape_);
    }

    return shape;
}

/* static */ Shape *
JSObject::getChildProperty(ExclusiveContext *cx,
                           HandleObject obj, HandleShape parent, StackShape &unrootedChild)
{
    RootedGeneric<StackShape*> child(cx, &unrootedChild);
    RootedShape shape(cx, getChildPropertyOnDictionary(cx, obj, parent, *child));

    if (!obj->inDictionaryMode()) {
        shape = cx->compartment()->propertyTree.getChild(cx, parent, *child);
        if (!shape)
            return nullptr;
        //JS_ASSERT(shape->parent == parent);
        //JS_ASSERT_IF(parent != lastProperty(), parent == lastProperty()->parent);
        if (!JSObject::setLastProperty(cx, obj, shape))
            return nullptr;
    }

    return shape;
}

/* static */ Shape *
JSObject::lookupChildProperty(ThreadSafeContext *cx,
                              HandleObject obj, HandleShape parent, StackShape &unrootedChild)
{
    RootedGeneric<StackShape*> child(cx, &unrootedChild);
    JS_ASSERT(cx->isThreadLocal(obj));

    RootedShape shape(cx, getChildPropertyOnDictionary(cx, obj, parent, *child));

    if (!obj->inDictionaryMode()) {
        shape = cx->compartment_->propertyTree.lookupChild(cx, parent, *child);
        if (!shape)
            return nullptr;
        if (!JSObject::setLastProperty(cx, obj, shape))
            return nullptr;
    }

    return shape;
}

bool
js::ObjectImpl::toDictionaryMode(ThreadSafeContext *cx)
{
    JS_ASSERT(!inDictionaryMode());

#ifdef JSGC_COMPACTING
    // TODO: This crashes if we run a compacting GC here.
    js::AutoDisableCompactingGC nogc(zone()->runtimeFromAnyThread());
#endif

    /* We allocate the shapes from cx->compartment(), so make sure it's right. */
    JS_ASSERT(cx->isInsideCurrentCompartment(this));

    /*
     * This function is thread safe as long as the object is thread local. It
     * does not modify the shared shapes, and only allocates newly allocated
     * (and thus also thread local) shapes.
     */
    JS_ASSERT(cx->isThreadLocal(this));

    uint32_t span = slotSpan();

    Rooted<ObjectImpl*> self(cx, this);

    /*
     * Clone the shapes into a new dictionary list. Don't update the
     * last property of this object until done, otherwise a GC
     * triggered while creating the dictionary will get the wrong
     * slot span for this object.
     */
    RootedShape root(cx);
    RootedShape dictionaryShape(cx);

    RootedShape shape(cx, lastProperty());
    while (shape) {
        JS_ASSERT(!shape->inDictionary());

        Shape *dprop = js_NewGCShape(cx);
        if (!dprop) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        HeapPtrShape *listp = dictionaryShape
                              ? &dictionaryShape->parent
                              : (HeapPtrShape *) root.address();

        StackShape child(shape);
        dprop->initDictionaryShape(child, self->numFixedSlots(), listp);

        JS_ASSERT(!dprop->hasTable());
        dictionaryShape = dprop;
        shape = shape->previous();
    }

    if (!Shape::hashify(cx, root)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    JS_ASSERT((Shape **) root->listp == root.address());
    root->listp = &self->shape_;
    self->shape_ = root;

    JS_ASSERT(self->inDictionaryMode());
    root->base()->setSlotSpan(span);

    return true;
}

/*
 * Normalize stub getter and setter values for faster is-stub testing in the
 * SHAPE_CALL_[GS]ETTER macros.
 */
static inline bool
NormalizeGetterAndSetter(JSObject *obj,
                         jsid id, unsigned attrs, unsigned flags,
                         PropertyOp &getter,
                         StrictPropertyOp &setter)
{
    if (setter == JS_StrictPropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_SETTER));
        setter = nullptr;
    }
    if (getter == JS_PropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_GETTER));
        getter = nullptr;
    }

    return true;
}

/* static */ Shape *
JSObject::addProperty(ExclusiveContext *cx, HandleObject obj, HandleId id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32_t slot, unsigned attrs,
                      unsigned flags, bool allowDictionary)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return nullptr;
    if (!extensible) {
        if (cx->isJSContext())
            obj->reportNotExtensible(cx->asJSContext());
        return nullptr;
    }

    NormalizeGetterAndSetter(obj, id, attrs, flags, getter, setter);

    Shape **spp = nullptr;
    if (obj->inDictionaryMode())
        spp = obj->lastProperty()->table().search(id, true);

    return addPropertyInternal<SequentialExecution>(cx, obj, id, getter, setter, slot, attrs,
                                                    flags, spp, allowDictionary);
}

static bool
ShouldConvertToDictionary(JSObject *obj)
{
    /*
     * Use a lower limit if this object is likely a hashmap (SETELEM was used
     * to set properties).
     */
    if (obj->hadElementsAccess())
        return obj->lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT_WITH_ELEMENTS_ACCESS;
    return obj->lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT;
}

template <ExecutionMode mode>
static inline UnownedBaseShape *
GetOrLookupUnownedBaseShape(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                            StackBaseShape &base)
{
    if (mode == ParallelExecution)
        return BaseShape::lookupUnowned(cx, base);
    return BaseShape::getUnowned(cx->asExclusiveContext(), base);
}

template <ExecutionMode mode>
/* static */ Shape *
JSObject::addPropertyInternal(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                              HandleObject obj, HandleId id,
                              PropertyOp getter, StrictPropertyOp setter,
                              uint32_t slot, unsigned attrs,
                              unsigned flags, Shape **spp,
                              bool allowDictionary)
{
    JS_ASSERT(cx->isThreadLocal(obj));
    JS_ASSERT_IF(!allowDictionary, !obj->inDictionaryMode());

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    /*
     * The code below deals with either converting obj to dictionary mode or
     * growing an object that's already in dictionary mode. Either way,
     * dictionray operations are safe if thread local.
     */
    ShapeTable *table = nullptr;
    if (!obj->inDictionaryMode()) {
        bool stableSlot =
            (slot == SHAPE_INVALID_SLOT) ||
            obj->lastProperty()->hasMissingSlot() ||
            (slot == obj->lastProperty()->maybeSlot() + 1);
        JS_ASSERT_IF(!allowDictionary, stableSlot);
        if (allowDictionary &&
            (!stableSlot || ShouldConvertToDictionary(obj)))
        {
            if (!obj->toDictionaryMode(cx))
                return nullptr;
            table = &obj->lastProperty()->table();
            spp = table->search(id, true);
        }
    } else {
        table = &obj->lastProperty()->table();
        if (table->needsToGrow()) {
            if (!table->grow(cx))
                return nullptr;
            spp = table->search(id, true);
            JS_ASSERT(!SHAPE_FETCH(spp));
        }
    }

    JS_ASSERT(!!table == !!spp);

    /* Find or create a property tree node labeled by our arguments. */
    RootedShape shape(cx);
    {
        RootedShape last(cx, obj->lastProperty());

        uint32_t index;
        bool indexed = js_IdIsIndex(id, &index);

        Rooted<UnownedBaseShape*> nbase(cx);
        if (last->base()->matchesGetterSetter(getter, setter) && !indexed) {
            nbase = last->base()->unowned();
        } else {
            StackBaseShape base(last->base());
            base.updateGetterSetter(attrs, getter, setter);
            if (indexed)
                base.flags |= BaseShape::INDEXED;
            nbase = GetOrLookupUnownedBaseShape<mode>(cx, base);
            if (!nbase)
                return nullptr;
        }

        StackShape child(nbase, id, slot, attrs, flags);
        shape = getOrLookupChildProperty<mode>(cx, obj, last, child);
    }

    if (shape) {
        JS_ASSERT(shape == obj->lastProperty());

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SHAPE_STORE_PRESERVING_COLLISION(spp, static_cast<Shape *>(shape));
            ++table->entryCount;

            /* Pass the table along to the new last property, namely shape. */
            JS_ASSERT(&shape->parent->table() == table);
            shape->parent->handoffTableTo(shape);
        }

        obj->checkShapeConsistency();
        return shape;
    }

    obj->checkShapeConsistency();
    return nullptr;
}

template /* static */ Shape *
JSObject::addPropertyInternal<SequentialExecution>(ExclusiveContext *cx,
                                                   HandleObject obj, HandleId id,
                                                   PropertyOp getter, StrictPropertyOp setter,
                                                   uint32_t slot, unsigned attrs,
                                                   unsigned flags, Shape **spp,
                                                   bool allowDictionary);
template /* static */ Shape *
JSObject::addPropertyInternal<ParallelExecution>(ForkJoinContext *cx,
                                                 HandleObject obj, HandleId id,
                                                 PropertyOp getter, StrictPropertyOp setter,
                                                 uint32_t slot, unsigned attrs,
                                                 unsigned flags, Shape **spp,
                                                 bool allowDictionary);

JSObject *
js::NewReshapedObject(JSContext *cx, HandleTypeObject type, JSObject *parent,
                      gc::AllocKind allocKind, HandleShape shape, NewObjectKind newKind)
{
    RootedObject res(cx, NewObjectWithType(cx, type, parent, allocKind, newKind));
    if (!res)
        return nullptr;

    if (shape->isEmptyShape())
        return res;

    /* Get all the ids in the object, in order. */
    js::AutoIdVector ids(cx);
    {
        for (unsigned i = 0; i <= shape->slot(); i++) {
            if (!ids.append(JSID_VOID))
                return nullptr;
        }
        Shape *nshape = shape;
        while (!nshape->isEmptyShape()) {
            ids[nshape->slot()].set(nshape->propid());
            nshape = nshape->previous();
        }
    }

    /* Construct the new shape, without updating type information. */
    RootedId id(cx);
    RootedShape newShape(cx, res->lastProperty());
    for (unsigned i = 0; i < ids.length(); i++) {
        id = ids[i];
        JS_ASSERT(!res->nativeContains(cx, id));

        uint32_t index;
        bool indexed = js_IdIsIndex(id, &index);

        Rooted<UnownedBaseShape*> nbase(cx, newShape->base()->unowned());
        if (indexed) {
            StackBaseShape base(nbase);
            base.flags |= BaseShape::INDEXED;
            nbase = GetOrLookupUnownedBaseShape<SequentialExecution>(cx, base);
            if (!nbase)
                return nullptr;
        }

        StackShape child(nbase, id, i, JSPROP_ENUMERATE, 0);
        newShape = cx->compartment()->propertyTree.getChild(cx, newShape, child);
        if (!newShape)
            return nullptr;
        if (!JSObject::setLastProperty(cx, res, newShape))
            return nullptr;
    }

    return res;
}

/*
 * Check and adjust the new attributes for the shape to make sure that our
 * slot access optimizations are sound. It is responsibility of the callers to
 * enforce all restrictions from ECMA-262 v5 8.12.9 [[DefineOwnProperty]].
 */
static inline bool
CheckCanChangeAttrs(ThreadSafeContext *cx, JSObject *obj, Shape *shape, unsigned *attrsp)
{
    if (shape->configurable())
        return true;

    /* A permanent property must stay permanent. */
    *attrsp |= JSPROP_PERMANENT;

    /* Reject attempts to remove a slot from the permanent data property. */
    if (shape->isDataDescriptor() && shape->hasSlot() &&
        (*attrsp & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED)))
    {
        if (cx->isJSContext())
            obj->reportNotConfigurable(cx->asJSContext(), shape->propid());
        return false;
    }

    return true;
}

template <ExecutionMode mode>
/* static */ Shape *
JSObject::putProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                      HandleObject obj, HandleId id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32_t slot, unsigned attrs, unsigned flags)
{
    JS_ASSERT(cx->isThreadLocal(obj));
    JS_ASSERT(!JSID_IS_VOID(id));

#ifdef DEBUG
    if (obj->is<ArrayObject>()) {
        ArrayObject *arr = &obj->as<ArrayObject>();
        uint32_t index;
        if (js_IdIsIndex(id, &index))
            JS_ASSERT(index < arr->length() || arr->lengthIsWritable());
    }
#endif

    NormalizeGetterAndSetter(obj, id, attrs, flags, getter, setter);

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    /*
     * Search for id in order to claim its entry if table has been allocated.
     *
     * Note that we can only try to claim an entry in a table that is thread
     * local. An object may be thread local *without* its shape being thread
     * local. The only thread local objects that *also* have thread local
     * shapes are dictionaries that were allocated/converted thread
     * locally. Only for those objects we can try to claim an entry in its
     * shape table.
     */
    Shape **spp;
    RootedShape shape(cx, (mode == ParallelExecution
                           ? Shape::searchThreadLocal(cx, obj->lastProperty(), id, &spp,
                                                      cx->isThreadLocal(obj->lastProperty()))
                           : Shape::search(cx->asExclusiveContext(), obj->lastProperty(), id,
                                           &spp, true)));
    if (!shape) {
        /*
         * You can't add properties to a non-extensible object, but you can change
         * attributes of properties in such objects.
         */
        bool extensible;

        if (mode == ParallelExecution) {
            if (obj->is<ProxyObject>())
                return nullptr;
            extensible = obj->nonProxyIsExtensible();
        } else {
            if (!JSObject::isExtensible(cx->asExclusiveContext(), obj, &extensible))
                return nullptr;
        }

        if (!extensible) {
            if (cx->isJSContext())
                obj->reportNotExtensible(cx->asJSContext());
            return nullptr;
        }

        return addPropertyInternal<mode>(cx, obj, id, getter, setter, slot, attrs, flags,
                                         spp, true);
    }

    /* Property exists: search must have returned a valid *spp. */
    JS_ASSERT_IF(spp, !SHAPE_IS_REMOVED(*spp));

    if (!CheckCanChangeAttrs(cx, obj, shape, &attrs))
        return nullptr;

    /*
     * If the caller wants to allocate a slot, but doesn't care which slot,
     * copy the existing shape's slot into slot so we can match shape, if all
     * other members match.
     */
    bool hadSlot = shape->hasSlot();
    uint32_t oldSlot = shape->maybeSlot();
    if (!(attrs & JSPROP_SHARED) && slot == SHAPE_INVALID_SLOT && hadSlot)
        slot = oldSlot;

    Rooted<UnownedBaseShape*> nbase(cx);
    {
        uint32_t index;
        bool indexed = js_IdIsIndex(id, &index);
        StackBaseShape base(obj->lastProperty()->base());
        base.updateGetterSetter(attrs, getter, setter);
        if (indexed)
            base.flags |= BaseShape::INDEXED;
        nbase = GetOrLookupUnownedBaseShape<mode>(cx, base);
        if (!nbase)
            return nullptr;
    }

    /*
     * Now that we've possibly preserved slot, check whether all members match.
     * If so, this is a redundant "put" and we can return without more work.
     */
    if (shape->matchesParamsAfterId(nbase, slot, attrs, flags))
        return shape;

    /*
     * Overwriting a non-last property requires switching to dictionary mode.
     * The shape tree is shared immutable, and we can't removeProperty and then
     * addPropertyInternal because a failure under add would lose data.
     */
    if (shape != obj->lastProperty() && !obj->inDictionaryMode()) {
        if (!obj->toDictionaryMode(cx))
            return nullptr;
        spp = obj->lastProperty()->table().search(shape->propid(), false);
        shape = SHAPE_FETCH(spp);
    }

    JS_ASSERT_IF(shape->hasSlot() && !(attrs & JSPROP_SHARED), shape->slot() == slot);

    if (obj->inDictionaryMode()) {
        /*
         * Updating some property in a dictionary-mode object. Create a new
         * shape for the existing property, and also generate a new shape for
         * the last property of the dictionary (unless the modified property
         * is also the last property).
         */
        bool updateLast = (shape == obj->lastProperty());
        shape = obj->replaceWithNewEquivalentShape(cx, shape);
        if (!shape)
            return nullptr;
        if (!updateLast && !obj->generateOwnShape(cx))
            return nullptr;

        /* FIXME bug 593129 -- slot allocation and JSObject *this must move out of here! */
        if (slot == SHAPE_INVALID_SLOT && !(attrs & JSPROP_SHARED)) {
            if (!allocSlot(cx, obj, &slot))
                return nullptr;
        }

        if (updateLast)
            shape->base()->adoptUnowned(nbase);
        else
            shape->base_ = nbase;

        JS_ASSERT_IF(attrs & (JSPROP_GETTER | JSPROP_SETTER), attrs & JSPROP_SHARED);

        shape->setSlot(slot);
        shape->attrs = uint8_t(attrs);
        shape->flags = flags | Shape::IN_DICTIONARY;
    } else {
        /*
         * Updating the last property in a non-dictionary-mode object. Find an
         * alternate shared child of the last property's previous shape.
         */
        StackBaseShape base(obj->lastProperty()->base());
        base.updateGetterSetter(attrs, getter, setter);

        UnownedBaseShape *nbase = GetOrLookupUnownedBaseShape<mode>(cx, base);
        if (!nbase)
            return nullptr;

        JS_ASSERT(shape == obj->lastProperty());

        /* Find or create a property tree node labeled by our arguments. */
        StackShape child(nbase, id, slot, attrs, flags);
        RootedShape parent(cx, shape->parent);
        Shape *newShape = getOrLookupChildProperty<mode>(cx, obj, parent, child);

        if (!newShape) {
            obj->checkShapeConsistency();
            return nullptr;
        }

        shape = newShape;
    }

    /*
     * Can't fail now, so free the previous incarnation's slot if the new shape
     * has no slot. But we do not need to free oldSlot (and must not, as trying
     * to will botch an assertion in JSObject::freeSlot) if the new last
     * property (shape here) has a slotSpan that does not cover it.
     */
    if (hadSlot && !shape->hasSlot()) {
        if (oldSlot < obj->slotSpan())
            obj->freeSlot(oldSlot);
        /* Note: The optimization based on propertyRemovals is only relevant to the main thread. */
        if (cx->isJSContext())
            ++cx->asJSContext()->runtime()->propertyRemovals;
    }

    obj->checkShapeConsistency();

    return shape;
}

template /* static */ Shape *
JSObject::putProperty<SequentialExecution>(ExclusiveContext *cx,
                                           HandleObject obj, HandleId id,
                                           PropertyOp getter, StrictPropertyOp setter,
                                           uint32_t slot, unsigned attrs,
                                           unsigned flags);
template /* static */ Shape *
JSObject::putProperty<ParallelExecution>(ForkJoinContext *cx,
                                         HandleObject obj, HandleId id,
                                         PropertyOp getter, StrictPropertyOp setter,
                                         uint32_t slot, unsigned attrs,
                                         unsigned flags);

template <ExecutionMode mode>
/* static */ Shape *
JSObject::changeProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                         HandleObject obj, HandleShape shape, unsigned attrs,
                         unsigned mask, PropertyOp getter, StrictPropertyOp setter)
{
    JS_ASSERT(cx->isThreadLocal(obj));
    JS_ASSERT(obj->nativeContainsPure(shape));

    attrs |= shape->attrs & mask;
    JS_ASSERT_IF(attrs & (JSPROP_GETTER | JSPROP_SETTER), attrs & JSPROP_SHARED);

    /* Allow only shared (slotless) => unshared (slotful) transition. */
    JS_ASSERT(!((attrs ^ shape->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    if (mode == ParallelExecution) {
        if (!types::IsTypePropertyIdMarkedNonData(obj, shape->propid()))
            return nullptr;
    } else {
        types::MarkTypePropertyNonData(cx->asExclusiveContext(), obj, shape->propid());
    }

    if (getter == JS_PropertyStub)
        getter = nullptr;
    if (setter == JS_StrictPropertyStub)
        setter = nullptr;

    if (!CheckCanChangeAttrs(cx, obj, shape, &attrs))
        return nullptr;

    if (shape->attrs == attrs && shape->getter() == getter && shape->setter() == setter)
        return shape;

    /*
     * Let JSObject::putProperty handle this |overwriting| case, including
     * the conservation of shape->slot (if it's valid). We must not call
     * removeProperty because it will free an allocated shape->slot, and
     * putProperty won't re-allocate it.
     */
    RootedId propid(cx, shape->propid());
    Shape *newShape = putProperty<mode>(cx, obj, propid, getter, setter,
                                        shape->maybeSlot(), attrs, shape->flags);

    obj->checkShapeConsistency();
    return newShape;
}

template /* static */ Shape *
JSObject::changeProperty<SequentialExecution>(ExclusiveContext *cx,
                                              HandleObject obj, HandleShape shape,
                                              unsigned attrs, unsigned mask,
                                              PropertyOp getter, StrictPropertyOp setter);
template /* static */ Shape *
JSObject::changeProperty<ParallelExecution>(ForkJoinContext *cx,
                                            HandleObject obj, HandleShape shape,
                                            unsigned attrs, unsigned mask,
                                            PropertyOp getter, StrictPropertyOp setter);

bool
JSObject::removeProperty(ExclusiveContext *cx, jsid id_)
{
    RootedId id(cx, id_);
    RootedObject self(cx, this);

    Shape **spp;
    RootedShape shape(cx, Shape::search(cx, lastProperty(), id, &spp));
    if (!shape)
        return true;

    /*
     * If shape is not the last property added, or the last property cannot
     * be removed, switch to dictionary mode.
     */
    if (!self->inDictionaryMode() && (shape != self->lastProperty() || !self->canRemoveLastProperty())) {
        if (!self->toDictionaryMode(cx))
            return false;
        spp = self->lastProperty()->table().search(shape->propid(), false);
        shape = SHAPE_FETCH(spp);
    }

    /*
     * If in dictionary mode, get a new shape for the last property after the
     * removal. We need a fresh shape for all dictionary deletions, even of
     * the last property. Otherwise, a shape could replay and caches might
     * return deleted DictionaryShapes! See bug 595365. Do this before changing
     * the object or table, so the remaining removal is infallible.
     */
    RootedShape spare(cx);
    if (self->inDictionaryMode()) {
        spare = js_NewGCShape(cx);
        if (!spare)
            return false;
        new (spare) Shape(shape->base()->unowned(), 0);
        if (shape == self->lastProperty()) {
            /*
             * Get an up to date unowned base shape for the new last property
             * when removing the dictionary's last property. Information in
             * base shapes for non-last properties may be out of sync with the
             * object's state.
             */
            RootedShape previous(cx, self->lastProperty()->parent);
            StackBaseShape base(self->lastProperty()->base());
            base.updateGetterSetter(previous->attrs, previous->getter(), previous->setter());
            BaseShape *nbase = BaseShape::getUnowned(cx, base);
            if (!nbase)
                return false;
            previous->base_ = nbase;
        }
    }

    /* If shape has a slot, free its slot number. */
    if (shape->hasSlot()) {
        self->freeSlot(shape->slot());
        if (cx->isJSContext())
            ++cx->asJSContext()->runtime()->propertyRemovals;
    }

    /*
     * A dictionary-mode object owns mutable, unique shapes on a non-circular
     * doubly linked list, hashed by lastProperty()->table. So we can edit the
     * list and hash in place.
     */
    if (self->inDictionaryMode()) {
        ShapeTable &table = self->lastProperty()->table();

        if (SHAPE_HAD_COLLISION(*spp)) {
            *spp = SHAPE_REMOVED;
            ++table.removedCount;
            --table.entryCount;
        } else {
            *spp = nullptr;
            --table.entryCount;

#ifdef DEBUG
            /*
             * Check the consistency of the table but limit the number of
             * checks not to alter significantly the complexity of the
             * delete in debug builds, see bug 534493.
             */
            Shape *aprop = self->lastProperty();
            for (int n = 50; --n >= 0 && aprop->parent; aprop = aprop->parent)
                JS_ASSERT_IF(aprop != shape, self->nativeContains(cx, aprop));
#endif
        }

        {
            /* Remove shape from its non-circular doubly linked list. */
            Shape *oldLastProp = self->lastProperty();
            shape->removeFromDictionary(self);

            /* Hand off table from the old to new last property. */
            oldLastProp->handoffTableTo(self->lastProperty());
        }

        /* Generate a new shape for the object, infallibly. */
        JS_ALWAYS_TRUE(self->generateOwnShape(cx, spare));

        /* Consider shrinking table if its load factor is <= .25. */
        uint32_t size = table.capacity();
        if (size > ShapeTable::MIN_SIZE && table.entryCount <= size >> 2)
            (void) table.change(-1, cx);
    } else {
        /*
         * Non-dictionary-mode shape tables are shared immutables, so all we
         * need do is retract the last property and we'll either get or else
         * lazily make via a later hashify the exact table for the new property
         * lineage.
         */
        JS_ASSERT(shape == self->lastProperty());
        self->removeLastProperty(cx);
    }

    self->checkShapeConsistency();
    return true;
}

/* static */ void
JSObject::clear(JSContext *cx, HandleObject obj)
{
    RootedShape shape(cx, obj->lastProperty());
    JS_ASSERT(obj->inDictionaryMode() == shape->inDictionary());

    while (shape->parent) {
        shape = shape->parent;
        JS_ASSERT(obj->inDictionaryMode() == shape->inDictionary());
    }
    JS_ASSERT(shape->isEmptyShape());

    if (obj->inDictionaryMode())
        shape->listp = &obj->shape_;

    JS_ALWAYS_TRUE(JSObject::setLastProperty(cx, obj, shape));

    ++cx->runtime()->propertyRemovals;
    obj->checkShapeConsistency();
}

/* static */ bool
JSObject::rollbackProperties(ExclusiveContext *cx, HandleObject obj, uint32_t slotSpan)
{
    /*
     * Remove properties from this object until it has a matching slot span.
     * The object cannot have escaped in a way which would prevent safe
     * removal of the last properties.
     */
    JS_ASSERT(!obj->inDictionaryMode() && slotSpan <= obj->slotSpan());
    while (true) {
        if (obj->lastProperty()->isEmptyShape()) {
            JS_ASSERT(slotSpan == 0);
            break;
        } else {
            uint32_t slot = obj->lastProperty()->slot();
            if (slot < slotSpan)
                break;
            JS_ASSERT(obj->getSlot(slot).isUndefined());
        }
        if (!obj->removeProperty(cx, obj->lastProperty()->propid()))
            return false;
    }

    return true;
}

Shape *
ObjectImpl::replaceWithNewEquivalentShape(ThreadSafeContext *cx, Shape *oldShape, Shape *newShape)
{
    JS_ASSERT(cx->isThreadLocal(this));
    JS_ASSERT(cx->isThreadLocal(oldShape));
    JS_ASSERT(cx->isInsideCurrentCompartment(oldShape));
    JS_ASSERT_IF(oldShape != lastProperty(),
                 inDictionaryMode() &&
                 ((cx->isExclusiveContext()
                   ? nativeLookup(cx->asExclusiveContext(), oldShape->propidRef())
                   : nativeLookupPure(oldShape->propidRef())) == oldShape));

    ObjectImpl *self = this;

    if (!inDictionaryMode()) {
        Rooted<ObjectImpl*> selfRoot(cx, self);
        RootedShape newRoot(cx, newShape);
        if (!toDictionaryMode(cx))
            return nullptr;
        oldShape = selfRoot->lastProperty();
        self = selfRoot;
        newShape = newRoot;
    }

    if (!newShape) {
        Rooted<ObjectImpl*> selfRoot(cx, self);
        RootedShape oldRoot(cx, oldShape);
        newShape = js_NewGCShape(cx);
        if (!newShape)
            return nullptr;
        new (newShape) Shape(oldRoot->base()->unowned(), 0);
        self = selfRoot;
        oldShape = oldRoot;
    }

    ShapeTable &table = self->lastProperty()->table();
    Shape **spp = oldShape->isEmptyShape()
                  ? nullptr
                  : table.search(oldShape->propidRef(), false);

    /*
     * Splice the new shape into the same position as the old shape, preserving
     * enumeration order (see bug 601399).
     */
    StackShape nshape(oldShape);
    newShape->initDictionaryShape(nshape, self->numFixedSlots(), oldShape->listp);

    JS_ASSERT(newShape->parent == oldShape);
    oldShape->removeFromDictionary(self);

    if (newShape == self->lastProperty())
        oldShape->handoffTableTo(newShape);

    if (spp)
        SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);
    return newShape;
}

bool
JSObject::shadowingShapeChange(ExclusiveContext *cx, const Shape &shape)
{
    return generateOwnShape(cx);
}

/* static */ bool
JSObject::clearParent(JSContext *cx, HandleObject obj)
{
    return setParent(cx, obj, NullPtr());
}

/* static */ bool
JSObject::setParent(JSContext *cx, HandleObject obj, HandleObject parent)
{
    if (parent && !parent->setDelegate(cx))
        return false;

    if (obj->inDictionaryMode()) {
        StackBaseShape base(obj->lastProperty());
        base.parent = parent;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        obj->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape = Shape::setObjectParent(cx, parent, obj->getTaggedProto(), obj->shape_);
    if (!newShape)
        return false;

    obj->shape_ = newShape;
    return true;
}

/* static */ Shape *
Shape::setObjectParent(ExclusiveContext *cx, JSObject *parent, TaggedProto proto, Shape *last)
{
    if (last->getObjectParent() == parent)
        return last;

    StackBaseShape base(last);
    base.parent = parent;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ bool
JSObject::setMetadata(JSContext *cx, HandleObject obj, HandleObject metadata)
{
    if (obj->inDictionaryMode()) {
        StackBaseShape base(obj->lastProperty());
        base.metadata = metadata;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        obj->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape = Shape::setObjectMetadata(cx, metadata, obj->getTaggedProto(), obj->shape_);
    if (!newShape)
        return false;

    obj->shape_ = newShape;
    return true;
}

/* static */ Shape *
Shape::setObjectMetadata(JSContext *cx, JSObject *metadata, TaggedProto proto, Shape *last)
{
    if (last->getObjectMetadata() == metadata)
        return last;

    StackBaseShape base(last);
    base.metadata = metadata;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ bool
js::ObjectImpl::preventExtensions(JSContext *cx, Handle<ObjectImpl*> obj)
{
#ifdef DEBUG
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    MOZ_ASSERT(extensible,
               "Callers must ensure |obj| is extensible before calling "
               "preventExtensions");
#endif

    if (Downcast(obj)->is<ProxyObject>()) {
        RootedObject object(cx, obj->asObjectPtr());
        return js::Proxy::preventExtensions(cx, object);
    }

    RootedObject self(cx, obj->asObjectPtr());

    /*
     * Force lazy properties to be resolved by iterating over the objects' own
     * properties.
     */
    AutoIdVector props(cx);
    if (!js::GetPropertyNames(cx, self, JSITER_HIDDEN | JSITER_OWNONLY, &props))
        return false;

    /*
     * Convert all dense elements to sparse properties. This will shrink the
     * initialized length and capacity of the object to zero and ensure that no
     * new dense elements can be added without calling growElements(), which
     * checks isExtensible().
     */
    if (self->isNative() && !JSObject::sparsifyDenseElements(cx, self))
        return false;

    return self->setFlag(cx, BaseShape::NOT_EXTENSIBLE, GENERATE_SHAPE);
}

bool
js::ObjectImpl::setFlag(ExclusiveContext *cx, /*BaseShape::Flag*/ uint32_t flag_,
                        GenerateShape generateShape)
{
    BaseShape::Flag flag = (BaseShape::Flag) flag_;

    if (lastProperty()->getObjectFlags() & flag)
        return true;

    Rooted<ObjectImpl*> self(cx, this);

    if (inDictionaryMode()) {
        if (generateShape == GENERATE_SHAPE && !generateOwnShape(cx))
            return false;
        StackBaseShape base(self->lastProperty());
        base.flags |= flag;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        self->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape =
        Shape::setObjectFlag(cx, flag, self->getTaggedProto(), self->lastProperty());
    if (!newShape)
        return false;

    self->shape_ = newShape;
    return true;
}

bool
js::ObjectImpl::clearFlag(ExclusiveContext *cx, /*BaseShape::Flag*/ uint32_t flag)
{
    JS_ASSERT(inDictionaryMode());
    JS_ASSERT(lastProperty()->getObjectFlags() & flag);

    RootedObject self(cx, this->asObjectPtr());

    StackBaseShape base(self->lastProperty());
    base.flags &= ~flag;
    UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return false;

    self->lastProperty()->base()->adoptUnowned(nbase);
    return true;
}

/* static */ Shape *
Shape::setObjectFlag(ExclusiveContext *cx, BaseShape::Flag flag, TaggedProto proto, Shape *last)
{
    if (last->getObjectFlags() & flag)
        return last;

    StackBaseShape base(last);
    base.flags |= flag;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ inline HashNumber
StackBaseShape::hash(const StackBaseShape *base)
{
    HashNumber hash = base->flags;
    hash = RotateLeft(hash, 4) ^ (uintptr_t(base->clasp) >> 3);
    hash = RotateLeft(hash, 4) ^ (uintptr_t(base->parent) >> 3);
    hash = RotateLeft(hash, 4) ^ (uintptr_t(base->metadata) >> 3);
    hash = RotateLeft(hash, 4) ^ uintptr_t(base->rawGetter);
    hash = RotateLeft(hash, 4) ^ uintptr_t(base->rawSetter);
    return hash;
}

/* static */ inline bool
StackBaseShape::match(UnownedBaseShape *key, const StackBaseShape *lookup)
{
    return key->flags == lookup->flags
        && key->clasp_ == lookup->clasp
        && key->parent == lookup->parent
        && key->metadata == lookup->metadata
        && key->rawGetter == lookup->rawGetter
        && key->rawSetter == lookup->rawSetter;
}

void
StackBaseShape::trace(JSTracer *trc)
{
    if (parent) {
        gc::MarkObjectRoot(trc, (JSObject**)&parent,
                           "StackBaseShape parent");
    }
    if (metadata) {
        gc::MarkObjectRoot(trc, (JSObject**)&metadata,
                           "StackBaseShape metadata");
    }
    if ((flags & BaseShape::HAS_GETTER_OBJECT) && rawGetter) {
        gc::MarkObjectRoot(trc, (JSObject**)&rawGetter,
                           "StackBaseShape getter");
    }
    if ((flags & BaseShape::HAS_SETTER_OBJECT) && rawSetter) {
        gc::MarkObjectRoot(trc, (JSObject**)&rawSetter,
                           "StackBaseShape setter");
    }
}

/* static */ UnownedBaseShape*
BaseShape::getUnowned(ExclusiveContext *cx, StackBaseShape &base)
{
    BaseShapeSet &table = cx->compartment()->baseShapes;

    if (!table.initialized() && !table.init())
        return nullptr;

    DependentAddPtr<BaseShapeSet> p(cx, table, &base);
    if (p)
        return *p;

    RootedGeneric<StackBaseShape*> root(cx, &base);

    BaseShape *nbase_ = js_NewGCBaseShape<CanGC>(cx);
    if (!nbase_)
        return nullptr;

    new (nbase_) BaseShape(*root);

    UnownedBaseShape *nbase = static_cast<UnownedBaseShape *>(nbase_);

    if (!p.add(cx, table, root, nbase))
        return nullptr;

    return nbase;
}

/* static */ UnownedBaseShape *
BaseShape::lookupUnowned(ThreadSafeContext *cx, const StackBaseShape &base)
{
    BaseShapeSet &table = cx->compartment_->baseShapes;

    if (!table.initialized())
        return nullptr;

    BaseShapeSet::Ptr p = table.readonlyThreadsafeLookup(&base);
    return *p;
}

void
BaseShape::assertConsistency()
{
#ifdef DEBUG
    if (isOwned()) {
        UnownedBaseShape *unowned = baseUnowned();
        JS_ASSERT(hasGetterObject() == unowned->hasGetterObject());
        JS_ASSERT(hasSetterObject() == unowned->hasSetterObject());
        JS_ASSERT_IF(hasGetterObject(), getterObject() == unowned->getterObject());
        JS_ASSERT_IF(hasSetterObject(), setterObject() == unowned->setterObject());
        JS_ASSERT(getObjectParent() == unowned->getObjectParent());
        JS_ASSERT(getObjectMetadata() == unowned->getObjectMetadata());
        JS_ASSERT(getObjectFlags() == unowned->getObjectFlags());
    }
#endif
}

void
JSCompartment::sweepBaseShapeTable()
{
    GCRuntime &gc = runtimeFromMainThread()->gc;
    gcstats::MaybeAutoPhase ap(gc.stats, !gc.isHeapCompacting(),
                               gcstats::PHASE_SWEEP_TABLES_BASE_SHAPE);

    if (baseShapes.initialized()) {
        for (BaseShapeSet::Enum e(baseShapes); !e.empty(); e.popFront()) {
            UnownedBaseShape *base = e.front().unbarrieredGet();
            if (IsBaseShapeAboutToBeFinalized(&base)) {
                e.removeFront();
            } else if (base != e.front()) {
                StackBaseShape sbase(base);
                ReadBarriered<UnownedBaseShape *> b(base);
                e.rekeyFront(&sbase, b);
            }
        }
    }
}

void
BaseShape::finalize(FreeOp *fop)
{
    if (table_) {
        fop->delete_(table_);
        table_ = nullptr;
    }
}

inline
InitialShapeEntry::InitialShapeEntry() : shape(nullptr), proto(nullptr)
{
}

inline
InitialShapeEntry::InitialShapeEntry(const ReadBarrieredShape &shape, TaggedProto proto)
  : shape(shape), proto(proto)
{
}

inline InitialShapeEntry::Lookup
InitialShapeEntry::getLookup() const
{
    return Lookup(shape->getObjectClass(), proto, shape->getObjectParent(), shape->getObjectMetadata(),
                  shape->numFixedSlots(), shape->getObjectFlags());
}

/* static */ inline HashNumber
InitialShapeEntry::hash(const Lookup &lookup)
{
    HashNumber hash = uintptr_t(lookup.clasp) >> 3;
    hash = RotateLeft(hash, 4) ^
        (uintptr_t(lookup.hashProto.toWord()) >> 3);
    hash = RotateLeft(hash, 4) ^
        (uintptr_t(lookup.hashParent) >> 3) ^
        (uintptr_t(lookup.hashMetadata) >> 3);
    return hash + lookup.nfixed;
}

/* static */ inline bool
InitialShapeEntry::match(const InitialShapeEntry &key, const Lookup &lookup)
{
    const Shape *shape = *key.shape.unsafeGet();
    return lookup.clasp == shape->getObjectClass()
        && lookup.matchProto.toWord() == key.proto.toWord()
        && lookup.matchParent == shape->getObjectParent()
        && lookup.matchMetadata == shape->getObjectMetadata()
        && lookup.nfixed == shape->numFixedSlots()
        && lookup.baseFlags == shape->getObjectFlags();
}

#ifdef JSGC_GENERATIONAL

/*
 * This class is used to add a post barrier on the initialShapes set, as the key
 * is calculated based on several objects which may be moved by generational GC.
 */
class InitialShapeSetRef : public BufferableRef
{
    InitialShapeSet *set;
    const Class *clasp;
    TaggedProto proto;
    JSObject *parent;
    JSObject *metadata;
    size_t nfixed;
    uint32_t objectFlags;

  public:
    InitialShapeSetRef(InitialShapeSet *set,
                       const Class *clasp,
                       TaggedProto proto,
                       JSObject *parent,
                       JSObject *metadata,
                       size_t nfixed,
                       uint32_t objectFlags)
        : set(set),
          clasp(clasp),
          proto(proto),
          parent(parent),
          metadata(metadata),
          nfixed(nfixed),
          objectFlags(objectFlags)
    {}

    void mark(JSTracer *trc) {
        TaggedProto priorProto = proto;
        JSObject *priorParent = parent;
        JSObject *priorMetadata = metadata;
        if (proto.isObject())
            Mark(trc, reinterpret_cast<JSObject**>(&proto), "initialShapes set proto");
        if (parent)
            Mark(trc, &parent, "initialShapes set parent");
        if (metadata)
            Mark(trc, &metadata, "initialShapes set metadata");
        if (proto == priorProto && parent == priorParent && metadata == priorMetadata)
            return;

        /* Find the original entry, which must still be present. */
        InitialShapeEntry::Lookup lookup(clasp, priorProto,
                                         priorParent, parent,
                                         priorMetadata, metadata,
                                         nfixed, objectFlags);
        InitialShapeSet::Ptr p = set->lookup(lookup);
        JS_ASSERT(p);

        /* Update the entry's possibly-moved proto, and ensure lookup will still match. */
        InitialShapeEntry &entry = const_cast<InitialShapeEntry&>(*p);
        entry.proto = proto;
        lookup.matchProto = proto;

        /* Rekey the entry. */
        set->rekeyAs(lookup,
                     InitialShapeEntry::Lookup(clasp, proto, parent, metadata, nfixed, objectFlags),
                     *p);
    }
};

#endif // JSGC_GENERATIONAL

#ifdef JSGC_HASH_TABLE_CHECKS

void
JSCompartment::checkInitialShapesTableAfterMovingGC()
{
    if (!initialShapes.initialized())
        return;

    /*
     * Assert that the postbarriers have worked and that nothing is left in
     * initialShapes that points into the nursery, and that the hash table
     * entries are discoverable.
     */
    for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
        InitialShapeEntry entry = e.front();
        TaggedProto proto = entry.proto;
        Shape *shape = entry.shape.get();

        if (proto.isObject())
            CheckGCThingAfterMovingGC(proto.toObject());
        if (shape->getObjectParent())
            CheckGCThingAfterMovingGC(shape->getObjectParent());
        if (shape->getObjectMetadata())
            CheckGCThingAfterMovingGC(shape->getObjectMetadata());

        InitialShapeEntry::Lookup lookup(shape->getObjectClass(),
                                         proto,
                                         shape->getObjectParent(),
                                         shape->getObjectMetadata(),
                                         shape->numFixedSlots(),
                                         shape->getObjectFlags());
        InitialShapeSet::Ptr ptr = initialShapes.lookup(lookup);
        JS_ASSERT(ptr.found() && &*ptr == &e.front());
    }
}

#endif // JSGC_HASH_TABLE_CHECKS

/* static */ Shape *
EmptyShape::getInitialShape(ExclusiveContext *cx, const Class *clasp, TaggedProto proto,
                            JSObject *parent, JSObject *metadata,
                            size_t nfixed, uint32_t objectFlags)
{
    JS_ASSERT_IF(proto.isObject(), cx->isInsideCurrentCompartment(proto.toObject()));
    JS_ASSERT_IF(parent, cx->isInsideCurrentCompartment(parent));

    InitialShapeSet &table = cx->compartment()->initialShapes;

    if (!table.initialized() && !table.init())
        return nullptr;

    typedef InitialShapeEntry::Lookup Lookup;
    DependentAddPtr<InitialShapeSet>
        p(cx, table, Lookup(clasp, proto, parent, metadata, nfixed, objectFlags));
    if (p)
        return p->shape;

    Rooted<TaggedProto> protoRoot(cx, proto);
    RootedObject parentRoot(cx, parent);
    RootedObject metadataRoot(cx, metadata);

    StackBaseShape base(cx, clasp, parent, metadata, objectFlags);
    Rooted<UnownedBaseShape*> nbase(cx, BaseShape::getUnowned(cx, base));
    if (!nbase)
        return nullptr;

    Shape *shape = cx->compartment()->propertyTree.newShape(cx);
    if (!shape)
        return nullptr;
    new (shape) EmptyShape(nbase, nfixed);

    Lookup lookup(clasp, protoRoot, parentRoot, metadataRoot, nfixed, objectFlags);
    if (!p.add(cx, table, lookup, InitialShapeEntry(ReadBarrieredShape(shape), protoRoot)))
        return nullptr;

#ifdef JSGC_GENERATIONAL
    if (cx->isJSContext()) {
        if ((protoRoot.isObject() && IsInsideNursery(protoRoot.toObject())) ||
            IsInsideNursery(parentRoot.get()) ||
            IsInsideNursery(metadataRoot.get()))
        {
            InitialShapeSetRef ref(
                &table, clasp, protoRoot, parentRoot, metadataRoot, nfixed, objectFlags);
            cx->asJSContext()->runtime()->gc.storeBuffer.putGeneric(ref);
        }
    }
#endif

    return shape;
}

/* static */ Shape *
EmptyShape::getInitialShape(ExclusiveContext *cx, const Class *clasp, TaggedProto proto,
                            JSObject *parent, JSObject *metadata,
                            AllocKind kind, uint32_t objectFlags)
{
    return getInitialShape(cx, clasp, proto, parent, metadata, GetGCKindSlots(kind, clasp), objectFlags);
}

void
NewObjectCache::invalidateEntriesForShape(JSContext *cx, HandleShape shape, HandleObject proto)
{
    const Class *clasp = shape->getObjectClass();

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    Rooted<GlobalObject *> global(cx, &shape->getObjectParent()->global());
    Rooted<types::TypeObject *> type(cx, cx->getNewType(clasp, TaggedProto(proto)));

    EntryIndex entry;
    if (lookupGlobal(clasp, global, kind, &entry))
        PodZero(&entries[entry]);
    if (!proto->is<GlobalObject>() && lookupProto(clasp, proto, kind, &entry))
        PodZero(&entries[entry]);
    if (lookupType(type, kind, &entry))
        PodZero(&entries[entry]);
}

/* static */ void
EmptyShape::insertInitialShape(ExclusiveContext *cx, HandleShape shape, HandleObject proto)
{
    InitialShapeEntry::Lookup lookup(shape->getObjectClass(), TaggedProto(proto),
                                     shape->getObjectParent(), shape->getObjectMetadata(),
                                     shape->numFixedSlots(), shape->getObjectFlags());

    InitialShapeSet::Ptr p = cx->compartment()->initialShapes.lookup(lookup);
    JS_ASSERT(p);

    InitialShapeEntry &entry = const_cast<InitialShapeEntry &>(*p);

    /* The new shape had better be rooted at the old one. */
#ifdef DEBUG
    Shape *nshape = shape;
    while (!nshape->isEmptyShape())
        nshape = nshape->previous();
    JS_ASSERT(nshape == entry.shape);
#endif

    entry.shape = ReadBarrieredShape(shape);

    /*
     * This affects the shape that will be produced by the various NewObject
     * methods, so clear any cache entry referring to the old shape. This is
     * not required for correctness: the NewObject must always check for a
     * nativeEmpty() result and generate the appropriate properties if found.
     * Clearing the cache entry avoids this duplicate regeneration.
     *
     * Clearing is not necessary when this context is running off the main
     * thread, as it will not use the new object cache for allocations.
     */
    if (cx->isJSContext()) {
        JSContext *ncx = cx->asJSContext();
        ncx->runtime()->newObjectCache.invalidateEntriesForShape(ncx, shape, proto);
    }
}

void
JSCompartment::sweepInitialShapeTable()
{
    GCRuntime &gc = runtimeFromMainThread()->gc;
    gcstats::MaybeAutoPhase ap(gc.stats, !gc.isHeapCompacting(),
                               gcstats::PHASE_SWEEP_TABLES_INITIAL_SHAPE);

    if (initialShapes.initialized()) {
        for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
            const InitialShapeEntry &entry = e.front();
            Shape *shape = entry.shape.unbarrieredGet();
            JSObject *proto = entry.proto.raw();
            if (IsShapeAboutToBeFinalized(&shape) ||
                (entry.proto.isObject() && IsObjectAboutToBeFinalized(&proto)))
            {
                e.removeFront();
            } else {
#ifdef DEBUG
                DebugOnly<JSObject *> parent = shape->getObjectParent();
                JS_ASSERT(!parent || !IsObjectAboutToBeFinalized(&parent));
                JS_ASSERT(parent == shape->getObjectParent());
#endif
                if (shape != entry.shape.unbarrieredGet() || proto != entry.proto.raw()) {
                    ReadBarrieredShape readBarrieredShape(shape);
                    InitialShapeEntry newKey(readBarrieredShape, TaggedProto(proto));
                    e.rekeyFront(newKey.getLookup(), newKey);
                }
            }
        }
    }
}

#ifdef JSGC_COMPACTING
void
JSCompartment::fixupInitialShapeTable()
{
    if (!initialShapes.initialized())
        return;

    for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
        InitialShapeEntry entry = e.front();
        bool needRekey = false;
        if (IsForwarded(entry.shape.get())) {
            entry.shape.set(Forwarded(entry.shape.get()));
            needRekey = true;
        }
        if (entry.proto.isObject() && IsForwarded(entry.proto.toObject())) {
            entry.proto = TaggedProto(Forwarded(entry.proto.toObject()));
            needRekey = true;
        }
        JSObject *parent = entry.shape->getObjectParent();
        if (parent) {
            parent = MaybeForwarded(parent);
            needRekey = true;
        }
        JSObject *metadata = entry.shape->getObjectMetadata();
        if (metadata) {
            metadata = MaybeForwarded(metadata);
            needRekey = true;
        }
        if (needRekey) {
            InitialShapeEntry::Lookup relookup(entry.shape->getObjectClass(),
                                               entry.proto,
                                               parent,
                                               metadata,
                                               entry.shape->numFixedSlots(),
                                               entry.shape->getObjectFlags());
            e.rekeyFront(relookup, entry);
        }
    }
}
#endif // JSGC_COMPACTING

void
AutoRooterGetterSetter::Inner::trace(JSTracer *trc)
{
    if ((attrs & JSPROP_GETTER) && *pgetter)
        gc::MarkObjectRoot(trc, (JSObject**) pgetter, "AutoRooterGetterSetter getter");
    if ((attrs & JSPROP_SETTER) && *psetter)
        gc::MarkObjectRoot(trc, (JSObject**) psetter, "AutoRooterGetterSetter setter");
}
