/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/UnboxedObject.h"

#include "jit/JitCommon.h"
#include "jit/Linker.h"

#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::PodCopy;
using mozilla::UniquePtr;

using namespace js;

/////////////////////////////////////////////////////////////////////
// UnboxedLayout
/////////////////////////////////////////////////////////////////////

void
UnboxedLayout::trace(JSTracer* trc)
{
    for (size_t i = 0; i < properties_.length(); i++)
        TraceManuallyBarrieredEdge(trc, &properties_[i].name, "unboxed_layout_name");

    if (newScript())
        newScript()->trace(trc);

    if (nativeGroup_)
        TraceEdge(trc, &nativeGroup_, "unboxed_layout_nativeGroup");

    if (nativeShape_)
        TraceEdge(trc, &nativeShape_, "unboxed_layout_nativeShape");

    if (replacementNewGroup_)
        TraceEdge(trc, &replacementNewGroup_, "unboxed_layout_replacementNewGroup");

    if (constructorCode_)
        TraceEdge(trc, &constructorCode_, "unboxed_layout_constructorCode");
}

size_t
UnboxedLayout::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    return mallocSizeOf(this)
         + properties_.sizeOfExcludingThis(mallocSizeOf)
         + (newScript() ? newScript()->sizeOfIncludingThis(mallocSizeOf) : 0)
         + mallocSizeOf(traceList());
}

void
UnboxedLayout::setNewScript(TypeNewScript* newScript, bool writeBarrier /* = true */)
{
    if (newScript_ && writeBarrier)
        TypeNewScript::writeBarrierPre(newScript_);
    newScript_ = newScript;
}

// Constructor code returns a 0x1 value to indicate the constructor code should
// be cleared.
static const uintptr_t CLEAR_CONSTRUCTOR_CODE_TOKEN = 0x1;

/* static */ bool
UnboxedLayout::makeConstructorCode(JSContext* cx, HandleObjectGroup group)
{
    using namespace jit;

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    UnboxedLayout& layout = group->unboxedLayout();
    MOZ_ASSERT(!layout.constructorCode());

    UnboxedPlainObject* templateObject = UnboxedPlainObject::create(cx, group, TenuredObject);
    if (!templateObject)
        return false;

    JitContext jitContext(cx, nullptr);

    MacroAssembler masm;

    Register propertiesReg, newKindReg;
#ifdef JS_CODEGEN_X86
    propertiesReg = eax;
    newKindReg = ecx;
    masm.loadPtr(Address(masm.getStackPointer(), sizeof(void*)), propertiesReg);
    masm.loadPtr(Address(masm.getStackPointer(), 2 * sizeof(void*)), newKindReg);
#else
    propertiesReg = IntArgReg0;
    newKindReg = IntArgReg1;
#endif

    MOZ_ASSERT(propertiesReg.volatile_());
    MOZ_ASSERT(newKindReg.volatile_());

    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(propertiesReg);
    regs.take(newKindReg);
    Register object = regs.takeAny(), scratch1 = regs.takeAny(), scratch2 = regs.takeAny();

    LiveGeneralRegisterSet savedNonVolatileRegisters = SavedNonVolatileRegisters(regs);
    for (GeneralRegisterForwardIterator iter(savedNonVolatileRegisters); iter.more(); ++iter)
        masm.Push(*iter);

    // The scratch double register might be used by MacroAssembler methods.
    if (ScratchDoubleReg.volatile_())
        masm.push(ScratchDoubleReg);

    Label failure, tenuredObject, allocated;
    masm.branch32(Assembler::NotEqual, newKindReg, Imm32(GenericObject), &tenuredObject);
    masm.branchTest32(Assembler::NonZero, AbsoluteAddress(group->addressOfFlags()),
                      Imm32(OBJECT_FLAG_PRE_TENURE), &tenuredObject);

    // Allocate an object in the nursery
    masm.createGCObject(object, scratch1, templateObject, gc::DefaultHeap, &failure,
                        /* initFixedSlots = */ false);

    masm.jump(&allocated);
    masm.bind(&tenuredObject);

    // Allocate an object in the tenured heap.
    masm.createGCObject(object, scratch1, templateObject, gc::TenuredHeap, &failure,
                        /* initFixedSlots = */ false);

    // If any of the properties being stored are in the nursery, add a store
    // buffer entry for the new object.
    Label postBarrier;
    for (size_t i = 0; i < layout.properties().length(); i++) {
        const UnboxedLayout::Property& property = layout.properties()[i];
        if (property.type == JSVAL_TYPE_OBJECT) {
            Address valueAddress(propertiesReg, i * sizeof(IdValuePair) + offsetof(IdValuePair, value));
            Label notObject;
            masm.branchTestObject(Assembler::NotEqual, valueAddress, &notObject);
            Register valueObject = masm.extractObject(valueAddress, scratch1);
            masm.branchPtrInNurseryRange(Assembler::Equal, valueObject, scratch2, &postBarrier);
            masm.bind(&notObject);
        }
    }

    masm.jump(&allocated);
    masm.bind(&postBarrier);

    masm.mov(ImmPtr(cx->runtime()), scratch1);
    masm.setupUnalignedABICall(2, scratch2);
    masm.passABIArg(scratch1);
    masm.passABIArg(object);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, PostWriteBarrier));

    masm.bind(&allocated);

    ValueOperand valueOperand;
