/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineIC_h
#define jit_BaselineIC_h

#include "mozilla/Assertions.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsopcode.h"

#include "builtin/TypedObject.h"
#include "gc/Barrier.h"
#include "jit/BaselineICList.h"
#include "jit/BaselineJIT.h"
#include "jit/SharedIC.h"
#include "jit/SharedICRegisters.h"
#include "js/GCVector.h"
#include "vm/ArrayObject.h"
#include "vm/UnboxedObject.h"

namespace js {
namespace jit {

// WarmUpCounter_Fallback

// A WarmUpCounter IC chain has only the fallback stub.
class ICWarmUpCounter_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICWarmUpCounter_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::WarmUpCounter_Fallback, stubCode)
    { }

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::WarmUpCounter_Fallback, Engine::Baseline)
        { }

        ICWarmUpCounter_Fallback* getStub(ICStubSpace* space) {
            return newStub<ICWarmUpCounter_Fallback>(space, getStubCode());
        }
    };
};


// TypeUpdate

extern const VMFunction DoTypeUpdateFallbackInfo;

// The TypeUpdate fallback is not a regular fallback, since it just
// forwards to a different entry point in the main fallback stub.
class ICTypeUpdate_Fallback : public ICStub
{
    friend class ICStubSpace;

    explicit ICTypeUpdate_Fallback(JitCode* stubCode)
      : ICStub(ICStub::TypeUpdate_Fallback, stubCode)
    {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::TypeUpdate_Fallback, Engine::Baseline)
        { }

        ICTypeUpdate_Fallback* getStub(ICStubSpace* space) {
            return newStub<ICTypeUpdate_Fallback>(space, getStubCode());
        }
    };
};

class ICTypeUpdate_PrimitiveSet : public TypeCheckPrimitiveSetStub
{
    friend class ICStubSpace;

    ICTypeUpdate_PrimitiveSet(JitCode* stubCode, uint16_t flags)
        : TypeCheckPrimitiveSetStub(TypeUpdate_PrimitiveSet, stubCode, flags)
    {}

  public:
    class Compiler : public TypeCheckPrimitiveSetStub::Compiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, ICTypeUpdate_PrimitiveSet* existingStub, JSValueType type)
          : TypeCheckPrimitiveSetStub::Compiler(cx, TypeUpdate_PrimitiveSet,
                                                Engine::Baseline, existingStub, type)
        {}

        ICTypeUpdate_PrimitiveSet* updateStub() {
            TypeCheckPrimitiveSetStub* stub =
                this->TypeCheckPrimitiveSetStub::Compiler::updateStub();
            if (!stub)
                return nullptr;
            return stub->toUpdateStub();
        }

        ICTypeUpdate_PrimitiveSet* getStub(ICStubSpace* space) {
            MOZ_ASSERT(!existingStub_);
            return newStub<ICTypeUpdate_PrimitiveSet>(space, getStubCode(), flags_);
        }
    };
};

// Type update stub to handle a singleton object.
class ICTypeUpdate_SingleObject : public ICStub
{
    friend class ICStubSpace;

    GCPtrObject obj_;

    ICTypeUpdate_SingleObject(JitCode* stubCode, JSObject* obj);

  public:
    GCPtrObject& object() {
        return obj_;
    }

    static size_t offsetOfObject() {
        return offsetof(ICTypeUpdate_SingleObject, obj_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObject obj_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, HandleObject obj)
          : ICStubCompiler(cx, TypeUpdate_SingleObject, Engine::Baseline),
            obj_(obj)
        { }

        ICTypeUpdate_SingleObject* getStub(ICStubSpace* space) {
            return newStub<ICTypeUpdate_SingleObject>(space, getStubCode(), obj_);
        }
    };
};

// Type update stub to handle a single ObjectGroup.
class ICTypeUpdate_ObjectGroup : public ICStub
{
    friend class ICStubSpace;

    GCPtrObjectGroup group_;

    ICTypeUpdate_ObjectGroup(JitCode* stubCode, ObjectGroup* group);

  public:
    GCPtrObjectGroup& group() {
        return group_;
    }

    static size_t offsetOfGroup() {
        return offsetof(ICTypeUpdate_ObjectGroup, group_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        HandleObjectGroup group_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, HandleObjectGroup group)
          : ICStubCompiler(cx, TypeUpdate_ObjectGroup, Engine::Baseline),
            group_(group)
        { }

        ICTypeUpdate_ObjectGroup* getStub(ICStubSpace* space) {
            return newStub<ICTypeUpdate_ObjectGroup>(space, getStubCode(), group_);
        }
    };
};

// ToBool
//      JSOP_IFNE

class ICToBool_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICToBool_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::ToBool_Fallback, stubCode) {}

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_Fallback, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_Fallback>(space, getStubCode());
        }
    };
};

class ICToBool_Int32 : public ICStub
{
    friend class ICStubSpace;

    explicit ICToBool_Int32(JitCode* stubCode)
      : ICStub(ICStub::ToBool_Int32, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_Int32, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_Int32>(space, getStubCode());
        }
    };
};

class ICToBool_String : public ICStub
{
    friend class ICStubSpace;

    explicit ICToBool_String(JitCode* stubCode)
      : ICStub(ICStub::ToBool_String, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_String, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_String>(space, getStubCode());
        }
    };
};

class ICToBool_NullUndefined : public ICStub
{
    friend class ICStubSpace;

    explicit ICToBool_NullUndefined(JitCode* stubCode)
      : ICStub(ICStub::ToBool_NullUndefined, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_NullUndefined, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_NullUndefined>(space, getStubCode());
        }
    };
};

class ICToBool_Double : public ICStub
{
    friend class ICStubSpace;

    explicit ICToBool_Double(JitCode* stubCode)
      : ICStub(ICStub::ToBool_Double, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_Double, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_Double>(space, getStubCode());
        }
    };
};

class ICToBool_Object : public ICStub
{
    friend class ICStubSpace;

    explicit ICToBool_Object(JitCode* stubCode)
      : ICStub(ICStub::ToBool_Object, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToBool_Object, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToBool_Object>(space, getStubCode());
        }
    };
};

// ToNumber
//     JSOP_POS

class ICToNumber_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICToNumber_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::ToNumber_Fallback, stubCode) {}

  public:
    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::ToNumber_Fallback, Engine::Baseline) {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICToNumber_Fallback>(space, getStubCode());
        }
    };
};

// GetElem
//      JSOP_GETELEM

class ICGetElem_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetElem_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetElem_Fallback, stubCode)
    { }

    static const uint16_t EXTRA_NON_NATIVE = 0x1;
    static const uint16_t EXTRA_NEGATIVE_INDEX = 0x2;
    static const uint16_t EXTRA_UNOPTIMIZABLE_ACCESS = 0x4;

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 16;

    void noteNonNativeAccess() {
        extra_ |= EXTRA_NON_NATIVE;
    }
    bool hasNonNativeAccess() const {
        return extra_ & EXTRA_NON_NATIVE;
    }

    void noteNegativeIndex() {
        extra_ |= EXTRA_NEGATIVE_INDEX;
    }
    bool hasNegativeIndex() const {
        return extra_ & EXTRA_NEGATIVE_INDEX;
    }
    void noteUnoptimizableAccess() {
        extra_ |= EXTRA_UNOPTIMIZABLE_ACCESS;
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & EXTRA_UNOPTIMIZABLE_ACCESS;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetElem_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            ICGetElem_Fallback* stub = newStub<ICGetElem_Fallback>(space, getStubCode());
            if (!stub)
                return nullptr;
            if (!stub->initMonitoringChain(cx, space, engine_))
                return nullptr;
            return stub;
        }
    };
};

// SetElem
//      JSOP_SETELEM
//      JSOP_INITELEM

