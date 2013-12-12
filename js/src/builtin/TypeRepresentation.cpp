/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TypeRepresentation.h"

#include "mozilla/HashFunctions.h"

#include "jscntxt.h"
#include "jsnum.h"
#include "jsutil.h"

#include "builtin/TypedObject.h"
#include "js/HashTable.h"
#include "vm/Runtime.h"
#include "vm/StringBuffer.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace mozilla;

///////////////////////////////////////////////////////////////////////////
// Class def'n for the owner object

const Class TypeRepresentation::class_ = {
    "TypeRepresentation",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEREPR_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    obj_finalize,
    nullptr,        /* checkAccess */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    obj_trace,
};

///////////////////////////////////////////////////////////////////////////
// Hashing

bool
TypeRepresentationHasher::match(TypeRepresentation *key1,
                                TypeRepresentation *key2)
{
    if (key1->kind() != key2->kind())
        return false;

    switch (key1->kind()) {
      case TypeRepresentation::Scalar:
        return matchScalars(key1->asScalar(), key2->asScalar());

      case TypeRepresentation::Reference:
        return matchReferences(key1->asReference(), key2->asReference());

      case TypeRepresentation::X4:
        return matchX4s(key1->asX4(), key2->asX4());

      case TypeRepresentation::Struct:
        return matchStructs(key1->asStruct(), key2->asStruct());

      case TypeRepresentation::SizedArray:
        return matchSizedArrays(key1->asSizedArray(),
                                key2->asSizedArray());

      case TypeRepresentation::UnsizedArray:
        return matchUnsizedArrays(key1->asUnsizedArray(),
                                  key2->asUnsizedArray());
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

bool
TypeRepresentationHasher::matchScalars(ScalarTypeRepresentation *key1,
                                       ScalarTypeRepresentation *key2)
{
    return key1->type() == key2->type();
}

bool
TypeRepresentationHasher::matchReferences(ReferenceTypeRepresentation *key1,
                                          ReferenceTypeRepresentation *key2)
{
    return key1->type() == key2->type();
}

bool
TypeRepresentationHasher::matchX4s(X4TypeRepresentation *key1,
                                   X4TypeRepresentation *key2)
{
    return key1->type() == key2->type();
}

bool
TypeRepresentationHasher::matchStructs(StructTypeRepresentation *key1,
                                       StructTypeRepresentation *key2)
{
    if (key1->fieldCount() != key2->fieldCount())
        return false;

    for (size_t i = 0; i < key1->fieldCount(); i++) {
        if (key1->field(i).propertyName != key2->field(i).propertyName)
            return false;

        if (key1->field(i).typeRepr != key2->field(i).typeRepr)
            return false;
    }

    return true;
}

bool
TypeRepresentationHasher::matchSizedArrays(SizedArrayTypeRepresentation *key1,
                                           SizedArrayTypeRepresentation *key2)
{
    // We assume that these pointers have been canonicalized:
    return key1->element() == key2->element() &&
           key1->length() == key2->length();
}

bool
TypeRepresentationHasher::matchUnsizedArrays(UnsizedArrayTypeRepresentation *key1,
                                             UnsizedArrayTypeRepresentation *key2)
{
    // We assume that these pointers have been canonicalized:
    return key1->element() == key2->element();
}

HashNumber
TypeRepresentationHasher::hash(TypeRepresentation *key) {
    switch (key->kind()) {
      case TypeRepresentation::Scalar:
        return hashScalar(key->asScalar());

      case TypeRepresentation::Reference:
        return hashReference(key->asReference());

      case TypeRepresentation::X4:
        return hashX4(key->asX4());

      case TypeRepresentation::Struct:
        return hashStruct(key->asStruct());

      case TypeRepresentation::UnsizedArray:
        return hashUnsizedArray(key->asUnsizedArray());

      case TypeRepresentation::SizedArray:
        return hashSizedArray(key->asSizedArray());
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

HashNumber
TypeRepresentationHasher::hashScalar(ScalarTypeRepresentation *key)
{
    return HashGeneric(key->kind(), key->type());
}

HashNumber
TypeRepresentationHasher::hashReference(ReferenceTypeRepresentation *key)
{
    return HashGeneric(key->kind(), key->type());
}

HashNumber
TypeRepresentationHasher::hashX4(X4TypeRepresentation *key)
{
    return HashGeneric(key->kind(), key->type());
}

HashNumber
TypeRepresentationHasher::hashStruct(StructTypeRepresentation *key)
{
    HashNumber hash = HashGeneric(key->kind());
    for (HashNumber i = 0; i < key->fieldCount(); i++) {
        hash = AddToHash(hash, key->field(i).propertyName.get());
        hash = AddToHash(hash, key->field(i).typeRepr);
    }
    return hash;
}

HashNumber
TypeRepresentationHasher::hashSizedArray(SizedArrayTypeRepresentation *key)
{
    return HashGeneric(key->kind(), key->element(), key->length());
}

HashNumber
TypeRepresentationHasher::hashUnsizedArray(UnsizedArrayTypeRepresentation *key)
{
    return HashGeneric(key->kind(), key->element());
}

///////////////////////////////////////////////////////////////////////////
// Constructors

TypeRepresentation::TypeRepresentation(Kind kind, bool opaque)
  : kind_(kind),
    opaque_(opaque)
{}

SizedTypeRepresentation::SizedTypeRepresentation(Kind kind, bool opaque,
                                                 size_t size, size_t align)
  : TypeRepresentation(kind, opaque),
    size_(size),
    alignment_(align)
{}

static size_t ScalarSizes[] = {
#define SCALAR_SIZE(_kind, _type, _name)                        \
    sizeof(_type),
    JS_FOR_EACH_SCALAR_TYPE_REPR(SCALAR_SIZE) 0
#undef SCALAR_SIZE
};

ScalarTypeRepresentation::ScalarTypeRepresentation(Type type)
  : SizedTypeRepresentation(Scalar, false, ScalarSizes[type], ScalarSizes[type]),
    type_(type)
{
}

static size_t X4Sizes[] = {
#define X4_SIZE(_kind, _type, _name)                        \
    sizeof(_type) * 4,
    JS_FOR_EACH_X4_TYPE_REPR(X4_SIZE) 0
#undef X4_SIZE
};

X4TypeRepresentation::X4TypeRepresentation(Type type)
  : SizedTypeRepresentation(X4, false, X4Sizes[type], X4Sizes[type]),
    type_(type)
{
}

ReferenceTypeRepresentation::ReferenceTypeRepresentation(Type type)
  : SizedTypeRepresentation(Reference, true, 0, 1),
    type_(type)
{
    switch (type) {
      case TYPE_ANY:
        size_ = sizeof(js::HeapValue);
        alignment_ = MOZ_ALIGNOF(js::HeapValue);
        break;

      case TYPE_OBJECT:
      case TYPE_STRING:
        size_ = sizeof(js::HeapPtrObject);
        alignment_ = MOZ_ALIGNOF(js::HeapPtrObject);
        break;
    }
}

SizedArrayTypeRepresentation::SizedArrayTypeRepresentation(SizedTypeRepresentation *element,
                                                           size_t length)
  : SizedTypeRepresentation(SizedArray, element->opaque(),
                            element->size() * length, element->alignment()),
    element_(element),
    length_(length)
{
}

UnsizedArrayTypeRepresentation::UnsizedArrayTypeRepresentation(SizedTypeRepresentation *element)
  : TypeRepresentation(UnsizedArray, element->opaque()),
    element_(element)
{
}

static inline size_t alignTo(size_t address, size_t align) {
    JS_ASSERT(IsPowerOfTwo(align));
    return (address + align - 1) & -align;
}

StructField::StructField(size_t index,
                         PropertyName *propertyName,
                         SizedTypeRepresentation *typeRepr,
                         size_t offset)
  : index(index),
    propertyName(propertyName),
    typeRepr(typeRepr),
    offset(offset)
{}

StructTypeRepresentation::StructTypeRepresentation()
  : SizedTypeRepresentation(Struct, false, 0, 1),
    fieldCount_(0) // see ::init() below!
{
    // note: size_, alignment_, and opaque_ are computed in ::init() below
}

bool
StructTypeRepresentation::init(JSContext *cx,
                               AutoPropertyNameVector &names,
                               AutoObjectVector &typeReprOwners)
{
    JS_ASSERT(names.length() == typeReprOwners.length());
    fieldCount_ = names.length();

    // We compute alignment into the field `align_` directly in the
    // loop below, but not `size_` because we have to very careful
    // about overflow. For now, we always use a uint32_t for
    // consistency across build environments.
    uint32_t totalSize = 0;

    // These will be adjusted in the loop below:
    alignment_ = 1;
    opaque_ = false;

    for (size_t i = 0; i < names.length(); i++) {
        SizedTypeRepresentation *fieldTypeRepr =
            fromOwnerObject(*typeReprOwners[i])->asSized();

        if (fieldTypeRepr->opaque())
            opaque_ = true;

        uint32_t alignedSize = alignTo(totalSize, fieldTypeRepr->alignment());
        if (alignedSize < totalSize) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return false;
        }

        new(fields() + i) StructField(i, names[i],
                                      fieldTypeRepr, alignedSize);
        alignment_ = js::Max(alignment_, fieldTypeRepr->alignment());

        uint32_t incrementedSize = alignedSize + fieldTypeRepr->size();
        if (incrementedSize < alignedSize) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return false;
        }

        totalSize = incrementedSize;
    }

    uint32_t alignedSize = alignTo(totalSize, alignment_);
    if (alignedSize < totalSize) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_TOO_BIG);
        return false;
    }

    size_ = alignedSize;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Interning

JSObject *
TypeRepresentation::addToTableOrFree(JSContext *cx,
                                     TypeRepresentationHash::AddPtr &p)
{
    JS_ASSERT(!ownerObject_);
    Rooted<GlobalObject*> global(cx, cx->global());
    JSCompartment *comp = cx->compartment();

    // First, try to create the typed object to associate with this
    // type representation. Since nothing is in the table yet, if this
    // fails we can just return and pretend this whole endeavor was
    // just a bad dream.
    RootedObject proto(cx);
    const Class *clasp;
    if (!global->getTypedObjectModule().getSuitableClaspAndProto(cx, kind(),
                                                                 &clasp, &proto))
    {
        return nullptr;
    }
    RootedTypeObject typeObject(cx, comp->types.newTypeObject(cx, clasp, proto));
    if (!typeObject)
        return nullptr;

    // Next, attempt to add the type representation to the table.
    if (!comp->typeReprs.relookupOrAdd(p, this, this)) {
        js_ReportOutOfMemory(cx);
        js_free(this); // do not finalize, not present in the table
        return nullptr;
    }

    // Now that the object is in the table, try to make the owner
    // object. If this succeeds, then the owner will remove from the
    // table once it is finalized. Otherwise, if this fails, we must
    // remove ourselves from the table ourselves and report an error.
    RootedObject ownerObject(cx,
        NewBuiltinClassInstance(cx,
                                &class_,
                                gc::GetGCObjectKind(&class_)));
    if (!ownerObject) {
        comp->typeReprs.remove(this);
        js_free(this);
        return nullptr;
    }

    ownerObject->setPrivate(this);

    // Assign the various reserved slots:
    ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_KIND, Int32Value(kind()));

    if (isSized()) {
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_SIZE,
                                      Int32Value(asSized()->size()));
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_ALIGNMENT,
                                      Int32Value(asSized()->alignment()));
    }

    switch (kind()) {
      case UnsizedArray:
        break;

      case SizedArray:
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_LENGTH,
                                      Int32Value(asSizedArray()->length()));
        break;

      case Scalar:
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_TYPE,
                                      Int32Value(asScalar()->type()));
        break;

      case Reference:
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_TYPE,
                                      Int32Value(asReference()->type()));
        break;

      case X4:
        ownerObject->initReservedSlot(JS_TYPEREPR_SLOT_TYPE,
                                      Int32Value(asX4()->type()));
        break;

      case Struct:
        break;
    }

    ownerObject_.init(ownerObject);
    typeObject_.init(typeObject);
    return &*ownerObject;
}