#ifdef JS_NUNBOX32
    valueOperand = ValueOperand(scratch1, scratch2);
#else
    valueOperand = ValueOperand(scratch1);
#endif

    Label failureStoreOther, failureStoreObject;

    for (size_t i = 0; i < layout.properties().length(); i++) {
        const UnboxedLayout::Property& property = layout.properties()[i];
        Address valueAddress(propertiesReg, i * sizeof(IdValuePair) + offsetof(IdValuePair, value));
        Address targetAddress(object, UnboxedPlainObject::offsetOfData() + property.offset);

        masm.loadValue(valueAddress, valueOperand);

        if (property.type == JSVAL_TYPE_OBJECT) {
            HeapTypeSet* types = group->maybeGetProperty(IdToTypeId(NameToId(property.name)));

            Label notObject;
            masm.branchTestObject(Assembler::NotEqual, valueOperand,
                                  types->mightBeMIRType(MIRType_Null) ? &notObject : &failureStoreObject);

            Register payloadReg = masm.extractObject(valueOperand, scratch1);

            if (!types->hasType(TypeSet::AnyObjectType())) {
                Register scratch = (payloadReg == scratch1) ? scratch2 : scratch1;
                masm.guardObjectType(payloadReg, types, scratch, &failureStoreObject);
            }

            masm.storeUnboxedProperty(targetAddress, JSVAL_TYPE_OBJECT,
                                      TypedOrValueRegister(MIRType_Object,
                                                           AnyRegister(payloadReg)), nullptr);

            if (notObject.used()) {
                Label done;
                masm.jump(&done);
                masm.bind(&notObject);
                masm.branchTestNull(Assembler::NotEqual, valueOperand, &failureStoreOther);
                masm.storeUnboxedProperty(targetAddress, JSVAL_TYPE_OBJECT, NullValue(), nullptr);
                masm.bind(&done);
            }
        } else {
            masm.storeUnboxedProperty(targetAddress, property.type,
                                      ConstantOrRegister(valueOperand), &failureStoreOther);
        }
    }

    Label done;
    masm.bind(&done);

    if (object != ReturnReg)
        masm.movePtr(object, ReturnReg);

    // Restore non-volatile registers which were saved on entry.
    if (ScratchDoubleReg.volatile_())
        masm.pop(ScratchDoubleReg);
    for (GeneralRegisterBackwardIterator iter(savedNonVolatileRegisters); iter.more(); ++iter)
        masm.Pop(*iter);

    masm.abiret();

    masm.bind(&failureStoreOther);

    // There was a failure while storing a value which cannot be stored at all
    // in the unboxed object. Initialize the object so it is safe for GC and
    // return null.
    masm.initUnboxedObjectContents(object, templateObject);

    masm.bind(&failure);

    masm.movePtr(ImmWord(0), object);
    masm.jump(&done);

    masm.bind(&failureStoreObject);

    // There was a failure while storing a value to an object slot of the
    // unboxed object. If the value is storable, the failure occurred due to
    // incomplete type information in the object, so return a token to trigger
    // regeneration of the jitcode after a new object is created in the VM.
    {
        Label isObject;
        masm.branchTestObject(Assembler::Equal, valueOperand, &isObject);
        masm.branchTestNull(Assembler::NotEqual, valueOperand, &failureStoreOther);
        masm.bind(&isObject);
    }

    // Initialize the object so it is safe for GC.
    masm.initUnboxedObjectContents(object, templateObject);

    masm.movePtr(ImmWord(CLEAR_CONSTRUCTOR_CODE_TOKEN), object);
    masm.jump(&done);

    Linker linker(masm);
    AutoFlushICache afc("UnboxedObject");
    JitCode* code = linker.newCode<NoGC>(cx, OTHER_CODE);
    if (!code)
        return false;

    layout.setConstructorCode(code);
    return true;
}

void
UnboxedLayout::detachFromCompartment()
{
    remove();
}

/////////////////////////////////////////////////////////////////////
// UnboxedPlainObject
/////////////////////////////////////////////////////////////////////