class ICSetElem_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICSetElem_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::SetElem_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    void noteArrayWriteHole() {
        extra_ = 1;
    }
    bool hasArrayWriteHole() const {
        return extra_;
    }

    // Compiler for this stub kind.
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::SetElem_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICSetElem_Fallback>(space, getStubCode());
        }
    };
};

class ICSetElem_DenseOrUnboxedArray : public ICUpdatedStub
{
    friend class ICStubSpace;

    GCPtrShape shape_; // null for unboxed arrays
    GCPtrObjectGroup group_;

    ICSetElem_DenseOrUnboxedArray(JitCode* stubCode, Shape* shape, ObjectGroup* group);

  public:
    static size_t offsetOfShape() {
        return offsetof(ICSetElem_DenseOrUnboxedArray, shape_);
    }
    static size_t offsetOfGroup() {
        return offsetof(ICSetElem_DenseOrUnboxedArray, group_);
    }

    GCPtrShape& shape() {
        return shape_;
    }
    GCPtrObjectGroup& group() {
        return group_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;
        RootedObjectGroup group_;
        JSValueType unboxedType_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(unboxedType_) << 17);
        }

        Compiler(JSContext* cx, Shape* shape, HandleObjectGroup group)
          : ICStubCompiler(cx, ICStub::SetElem_DenseOrUnboxedArray, Engine::Baseline),
            shape_(cx, shape),
            group_(cx, group),
            unboxedType_(shape
                         ? JSVAL_TYPE_MAGIC
                         : group->unboxedLayoutDontCheckGeneration().elementType())
        {}

        ICUpdatedStub* getStub(ICStubSpace* space) {
            ICSetElem_DenseOrUnboxedArray* stub =
                newStub<ICSetElem_DenseOrUnboxedArray>(space, getStubCode(), shape_, group_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return nullptr;
            return stub;
        }

        bool needsUpdateStubs() {
            return unboxedType_ == JSVAL_TYPE_MAGIC || unboxedType_ == JSVAL_TYPE_OBJECT;
        }
    };
};

template <size_t ProtoChainDepth> class ICSetElem_DenseOrUnboxedArrayAddImpl;

class ICSetElem_DenseOrUnboxedArrayAdd : public ICUpdatedStub
{
    friend class ICStubSpace;

  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 4;

  protected:
    GCPtrObjectGroup group_;

    ICSetElem_DenseOrUnboxedArrayAdd(JitCode* stubCode, ObjectGroup* group, size_t protoChainDepth);

  public:
    static size_t offsetOfGroup() {
        return offsetof(ICSetElem_DenseOrUnboxedArrayAdd, group_);
    }

    GCPtrObjectGroup& group() {
        return group_;
    }
    size_t protoChainDepth() const {
        MOZ_ASSERT(extra_ <= MAX_PROTO_CHAIN_DEPTH);
        return extra_;
    }

    template <size_t ProtoChainDepth>
    ICSetElem_DenseOrUnboxedArrayAddImpl<ProtoChainDepth>* toImplUnchecked() {
        return static_cast<ICSetElem_DenseOrUnboxedArrayAddImpl<ProtoChainDepth>*>(this);
    }

    template <size_t ProtoChainDepth>
    ICSetElem_DenseOrUnboxedArrayAddImpl<ProtoChainDepth>* toImpl() {
        MOZ_ASSERT(ProtoChainDepth == protoChainDepth());
        return toImplUnchecked<ProtoChainDepth>();
    }
};

template <size_t ProtoChainDepth>
class ICSetElem_DenseOrUnboxedArrayAddImpl : public ICSetElem_DenseOrUnboxedArrayAdd
{
    friend class ICStubSpace;

    // Note: for unboxed arrays, the first shape is null.
    static const size_t NumShapes = ProtoChainDepth + 1;
    mozilla::Array<GCPtrShape, NumShapes> shapes_;

    ICSetElem_DenseOrUnboxedArrayAddImpl(JitCode* stubCode, ObjectGroup* group,
                                         Handle<ShapeVector> shapes)
      : ICSetElem_DenseOrUnboxedArrayAdd(stubCode, group, ProtoChainDepth)
    {
        MOZ_ASSERT(shapes.length() == NumShapes);
        for (size_t i = 0; i < NumShapes; i++)
            shapes_[i].init(shapes[i]);
    }

  public:
    void traceShapes(JSTracer* trc) {
        for (size_t i = 0; i < NumShapes; i++)
            TraceNullableEdge(trc, &shapes_[i], "baseline-setelem-denseadd-stub-shape");
    }
    Shape* shape(size_t i) const {
        MOZ_ASSERT(i < NumShapes);
        return shapes_[i];
    }
    static size_t offsetOfShape(size_t idx) {
        return offsetof(ICSetElem_DenseOrUnboxedArrayAddImpl, shapes_) + idx * sizeof(GCPtrShape);
    }
};

class ICSetElemDenseOrUnboxedArrayAddCompiler : public ICStubCompiler {
    RootedObject obj_;
    size_t protoChainDepth_;
    JSValueType unboxedType_;

    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(engine_) |
              (static_cast<int32_t>(kind) << 1) |
              (static_cast<int32_t>(protoChainDepth_) << 17) |
              (static_cast<int32_t>(unboxedType_) << 20);
    }

  public:
    ICSetElemDenseOrUnboxedArrayAddCompiler(JSContext* cx, HandleObject obj, size_t protoChainDepth)
        : ICStubCompiler(cx, ICStub::SetElem_DenseOrUnboxedArrayAdd, Engine::Baseline),
          obj_(cx, obj),
          protoChainDepth_(protoChainDepth),
          unboxedType_(obj->is<UnboxedArrayObject>()
                       ? obj->as<UnboxedArrayObject>().elementType()
                       : JSVAL_TYPE_MAGIC)
    {}

    template <size_t ProtoChainDepth>
    ICUpdatedStub* getStubSpecific(ICStubSpace* space, Handle<ShapeVector> shapes);

    ICUpdatedStub* getStub(ICStubSpace* space);

    bool needsUpdateStubs() {
        return unboxedType_ == JSVAL_TYPE_MAGIC || unboxedType_ == JSVAL_TYPE_OBJECT;
    }
};

// Accesses scalar elements of a typed array or typed object.
class ICSetElem_TypedArray : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    GCPtrShape shape_;

    ICSetElem_TypedArray(JitCode* stubCode, Shape* shape, Scalar::Type type,
                         bool expectOutOfBounds);

  public:
    Scalar::Type type() const {
        return (Scalar::Type) (extra_ & 0xff);
    }

    bool expectOutOfBounds() const {
        return (extra_ >> 8) & 1;
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetElem_TypedArray, shape_);
    }

    GCPtrShape& shape() {
        return shape_;
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;
        Scalar::Type type_;
        TypedThingLayout layout_;
        bool expectOutOfBounds_;

      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(type_) << 17) |
                  (static_cast<int32_t>(layout_) << 25) |
                  (static_cast<int32_t>(expectOutOfBounds_) << 29);
        }

      public:
        Compiler(JSContext* cx, Shape* shape, Scalar::Type type, bool expectOutOfBounds)
          : ICStubCompiler(cx, ICStub::SetElem_TypedArray, Engine::Baseline),
            shape_(cx, shape),
            type_(type),
            layout_(GetTypedThingLayout(shape->getObjectClass())),
            expectOutOfBounds_(expectOutOfBounds)
        {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICSetElem_TypedArray>(space, getStubCode(), shape_, type_,
                                                 expectOutOfBounds_);
        }
    };
};

// In
//      JSOP_IN
class ICIn_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIn_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::In_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::In_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIn_Fallback>(space, getStubCode());
        }
    };
};

// Base class for In_Native and In_NativePrototype stubs.
class ICInNativeStub : public ICStub
{
    GCPtrShape shape_;
    GCPtrPropertyName name_;