namespace js {
class TypeRepresentationHelper {
  public:
    template<typename T>
    static JSObject *CreateSimple(JSContext *cx, typename T::Type type) {
        JSCompartment *comp = cx->compartment();

        T sample(type);
        TypeRepresentationHash::AddPtr p = comp->typeReprs.lookupForAdd(&sample);
        if (p)
            return (*p)->ownerObject();

        // Note: cannot use cx->new_ because constructor is private.
        T *ptr = (T *) cx->malloc_(sizeof(T));
        if (!ptr)
            return nullptr;
        new(ptr) T(type);

        return ptr->addToTableOrFree(cx, p);
    }
};
} // namespace js

/*static*/
JSObject *
ScalarTypeRepresentation::Create(JSContext *cx,
                                 ScalarTypeRepresentation::Type type)
{
    return TypeRepresentationHelper::CreateSimple<ScalarTypeRepresentation>(cx, type);
}

/*static*/
JSObject *
X4TypeRepresentation::Create(JSContext *cx,
                             X4TypeRepresentation::Type type)
{
    return TypeRepresentationHelper::CreateSimple<X4TypeRepresentation>(cx, type);
}

/*static*/
JSObject *
ReferenceTypeRepresentation::Create(JSContext *cx,
                                    ReferenceTypeRepresentation::Type type)
{
    JSCompartment *comp = cx->compartment();

    ReferenceTypeRepresentation sample(type);
    TypeRepresentationHash::AddPtr p = comp->typeReprs.lookupForAdd(&sample);
    if (p)
        return (*p)->ownerObject();

    // Note: cannot use cx->new_ because constructor is private.
    ReferenceTypeRepresentation *ptr =
        (ReferenceTypeRepresentation *) cx->malloc_(
            sizeof(ReferenceTypeRepresentation));
    if (!ptr)
        return nullptr;
    new(ptr) ReferenceTypeRepresentation(type);

    return ptr->addToTableOrFree(cx, p);
}