bool
UnboxedPlainObject::setValue(ExclusiveContext* cx, const UnboxedLayout::Property& property,
                             const Value& v)
{
    uint8_t* p = &data_[property.offset];

    switch (property.type) {
      case JSVAL_TYPE_BOOLEAN:
        if (v.isBoolean()) {
            *p = v.toBoolean();
            return true;
        }
        return false;

      case JSVAL_TYPE_INT32:
        if (v.isInt32()) {
            *reinterpret_cast<int32_t*>(p) = v.toInt32();
            return true;
        }
        return false;

      case JSVAL_TYPE_DOUBLE:
        if (v.isNumber()) {
            *reinterpret_cast<double*>(p) = v.toNumber();
            return true;
        }
        return false;

      case JSVAL_TYPE_STRING:
        if (v.isString()) {
            MOZ_ASSERT(!IsInsideNursery(v.toString()));
            *reinterpret_cast<PreBarrieredString*>(p) = v.toString();
            return true;
        }
        return false;

      case JSVAL_TYPE_OBJECT:
        if (v.isObjectOrNull()) {
            // Update property types when writing object properties. Types for
            // other properties were captured when the unboxed layout was
            // created.
            AddTypePropertyId(cx, this, NameToId(property.name), v);

            // Manually trigger post barriers on the whole object. If we treat
            // the pointer as a HeapPtrObject we will get confused later if the
            // object is converted to its native representation.
            JSObject* obj = v.toObjectOrNull();
            if (IsInsideNursery(v.toObjectOrNull()) && !IsInsideNursery(this))
                cx->asJSContext()->runtime()->gc.storeBuffer.putWholeCellFromMainThread(this);

            *reinterpret_cast<PreBarrieredObject*>(p) = obj;
            return true;
        }
        return false;

      default:
        MOZ_CRASH("Invalid type for unboxed value");
    }
}

Value
UnboxedPlainObject::getValue(const UnboxedLayout::Property& property)
{
    uint8_t* p = &data_[property.offset];

    switch (property.type) {
      case JSVAL_TYPE_BOOLEAN:
        return BooleanValue(*p != 0);

      case JSVAL_TYPE_INT32:
        return Int32Value(*reinterpret_cast<int32_t*>(p));

      case JSVAL_TYPE_DOUBLE:
        return DoubleValue(*reinterpret_cast<double*>(p));

      case JSVAL_TYPE_STRING:
        return StringValue(*reinterpret_cast<JSString**>(p));

      case JSVAL_TYPE_OBJECT:
        return ObjectOrNullValue(*reinterpret_cast<JSObject**>(p));

      default:
        MOZ_CRASH("Invalid type for unboxed value");
    }
}

void
UnboxedPlainObject::trace(JSTracer* trc, JSObject* obj)
{
    if (obj->as<UnboxedPlainObject>().expando_) {
        TraceManuallyBarrieredEdge(trc,
            reinterpret_cast<NativeObject**>(&obj->as<UnboxedPlainObject>().expando_),
            "unboxed_expando");
    }

    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layoutDontCheckGeneration();
    const int32_t* list = layout.traceList();
    if (!list)
        return;

    uint8_t* data = obj->as<UnboxedPlainObject>().data();
    while (*list != -1) {
        HeapPtrString* heap = reinterpret_cast<HeapPtrString*>(data + *list);
        TraceEdge(trc, heap, "unboxed_string");
        list++;
    }
    list++;
    while (*list != -1) {
        HeapPtrObject* heap = reinterpret_cast<HeapPtrObject*>(data + *list);
        if (*heap)
            TraceEdge(trc, heap, "unboxed_object");
        list++;
    }

    // Unboxed objects don't have Values to trace.
    MOZ_ASSERT(*(list + 1) == -1);
}

/* static */ UnboxedExpandoObject*
UnboxedPlainObject::ensureExpando(JSContext* cx, Handle<UnboxedPlainObject*> obj)
{
    if (obj->expando_)
        return obj->expando_;

    UnboxedExpandoObject* expando = NewObjectWithGivenProto<UnboxedExpandoObject>(cx, NullPtr());
    if (!expando)
        return nullptr;

    // As with setValue(), we need to manually trigger post barriers on the
    // whole object. If we treat the field as a HeapPtrObject and later convert
    // the object to its native representation, we will end up with a corrupted
    // store buffer entry.
    if (IsInsideNursery(expando) && !IsInsideNursery(obj))
        cx->runtime()->gc.storeBuffer.putWholeCellFromMainThread(obj);

    obj->expando_ = expando;
    return expando;
}

bool
UnboxedPlainObject::containsUnboxedOrExpandoProperty(ExclusiveContext* cx, jsid id) const
{
    if (layout().lookup(id))
        return true;

    if (maybeExpando() && maybeExpando()->containsShapeOrElement(cx, id))
        return true;

    return false;
}