  protected:
    ICInNativeStub(ICStub::Kind kind, JitCode* stubCode, HandleShape shape,
                   HandlePropertyName name);

  public:
    GCPtrShape& shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICInNativeStub, shape_);
    }

    GCPtrPropertyName& name() {
        return name_;
    }
    static size_t offsetOfName() {
        return offsetof(ICInNativeStub, name_);
    }
};

// Stub for confirming an own property on a native object.
class ICIn_Native : public ICInNativeStub
{
    friend class ICStubSpace;

    ICIn_Native(JitCode* stubCode, HandleShape shape, HandlePropertyName name)
      : ICInNativeStub(In_Native, stubCode, shape, name)
    {}
};

// Stub for confirming a property on a native object's prototype. Note that due to
// the shape teleporting optimization, we only have to guard on the object's shape
// and the holder's shape.
class ICIn_NativePrototype : public ICInNativeStub
{
    friend class ICStubSpace;

    GCPtrObject holder_;
    GCPtrShape holderShape_;

    ICIn_NativePrototype(JitCode* stubCode, HandleShape shape, HandlePropertyName name,
                         HandleObject holder, HandleShape holderShape);

  public:
    GCPtrObject& holder() {
        return holder_;
    }
    GCPtrShape& holderShape() {
        return holderShape_;
    }
    static size_t offsetOfHolder() {
        return offsetof(ICIn_NativePrototype, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICIn_NativePrototype, holderShape_);
    }
};

// Compiler for In_Native and In_NativePrototype stubs.
class ICInNativeCompiler : public ICStubCompiler
{
    RootedObject obj_;
    RootedObject holder_;
    RootedPropertyName name_;

    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

  public:
    ICInNativeCompiler(JSContext* cx, ICStub::Kind kind, HandleObject obj, HandleObject holder,
                       HandlePropertyName name)
      : ICStubCompiler(cx, kind, Engine::Baseline),
        obj_(cx, obj),
        holder_(cx, holder),
        name_(cx, name)
    {}

    ICStub* getStub(ICStubSpace* space) {
        RootedShape shape(cx, obj_->as<NativeObject>().lastProperty());
        if (kind == ICStub::In_Native) {
            MOZ_ASSERT(obj_ == holder_);
            return newStub<ICIn_Native>(space, getStubCode(), shape, name_);
        }

        MOZ_ASSERT(obj_ != holder_);
        MOZ_ASSERT(kind == ICStub::In_NativePrototype);
        RootedShape holderShape(cx, holder_->as<NativeObject>().lastProperty());
        return newStub<ICIn_NativePrototype>(space, getStubCode(), shape, name_, holder_,
                                             holderShape);
    }
};

template <size_t ProtoChainDepth> class ICIn_NativeDoesNotExistImpl;

class ICIn_NativeDoesNotExist : public ICStub
{
    friend class ICStubSpace;

    GCPtrPropertyName name_;

  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 8;

  protected:
    ICIn_NativeDoesNotExist(JitCode* stubCode, size_t protoChainDepth,
                            HandlePropertyName name);

  public:
    size_t protoChainDepth() const {
        MOZ_ASSERT(extra_ <= MAX_PROTO_CHAIN_DEPTH);
        return extra_;
    }
    GCPtrPropertyName& name() {
        return name_;
    }

    template <size_t ProtoChainDepth>
    ICIn_NativeDoesNotExistImpl<ProtoChainDepth>* toImpl() {
        MOZ_ASSERT(ProtoChainDepth == protoChainDepth());
        return static_cast<ICIn_NativeDoesNotExistImpl<ProtoChainDepth>*>(this);
    }

    static size_t offsetOfShape(size_t idx);
    static size_t offsetOfName() {
        return offsetof(ICIn_NativeDoesNotExist, name_);
    }
};

template <size_t ProtoChainDepth>
class ICIn_NativeDoesNotExistImpl : public ICIn_NativeDoesNotExist
{
    friend class ICStubSpace;

  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 8;
    static const size_t NumShapes = ProtoChainDepth + 1;

  private:
    mozilla::Array<GCPtrShape, NumShapes> shapes_;

    ICIn_NativeDoesNotExistImpl(JitCode* stubCode, Handle<ShapeVector> shapes,
                                HandlePropertyName name);

  public:
    void traceShapes(JSTracer* trc) {
        for (size_t i = 0; i < NumShapes; i++)
            TraceEdge(trc, &shapes_[i], "baseline-innativedoesnotexist-stub-shape");
    }

    static size_t offsetOfShape(size_t idx) {
        return offsetof(ICIn_NativeDoesNotExistImpl, shapes_) + (idx * sizeof(GCPtrShape));
    }
};

class ICInNativeDoesNotExistCompiler : public ICStubCompiler
{
    RootedObject obj_;
    RootedPropertyName name_;
    size_t protoChainDepth_;

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(engine_) |
              (static_cast<int32_t>(kind) << 1) |
              (static_cast<int32_t>(protoChainDepth_) << 17);
    }

    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

  public:
    ICInNativeDoesNotExistCompiler(JSContext* cx, HandleObject obj, HandlePropertyName name,
                                   size_t protoChainDepth);

    template <size_t ProtoChainDepth>
    ICStub* getStubSpecific(ICStubSpace* space, Handle<ShapeVector> shapes) {
        return newStub<ICIn_NativeDoesNotExistImpl<ProtoChainDepth>>(space, getStubCode(), shapes,
                                                                     name_);}

    ICStub* getStub(ICStubSpace* space);
};

class ICIn_Dense : public ICStub
{
    friend class ICStubSpace;

    GCPtrShape shape_;

    ICIn_Dense(JitCode* stubCode, HandleShape shape);

  public:
    GCPtrShape& shape() {
        return shape_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICIn_Dense, shape_);
    }

    class Compiler : public ICStubCompiler {
      RootedShape shape_;

      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, Shape* shape)
          : ICStubCompiler(cx, ICStub::In_Dense, Engine::Baseline),
            shape_(cx, shape)
        {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIn_Dense>(space, getStubCode(), shape_);
        }
    };
};

// GetName
//      JSOP_GETNAME
//      JSOP_GETGNAME
class ICGetName_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetName_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetName_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;
    static const size_t UNOPTIMIZABLE_ACCESS_BIT = 0;

    void noteUnoptimizableAccess() {
        extra_ |= (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetName_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            ICGetName_Fallback* stub = newStub<ICGetName_Fallback>(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space, engine_))
                return nullptr;
            return stub;
        }
    };
};

// BindName
//      JSOP_BINDNAME
class ICBindName_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICBindName_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::BindName_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::BindName_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICBindName_Fallback>(space, getStubCode());
        }
    };
};

// GetIntrinsic
//      JSOP_GETINTRINSIC
class ICGetIntrinsic_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;

    explicit ICGetIntrinsic_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::GetIntrinsic_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            ICGetIntrinsic_Fallback* stub =
                newStub<ICGetIntrinsic_Fallback>(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space, engine_))
                return nullptr;
            return stub;
        }
    };
};

// Stub that loads the constant result of a GETINTRINSIC operation.
class ICGetIntrinsic_Constant : public ICStub
{
    friend class ICStubSpace;

    GCPtrValue value_;

    ICGetIntrinsic_Constant(JitCode* stubCode, const Value& value);
    ~ICGetIntrinsic_Constant();

  public:
    GCPtrValue& value() {
        return value_;
    }
    static size_t offsetOfValue() {
        return offsetof(ICGetIntrinsic_Constant, value_);
    }

    class Compiler : public ICStubCompiler {
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        HandleValue value_;

      public:
        Compiler(JSContext* cx, HandleValue value)
          : ICStubCompiler(cx, ICStub::GetIntrinsic_Constant, Engine::Baseline),
            value_(value)
        {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICGetIntrinsic_Constant>(space, getStubCode(), value_);
        }
    };
};