/*static*/
JSObject *
SizedArrayTypeRepresentation::Create(JSContext *cx,
                                     SizedTypeRepresentation *element,
                                     size_t length)
{
    JSCompartment *comp = cx->compartment();

    // Overly conservative, since we are using `size_t` to represent
    // size, but `SafeMul` operators on `int32_t` types. Still, it
    // should be good enough for now.
    int32_t temp;
    if (!SafeMul(element->size(), length, &temp)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_TOO_BIG);
        return nullptr;
    }

    SizedArrayTypeRepresentation sample(element, length);
    TypeRepresentationHash::AddPtr p = comp->typeReprs.lookupForAdd(&sample);
    if (p)
        return (*p)->ownerObject();

    // Note: cannot use cx->new_ because constructor is private.
    SizedArrayTypeRepresentation *ptr =
        (SizedArrayTypeRepresentation *) cx->malloc_(
            sizeof(SizedArrayTypeRepresentation));
    if (!ptr)
        return nullptr;
    new(ptr) SizedArrayTypeRepresentation(element, length);

    return ptr->addToTableOrFree(cx, p);
}


/*static*/
JSObject *
UnsizedArrayTypeRepresentation::Create(JSContext *cx,
                                       SizedTypeRepresentation *element)
{
    JSCompartment *comp = cx->compartment();

    UnsizedArrayTypeRepresentation sample(element);
    TypeRepresentationHash::AddPtr p = comp->typeReprs.lookupForAdd(&sample);
    if (p)
        return (*p)->ownerObject();

    // Note: cannot use cx->new_ because constructor is private.
    UnsizedArrayTypeRepresentation *ptr =
        (UnsizedArrayTypeRepresentation *) cx->malloc_(
            sizeof(UnsizedArrayTypeRepresentation));
    if (!ptr)
        return nullptr;
    new(ptr) UnsizedArrayTypeRepresentation(element);

    return ptr->addToTableOrFree(cx, p);
}