/* static */ bool
UnboxedLayout::makeNativeGroup(JSContext* cx, ObjectGroup* group)
{
    AutoEnterAnalysis enter(cx);

    UnboxedLayout& layout = group->unboxedLayout();
    Rooted<TaggedProto> proto(cx, group->proto());

    MOZ_ASSERT(!layout.nativeGroup());

    // Immediately clear any new script on the group. This is done by replacing
    // the existing new script with one for a replacement default new group.
    // This is done so that the size of the replacment group's objects is the
    // same as that for the unboxed group, so that we do not see polymorphic
    // slot accesses later on for sites that see converted objects from this
    // group and objects that were allocated using the replacement new group.
    RootedObjectGroup replacementNewGroup(cx);
    if (layout.newScript()) {
        replacementNewGroup = ObjectGroupCompartment::makeGroup(cx, &PlainObject::class_, proto);
        if (!replacementNewGroup)
            return false;

        PlainObject* templateObject = NewObjectWithGroup<PlainObject>(cx, replacementNewGroup,
                                                                      layout.getAllocKind(),
                                                                      TenuredObject);
        if (!templateObject)
            return false;

        for (size_t i = 0; i < layout.properties().length(); i++) {
            const UnboxedLayout::Property& property = layout.properties()[i];
            if (!templateObject->addDataProperty(cx, NameToId(property.name), i, JSPROP_ENUMERATE))
                return false;
            MOZ_ASSERT(templateObject->slotSpan() == i + 1);
            MOZ_ASSERT(!templateObject->inDictionaryMode());
        }

        TypeNewScript* replacementNewScript =
            TypeNewScript::makeNativeVersion(cx, layout.newScript(), templateObject);
        if (!replacementNewScript)
            return false;

        replacementNewGroup->setNewScript(replacementNewScript);
        gc::TraceTypeNewScript(replacementNewGroup);

        group->clearNewScript(cx, replacementNewGroup);
    }

    size_t nfixed = gc::GetGCKindSlots(layout.getAllocKind());
    RootedShape shape(cx, EmptyShape::getInitialShape(cx, &PlainObject::class_, proto, nfixed, 0));
    if (!shape)
        return false;

    for (size_t i = 0; i < layout.properties().length(); i++) {
        const UnboxedLayout::Property& property = layout.properties()[i];

        StackShape unrootedChild(shape->base()->unowned(), NameToId(property.name), i,
                                 JSPROP_ENUMERATE, 0);
        RootedGeneric<StackShape*> child(cx, &unrootedChild);
        shape = cx->compartment()->propertyTree.getChild(cx, shape, *child);
        if (!shape)
            return false;
    }

    ObjectGroup* nativeGroup =
        ObjectGroupCompartment::makeGroup(cx, &PlainObject::class_, proto,
                                          group->flags() & OBJECT_FLAG_DYNAMIC_MASK);
    if (!nativeGroup)
        return false;

    // Propagate all property types from the old group to the new group.
    for (size_t i = 0; i < layout.properties().length(); i++) {
        const UnboxedLayout::Property& property = layout.properties()[i];
        jsid id = NameToId(property.name);

        HeapTypeSet* typeProperty = group->maybeGetProperty(id);
        TypeSet::TypeList types;
        if (!typeProperty->enumerateTypes(&types))
            return false;
        MOZ_ASSERT(!types.empty());
        for (size_t j = 0; j < types.length(); j++)
            AddTypePropertyId(cx, nativeGroup, nullptr, id, types[j]);
        HeapTypeSet* nativeProperty = nativeGroup->maybeGetProperty(id);
        if (nativeProperty->canSetDefinite(i))
            nativeProperty->setDefinite(i);
    }

    layout.nativeGroup_ = nativeGroup;
    layout.nativeShape_ = shape;
    layout.replacementNewGroup_ = replacementNewGroup;

    nativeGroup->setOriginalUnboxedGroup(group);

    group->markStateChange(cx);

    return true;
}

/* static */ bool
UnboxedPlainObject::convertToNative(JSContext* cx, JSObject* obj)
{
    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();
    UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();

    if (!layout.nativeGroup()) {
        if (!UnboxedLayout::makeNativeGroup(cx, obj->group()))
            return false;

        // makeNativeGroup can reentrantly invoke this method.
        if (obj->is<PlainObject>())
            return true;
    }

    AutoValueVector values(cx);
    for (size_t i = 0; i < layout.properties().length(); i++) {
        if (!values.append(obj->as<UnboxedPlainObject>().getValue(layout.properties()[i])))
            return false;
    }

    obj->setGroup(layout.nativeGroup());
    obj->as<PlainObject>().setLastPropertyMakeNative(cx, layout.nativeShape());

    for (size_t i = 0; i < values.length(); i++)
        obj->as<PlainObject>().initSlotUnchecked(i, values[i]);

    if (expando) {
        // Add properties from the expando object to the object, in order.
        // Suppress GC here, so that callers don't need to worry about this
        // method collecting. The stuff below can only fail due to OOM, in
        // which case the object will not have been completely filled back in.
        gc::AutoSuppressGC suppress(cx);

        Vector<jsid> ids(cx);
        for (Shape::Range<NoGC> r(expando->lastProperty()); !r.empty(); r.popFront()) {
            if (!ids.append(r.front().propid()))
                return false;
        }
        for (size_t i = 0; i < expando->getDenseInitializedLength(); i++) {
            if (!expando->getDenseElement(i).isMagic(JS_ELEMENTS_HOLE)) {
                if (!ids.append(INT_TO_JSID(i)))
                    return false;
            }
        }
        ::Reverse(ids.begin(), ids.end());

        RootedPlainObject nobj(cx, &obj->as<PlainObject>());
        Rooted<UnboxedExpandoObject*> nexpando(cx, expando);
        RootedId id(cx);
        Rooted<PropertyDescriptor> desc(cx);
        for (size_t i = 0; i < ids.length(); i++) {
            id = ids[i];
            if (!GetOwnPropertyDescriptor(cx, nexpando, id, &desc))
                return false;
            ObjectOpResult result;
            if (!StandardDefineProperty(cx, nobj, id, desc, result))
                return false;
            MOZ_ASSERT(result.ok());
        }
    }

    return true;
}