// SetProp
//     JSOP_SETPROP
//     JSOP_SETNAME
//     JSOP_SETGNAME
//     JSOP_INITPROP

class ICSetProp_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICSetProp_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::SetProp_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    static const size_t UNOPTIMIZABLE_ACCESS_BIT = 0;
    void noteUnoptimizableAccess() {
        extra_ |= (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & (1u << UNOPTIMIZABLE_ACCESS_BIT);
    }

    class Compiler : public ICStubCompiler {
      public:
        static const int32_t BASELINE_KEY =
            (static_cast<int32_t>(Engine::Baseline)) |
            (static_cast<int32_t>(ICStub::SetProp_Fallback) << 1);

      protected:
        uint32_t returnOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);
        void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::SetProp_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICSetProp_Fallback>(space, getStubCode());
        }
    };
};

// Optimized SETPROP/SETGNAME/SETNAME stub.
class ICSetProp_Native : public ICUpdatedStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    GCPtrObjectGroup group_;
    GCPtrShape shape_;
    uint32_t offset_;

    ICSetProp_Native(JitCode* stubCode, ObjectGroup* group, Shape* shape, uint32_t offset);

  public:
    GCPtrObjectGroup& group() {
        return group_;
    }
    GCPtrShape& shape() {
        return shape_;
    }
    void notePreliminaryObject() {
        extra_ = 1;
    }
    bool hasPreliminaryObject() const {
        return extra_;
    }
    static size_t offsetOfGroup() {
        return offsetof(ICSetProp_Native, group_);
    }
    static size_t offsetOfShape() {
        return offsetof(ICSetProp_Native, shape_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICSetProp_Native, offset_);
    }

    class Compiler : public ICStubCompiler {
        RootedObject obj_;
        bool isFixedSlot_;
        uint32_t offset_;

      protected:
        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(isFixedSlot_) << 17) |
                  (static_cast<int32_t>(obj_->is<UnboxedPlainObject>()) << 18);
        }

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, HandleObject obj, bool isFixedSlot, uint32_t offset)
          : ICStubCompiler(cx, ICStub::SetProp_Native, Engine::Baseline),
            obj_(cx, obj),
            isFixedSlot_(isFixedSlot),
            offset_(offset)
        {}

        ICSetProp_Native* getStub(ICStubSpace* space);
    };
};


template <size_t ProtoChainDepth> class ICSetProp_NativeAddImpl;

class ICSetProp_NativeAdd : public ICUpdatedStub
{
  public:
    static const size_t MAX_PROTO_CHAIN_DEPTH = 4;

  protected: // Protected to silence Clang warning.
    GCPtrObjectGroup group_;
    GCPtrShape newShape_;
    GCPtrObjectGroup newGroup_;
    uint32_t offset_;

    ICSetProp_NativeAdd(JitCode* stubCode, ObjectGroup* group, size_t protoChainDepth,
                        Shape* newShape, ObjectGroup* newGroup, uint32_t offset);

  public:
    size_t protoChainDepth() const {
        return extra_;
    }
    GCPtrObjectGroup& group() {
        return group_;
    }
    GCPtrShape& newShape() {
        return newShape_;
    }
    GCPtrObjectGroup& newGroup() {
        return newGroup_;
    }

    template <size_t ProtoChainDepth>
    ICSetProp_NativeAddImpl<ProtoChainDepth>* toImpl() {
        MOZ_ASSERT(ProtoChainDepth == protoChainDepth());
        return static_cast<ICSetProp_NativeAddImpl<ProtoChainDepth>*>(this);
    }

    static size_t offsetOfGroup() {
        return offsetof(ICSetProp_NativeAdd, group_);
    }
    static size_t offsetOfNewShape() {
        return offsetof(ICSetProp_NativeAdd, newShape_);
    }
    static size_t offsetOfNewGroup() {
        return offsetof(ICSetProp_NativeAdd, newGroup_);
    }
    static size_t offsetOfOffset() {
        return offsetof(ICSetProp_NativeAdd, offset_);
    }
};

template <size_t ProtoChainDepth>
class ICSetProp_NativeAddImpl : public ICSetProp_NativeAdd
{
    friend class ICStubSpace;

    static const size_t NumShapes = ProtoChainDepth + 1;
    mozilla::Array<GCPtrShape, NumShapes> shapes_;

    ICSetProp_NativeAddImpl(JitCode* stubCode, ObjectGroup* group,
                            Handle<ShapeVector> shapes,
                            Shape* newShape, ObjectGroup* newGroup, uint32_t offset);

  public:
    void traceShapes(JSTracer* trc) {
        for (size_t i = 0; i < NumShapes; i++)
            TraceEdge(trc, &shapes_[i], "baseline-setpropnativeadd-stub-shape");
    }

    static size_t offsetOfShape(size_t idx) {
        return offsetof(ICSetProp_NativeAddImpl, shapes_) + (idx * sizeof(GCPtrShape));
    }
};

class ICSetPropNativeAddCompiler : public ICStubCompiler
{
    RootedObject obj_;
    RootedShape oldShape_;
    RootedObjectGroup oldGroup_;
    size_t protoChainDepth_;
    bool isFixedSlot_;
    uint32_t offset_;

  protected:
    virtual int32_t getKey() const {
        return static_cast<int32_t>(engine_) |
              (static_cast<int32_t>(kind) << 1) |
              (static_cast<int32_t>(isFixedSlot_) << 17) |
              (static_cast<int32_t>(obj_->is<UnboxedPlainObject>()) << 18) |
              (static_cast<int32_t>(protoChainDepth_) << 19);
    }

    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

  public:
    ICSetPropNativeAddCompiler(JSContext* cx, HandleObject obj,
                               HandleShape oldShape, HandleObjectGroup oldGroup,
                               size_t protoChainDepth, bool isFixedSlot, uint32_t offset);

    template <size_t ProtoChainDepth>
    ICUpdatedStub* getStubSpecific(ICStubSpace* space, Handle<ShapeVector> shapes)
    {
        RootedObjectGroup newGroup(cx, JSObject::getGroup(cx, obj_));
        if (!newGroup)
            return nullptr;

        // Only specify newGroup when the object's group changes due to the
        // object becoming fully initialized per the acquired properties
        // analysis.
        if (newGroup == oldGroup_)
            newGroup = nullptr;

        RootedShape newShape(cx);
        if (obj_->isNative())
            newShape = obj_->as<NativeObject>().lastProperty();
        else
            newShape = obj_->as<UnboxedPlainObject>().maybeExpando()->lastProperty();

        return newStub<ICSetProp_NativeAddImpl<ProtoChainDepth>>(
            space, getStubCode(), oldGroup_, shapes, newShape, newGroup, offset_);
    }

    ICUpdatedStub* getStub(ICStubSpace* space);
};

class ICSetProp_Unboxed : public ICUpdatedStub
{
    friend class ICStubSpace;

    GCPtrObjectGroup group_;
    uint32_t fieldOffset_;

    ICSetProp_Unboxed(JitCode* stubCode, ObjectGroup* group, uint32_t fieldOffset)
      : ICUpdatedStub(ICStub::SetProp_Unboxed, stubCode),
        group_(group),
        fieldOffset_(fieldOffset)
    {
        (void) fieldOffset_; // Silence clang warning
    }

  public:
    GCPtrObjectGroup& group() {
        return group_;
    }

    static size_t offsetOfGroup() {
        return offsetof(ICSetProp_Unboxed, group_);
    }
    static size_t offsetOfFieldOffset() {
        return offsetof(ICSetProp_Unboxed, fieldOffset_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedObjectGroup group_;
        uint32_t fieldOffset_;
        JSValueType fieldType_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(fieldType_) << 17);
        }