/*static*/
JSObject *
StructTypeRepresentation::Create(JSContext *cx,
                                 AutoPropertyNameVector &names,
                                 AutoObjectVector &typeReprOwners)
{
    size_t count = names.length();
    JSCompartment *comp = cx->compartment();

    // Note: cannot use cx->new_ because constructor is private.
    size_t size = sizeof(StructTypeRepresentation) + count * sizeof(StructField);
    StructTypeRepresentation *ptr =
        (StructTypeRepresentation *) cx->malloc_(size);
    new(ptr) StructTypeRepresentation();
    if (!ptr->init(cx, names, typeReprOwners))
        return nullptr;

    TypeRepresentationHash::AddPtr p = comp->typeReprs.lookupForAdd(ptr);
    if (p) {
        js_free(ptr); // do not finalize, not present in the table
        return (*p)->ownerObject();
    }

    return ptr->addToTableOrFree(cx, p);
}

///////////////////////////////////////////////////////////////////////////
// Tracing

void
TypeRepresentation::mark(JSTracer *trace)
{
    // Push our owner object onto the mark stack. When our owner
    // object's trace callback is called, we will trace its
    // contents. This is the typical scheme for marking objects.  See
    // gc/Marking.cpp for more details.
    gc::MarkObject(trace, &ownerObject_, "typeRepresentation_ownerObject");
    gc::MarkTypeObject(trace, &typeObject_, "typeRepresentation_typeObject");
}