/* static */
UnboxedPlainObject*
UnboxedPlainObject::create(ExclusiveContext* cx, HandleObjectGroup group, NewObjectKind newKind)
{
    MOZ_ASSERT(group->clasp() == &class_);
    gc::AllocKind allocKind = group->unboxedLayout().getAllocKind();

    UnboxedPlainObject* res =
        NewObjectWithGroup<UnboxedPlainObject>(cx, group, allocKind, newKind);
    if (!res)
        return nullptr;

    // Overwrite the dummy shape which was written to the object's expando field.
    res->initExpando();

    // Initialize reference fields of the object. All fields in the object will
    // be overwritten shortly, but references need to be safe for the GC.
    const int32_t* list = res->layout().traceList();
    if (list) {
        uint8_t* data = res->data();
        while (*list != -1) {
            HeapPtrString* heap = reinterpret_cast<HeapPtrString*>(data + *list);
            heap->init(cx->names().empty);
            list++;
        }
        list++;
        while (*list != -1) {
            HeapPtrObject* heap = reinterpret_cast<HeapPtrObject*>(data + *list);
            heap->init(nullptr);
            list++;
        }
        // Unboxed objects don't have Values to initialize.
        MOZ_ASSERT(*(list + 1) == -1);
    }

    return res;
}

/* static */ JSObject*
UnboxedPlainObject::createWithProperties(ExclusiveContext* cx, HandleObjectGroup group,
                                         NewObjectKind newKind, IdValuePair* properties)
{
    MOZ_ASSERT(newKind == GenericObject || newKind == TenuredObject);

    UnboxedLayout& layout = group->unboxedLayout();

    if (layout.constructorCode()) {
        MOZ_ASSERT(cx->isJSContext());

        typedef JSObject* (*ConstructorCodeSignature)(IdValuePair*, NewObjectKind);
        ConstructorCodeSignature function =
            reinterpret_cast<ConstructorCodeSignature>(layout.constructorCode()->raw());

        JSObject* obj;
        {
            JS::AutoSuppressGCAnalysis nogc;
            obj = reinterpret_cast<JSObject*>(CALL_GENERATED_2(function, properties, newKind));
        }
        if (obj > reinterpret_cast<JSObject*>(CLEAR_CONSTRUCTOR_CODE_TOKEN))
            return obj;

        if (obj == reinterpret_cast<JSObject*>(CLEAR_CONSTRUCTOR_CODE_TOKEN))
            layout.setConstructorCode(nullptr);
    }

    UnboxedPlainObject* obj = UnboxedPlainObject::create(cx, group, newKind);
    if (!obj)
        return nullptr;

    for (size_t i = 0; i < layout.properties().length(); i++) {
        if (!obj->setValue(cx, layout.properties()[i], properties[i].value))
            return NewPlainObjectWithProperties(cx, properties, layout.properties().length(), newKind);
    }

#ifndef JS_CODEGEN_NONE
    if (cx->isJSContext() &&
        !layout.constructorCode() &&
        cx->asJSContext()->runtime()->jitSupportsFloatingPoint)
    {
        if (!UnboxedLayout::makeConstructorCode(cx->asJSContext(), group))
            return nullptr;
    }
#endif

    return obj;
}

/* static */ bool
UnboxedPlainObject::obj_lookupProperty(JSContext* cx, HandleObject obj,
                                       HandleId id, MutableHandleObject objp,
                                       MutableHandleShape propp)
{
    if (obj->as<UnboxedPlainObject>().containsUnboxedOrExpandoProperty(cx, id)) {
        MarkNonNativePropertyFound<CanGC>(propp);
        objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        objp.set(nullptr);
        propp.set(nullptr);
        return true;
    }

    return LookupProperty(cx, proto, id, objp, propp);
}

/* static */ bool
UnboxedPlainObject::obj_defineProperty(JSContext* cx, HandleObject obj, HandleId id,
                                       Handle<JSPropertyDescriptor> desc,
                                       ObjectOpResult& result)
{
    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

    if (const UnboxedLayout::Property* property = layout.lookup(id)) {
        if (!desc.getter() && !desc.setter() && desc.attributes() == JSPROP_ENUMERATE) {
            // This define is equivalent to setting an existing property.
            if (obj->as<UnboxedPlainObject>().setValue(cx, *property, desc.value()))
                return true;
        }

        // Trying to incompatibly redefine an existing property requires the
        // object to be converted to a native object.
        if (!convertToNative(cx, obj))
            return false;

        return DefineProperty(cx, obj, id, desc, result) &&
               result.checkStrict(cx, obj, id);
    }

    // Define the property on the expando object.
    Rooted<UnboxedExpandoObject*> expando(cx, ensureExpando(cx, obj.as<UnboxedPlainObject>()));
    if (!expando)
        return false;

    // Update property types on the unboxed object as well.
    AddTypePropertyId(cx, obj, id, desc.value());

    return DefineProperty(cx, expando, id, desc, result);
}

/* static */ bool
UnboxedPlainObject::obj_hasProperty(JSContext* cx, HandleObject obj, HandleId id, bool* foundp)
{
    if (obj->as<UnboxedPlainObject>().containsUnboxedOrExpandoProperty(cx, id)) {
        *foundp = true;
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *foundp = false;
        return true;
    }

    return HasProperty(cx, proto, id, foundp);
}