      public:
        Compiler(JSContext* cx, ObjectGroup* group, uint32_t fieldOffset,
                 JSValueType fieldType)
          : ICStubCompiler(cx, ICStub::SetProp_Unboxed, Engine::Baseline),
            group_(cx, group),
            fieldOffset_(fieldOffset),
            fieldType_(fieldType)
        {}

        ICUpdatedStub* getStub(ICStubSpace* space) {
            ICUpdatedStub* stub = newStub<ICSetProp_Unboxed>(space, getStubCode(), group_,
                                                             fieldOffset_);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return nullptr;
            return stub;
        }

        bool needsUpdateStubs() {
            return fieldType_ == JSVAL_TYPE_OBJECT;
        }
    };
};

class ICSetProp_TypedObject : public ICUpdatedStub
{
    friend class ICStubSpace;

    GCPtrShape shape_;
    GCPtrObjectGroup group_;
    uint32_t fieldOffset_;
    bool isObjectReference_;

    ICSetProp_TypedObject(JitCode* stubCode, Shape* shape, ObjectGroup* group,
                          uint32_t fieldOffset, bool isObjectReference)
      : ICUpdatedStub(ICStub::SetProp_TypedObject, stubCode),
        shape_(shape),
        group_(group),
        fieldOffset_(fieldOffset),
        isObjectReference_(isObjectReference)
    {
        (void) fieldOffset_; // Silence clang warning
    }

  public:
    GCPtrShape& shape() {
        return shape_;
    }
    GCPtrObjectGroup& group() {
        return group_;
    }
    bool isObjectReference() {
        return isObjectReference_;
    }

    static size_t offsetOfShape() {
        return offsetof(ICSetProp_TypedObject, shape_);
    }
    static size_t offsetOfGroup() {
        return offsetof(ICSetProp_TypedObject, group_);
    }
    static size_t offsetOfFieldOffset() {
        return offsetof(ICSetProp_TypedObject, fieldOffset_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedShape shape_;
        RootedObjectGroup group_;
        uint32_t fieldOffset_;
        TypedThingLayout layout_;
        Rooted<SimpleTypeDescr*> fieldDescr_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(SimpleTypeDescrKey(fieldDescr_)) << 17) |
                  (static_cast<int32_t>(layout_) << 25);
        }

      public:
        Compiler(JSContext* cx, Shape* shape, ObjectGroup* group, uint32_t fieldOffset,
                 SimpleTypeDescr* fieldDescr)
          : ICStubCompiler(cx, ICStub::SetProp_TypedObject, Engine::Baseline),
            shape_(cx, shape),
            group_(cx, group),
            fieldOffset_(fieldOffset),
            layout_(GetTypedThingLayout(shape->getObjectClass())),
            fieldDescr_(cx, fieldDescr)
        {}

        ICUpdatedStub* getStub(ICStubSpace* space) {
            bool isObjectReference =
                fieldDescr_->is<ReferenceTypeDescr>() &&
                fieldDescr_->as<ReferenceTypeDescr>().type() == ReferenceTypeDescr::TYPE_OBJECT;
            ICUpdatedStub* stub = newStub<ICSetProp_TypedObject>(space, getStubCode(), shape_,
                                                                 group_, fieldOffset_,
                                                                 isObjectReference);
            if (!stub || !stub->initUpdatingChain(cx, space))
                return nullptr;
            return stub;
        }

        bool needsUpdateStubs() {
            return fieldDescr_->is<ReferenceTypeDescr>() &&
                   fieldDescr_->as<ReferenceTypeDescr>().type() != ReferenceTypeDescr::TYPE_STRING;
        }
    };
};

// Base stub for calling a setters on a native or unboxed object.
class ICSetPropCallSetter : public ICStub
{
    friend class ICStubSpace;

  protected:
    // Shape/group of receiver object. Used for both own and proto setters.
    HeapReceiverGuard receiverGuard_;

    // Holder and holder shape. For own setters, guarding on receiverGuard_ is
    // sufficient, although Ion may use holder_ and holderShape_ even for own
    // setters. In this case holderShape_ == receiverGuard_.shape_ (isOwnSetter
    // below relies on this).
    GCPtrObject holder_;
    GCPtrShape holderShape_;

    // Function to call.
    GCPtrFunction setter_;

    // PC of call, for profiler
    uint32_t pcOffset_;

    ICSetPropCallSetter(Kind kind, JitCode* stubCode, ReceiverGuard receiverGuard,
                        JSObject* holder, Shape* holderShape, JSFunction* setter,
                        uint32_t pcOffset);

  public:
    HeapReceiverGuard& receiverGuard() {
        return receiverGuard_;
    }
    GCPtrObject& holder() {
        return holder_;
    }
    GCPtrShape& holderShape() {
        return holderShape_;
    }
    GCPtrFunction& setter() {
        return setter_;
    }

    bool isOwnSetter() const {
        MOZ_ASSERT(holder_->isNative());
        MOZ_ASSERT(holderShape_);
        return receiverGuard_.shape() == holderShape_;
    }

    static size_t offsetOfReceiverGuard() {
        return offsetof(ICSetPropCallSetter, receiverGuard_);
    }
    static size_t offsetOfHolder() {
        return offsetof(ICSetPropCallSetter, holder_);
    }
    static size_t offsetOfHolderShape() {
        return offsetof(ICSetPropCallSetter, holderShape_);
    }
    static size_t offsetOfSetter() {
        return offsetof(ICSetPropCallSetter, setter_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICSetPropCallSetter, pcOffset_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedObject receiver_;
        RootedObject holder_;
        RootedFunction setter_;
        uint32_t pcOffset_;

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (HeapReceiverGuard::keyBits(receiver_) << 17) |
                  (static_cast<int32_t>(receiver_ != holder_) << 20);
        }

      public:
        Compiler(JSContext* cx, ICStub::Kind kind, HandleObject receiver, HandleObject holder,
                 HandleFunction setter, uint32_t pcOffset)
          : ICStubCompiler(cx, kind, Engine::Baseline),
            receiver_(cx, receiver),
            holder_(cx, holder),
            setter_(cx, setter),
            pcOffset_(pcOffset)
        {
            MOZ_ASSERT(kind == ICStub::SetProp_CallScripted || kind == ICStub::SetProp_CallNative);
        }
    };
};

// Stub for calling a scripted setter on a native object.
class ICSetProp_CallScripted : public ICSetPropCallSetter
{
    friend class ICStubSpace;

  protected:
    ICSetProp_CallScripted(JitCode* stubCode, ReceiverGuard guard, JSObject* holder,
                           Shape* holderShape, JSFunction* setter, uint32_t pcOffset)
      : ICSetPropCallSetter(SetProp_CallScripted, stubCode, guard, holder, holderShape,
                            setter, pcOffset)
    {}

  public:
    static ICSetProp_CallScripted* Clone(JSContext* cx, ICStubSpace* space, ICStub*,
                                         ICSetProp_CallScripted& other);

    class Compiler : public ICSetPropCallSetter::Compiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, HandleObject obj, HandleObject holder, HandleFunction setter,
                 uint32_t pcOffset)
          : ICSetPropCallSetter::Compiler(cx, ICStub::SetProp_CallScripted,
                                          obj, holder, setter, pcOffset)
        {}

        ICStub* getStub(ICStubSpace* space) {
            ReceiverGuard guard(receiver_);
            Shape* holderShape = holder_->as<NativeObject>().lastProperty();
            return newStub<ICSetProp_CallScripted>(space, getStubCode(), guard, holder_,
                                                       holderShape, setter_, pcOffset_);
        }
    };
};

// Stub for calling a native setter on a native object.
class ICSetProp_CallNative : public ICSetPropCallSetter
{
    friend class ICStubSpace;

  protected:
    ICSetProp_CallNative(JitCode* stubCode, ReceiverGuard guard, JSObject* holder,
                         Shape* holderShape, JSFunction* setter, uint32_t pcOffset)
      : ICSetPropCallSetter(SetProp_CallNative, stubCode, guard, holder, holderShape,
                            setter, pcOffset)
    {}