/*static*/ void
TypeRepresentation::obj_trace(JSTracer *trace, JSObject *object)
{
    fromOwnerObject(*object)->traceFields(trace);
}

void
TypeRepresentation::traceFields(JSTracer *trace)
{
    mark(trace); // don't forget to mark the self-reference here!

    switch (kind()) {
      case Scalar:
      case Reference:
      case X4:
        break;

      case Struct:
        asStruct()->traceStructFields(trace);
        break;

      case SizedArray:
        asSizedArray()->traceSizedArrayFields(trace);
        break;

      case UnsizedArray:
        asUnsizedArray()->traceUnsizedArrayFields(trace);
        break;
    }
}

void
StructTypeRepresentation::traceStructFields(JSTracer *trace)
{
    for (size_t i = 0; i < fieldCount(); i++) {
        gc::MarkString(trace, &fields()[i].propertyName, "typerepr_field_propertyName");
        fields()[i].typeRepr->mark(trace);
    }
}

void
SizedArrayTypeRepresentation::traceSizedArrayFields(JSTracer *trace)
{
    this->mark(trace);
    element_->mark(trace);
}

void
UnsizedArrayTypeRepresentation::traceUnsizedArrayFields(JSTracer *trace)
{
    this->mark(trace);
    element_->mark(trace);
}

///////////////////////////////////////////////////////////////////////////
// Finalization

/*static*/ void
TypeRepresentation::obj_finalize(js::FreeOp *fop, JSObject *object)
{
    JSCompartment *comp = object->compartment();
    TypeRepresentation *typeRepr = fromOwnerObject(*object);
    comp->typeReprs.remove(typeRepr);
    js_free(typeRepr);
}

///////////////////////////////////////////////////////////////////////////
// To string