/* static */ bool
UnboxedPlainObject::obj_getProperty(JSContext* cx, HandleObject obj, HandleObject receiver,
                                    HandleId id, MutableHandleValue vp)
{
    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

    if (const UnboxedLayout::Property* property = layout.lookup(id)) {
        vp.set(obj->as<UnboxedPlainObject>().getValue(*property));
        return true;
    }

    if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
        if (expando->containsShapeOrElement(cx, id)) {
            RootedObject nexpando(cx, expando);
            RootedObject nreceiver(cx, (obj == receiver) ? expando : receiver.get());
            return GetProperty(cx, nexpando, nreceiver, id, vp);
        }
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return GetProperty(cx, proto, receiver, id, vp);
}

/* static */ bool
UnboxedPlainObject::obj_setProperty(JSContext* cx, HandleObject obj, HandleId id, HandleValue v,
                                    HandleValue receiver, ObjectOpResult& result)
{
    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

    if (const UnboxedLayout::Property* property = layout.lookup(id)) {
        if (receiver.isObject() && obj == &receiver.toObject()) {
            if (obj->as<UnboxedPlainObject>().setValue(cx, *property, v))
                return result.succeed();

            if (!convertToNative(cx, obj))
                return false;
            return SetProperty(cx, obj, id, v, receiver, result);
        }

        return SetPropertyByDefining(cx, obj, id, v, receiver, false, result);
    }

    if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
        if (expando->containsShapeOrElement(cx, id)) {
            // Update property types on the unboxed object as well.
            AddTypePropertyId(cx, obj, id, v);

            RootedObject nexpando(cx, expando);
            RootedValue nreceiver(cx, (receiver.isObject() && obj == &receiver.toObject())
                                      ? ObjectValue(*expando)
                                      : receiver);
            return SetProperty(cx, nexpando, id, v, nreceiver, result);
        }
    }

    return SetPropertyOnProto(cx, obj, id, v, receiver, result);
}

/* static */ bool
UnboxedPlainObject::obj_getOwnPropertyDescriptor(JSContext* cx, HandleObject obj, HandleId id,
                                                 MutableHandle<JSPropertyDescriptor> desc)
{
    const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();

    if (const UnboxedLayout::Property* property = layout.lookup(id)) {
        desc.value().set(obj->as<UnboxedPlainObject>().getValue(*property));
        desc.setAttributes(JSPROP_ENUMERATE);
        desc.object().set(obj);
        return true;
    }

    if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
        if (expando->containsShapeOrElement(cx, id)) {
            RootedObject nexpando(cx, expando);
            return GetOwnPropertyDescriptor(cx, nexpando, id, desc);
        }
    }

    desc.object().set(nullptr);
    return true;
}

/* static */ bool
UnboxedPlainObject::obj_deleteProperty(JSContext* cx, HandleObject obj, HandleId id,
                                       ObjectOpResult& result)
{
    if (!convertToNative(cx, obj))
        return false;
    return DeleteProperty(cx, obj, id, result);
}

/* static */ bool
UnboxedPlainObject::obj_watch(JSContext* cx, HandleObject obj, HandleId id, HandleObject callable)
{
    if (!convertToNative(cx, obj))
        return false;
    return WatchProperty(cx, obj, id, callable);
}

/* static */ bool
UnboxedPlainObject::obj_enumerate(JSContext* cx, HandleObject obj, AutoIdVector& properties)
{
    UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();

    // Add dense elements in the expando first, for consistency with plain objects.
    if (expando) {
        for (size_t i = 0; i < expando->getDenseInitializedLength(); i++) {
            if (!expando->getDenseElement(i).isMagic(JS_ELEMENTS_HOLE)) {
                if (!properties.append(INT_TO_JSID(i)))
                    return false;
            }
        }
    }

    const UnboxedLayout::PropertyVector& unboxed = obj->as<UnboxedPlainObject>().layout().properties();
    for (size_t i = 0; i < unboxed.length(); i++) {
        if (!properties.append(NameToId(unboxed[i].name)))
            return false;
    }

    if (expando) {
        Vector<jsid> ids(cx);
        for (Shape::Range<NoGC> r(expando->lastProperty()); !r.empty(); r.popFront()) {
            if (!ids.append(r.front().propid()))
                return false;
        }
        ::Reverse(ids.begin(), ids.end());
        if (!properties.append(ids.begin(), ids.length()))
            return false;
    }

    return true;
}

const Class UnboxedExpandoObject::class_ = {
    "UnboxedExpandoObject",
    JSCLASS_IMPLEMENTS_BARRIERS
};