  public:
    static ICSetProp_CallNative* Clone(JSContext* cx,
                                       ICStubSpace* space, ICStub*,
                                       ICSetProp_CallNative& other);

    class Compiler : public ICSetPropCallSetter::Compiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, HandleObject obj, HandleObject holder, HandleFunction setter,
                 uint32_t pcOffset)
          : ICSetPropCallSetter::Compiler(cx, ICStub::SetProp_CallNative,
                                          obj, holder, setter, pcOffset)
        {}

        ICStub* getStub(ICStubSpace* space) {
            ReceiverGuard guard(receiver_);
            Shape* holderShape = holder_->as<NativeObject>().lastProperty();
            return newStub<ICSetProp_CallNative>(space, getStubCode(), guard, holder_, holderShape,
                                                 setter_, pcOffset_);
        }
    };
};

// Call
//      JSOP_CALL
//      JSOP_FUNAPPLY
//      JSOP_FUNCALL
//      JSOP_NEW
//      JSOP_SPREADCALL
//      JSOP_SPREADNEW
//      JSOP_SPREADEVAL

class ICCallStubCompiler : public ICStubCompiler
{
  protected:
    ICCallStubCompiler(JSContext* cx, ICStub::Kind kind)
      : ICStubCompiler(cx, kind, Engine::Baseline)
    { }

    enum FunApplyThing {
        FunApply_MagicArgs,
        FunApply_Array
    };

    void pushCallArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                           Register argcReg, bool isJitCall, bool isConstructing = false);
    void pushSpreadCallArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                                 Register argcReg, bool isJitCall, bool isConstructing);
    void guardSpreadCall(MacroAssembler& masm, Register argcReg, Label* failure,
                         bool isConstructing);
    Register guardFunApply(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                           Register argcReg, bool checkNative, FunApplyThing applyThing,
                           Label* failure);
    void pushCallerArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs);
    void pushArrayArguments(MacroAssembler& masm, Address arrayVal,
                            AllocatableGeneralRegisterSet regs);
};

class ICCall_Fallback : public ICMonitoredFallbackStub
{
    friend class ICStubSpace;
  public:
    static const unsigned UNOPTIMIZABLE_CALL_FLAG = 0x1;

    static const uint32_t MAX_OPTIMIZED_STUBS = 16;
    static const uint32_t MAX_SCRIPTED_STUBS = 7;
    static const uint32_t MAX_NATIVE_STUBS = 7;
  private:

    explicit ICCall_Fallback(JitCode* stubCode)
      : ICMonitoredFallbackStub(ICStub::Call_Fallback, stubCode)
    { }

  public:
    void noteUnoptimizableCall() {
        extra_ |= UNOPTIMIZABLE_CALL_FLAG;
    }
    bool hadUnoptimizableCall() const {
        return extra_ & UNOPTIMIZABLE_CALL_FLAG;
    }

    unsigned scriptedStubCount() const {
        return numStubsWithKind(Call_Scripted);
    }
    bool scriptedStubsAreGeneralized() const {
        return hasStub(Call_AnyScripted);
    }

    unsigned nativeStubCount() const {
        return numStubsWithKind(Call_Native);
    }
    bool nativeStubsAreGeneralized() const {
        // Return hasStub(Call_AnyNative) after Call_AnyNative stub is added.
        return false;
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      public:
        static const int32_t BASELINE_CALL_KEY =
            (static_cast<int32_t>(Engine::Baseline)) |
            (static_cast<int32_t>(ICStub::Call_Fallback) << 1) |
            (0 << 17) | // spread
            (0 << 18);  // constructing
        static const int32_t BASELINE_CONSTRUCT_KEY =
            (static_cast<int32_t>(Engine::Baseline)) |
            (static_cast<int32_t>(ICStub::Call_Fallback) << 1) |
            (0 << 17) | // spread
            (1 << 18);  // constructing

      protected:
        bool isConstructing_;
        bool isSpread_;
        uint32_t returnOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);
        void postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(isSpread_) << 17) |
                  (static_cast<int32_t>(isConstructing_) << 18);
        }

      public:
        Compiler(JSContext* cx, bool isConstructing, bool isSpread)
          : ICCallStubCompiler(cx, ICStub::Call_Fallback),
            isConstructing_(isConstructing),
            isSpread_(isSpread)
        { }

        ICStub* getStub(ICStubSpace* space) {
            ICCall_Fallback* stub = newStub<ICCall_Fallback>(space, getStubCode());
            if (!stub || !stub->initMonitoringChain(cx, space, engine_))
                return nullptr;
            return stub;
        }
    };
};

class ICCall_Scripted : public ICMonitoredStub
{
    friend class ICStubSpace;
  public:
    // The maximum number of inlineable spread call arguments. Keep this small
    // to avoid controllable stack overflows by attackers passing large arrays
    // to spread call. This value is shared with ICCall_Native.
    static const uint32_t MAX_ARGS_SPREAD_LENGTH = 16;

  protected:
    GCPtrFunction callee_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

    ICCall_Scripted(JitCode* stubCode, ICStub* firstMonitorStub,
                    JSFunction* callee, JSObject* templateObject,
                    uint32_t pcOffset);

  public:
    static ICCall_Scripted* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                  ICCall_Scripted& other);

    GCPtrFunction& callee() {
        return callee_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Scripted, callee_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_Scripted, pcOffset_);
    }
};

class ICCall_AnyScripted : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_AnyScripted(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_AnyScripted, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    { }

  public:
    static ICCall_AnyScripted* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                     ICCall_AnyScripted& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_AnyScripted, pcOffset_);
    }
};

// Compiler for Call_Scripted and Call_AnyScripted stubs.
class ICCallScriptedCompiler : public ICCallStubCompiler {
  protected:
    ICStub* firstMonitorStub_;
    bool isConstructing_;
    bool isSpread_;
    RootedFunction callee_;
    RootedObject templateObject_;
    uint32_t pcOffset_;
    MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

    virtual int32_t getKey() const {
        return static_cast<int32_t>(engine_) |
              (static_cast<int32_t>(kind) << 1) |
              (static_cast<int32_t>(isConstructing_) << 17) |
              (static_cast<int32_t>(isSpread_) << 18);
    }

  public:
    ICCallScriptedCompiler(JSContext* cx, ICStub* firstMonitorStub,
                           JSFunction* callee, JSObject* templateObject,
                           bool isConstructing, bool isSpread, uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_Scripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        isSpread_(isSpread),
        callee_(cx, callee),
        templateObject_(cx, templateObject),
        pcOffset_(pcOffset)
    { }

    ICCallScriptedCompiler(JSContext* cx, ICStub* firstMonitorStub, bool isConstructing,
                           bool isSpread, uint32_t pcOffset)
      : ICCallStubCompiler(cx, ICStub::Call_AnyScripted),
        firstMonitorStub_(firstMonitorStub),
        isConstructing_(isConstructing),
        isSpread_(isSpread),
        callee_(cx, nullptr),
        templateObject_(cx, nullptr),
        pcOffset_(pcOffset)
    { }

    ICStub* getStub(ICStubSpace* space) {
        if (callee_) {
            return newStub<ICCall_Scripted>(space, getStubCode(), firstMonitorStub_, callee_,
                                            templateObject_, pcOffset_);
        }
        return newStub<ICCall_AnyScripted>(space, getStubCode(), firstMonitorStub_, pcOffset_);
    }
};

class ICCall_Native : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    GCPtrFunction callee_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

#ifdef JS_SIMULATOR
    void *native_;
#endif

    ICCall_Native(JitCode* stubCode, ICStub* firstMonitorStub,
                  JSFunction* callee, JSObject* templateObject,
                  uint32_t pcOffset);