bool
TypeRepresentation::appendString(JSContext *cx, StringBuffer &contents)
{
    switch (kind()) {
      case Scalar:
        return asScalar()->appendStringScalar(cx, contents);

      case Reference:
        return asReference()->appendStringReference(cx, contents);

      case X4:
        return asX4()->appendStringX4(cx, contents);

      case SizedArray:
        return asSizedArray()->appendStringSizedArray(cx, contents);

      case UnsizedArray:
        return asUnsizedArray()->appendStringUnsizedArray(cx, contents);

      case Struct:
        return asStruct()->appendStringStruct(cx, contents);
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
    return false;
}

/*static*/ const char *
ScalarTypeRepresentation::typeName(Type type)
{
    switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_) \
        case constant_: return #name_;
        JS_FOR_EACH_SCALAR_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
ScalarTypeRepresentation::appendStringScalar(JSContext *cx, StringBuffer &contents)
{
    switch (type()) {
#define NUMERIC_TYPE_APPEND_STRING(constant_, type_, name_)                   \
        case constant_: return contents.append(#name_);
        JS_FOR_EACH_SCALAR_TYPE_REPR(NUMERIC_TYPE_APPEND_STRING)
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

/*static*/ const char *
ReferenceTypeRepresentation::typeName(Type type)
{
    switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_) \
        case constant_: return #name_;
        JS_FOR_EACH_REFERENCE_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
ReferenceTypeRepresentation::appendStringReference(JSContext *cx, StringBuffer &contents)
{
    switch (type()) {
#define NUMERIC_TYPE_APPEND_STRING(constant_, type_, name_)                   \
        case constant_: return contents.append(#name_);
        JS_FOR_EACH_REFERENCE_TYPE_REPR(NUMERIC_TYPE_APPEND_STRING)
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
X4TypeRepresentation::appendStringX4(JSContext *cx, StringBuffer &contents)
{
    switch (type()) {
      case TYPE_FLOAT32:
        return contents.append("float32x4");
      case TYPE_INT32:
        return contents.append("int32x4");
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
SizedArrayTypeRepresentation::appendStringSizedArray(JSContext *cx, StringBuffer &contents)
{
    SizedTypeRepresentation *elementType = element();
    while (elementType->isSizedArray())
        elementType = elementType->asSizedArray()->element();
    if (!elementType->appendString(cx, contents))
        return false;

    contents.append(".array(");
    SizedArrayTypeRepresentation *arrayType = this;
    while (arrayType != NULL) {
        if (!NumberValueToStringBuffer(cx, NumberValue(length()), contents))
            return false;

        if (arrayType->element()->isSizedArray()) {
            if (!contents.append(","))
                return false;
            arrayType = arrayType->element()->asSizedArray();
        } else {
            break;
        }
    }

    if (!contents.append(")"))
        return false;

    return true;
}

bool
UnsizedArrayTypeRepresentation::appendStringUnsizedArray(JSContext *cx, StringBuffer &contents)
{
    if (!element()->appendString(cx, contents))
        return false;

    if (!contents.append(".array()"))
        return false;

    return true;
}

bool
StructTypeRepresentation::appendStringStruct(JSContext *cx, StringBuffer &contents)
{
    if (!contents.append("StructType({"))
        return false;

    for (size_t i = 0; i < fieldCount(); i++) {
        const StructField &fld = field(i);

        if (i > 0)
            contents.append(", ");

        RootedString idString(cx, fld.propertyName);
        if (!idString)
            return false;

        if (!contents.append(idString))
            return false;

        if (!contents.append(": "))
            return false;

        if (!fld.typeRepr->appendString(cx, contents))
            return false;
    }

    if (!contents.append("})"))
        return false;

    return true;
}

///////////////////////////////////////////////////////////////////////////
// Walking memory

template<typename V>
static void
visitReferences(SizedTypeRepresentation *repr,
                uint8_t *mem,
                V& visitor)
{
    if (repr->transparent())
        return;

    switch (repr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::X4:
        return;

      case TypeRepresentation::Reference:
        visitor.visitReference(repr->asReference(), mem);
        return;

      case TypeRepresentation::SizedArray:
      {
        SizedArrayTypeRepresentation *arrayRepr = repr->asSizedArray();
        SizedTypeRepresentation *elementRepr = arrayRepr->element();
        for (size_t i = 0; i < arrayRepr->length(); i++) {
            visitReferences(elementRepr, mem, visitor);
            mem += elementRepr->size();
        }
        return;
      }

      case TypeRepresentation::UnsizedArray:
      {
        MOZ_ASSUME_UNREACHABLE("Only Sized Type representations");
      }

      case TypeRepresentation::Struct:
      {
        StructTypeRepresentation *structRepr = repr->asStruct();
        for (size_t i = 0; i < structRepr->fieldCount(); i++) {
            const StructField &f = structRepr->field(i);
            visitReferences(f.typeRepr, mem + f.offset, visitor);
        }
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid type repr kind");
}

///////////////////////////////////////////////////////////////////////////
// Initializing instances

namespace js {
class MemoryInitVisitor {
    const JSRuntime *rt_;

  public:
    MemoryInitVisitor(const JSRuntime *rt)
      : rt_(rt)
    {}

    void visitReference(ReferenceTypeRepresentation *repr, uint8_t *mem);
};
} // namespace js

void
js::MemoryInitVisitor::visitReference(ReferenceTypeRepresentation *repr, uint8_t *mem)
{
    switch (repr->type()) {
      case ReferenceTypeRepresentation::TYPE_ANY:
      {
        js::HeapValue *heapValue = reinterpret_cast<js::HeapValue *>(mem);
        heapValue->init(UndefinedValue());
        return;
      }

      case ReferenceTypeRepresentation::TYPE_OBJECT:
      {
        js::HeapPtrObject *objectPtr =
            reinterpret_cast<js::HeapPtrObject *>(mem);
        objectPtr->init(nullptr);
        return;
      }

      case ReferenceTypeRepresentation::TYPE_STRING:
      {
        js::HeapPtrString *stringPtr =
            reinterpret_cast<js::HeapPtrString *>(mem);
        stringPtr->init(rt_->emptyString);
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

void
SizedTypeRepresentation::initInstance(const JSRuntime *rt,
                                      uint8_t *mem,
                                      size_t length)
{
    MemoryInitVisitor visitor(rt);

    // Initialize the 0th instance
    memset(mem, 0, size());
    if (opaque())
        visitReferences(this, mem, visitor);

    // Stamp out N copies of later instances
    uint8_t *target = mem;
    for (size_t i = 1; i < length; i++) {
        target += size();
        memcpy(target, mem, size());
    }
}

///////////////////////////////////////////////////////////////////////////
// Tracing instances

namespace js {
class MemoryTracingVisitor {
    JSTracer *trace_;

  public:

    MemoryTracingVisitor(JSTracer *trace)
      : trace_(trace)
    {}

    void visitReference(ReferenceTypeRepresentation *repr, uint8_t *mem);
};
} // namespace js

void
js::MemoryTracingVisitor::visitReference(ReferenceTypeRepresentation *repr, uint8_t *mem)
{
    switch (repr->type()) {
      case ReferenceTypeRepresentation::TYPE_ANY:
      {
        js::HeapValue *heapValue = reinterpret_cast<js::HeapValue *>(mem);
        gc::MarkValue(trace_, heapValue, "reference-val");
        return;
      }

      case ReferenceTypeRepresentation::TYPE_OBJECT:
      {
        js::HeapPtrObject *objectPtr =
            reinterpret_cast<js::HeapPtrObject *>(mem);
        if (*objectPtr)
            gc::MarkObject(trace_, objectPtr, "reference-obj");
        return;
      }

      case ReferenceTypeRepresentation::TYPE_STRING:
      {
        js::HeapPtrString *stringPtr =
            reinterpret_cast<js::HeapPtrString *>(mem);
        if (*stringPtr)
            gc::MarkString(trace_, stringPtr, "reference-str");
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

void
SizedTypeRepresentation::traceInstance(JSTracer *trace,
                                       uint8_t *mem,
                                       size_t length)
{
    MemoryTracingVisitor visitor(trace);

    for (size_t i = 0; i < length; i++) {
        visitReferences(this, mem, visitor);
        mem += size();
    }
}

///////////////////////////////////////////////////////////////////////////
// Misc

const StructField *
StructTypeRepresentation::fieldNamed(jsid id) const
{
    if (!JSID_IS_ATOM(id))
        return nullptr;

    uint32_t unused;
    JSAtom *atom = JSID_TO_ATOM(id);
    if (atom->isIndex(&unused))
        return nullptr;

    PropertyName *name = atom->asPropertyName();

    for (size_t i = 0; i < fieldCount(); i++) {
        if (field(i).propertyName.get() == name)
            return &field(i);
    }
    return nullptr;
}

/*static*/ bool
TypeRepresentation::isOwnerObject(JSObject &obj)
{
    return obj.getClass() == &class_;
}

/*static*/ TypeRepresentation *
TypeRepresentation::fromOwnerObject(JSObject &obj)
{
    JS_ASSERT(obj.getClass() == &class_);
    return (TypeRepresentation*) obj.getPrivate();
}