const Class UnboxedPlainObject::class_ = {
    js_Object_str,
    Class::NON_NATIVE | JSCLASS_IMPLEMENTS_BARRIERS,
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* getProperty */
    nullptr,        /* setProperty */
    nullptr,        /* enumerate   */
    nullptr,        /* resolve     */
    nullptr,        /* convert     */
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    UnboxedPlainObject::trace,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    {
        UnboxedPlainObject::obj_lookupProperty,
        UnboxedPlainObject::obj_defineProperty,
        UnboxedPlainObject::obj_hasProperty,
        UnboxedPlainObject::obj_getProperty,
        UnboxedPlainObject::obj_setProperty,
        UnboxedPlainObject::obj_getOwnPropertyDescriptor,
        UnboxedPlainObject::obj_deleteProperty,
        UnboxedPlainObject::obj_watch,
        nullptr,   /* No unwatch needed, as watch() converts the object to native */
        nullptr,   /* getElements */
        UnboxedPlainObject::obj_enumerate,
        nullptr, /* thisObject */
    }
};

/////////////////////////////////////////////////////////////////////
// API
/////////////////////////////////////////////////////////////////////

static bool
UnboxedTypeIncludes(JSValueType supertype, JSValueType subtype)
{
    if (supertype == JSVAL_TYPE_DOUBLE && subtype == JSVAL_TYPE_INT32)
        return true;
    if (supertype == JSVAL_TYPE_OBJECT && subtype == JSVAL_TYPE_NULL)
        return true;
    return false;
}