  public:
    static ICCall_Native* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                ICCall_Native& other);

    GCPtrFunction& callee() {
        return callee_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfCallee() {
        return offsetof(ICCall_Native, callee_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_Native, pcOffset_);
    }

#ifdef JS_SIMULATOR
    static size_t offsetOfNative() {
        return offsetof(ICCall_Native, native_);
    }
#endif

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        bool isConstructing_;
        bool isSpread_;
        RootedFunction callee_;
        RootedObject templateObject_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(isConstructing_) << 17) |
                  (static_cast<int32_t>(isSpread_) << 18);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub,
                 HandleFunction callee, HandleObject templateObject,
                 bool isConstructing, bool isSpread, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_Native),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            isSpread_(isSpread),
            callee_(cx, callee),
            templateObject_(cx, templateObject),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_Native>(space, getStubCode(), firstMonitorStub_, callee_,
                                          templateObject_, pcOffset_);
        }
    };
};

class ICCall_ClassHook : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    const Class* clasp_;
    void* native_;
    GCPtrObject templateObject_;
    uint32_t pcOffset_;

    ICCall_ClassHook(JitCode* stubCode, ICStub* firstMonitorStub,
                     const Class* clasp, Native native, JSObject* templateObject,
                     uint32_t pcOffset);

  public:
    static ICCall_ClassHook* Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                   ICCall_ClassHook& other);

    const Class* clasp() {
        return clasp_;
    }
    void* native() {
        return native_;
    }
    GCPtrObject& templateObject() {
        return templateObject_;
    }

    static size_t offsetOfClass() {
        return offsetof(ICCall_ClassHook, clasp_);
    }
    static size_t offsetOfNative() {
        return offsetof(ICCall_ClassHook, native_);
    }
    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ClassHook, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        bool isConstructing_;
        const Class* clasp_;
        Native native_;
        RootedObject templateObject_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(isConstructing_) << 17);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub,
                 const Class* clasp, Native native,
                 HandleObject templateObject, uint32_t pcOffset,
                 bool isConstructing)
          : ICCallStubCompiler(cx, ICStub::Call_ClassHook),
            firstMonitorStub_(firstMonitorStub),
            isConstructing_(isConstructing),
            clasp_(clasp),
            native_(native),
            templateObject_(cx, templateObject),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_ClassHook>(space, getStubCode(), firstMonitorStub_, clasp_,
                                             native_, templateObject_, pcOffset_);
        }
    };
};

class ICCall_ScriptedApplyArray : public ICMonitoredStub
{
    friend class ICStubSpace;
  public:
    // The maximum length of an inlineable funcall array.
    // Keep this small to avoid controllable stack overflows by attackers passing large
    // arrays to fun.apply.
    static const uint32_t MAX_ARGS_ARRAY_LENGTH = 16;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedApplyArray(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedApplyArray, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedApplyArray* Clone(JSContext* cx,
                                            ICStubSpace* space,
                                            ICStub* firstMonitorStub,
                                            ICCall_ScriptedApplyArray& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedApplyArray, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedApplyArray),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_ScriptedApplyArray>(space, getStubCode(), firstMonitorStub_,
                                                      pcOffset_);
        }
    };
};

class ICCall_ScriptedApplyArguments : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedApplyArguments(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedApplyArguments, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedApplyArguments* Clone(JSContext* cx,
                                                ICStubSpace* space,
                                                ICStub* firstMonitorStub,
                                                ICCall_ScriptedApplyArguments& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedApplyArguments, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedApplyArguments),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_ScriptedApplyArguments>(space, getStubCode(), firstMonitorStub_,
                                                          pcOffset_);
        }
    };
};

// Handles calls of the form |fun.call(...)| where fun is a scripted function.
class ICCall_ScriptedFunCall : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;

    ICCall_ScriptedFunCall(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset)
      : ICMonitoredStub(ICStub::Call_ScriptedFunCall, stubCode, firstMonitorStub),
        pcOffset_(pcOffset)
    {}

  public:
    static ICCall_ScriptedFunCall* Clone(JSContext* cx, ICStubSpace* space,
                                         ICStub* firstMonitorStub, ICCall_ScriptedFunCall& other);

    static size_t offsetOfPCOffset() {
        return offsetof(ICCall_ScriptedFunCall, pcOffset_);
    }

    // Compiler for this stub kind.
    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset)
          : ICCallStubCompiler(cx, ICStub::Call_ScriptedFunCall),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_ScriptedFunCall>(space, getStubCode(), firstMonitorStub_,
                                                   pcOffset_);
        }
    };
};

class ICCall_StringSplit : public ICMonitoredStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;
    GCPtrString expectedStr_;
    GCPtrString expectedSep_;
    GCPtrObject templateObject_;

    ICCall_StringSplit(JitCode* stubCode, ICStub* firstMonitorStub, uint32_t pcOffset, JSString* str,
                       JSString* sep, JSObject* templateObject)
      : ICMonitoredStub(ICStub::Call_StringSplit, stubCode, firstMonitorStub),
        pcOffset_(pcOffset), expectedStr_(str), expectedSep_(sep),
        templateObject_(templateObject)
    { }

  public:
    static size_t offsetOfExpectedStr() {
        return offsetof(ICCall_StringSplit, expectedStr_);
    }

    static size_t offsetOfExpectedSep() {
        return offsetof(ICCall_StringSplit, expectedSep_);
    }

    static size_t offsetOfTemplateObject() {
        return offsetof(ICCall_StringSplit, templateObject_);
    }

    GCPtrString& expectedStr() {
        return expectedStr_;
    }

    GCPtrString& expectedSep() {
        return expectedSep_;
    }

    GCPtrObject& templateObject() {
        return templateObject_;
    }

    class Compiler : public ICCallStubCompiler {
      protected:
        ICStub* firstMonitorStub_;
        uint32_t pcOffset_;
        RootedString expectedStr_;
        RootedString expectedSep_;
        RootedObject templateObject_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1);
        }

      public:
        Compiler(JSContext* cx, ICStub* firstMonitorStub, uint32_t pcOffset, HandleString str,
                 HandleString sep, HandleValue templateObject)
          : ICCallStubCompiler(cx, ICStub::Call_StringSplit),
            firstMonitorStub_(firstMonitorStub),
            pcOffset_(pcOffset),
            expectedStr_(cx, str),
            expectedSep_(cx, sep),
            templateObject_(cx, &templateObject.toObject())
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_StringSplit>(space, getStubCode(), firstMonitorStub_, pcOffset_,
                                               expectedStr_, expectedSep_, templateObject_);
        }
   };
};

class ICCall_IsSuspendedStarGenerator : public ICStub
{
    friend class ICStubSpace;

  protected:
    explicit ICCall_IsSuspendedStarGenerator(JitCode* stubCode)
      : ICStub(ICStub::Call_IsSuspendedStarGenerator, stubCode)
    {}

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::Call_IsSuspendedStarGenerator, Engine::Baseline)
        {}
        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICCall_IsSuspendedStarGenerator>(space, getStubCode());
        }
   };
};

// Stub for performing a TableSwitch, updating the IC's return address to jump
// to whatever point the switch is branching to.
class ICTableSwitch : public ICStub
{
    friend class ICStubSpace;

  protected: // Protected to silence Clang warning.
    void** table_;
    int32_t min_;
    int32_t length_;
    void* defaultTarget_;

    ICTableSwitch(JitCode* stubCode, void** table,
                  int32_t min, int32_t length, void* defaultTarget)
      : ICStub(TableSwitch, stubCode), table_(table),
        min_(min), length_(length), defaultTarget_(defaultTarget)
    {}

  public:
    void fixupJumpTable(JSScript* script, BaselineScript* baseline);

    class Compiler : public ICStubCompiler {
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        jsbytecode* pc_;