// Return whether the property names and types in layout are a subset of the
// specified vector.
static bool
PropertiesAreSuperset(const UnboxedLayout::PropertyVector& properties, UnboxedLayout* layout)
{
    for (size_t i = 0; i < layout->properties().length(); i++) {
        const UnboxedLayout::Property& layoutProperty = layout->properties()[i];
        bool found = false;
        for (size_t j = 0; j < properties.length(); j++) {
            if (layoutProperty.name == properties[j].name) {
                found = (layoutProperty.type == properties[j].type);
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

bool
js::TryConvertToUnboxedLayout(ExclusiveContext* cx, Shape* templateShape,
                              ObjectGroup* group, PreliminaryObjectArray* objects)
{
    // Unboxed objects are nightly only for now. The getenv() call will be
    // removed when they are on by default. See bug 1153266.
#ifdef NIGHTLY_BUILD
    if (!getenv("JS_OPTION_USE_UNBOXED_OBJECTS")) {
        if (!templateShape->runtimeFromAnyThread()->options().unboxedObjects())
            return true;
    }
#else
    return true;
#endif

    if (templateShape->runtimeFromAnyThread()->isSelfHostingGlobal(cx->global()))
        return true;

    if (templateShape->slotSpan() == 0)
        return true;

    UnboxedLayout::PropertyVector properties;
    if (!properties.appendN(UnboxedLayout::Property(), templateShape->slotSpan()))
        return false;

    size_t objectCount = 0;
    for (size_t i = 0; i < PreliminaryObjectArray::COUNT; i++) {
        JSObject* obj = objects->get(i);
        if (!obj)
            continue;

        objectCount++;

        // All preliminary objects must have been created with enough space to
        // fill in their unboxed data inline. This is ensured either by using
        // the largest allocation kind (which limits the maximum size of an
        // unboxed object), or by using an allocation kind that covers all
        // properties in the template, as the space used by unboxed properties
        // less than or equal to that used by boxed properties.
        MOZ_ASSERT(gc::GetGCKindSlots(obj->asTenured().getAllocKind()) >=
                   Min(NativeObject::MAX_FIXED_SLOTS, templateShape->slotSpan()));

        if (obj->as<PlainObject>().lastProperty() != templateShape ||
            obj->as<PlainObject>().hasDynamicElements())
        {
            // Only use an unboxed representation if all created objects match
            // the template shape exactly.
            return true;
        }

        for (size_t i = 0; i < templateShape->slotSpan(); i++) {
            Value val = obj->as<PlainObject>().getSlot(i);

            JSValueType& existing = properties[i].type;
            JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

            if (existing == JSVAL_TYPE_MAGIC || existing == type || UnboxedTypeIncludes(type, existing))
                existing = type;
            else if (!UnboxedTypeIncludes(existing, type))
                return true;
        }
    }

    if (objectCount <= 1) {
        // If only one of the objects has been created, it is more likely to
        // have new properties added later.
        return true;
    }

    for (size_t i = 0; i < templateShape->slotSpan(); i++) {
        // We can't use an unboxed representation if e.g. all the objects have
        // a null value for one of the properties, as we can't decide what type
        // it is supposed to have.
        if (UnboxedTypeSize(properties[i].type) == 0)
            return true;
    }

    // Fill in the names for all the object's properties.
    for (Shape::Range<NoGC> r(templateShape); !r.empty(); r.popFront()) {
        size_t slot = r.front().slot();
        MOZ_ASSERT(!properties[slot].name);
        properties[slot].name = JSID_TO_ATOM(r.front().propid())->asPropertyName();
    }

    // Fill in all the unboxed object's property offsets.
    uint32_t offset = 0;

    // Search for an existing unboxed layout which is a subset of this one.
    // If there are multiple such layouts, use the largest one. If we're able
    // to find such a layout, use the same property offsets for the shared
    // properties, which will allow us to generate better code if the objects
    // have a subtype/supertype relation and are accessed at common sites.
    UnboxedLayout* bestExisting = nullptr;
    for (UnboxedLayout* existing = cx->compartment()->unboxedLayouts.getFirst();
         existing;
         existing = existing->getNext())
    {
        if (PropertiesAreSuperset(properties, existing)) {
            if (!bestExisting ||
                existing->properties().length() > bestExisting->properties().length())
            {
                bestExisting = existing;
            }
        }
    }
    if (bestExisting) {
        for (size_t i = 0; i < bestExisting->properties().length(); i++) {
            const UnboxedLayout::Property& existingProperty = bestExisting->properties()[i];
            for (size_t j = 0; j < templateShape->slotSpan(); j++) {
                if (existingProperty.name == properties[j].name) {
                    MOZ_ASSERT(existingProperty.type == properties[j].type);
                    properties[j].offset = existingProperty.offset;
                }
            }
        }
        offset = bestExisting->size();
    }

    // Order remaining properties from the largest down for the best space
    // utilization.
    static const size_t typeSizes[] = { 8, 4, 1 };

    for (size_t i = 0; i < ArrayLength(typeSizes); i++) {
        size_t size = typeSizes[i];
        for (size_t j = 0; j < templateShape->slotSpan(); j++) {
            if (properties[j].offset != UINT32_MAX)
                continue;
            JSValueType type = properties[j].type;
            if (UnboxedTypeSize(type) == size) {
                offset = JS_ROUNDUP(offset, size);
                properties[j].offset = offset;
                offset += size;
            }
        }
    }

    // The entire object must be allocatable inline.
    if (sizeof(JSObject) + offset > JSObject::MAX_BYTE_SIZE)
        return true;

    UniquePtr<UnboxedLayout, JS::DeletePolicy<UnboxedLayout> > layout;
    layout.reset(group->zone()->new_<UnboxedLayout>(properties, offset));
    if (!layout)
        return false;

    cx->compartment()->unboxedLayouts.insertFront(layout.get());

    // Figure out the offsets of any objects or string properties.
    Vector<int32_t, 8, SystemAllocPolicy> objectOffsets, stringOffsets;
    for (size_t i = 0; i < templateShape->slotSpan(); i++) {
        MOZ_ASSERT(properties[i].offset != UINT32_MAX);
        JSValueType type = properties[i].type;
        if (type == JSVAL_TYPE_OBJECT) {
            if (!objectOffsets.append(properties[i].offset))
                return false;
        } else if (type == JSVAL_TYPE_STRING) {
            if (!stringOffsets.append(properties[i].offset))
                return false;
        }
    }

    // Construct the layout's trace list.
    if (!objectOffsets.empty() || !stringOffsets.empty()) {
        Vector<int32_t, 8, SystemAllocPolicy> entries;
        if (!entries.appendAll(stringOffsets) ||
            !entries.append(-1) ||
            !entries.appendAll(objectOffsets) ||
            !entries.append(-1) ||
            !entries.append(-1))
        {
            return false;
        }
        int32_t* traceList = group->zone()->pod_malloc<int32_t>(entries.length());
        if (!traceList)
            return false;
        PodCopy(traceList, entries.begin(), entries.length());
        layout->setTraceList(traceList);
    }

    // We've determined that all the preliminary objects can use the new layout
    // just constructed, so convert the existing group to be an
    // UnboxedPlainObject rather than a PlainObject, and update the preliminary
    // objects to use the new layout. Do the fallible stuff first before
    // modifying any objects.

    // Get an empty shape which we can use for the preliminary objects.
    Shape* newShape = EmptyShape::getInitialShape(cx, &UnboxedPlainObject::class_,
                                                  group->proto(),
                                                  templateShape->getObjectFlags());
    if (!newShape) {
        cx->recoverFromOutOfMemory();
        return false;
    }

    // Accumulate a list of all the properties in each preliminary object, and
    // update their shapes.
    Vector<Value, 0, SystemAllocPolicy> values;
    if (!values.reserve(objectCount * templateShape->slotSpan()))
        return false;
    for (size_t i = 0; i < PreliminaryObjectArray::COUNT; i++) {
        if (!objects->get(i))
            continue;

        RootedNativeObject obj(cx, &objects->get(i)->as<NativeObject>());
        for (size_t j = 0; j < templateShape->slotSpan(); j++)
            values.infallibleAppend(obj->getSlot(j));

        // Clear the object to remove any dynamically allocated information.
        NativeObject::clear(cx, obj);

        obj->setLastPropertyMakeNonNative(newShape);
    }

    if (TypeNewScript* newScript = group->newScript())
        layout->setNewScript(newScript);

    group->setClasp(&UnboxedPlainObject::class_);
    group->setUnboxedLayout(layout.get());

    size_t valueCursor = 0;
    for (size_t i = 0; i < PreliminaryObjectArray::COUNT; i++) {
        if (!objects->get(i))
            continue;
        UnboxedPlainObject* obj = &objects->get(i)->as<UnboxedPlainObject>();
        obj->initExpando();
        memset(obj->data(), 0, layout->size());
        for (size_t j = 0; j < templateShape->slotSpan(); j++) {
            Value v = values[valueCursor++];
            JS_ALWAYS_TRUE(obj->setValue(cx, properties[j], v));
        }
    }

    MOZ_ASSERT(valueCursor == values.length());
    layout.release();
    return true;
}