      public:
        Compiler(JSContext* cx, jsbytecode* pc)
          : ICStubCompiler(cx, ICStub::TableSwitch, Engine::Baseline), pc_(pc)
        {}

        ICStub* getStub(ICStubSpace* space);
    };
};

// IC for constructing an iterator from an input value.
class ICIteratorNew_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIteratorNew_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::IteratorNew_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorNew_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIteratorNew_Fallback>(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in an iterator.
class ICIteratorMore_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIteratorMore_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::IteratorMore_Fallback, stubCode)
    { }

  public:
    void setHasNonStringResult() {
        extra_ = 1;
    }
    bool hasNonStringResult() const {
        MOZ_ASSERT(extra_ <= 1);
        return extra_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIteratorMore_Fallback>(space, getStubCode());
        }
    };
};

// IC for testing if there are more values in a native iterator.
class ICIteratorMore_Native : public ICStub
{
    friend class ICStubSpace;

    explicit ICIteratorMore_Native(JitCode* stubCode)
      : ICStub(ICStub::IteratorMore_Native, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorMore_Native, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIteratorMore_Native>(space, getStubCode());
        }
    };
};

// IC for closing an iterator.
class ICIteratorClose_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICIteratorClose_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::IteratorClose_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::IteratorClose_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICIteratorClose_Fallback>(space, getStubCode());
        }
    };
};

// InstanceOf
//      JSOP_INSTANCEOF
class ICInstanceOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICInstanceOf_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::InstanceOf_Fallback, stubCode)
    { }

    static const uint16_t UNOPTIMIZABLE_ACCESS_BIT = 0x1;

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 4;

    void noteUnoptimizableAccess() {
        extra_ |= UNOPTIMIZABLE_ACCESS_BIT;
    }
    bool hadUnoptimizableAccess() const {
        return extra_ & UNOPTIMIZABLE_ACCESS_BIT;
    }

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::InstanceOf_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICInstanceOf_Fallback>(space, getStubCode());
        }
    };
};

class ICInstanceOf_Function : public ICStub
{
    friend class ICStubSpace;

    GCPtrShape shape_;
    GCPtrObject prototypeObj_;
    uint32_t slot_;

    ICInstanceOf_Function(JitCode* stubCode, Shape* shape, JSObject* prototypeObj, uint32_t slot);

  public:
    GCPtrShape& shape() {
        return shape_;
    }
    GCPtrObject& prototypeObject() {
        return prototypeObj_;
    }
    uint32_t slot() const {
        return slot_;
    }
    static size_t offsetOfShape() {
        return offsetof(ICInstanceOf_Function, shape_);
    }
    static size_t offsetOfPrototypeObject() {
        return offsetof(ICInstanceOf_Function, prototypeObj_);
    }
    static size_t offsetOfSlot() {
        return offsetof(ICInstanceOf_Function, slot_);
    }

    class Compiler : public ICStubCompiler {
        RootedShape shape_;
        RootedObject prototypeObj_;
        uint32_t slot_;

      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, Shape* shape, JSObject* prototypeObj, uint32_t slot)
          : ICStubCompiler(cx, ICStub::InstanceOf_Function, Engine::Baseline),
            shape_(cx, shape),
            prototypeObj_(cx, prototypeObj),
            slot_(slot)
        {}

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICInstanceOf_Function>(space, getStubCode(), shape_, prototypeObj_,
                                                  slot_);
        }
    };
};

// TypeOf
//      JSOP_TYPEOF
//      JSOP_TYPEOFEXPR
class ICTypeOf_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICTypeOf_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::TypeOf_Fallback, stubCode)
    { }

  public:
    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::TypeOf_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICTypeOf_Fallback>(space, getStubCode());
        }
    };
};

class ICTypeOf_Typed : public ICFallbackStub
{
    friend class ICStubSpace;

    ICTypeOf_Typed(JitCode* stubCode, JSType type)
      : ICFallbackStub(ICStub::TypeOf_Typed, stubCode)
    {
        extra_ = uint16_t(type);
        MOZ_ASSERT(JSType(extra_) == type);
    }

  public:
    JSType type() const {
        return JSType(extra_);
    }

    class Compiler : public ICStubCompiler {
      protected:
        JSType type_;
        RootedString typeString_;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

        virtual int32_t getKey() const {
            return static_cast<int32_t>(engine_) |
                  (static_cast<int32_t>(kind) << 1) |
                  (static_cast<int32_t>(type_) << 17);
        }

      public:
        Compiler(JSContext* cx, JSType type, HandleString string)
          : ICStubCompiler(cx, ICStub::TypeOf_Typed, Engine::Baseline),
            type_(type),
            typeString_(cx, string)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICTypeOf_Typed>(space, getStubCode(), type_);
        }
    };
};

class ICRest_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    GCPtrArrayObject templateObject_;

    ICRest_Fallback(JitCode* stubCode, ArrayObject* templateObject)
      : ICFallbackStub(ICStub::Rest_Fallback, stubCode), templateObject_(templateObject)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    GCPtrArrayObject& templateObject() {
        return templateObject_;
    }

    class Compiler : public ICStubCompiler {
      protected:
        RootedArrayObject templateObject;
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, ArrayObject* templateObject)
          : ICStubCompiler(cx, ICStub::Rest_Fallback, Engine::Baseline),
            templateObject(cx, templateObject)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICRest_Fallback>(space, getStubCode(), templateObject);
        }
    };
};

// Stub for JSOP_RETSUB ("returning" from a |finally| block).
class ICRetSub_Fallback : public ICFallbackStub
{
    friend class ICStubSpace;

    explicit ICRetSub_Fallback(JitCode* stubCode)
      : ICFallbackStub(ICStub::RetSub_Fallback, stubCode)
    { }

  public:
    static const uint32_t MAX_OPTIMIZED_STUBS = 8;

    class Compiler : public ICStubCompiler {
      protected:
        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        explicit Compiler(JSContext* cx)
          : ICStubCompiler(cx, ICStub::RetSub_Fallback, Engine::Baseline)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICRetSub_Fallback>(space, getStubCode());
        }
    };
};

// Optimized JSOP_RETSUB stub. Every stub maps a single pc offset to its
// native code address.
class ICRetSub_Resume : public ICStub
{
    friend class ICStubSpace;

  protected:
    uint32_t pcOffset_;
    uint8_t* addr_;

    ICRetSub_Resume(JitCode* stubCode, uint32_t pcOffset, uint8_t* addr)
      : ICStub(ICStub::RetSub_Resume, stubCode),
        pcOffset_(pcOffset),
        addr_(addr)
    { }

  public:
    static size_t offsetOfPCOffset() {
        return offsetof(ICRetSub_Resume, pcOffset_);
    }
    static size_t offsetOfAddr() {
        return offsetof(ICRetSub_Resume, addr_);
    }

    class Compiler : public ICStubCompiler {
        uint32_t pcOffset_;
        uint8_t* addr_;

        MOZ_MUST_USE bool generateStubCode(MacroAssembler& masm);

      public:
        Compiler(JSContext* cx, uint32_t pcOffset, uint8_t* addr)
          : ICStubCompiler(cx, ICStub::RetSub_Resume, Engine::Baseline),
            pcOffset_(pcOffset),
            addr_(addr)
        { }

        ICStub* getStub(ICStubSpace* space) {
            return newStub<ICRetSub_Resume>(space, getStubCode(), pcOffset_, addr_);
        }
    };
};

inline bool
IsCacheableDOMProxy(JSObject* obj)
{
    if (!obj->is<ProxyObject>())
        return false;

    const BaseProxyHandler* handler = obj->as<ProxyObject>().handler();
    return handler->family() == GetDOMProxyHandlerFamily();
}

struct IonOsrTempData;

} // namespace jit
} // namespace js

#endif /* jit_BaselineIC_h */
