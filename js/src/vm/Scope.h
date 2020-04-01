/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Scope_h
#define vm_Scope_h

#include "mozilla/Maybe.h"
#include "mozilla/Variant.h"

#include <stddef.h>

#include "gc/DeletePolicy.h"
#include "gc/Policy.h"
#include "js/UbiNode.h"
#include "js/UniquePtr.h"
#include "util/Poison.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSObject.h"
#include "vm/Printer.h"  // GenericPrinter
#include "vm/ScopeKind.h"
#include "vm/Xdr.h"

namespace js {

namespace frontend {
class ScopeCreationData;
class EnvironmentShapeCreationData;
};  // namespace frontend

class BaseScopeData;
class ModuleObject;
class AbstractScopePtr;

enum class BindingKind : uint8_t {
  Import,
  FormalParameter,
  Var,
  Let,
  Const,

  // So you think named lambda callee names are consts? Nope! They don't
  // throw when being assigned to in sloppy mode.
  NamedLambdaCallee
};

static inline bool BindingKindIsLexical(BindingKind kind) {
  return kind == BindingKind::Let || kind == BindingKind::Const;
}

enum class IsFieldInitializer : bool { No, Yes };

static inline bool ScopeKindIsCatch(ScopeKind kind) {
  return kind == ScopeKind::SimpleCatch || kind == ScopeKind::Catch;
}

static inline bool ScopeKindIsInBody(ScopeKind kind) {
  return kind == ScopeKind::Lexical || kind == ScopeKind::SimpleCatch ||
         kind == ScopeKind::Catch || kind == ScopeKind::With ||
         kind == ScopeKind::FunctionLexical ||
         kind == ScopeKind::FunctionBodyVar;
}

const char* BindingKindString(BindingKind kind);
const char* ScopeKindString(ScopeKind kind);

class BindingName {
  // A JSAtom* with its low bit used as a tag for the:
  //  * whether it is closed over (i.e., exists in the environment shape)
  //  * whether it is a top-level function binding in global or eval scope,
  //    instead of var binding (both are in the same range in Scope data)
  uintptr_t bits_;

  static const uintptr_t ClosedOverFlag = 0x1;
  // TODO: We should reuse this bit for let vs class distinction to
  //       show the better redeclaration error message (bug 1428672).
  static const uintptr_t TopLevelFunctionFlag = 0x2;
  static const uintptr_t FlagMask = 0x3;

 public:
  BindingName() : bits_(0) {}

  BindingName(JSAtom* name, bool closedOver, bool isTopLevelFunction = false)
      : bits_(uintptr_t(name) | (closedOver ? ClosedOverFlag : 0x0) |
              (isTopLevelFunction ? TopLevelFunctionFlag : 0x0)) {}

 private:
  // For fromXDR.
  BindingName(JSAtom* name, uint8_t flags) : bits_(uintptr_t(name) | flags) {
    static_assert(FlagMask < alignof(JSAtom),
                  "Flags should fit into unused bits of JSAtom pointer");
    MOZ_ASSERT((flags & FlagMask) == flags);
  }

 public:
  static BindingName fromXDR(JSAtom* name, uint8_t flags) {
    return BindingName(name, flags);
  }

  uint8_t flagsForXDR() const { return static_cast<uint8_t>(bits_ & FlagMask); }

  JSAtom* name() const { return reinterpret_cast<JSAtom*>(bits_ & ~FlagMask); }

  bool closedOver() const { return bits_ & ClosedOverFlag; }

 private:
  friend class BindingIter;
  // This method should be called only for binding names in `vars` range in
  // BindingIter.
  bool isTopLevelFunction() const { return bits_ & TopLevelFunctionFlag; }

 public:
  void trace(JSTracer* trc);
};

/**
 * The various {Global,Module,...}Scope::Data classes consist of always-present
 * bits, then a trailing array of BindingNames.  The various Data classes all
 * end in a TrailingNamesArray that contains sized/aligned space for *one*
 * BindingName.  Data instances that contain N BindingNames, are then allocated
 * in sizeof(Data) + (space for (N - 1) BindingNames).  Because this class's
 * |data_| field is properly sized/aligned, the N-BindingName array can start
 * at |data_|.
 *
 * This is concededly a very low-level representation, but we want to only
 * allocate once for data+bindings both, and this does so approximately as
 * elegantly as C++ allows.
 */
class TrailingNamesArray {
 private:
  alignas(BindingName) unsigned char data_[sizeof(BindingName)];

 private:
  // Some versions of GCC treat it as a -Wstrict-aliasing violation (ergo a
  // -Werror compile error) to reinterpret_cast<> |data_| to |T*|, even
  // through |void*|.  Placing the latter cast in these separate functions
  // breaks the chain such that affected GCC versions no longer warn/error.
  void* ptr() { return data_; }

 public:
  // Explicitly ensure no one accidentally allocates scope data without
  // poisoning its trailing names.
  TrailingNamesArray() = delete;

  explicit TrailingNamesArray(size_t nameCount) {
    if (nameCount) {
      AlwaysPoison(&data_, JS_SCOPE_DATA_TRAILING_NAMES_PATTERN,
                   sizeof(BindingName) * nameCount,
                   MemCheckKind::MakeUndefined);
    }
  }

  BindingName* start() { return reinterpret_cast<BindingName*>(ptr()); }

  BindingName& get(size_t i) { return start()[i]; }
  BindingName& operator[](size_t i) { return get(i); }
};

class BindingLocation {
 public:
  enum class Kind {
    Global,
    Argument,
    Frame,
    Environment,
    Import,
    NamedLambdaCallee
  };

 private:
  Kind kind_;
  uint32_t slot_;

  BindingLocation(Kind kind, uint32_t slot) : kind_(kind), slot_(slot) {}

 public:
  static BindingLocation Global() {
    return BindingLocation(Kind::Global, UINT32_MAX);
  }

  static BindingLocation Argument(uint16_t slot) {
    return BindingLocation(Kind::Argument, slot);
  }

  static BindingLocation Frame(uint32_t slot) {
    MOZ_ASSERT(slot < LOCALNO_LIMIT);
    return BindingLocation(Kind::Frame, slot);
  }

  static BindingLocation Environment(uint32_t slot) {
    MOZ_ASSERT(slot < ENVCOORD_SLOT_LIMIT);
    return BindingLocation(Kind::Environment, slot);
  }

  static BindingLocation Import() {
    return BindingLocation(Kind::Import, UINT32_MAX);
  }

  static BindingLocation NamedLambdaCallee() {
    return BindingLocation(Kind::NamedLambdaCallee, UINT32_MAX);
  }

  bool operator==(const BindingLocation& other) const {
    return kind_ == other.kind_ && slot_ == other.slot_;
  }

  bool operator!=(const BindingLocation& other) const {
    return !operator==(other);
  }

  Kind kind() const { return kind_; }

  uint32_t slot() const {
    MOZ_ASSERT(kind_ == Kind::Frame || kind_ == Kind::Environment);
    return slot_;
  }

  uint16_t argumentSlot() const {
    MOZ_ASSERT(kind_ == Kind::Argument);
    return mozilla::AssertedCast<uint16_t>(slot_);
  }
};

//
// Allow using is<T> and as<T> on Rooted<Scope*> and Handle<Scope*>.
//
template <typename Wrapper>
class WrappedPtrOperations<Scope*, Wrapper> {
 public:
  template <class U>
  JS::Handle<U*> as() const {
    const Wrapper& self = *static_cast<const Wrapper*>(this);
    MOZ_ASSERT_IF(self, self->template is<U>());
    return Handle<U*>::fromMarkedLocation(
        reinterpret_cast<U* const*>(self.address()));
  }
};

//
// The base class of all Scopes.
//
class Scope : public js::gc::TenuredCell {
  friend class GCMarker;
  friend class frontend::ScopeCreationData;

  // If there are any aliased bindings, the shape for the
  // EnvironmentObject. Otherwise nullptr.
  using HeaderWithShape = gc::CellHeaderWithTenuredGCPointer<Shape>;
  HeaderWithShape headerAndEnvironmentShape_;

  // The enclosing scope or nullptr.
  const GCPtrScope enclosing_;

  // The kind determines data_.
  const ScopeKind kind_;

 protected:
  BaseScopeData* data_;

  Scope(ScopeKind kind, Scope* enclosing, Shape* environmentShape)
      : headerAndEnvironmentShape_(environmentShape),
        enclosing_(enclosing),
        kind_(kind),
        data_(nullptr) {}

  static Scope* create(JSContext* cx, ScopeKind kind, HandleScope enclosing,
                       HandleShape envShape);

  template <typename ConcreteScope, XDRMode mode>
  static XDRResult XDRSizedBindingNames(
      XDRState<mode>* xdr, Handle<ConcreteScope*> scope,
      MutableHandle<typename ConcreteScope::Data*> data);

  Shape* maybeCloneEnvironmentShape(JSContext* cx);

  template <typename ConcreteScope>
  void initData(MutableHandle<UniquePtr<typename ConcreteScope::Data>> data);

  template <typename F>
  void applyScopeDataTyped(F&& f);

 public:
  template <typename ConcreteScope>
  static ConcreteScope* create(
      JSContext* cx, ScopeKind kind, HandleScope enclosing,
      HandleShape envShape,
      MutableHandle<UniquePtr<typename ConcreteScope::Data>> data);

  static const JS::TraceKind TraceKind = JS::TraceKind::Scope;
  const gc::CellHeader& cellHeader() const {
    return headerAndEnvironmentShape_;
  }

  template <typename T>
  bool is() const {
    return kind_ == T::classScopeKind_;
  }

  template <typename T>
  T& as() {
    MOZ_ASSERT(this->is<T>());
    return *static_cast<T*>(this);
  }

  template <typename T>
  const T& as() const {
    MOZ_ASSERT(this->is<T>());
    return *static_cast<const T*>(this);
  }

  ScopeKind kind() const { return kind_; }

  Scope* enclosing() const { return enclosing_; }

  Shape* environmentShape() const { return headerAndEnvironmentShape_.ptr(); }

  static bool hasEnvironment(ScopeKind kind, bool environmentShape) {
    switch (kind) {
      case ScopeKind::With:
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        return true;
      default:
        // If there's a shape, an environment must be created for this scope.
        return environmentShape;
    }
  }

  bool hasEnvironment() const {
    return hasEnvironment(kind_, environmentShape());
  }

  uint32_t chainLength() const;
  uint32_t environmentChainLength() const;

  template <typename T>
  bool hasOnChain() const {
    for (const Scope* it = this; it; it = it->enclosing()) {
      if (it->is<T>()) {
        return true;
      }
    }
    return false;
  }

  bool hasOnChain(ScopeKind kind) const {
    for (const Scope* it = this; it; it = it->enclosing()) {
      if (it->kind() == kind) {
        return true;
      }
    }
    return false;
  }

  static Scope* clone(JSContext* cx, HandleScope scope, HandleScope enclosing);

  void traceChildren(JSTracer* trc);
  void finalize(JSFreeOp* fop);

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  void dump();
#if defined(DEBUG) || defined(JS_JITSPEW)
  static bool dumpForDisassemble(JSContext* cx, JS::Handle<Scope*> scope,
                                 GenericPrinter& out, const char* indent);
#endif /* defined(DEBUG) || defined(JS_JITSPEW) */
};

/** Empty base class for scope Data classes to inherit from. */
class BaseScopeData {};

template <class Data>
inline size_t SizeOfData(uint32_t numBindings) {
  static_assert(std::is_base_of<BaseScopeData, Data>::value,
                "Data must be the correct sort of data, i.e. it must "
                "inherit from BaseScopeData");
  return sizeof(Data) +
         (numBindings ? numBindings - 1 : 0) * sizeof(BindingName);
}

//
// A lexical scope that holds let and const bindings. There are 4 kinds of
// LexicalScopes.
//
// Lexical
//   A plain lexical scope.
//
// SimpleCatch
//   Holds the single catch parameter of a catch block.
//
// Catch
//   Holds the catch parameters (and only the catch parameters) of a catch
//   block.
//
// NamedLambda
// StrictNamedLambda
//   Holds the single name of the callee for a named lambda expression.
//
// All kinds of LexicalScopes correspond to LexicalEnvironmentObjects on the
// environment chain.
//
class LexicalScope : public Scope {
  friend class Scope;
  friend class BindingIter;
  friend class GCMarker;
  friend class frontend::ScopeCreationData;

 public:
  // Data is public because it is created by the frontend. See
  // Parser<FullParseHandler>::newLexicalScopeData.
  struct Data : public BaseScopeData {
    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // Bindings are sorted by kind in both frames and environments.
    //
    //   lets - [0, constStart)
    // consts - [constStart, length)
    uint32_t constStart = 0;
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, ScopeKind kind,
                       HandleScope enclosing, MutableHandleScope scope);

 private:
  static LexicalScope* createWithData(JSContext* cx, ScopeKind kind,
                                      MutableHandle<UniquePtr<Data>> data,
                                      uint32_t firstFrameSlot,
                                      HandleScope enclosing);

  template <typename ShapeType>
  static bool prepareForScopeCreation(JSContext* cx, ScopeKind kind,
                                      uint32_t firstFrameSlot,
                                      Handle<AbstractScopePtr> enclosing,
                                      MutableHandle<UniquePtr<Data>> data,
                                      ShapeType envShape);

  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

  static uint32_t nextFrameSlot(const AbstractScopePtr& scope);

 public:
  uint32_t firstFrameSlot() const;

  uint32_t nextFrameSlot() const { return data().nextFrameSlot; }

  // Returns an empty shape for extensible global and non-syntactic lexical
  // scopes.
  static Shape* getEmptyExtensibleEnvironmentShape(JSContext* cx);
};

template <>
inline bool Scope::is<LexicalScope>() const {
  return kind_ == ScopeKind::Lexical || kind_ == ScopeKind::SimpleCatch ||
         kind_ == ScopeKind::Catch || kind_ == ScopeKind::NamedLambda ||
         kind_ == ScopeKind::StrictNamedLambda ||
         kind_ == ScopeKind::FunctionLexical;
}

//
// Scope corresponding to a function. Holds formal parameter names, special
// internal names (see FunctionScope::isSpecialName), and, if the function
// parameters contain no expressions that might possibly be evaluated, the
// function's var bindings. For example, in these functions, the FunctionScope
// will store a/b/c bindings but not d/e/f bindings:
//
//   function f1(a, b) {
//     var c;
//     let e;
//     const f = 3;
//   }
//   function f2([a], b = 4, ...c) {
//     var d, e, f; // stored in VarScope
//   }
//
// Corresponds to CallObject on environment chain.
//
class FunctionScope : public Scope {
  friend class GCMarker;
  friend class BindingIter;
  friend class PositionalFormalParameterIter;
  friend class Scope;
  friend class AbstractScopePtr;
  static const ScopeKind classScopeKind_ = ScopeKind::Function;

 public:
  // Data is public because it is created by the
  // frontend. See Parser<FullParseHandler>::newFunctionScopeData.
  struct Data : public BaseScopeData {
    // The canonical function of the scope, as during a scope walk we
    // often query properties of the JSFunction (e.g., is the function an
    // arrow).
    GCPtrFunction canonicalFunction = {};

    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // If parameter expressions are present, parameters act like lexical
    // bindings.
    bool hasParameterExprs = false;

    // Yes if the corresponding function is a field initializer lambda.
    IsFieldInitializer isFieldInitializer = IsFieldInitializer::No;

    // Bindings are sorted by kind in both frames and environments.
    //
    // Positional formal parameter names are those that are not
    // destructured. They may be referred to by argument slots if
    // !script()->hasParameterExprs().
    //
    // An argument slot that needs to be skipped due to being destructured
    // or having defaults will have a nullptr name in the name array to
    // advance the argument slot.
    //
    // Rest parameter binding is also included in positional formals.
    // This also becomes nullptr if destructuring.
    //
    // The number of positional formals is equal to function.length if
    // there's no rest, function.length+1 otherwise.
    //
    // Destructuring parameters and destructuring rest are included in
    // "other formals" below.
    //
    // "vars" contains the following:
    //   * function's top level vars if !script()->hasParameterExprs()
    //   * special internal names (arguments, .this, .generator) if
    //     they're used.
    //
    // positional formals - [0, nonPositionalFormalStart)
    //      other formals - [nonPositionalParamStart, varStart)
    //               vars - [varStart, length)
    uint16_t nonPositionalFormalStart = 0;
    uint16_t varStart = 0;
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  template <typename ShapeType>
  static bool prepareForScopeCreation(JSContext* cx,
                                      MutableHandle<UniquePtr<Data>> data,
                                      bool hasParameterExprs,
                                      IsFieldInitializer isFieldInitializer,
                                      bool needsEnvironment, HandleFunction fun,
                                      ShapeType envShape);

  static bool updateEnvShapeIfRequired(JSContext* cx, MutableHandleShape shape,
                                       bool needsEnvironment,
                                       bool hasParameterExprs);

  static bool updateEnvShapeIfRequired(
      JSContext* cx,
      MutableHandle<frontend::EnvironmentShapeCreationData> shape,
      bool needsEnvironment, bool hasParameterExprs);

  static FunctionScope* clone(JSContext* cx, Handle<FunctionScope*> scope,
                              HandleFunction fun, HandleScope enclosing);

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, HandleFunction fun,
                       HandleScope enclosing, MutableHandleScope scope);

 private:
  static FunctionScope* createWithData(
      JSContext* cx, MutableHandle<UniquePtr<Data>> data,
      bool hasParameterExprs, IsFieldInitializer isFieldInitializer,
      bool needsEnvironment, HandleFunction fun, HandleScope enclosing);

  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  uint32_t nextFrameSlot() const { return data().nextFrameSlot; }

  JSFunction* canonicalFunction() const { return data().canonicalFunction; }

  JSScript* script() const;

  bool hasParameterExprs() const { return data().hasParameterExprs; }

  IsFieldInitializer isFieldInitializer() const {
    return data().isFieldInitializer;
  }

  uint32_t numPositionalFormalParameters() const {
    return data().nonPositionalFormalStart;
  }

  static bool isSpecialName(JSContext* cx, JSAtom* name);

  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

//
// Scope holding only vars. There is a single kind of VarScopes.
//
// FunctionBodyVar
//   Corresponds to the extra var scope present in functions with parameter
//   expressions. See examples in comment above FunctionScope.
//
// Corresponds to VarEnvironmentObject on environment chain.
//
class VarScope : public Scope {
  friend class GCMarker;
  friend class BindingIter;
  friend class Scope;
  friend class frontend::ScopeCreationData;

 public:
  // Data is public because it is created by the
  // frontend. See Parser<FullParseHandler>::newVarScopeData.
  struct Data : public BaseScopeData {
    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // All bindings are vars.
    //
    //            vars - [0, length)
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, ScopeKind kind,
                       HandleScope enclosing, MutableHandleScope scope);

 private:
  static VarScope* createWithData(JSContext* cx, ScopeKind kind,
                                  MutableHandle<UniquePtr<Data>> data,
                                  uint32_t firstFrameSlot,
                                  bool needsEnvironment, HandleScope enclosing);

  template <typename ShapeType>
  static bool prepareForScopeCreation(JSContext* cx, ScopeKind kind,
                                      MutableHandle<UniquePtr<Data>> data,
                                      uint32_t firstFrameSlot,
                                      bool needsEnvironment,
                                      ShapeType envShape);

  static bool updateEnvShapeIfRequired(JSContext* cx,
                                       MutableHandleShape envShape,
                                       bool needsEnvironment);
  static bool updateEnvShapeIfRequired(
      JSContext* cx,
      MutableHandle<frontend::EnvironmentShapeCreationData> envShape,
      bool needsEnvironment);
  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  uint32_t firstFrameSlot() const;

  uint32_t nextFrameSlot() const { return data().nextFrameSlot; }

  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

template <>
inline bool Scope::is<VarScope>() const {
  return kind_ == ScopeKind::FunctionBodyVar;
}

//
// Scope corresponding to both the global object scope and the global lexical
// scope.
//
// Both are extensible and are singletons across <script> tags, so these
// scopes are a fragment of the names in global scope. In other words, two
// global scripts may have two different GlobalScopes despite having the same
// GlobalObject.
//
// There are 2 kinds of GlobalScopes.
//
// Global
//   Corresponds to a GlobalObject and its global LexicalEnvironmentObject on
//   the environment chain.
//
// NonSyntactic
//   Corresponds to a non-GlobalObject created by the embedding on the
//   environment chain. This distinction is important for optimizations.
//
class GlobalScope : public Scope {
  friend class Scope;
  friend class BindingIter;
  friend class GCMarker;

 public:
  // Data is public because it is created by the frontend. See
  // Parser<FullParseHandler>::newGlobalScopeData.
  struct Data : BaseScopeData {
    // Bindings are sorted by kind.
    // `vars` includes top-level functions which is distinguished by a bit
    // on the BindingName.
    //
    //            vars - [0, letStart)
    //            lets - [letStart, constStart)
    //          consts - [constStart, length)
    uint32_t letStart = 0;
    uint32_t constStart = 0;
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  static GlobalScope* create(JSContext* cx, ScopeKind kind, Handle<Data*> data);

  static GlobalScope* createEmpty(JSContext* cx, ScopeKind kind) {
    return create(cx, kind, nullptr);
  }

  static GlobalScope* clone(JSContext* cx, Handle<GlobalScope*> scope,
                            ScopeKind kind);

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, ScopeKind kind,
                       MutableHandleScope scope);

 private:
  static GlobalScope* createWithData(JSContext* cx, ScopeKind kind,
                                     MutableHandle<UniquePtr<Data>> data);

  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  bool isSyntactic() const { return kind() != ScopeKind::NonSyntactic; }

  bool hasBindings() const { return data().length > 0; }
};

template <>
inline bool Scope::is<GlobalScope>() const {
  return kind_ == ScopeKind::Global || kind_ == ScopeKind::NonSyntactic;
}

//
// Scope of a 'with' statement. Has no bindings.
//
// Corresponds to a WithEnvironmentObject on the environment chain.
class WithScope : public Scope {
  friend class Scope;
  friend class AbstractScopePtr;
  static const ScopeKind classScopeKind_ = ScopeKind::With;

 public:
  static WithScope* create(JSContext* cx, HandleScope enclosing);

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, HandleScope enclosing,
                       MutableHandleScope scope);
};

//
// Scope of an eval. Holds var bindings. There are 2 kinds of EvalScopes.
//
// StrictEval
//   A strict eval. Corresponds to a VarEnvironmentObject, where its var
//   bindings lives.
//
// Eval
//   A sloppy eval. This is an empty scope, used only in the frontend, to
//   detect redeclaration errors. It has no Environment. Any `var`s declared
//   in the eval code are bound on the nearest enclosing var environment.
//
class EvalScope : public Scope {
  friend class Scope;
  friend class BindingIter;
  friend class GCMarker;
  friend class frontend::ScopeCreationData;

 public:
  // Data is public because it is created by the frontend. See
  // Parser<FullParseHandler>::newEvalScopeData.
  struct Data : public BaseScopeData {
    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // All bindings in an eval script are 'var' bindings. The implicit
    // lexical scope around the eval is present regardless of strictness
    // and is its own LexicalScope.
    // `vars` includes top-level functions which is distinguished by a bit
    // on the BindingName.
    //
    //            vars - [0, length)
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, ScopeKind kind,
                       HandleScope enclosing, MutableHandleScope scope);

 private:
  static EvalScope* createWithData(JSContext* cx, ScopeKind kind,
                                   MutableHandle<UniquePtr<Data>> data,
                                   HandleScope enclosing);

  template <typename ShapeType>
  static bool prepareForScopeCreation(JSContext* cx, ScopeKind scopeKind,
                                      MutableHandle<UniquePtr<Data>> data,
                                      ShapeType envShape);

  static bool updateEnvShapeIfRequired(JSContext* cx,
                                       MutableHandleShape envShape,
                                       ScopeKind scopeKind);
  static bool updateEnvShapeIfRequired(
      JSContext* cx,
      MutableHandle<frontend::EnvironmentShapeCreationData> envShape,
      ScopeKind scopeKind);
  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  // Starting a scope, the nearest var scope that a direct eval can
  // introduce vars on.
  static Scope* nearestVarScopeForDirectEval(Scope* scope);

  uint32_t nextFrameSlot() const { return data().nextFrameSlot; }

  bool strict() const { return kind() == ScopeKind::StrictEval; }

  bool hasBindings() const { return data().length > 0; }

  bool isNonGlobal() const {
    if (strict()) {
      return true;
    }
    return !nearestVarScopeForDirectEval(enclosing())->is<GlobalScope>();
  }

  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

template <>
inline bool Scope::is<EvalScope>() const {
  return kind_ == ScopeKind::Eval || kind_ == ScopeKind::StrictEval;
}

//
// Scope corresponding to the toplevel script in an ES module.
//
// Like GlobalScopes, these scopes contain both vars and lexical bindings, as
// the treating of imports and exports requires putting them in one scope.
//
// Corresponds to a ModuleEnvironmentObject on the environment chain.
//
class ModuleScope : public Scope {
  friend class GCMarker;
  friend class BindingIter;
  friend class Scope;
  friend class AbstractScopePtr;
  friend class frontend::ScopeCreationData;
  static const ScopeKind classScopeKind_ = ScopeKind::Module;

 public:
  // Data is public because it is created by the frontend. See
  // Parser<FullParseHandler>::newModuleScopeData.
  struct Data : BaseScopeData {
    // The module of the scope.
    GCPtr<ModuleObject*> module = {};

    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // Bindings are sorted by kind.
    //
    // imports - [0, varStart)
    //    vars - [varStart, letStart)
    //    lets - [letStart, constStart)
    //  consts - [constStart, length)
    uint32_t varStart = 0;
    uint32_t letStart = 0;
    uint32_t constStart = 0;
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
    Zone* zone() const;
  };

  template <XDRMode mode>
  static XDRResult XDR(XDRState<mode>* xdr, HandleModuleObject module,
                       HandleScope enclosing, MutableHandleScope scope);

 private:
  static ModuleScope* createWithData(JSContext* cx,
                                     MutableHandle<UniquePtr<Data>> data,
                                     Handle<ModuleObject*> module,
                                     HandleScope enclosing);
  template <typename ShapeType>
  static bool prepareForScopeCreation(JSContext* cx,
                                      MutableHandle<UniquePtr<Data>> data,
                                      HandleModuleObject module,
                                      ShapeType envShape);

  static bool updateEnvShapeIfRequired(JSContext* cx,
                                       MutableHandleShape envShape);
  static bool updateEnvShapeIfRequired(
      JSContext* cx,
      MutableHandle<frontend::EnvironmentShapeCreationData> envShape);

  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  uint32_t nextFrameSlot() const { return data().nextFrameSlot; }

  ModuleObject* module() const { return data().module; }

  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

class WasmInstanceScope : public Scope {
  friend class BindingIter;
  friend class Scope;
  friend class GCMarker;
  friend class AbstractScopePtr;
  static const ScopeKind classScopeKind_ = ScopeKind::WasmInstance;

 public:
  struct Data : public BaseScopeData {
    // The wasm instance of the scope.
    GCPtr<WasmInstanceObject*> instance = {};

    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // Bindings list the WASM memories and globals.
    //
    // memories - [0, globalsStart)
    //  globals - [globalsStart, length)
    uint32_t globalsStart = 0;
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  static WasmInstanceScope* create(JSContext* cx, WasmInstanceObject* instance);

 private:
  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  WasmInstanceObject* instance() const { return data().instance; }

  uint32_t memoriesStart() const { return 0; }

  uint32_t globalsStart() const { return data().globalsStart; }

  uint32_t namesCount() const { return data().length; }

  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

// Scope corresponding to the wasm function. A WasmFunctionScope is used by
// Debugger only, and not for wasm execution.
//
class WasmFunctionScope : public Scope {
  friend class BindingIter;
  friend class Scope;
  friend class GCMarker;
  friend class AbstractScopePtr;
  static const ScopeKind classScopeKind_ = ScopeKind::WasmFunction;

 public:
  struct Data : public BaseScopeData {
    // Frame slots [0, nextFrameSlot) are live when this is the innermost
    // scope.
    uint32_t nextFrameSlot = 0;

    // Bindings are the local variable names.
    //
    //    vars - [0, length)
    uint32_t length = 0;

    // Tagged JSAtom* names, allocated beyond the end of the struct.
    TrailingNamesArray trailingNames;

    explicit Data(size_t nameCount) : trailingNames(nameCount) {}
    Data() = delete;

    void trace(JSTracer* trc);
  };

  static WasmFunctionScope* create(JSContext* cx, HandleScope enclosing,
                                   uint32_t funcIndex);

 private:
  Data& data() { return *static_cast<Data*>(data_); }

  const Data& data() const { return *static_cast<Data*>(data_); }

 public:
  static Shape* getEmptyEnvironmentShape(JSContext* cx);
};

template <typename F>
void Scope::applyScopeDataTyped(F&& f) {
  switch (kind()) {
    case ScopeKind::Function: {
      f(&as<FunctionScope>().data());
      break;
      case ScopeKind::FunctionBodyVar:
        f(&as<VarScope>().data());
        break;
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
      case ScopeKind::FunctionLexical:
        f(&as<LexicalScope>().data());
        break;
      case ScopeKind::With:
        // With scopes do not have data.
        break;
      case ScopeKind::Eval:
      case ScopeKind::StrictEval:
        f(&as<EvalScope>().data());
        break;
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        f(&as<GlobalScope>().data());
        break;
      case ScopeKind::Module:
        f(&as<ModuleScope>().data());
        break;
      case ScopeKind::WasmInstance:
        f(&as<WasmInstanceScope>().data());
        break;
      case ScopeKind::WasmFunction:
        f(&as<WasmFunctionScope>().data());
        break;
      default:
        MOZ_CRASH("Unexpected scope type in ApplyScopeDataTyped");
    }
  }
}

//
// An iterator for a Scope's bindings. This is the source of truth for frame
// and environment object layout.
//
// It may be placed in GC containers; for example:
//
//   for (Rooted<BindingIter> bi(cx, BindingIter(scope)); bi; bi++) {
//     use(bi);
//     SomeMayGCOperation();
//     use(bi);
//   }
//
class BindingIter {
 protected:
  // Bindings are sorted by kind. Because different Scopes have differently
  // laid out Data for packing, BindingIter must handle all binding kinds.
  //
  // Kind ranges:
  //
  //            imports - [0, positionalFormalStart)
  // positional formals - [positionalFormalStart, nonPositionalFormalStart)
  //      other formals - [nonPositionalParamStart, varStart)
  //               vars - [varStart, letStart)
  //               lets - [letStart, constStart)
  //             consts - [constStart, length)
  //
  // Access method when not closed over:
  //
  //            imports - name
  // positional formals - argument slot
  //      other formals - frame slot
  //               vars - frame slot
  //               lets - frame slot
  //             consts - frame slot
  //
  // Access method when closed over:
  //
  //            imports - name
  // positional formals - environment slot or name
  //      other formals - environment slot or name
  //               vars - environment slot or name
  //               lets - environment slot or name
  //             consts - environment slot or name
  MOZ_INIT_OUTSIDE_CTOR uint32_t positionalFormalStart_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t nonPositionalFormalStart_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t varStart_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t letStart_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t constStart_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t length_;

  MOZ_INIT_OUTSIDE_CTOR uint32_t index_;

  enum Flags : uint8_t {
    CannotHaveSlots = 0,
    CanHaveArgumentSlots = 1 << 0,
    CanHaveFrameSlots = 1 << 1,
    CanHaveEnvironmentSlots = 1 << 2,

    // See comment in settle below.
    HasFormalParameterExprs = 1 << 3,
    IgnoreDestructuredFormalParameters = 1 << 4,

    // Truly I hate named lambdas.
    IsNamedLambda = 1 << 5
  };

  static const uint8_t CanHaveSlotsMask = 0x7;

  MOZ_INIT_OUTSIDE_CTOR uint8_t flags_;
  MOZ_INIT_OUTSIDE_CTOR uint16_t argumentSlot_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t frameSlot_;
  MOZ_INIT_OUTSIDE_CTOR uint32_t environmentSlot_;

  MOZ_INIT_OUTSIDE_CTOR BindingName* names_;

  void init(uint32_t positionalFormalStart, uint32_t nonPositionalFormalStart,
            uint32_t varStart, uint32_t letStart, uint32_t constStart,
            uint8_t flags, uint32_t firstFrameSlot,
            uint32_t firstEnvironmentSlot, BindingName* names,
            uint32_t length) {
    positionalFormalStart_ = positionalFormalStart;
    nonPositionalFormalStart_ = nonPositionalFormalStart;
    varStart_ = varStart;
    letStart_ = letStart;
    constStart_ = constStart;
    length_ = length;
    index_ = 0;
    flags_ = flags;
    argumentSlot_ = 0;
    frameSlot_ = firstFrameSlot;
    environmentSlot_ = firstEnvironmentSlot;
    names_ = names;

    settle();
  }

  void init(LexicalScope::Data& data, uint32_t firstFrameSlot, uint8_t flags);
  void init(FunctionScope::Data& data, uint8_t flags);
  void init(VarScope::Data& data, uint32_t firstFrameSlot);
  void init(GlobalScope::Data& data);
  void init(EvalScope::Data& data, bool strict);
  void init(ModuleScope::Data& data);
  void init(WasmInstanceScope::Data& data);
  void init(WasmFunctionScope::Data& data);

  bool hasFormalParameterExprs() const {
    return flags_ & HasFormalParameterExprs;
  }

  bool ignoreDestructuredFormalParameters() const {
    return flags_ & IgnoreDestructuredFormalParameters;
  }

  bool isNamedLambda() const { return flags_ & IsNamedLambda; }

  void increment() {
    MOZ_ASSERT(!done());
    if (flags_ & CanHaveSlotsMask) {
      if (canHaveArgumentSlots()) {
        if (index_ < nonPositionalFormalStart_) {
          MOZ_ASSERT(index_ >= positionalFormalStart_);
          argumentSlot_++;
        }
      }
      if (closedOver()) {
        // Imports must not be given known slots. They are
        // indirect bindings.
        MOZ_ASSERT(kind() != BindingKind::Import);
        MOZ_ASSERT(canHaveEnvironmentSlots());
        environmentSlot_++;
      } else if (canHaveFrameSlots()) {
        // Usually positional formal parameters don't have frame
        // slots, except when there are parameter expressions, in
        // which case they act like lets.
        if (index_ >= nonPositionalFormalStart_ ||
            (hasFormalParameterExprs() && name())) {
          frameSlot_++;
        }
      }
    }
    index_++;
  }

  void settle() {
    if (ignoreDestructuredFormalParameters()) {
      while (!done() && !name()) {
        increment();
      }
    }
  }

 public:
  explicit BindingIter(Scope* scope);
  explicit BindingIter(JSScript* script);

  BindingIter(LexicalScope::Data& data, uint32_t firstFrameSlot,
              bool isNamedLambda) {
    init(data, firstFrameSlot, isNamedLambda ? IsNamedLambda : 0);
  }

  BindingIter(FunctionScope::Data& data, bool hasParameterExprs) {
    init(data, IgnoreDestructuredFormalParameters |
                   (hasParameterExprs ? HasFormalParameterExprs : 0));
  }

  BindingIter(VarScope::Data& data, uint32_t firstFrameSlot) {
    init(data, firstFrameSlot);
  }

  explicit BindingIter(GlobalScope::Data& data) { init(data); }

  explicit BindingIter(ModuleScope::Data& data) { init(data); }

  explicit BindingIter(WasmFunctionScope::Data& data) { init(data); }

  BindingIter(EvalScope::Data& data, bool strict) { init(data, strict); }

  MOZ_IMPLICIT BindingIter(const BindingIter& bi) = default;

  bool done() const { return index_ == length_; }

  explicit operator bool() const { return !done(); }

  void operator++(int) {
    increment();
    settle();
  }

  bool isLast() const {
    MOZ_ASSERT(!done());
    return index_ + 1 == length_;
  }

  bool canHaveArgumentSlots() const { return flags_ & CanHaveArgumentSlots; }

  bool canHaveFrameSlots() const { return flags_ & CanHaveFrameSlots; }

  bool canHaveEnvironmentSlots() const {
    return flags_ & CanHaveEnvironmentSlots;
  }

  JSAtom* name() const {
    MOZ_ASSERT(!done());
    return names_[index_].name();
  }

  bool closedOver() const {
    MOZ_ASSERT(!done());
    return names_[index_].closedOver();
  }

  BindingLocation location() const {
    MOZ_ASSERT(!done());
    if (!(flags_ & CanHaveSlotsMask)) {
      return BindingLocation::Global();
    }
    if (index_ < positionalFormalStart_) {
      return BindingLocation::Import();
    }
    if (closedOver()) {
      MOZ_ASSERT(canHaveEnvironmentSlots());
      return BindingLocation::Environment(environmentSlot_);
    }
    if (index_ < nonPositionalFormalStart_ && canHaveArgumentSlots()) {
      return BindingLocation::Argument(argumentSlot_);
    }
    if (canHaveFrameSlots()) {
      return BindingLocation::Frame(frameSlot_);
    }
    MOZ_ASSERT(isNamedLambda());
    return BindingLocation::NamedLambdaCallee();
  }

  BindingKind kind() const {
    MOZ_ASSERT(!done());
    if (index_ < positionalFormalStart_) {
      return BindingKind::Import;
    }
    if (index_ < varStart_) {
      // When the parameter list has expressions, the parameters act
      // like lexical bindings and have TDZ.
      if (hasFormalParameterExprs()) {
        return BindingKind::Let;
      }
      return BindingKind::FormalParameter;
    }
    if (index_ < letStart_) {
      return BindingKind::Var;
    }
    if (index_ < constStart_) {
      return BindingKind::Let;
    }
    if (isNamedLambda()) {
      return BindingKind::NamedLambdaCallee;
    }
    return BindingKind::Const;
  }

  bool isTopLevelFunction() const {
    MOZ_ASSERT(!done());
    bool result = names_[index_].isTopLevelFunction();
    MOZ_ASSERT_IF(result, kind() == BindingKind::Var);
    return result;
  }

  bool hasArgumentSlot() const {
    MOZ_ASSERT(!done());
    if (hasFormalParameterExprs()) {
      return false;
    }
    return index_ >= positionalFormalStart_ &&
           index_ < nonPositionalFormalStart_;
  }

  uint16_t argumentSlot() const {
    MOZ_ASSERT(canHaveArgumentSlots());
    return mozilla::AssertedCast<uint16_t>(index_);
  }

  uint32_t nextFrameSlot() const {
    MOZ_ASSERT(canHaveFrameSlots());
    return frameSlot_;
  }

  uint32_t nextEnvironmentSlot() const {
    MOZ_ASSERT(canHaveEnvironmentSlots());
    return environmentSlot_;
  }

  void trace(JSTracer* trc);
};

void DumpBindings(JSContext* cx, Scope* scope);
JSAtom* FrameSlotName(JSScript* script, jsbytecode* pc);

Shape* EmptyEnvironmentShape(JSContext* cx, const JSClass* cls,
                             uint32_t numSlots, uint32_t baseShapeFlags);

//
// A refinement BindingIter that only iterates over positional formal
// parameters of a function.
//
class PositionalFormalParameterIter : public BindingIter {
  void settle() {
    if (index_ >= nonPositionalFormalStart_) {
      index_ = length_;
    }
  }

 public:
  explicit PositionalFormalParameterIter(Scope* scope);
  explicit PositionalFormalParameterIter(JSScript* script);

  void operator++(int) {
    BindingIter::operator++(1);
    settle();
  }

  bool isDestructured() const { return !name(); }
};

//
// Iterator for walking the scope chain.
//
// It may be placed in GC containers; for example:
//
//   for (Rooted<ScopeIter> si(cx, ScopeIter(scope)); si; si++) {
//     use(si);
//     SomeMayGCOperation();
//     use(si);
//   }
//
class MOZ_STACK_CLASS ScopeIter {
  Scope* scope_;

 public:
  explicit ScopeIter(Scope* scope) : scope_(scope) {}

  explicit ScopeIter(JSScript* script);

  explicit ScopeIter(const ScopeIter& si) = default;

  bool done() const { return !scope_; }

  explicit operator bool() const { return !done(); }

  void operator++(int) {
    MOZ_ASSERT(!done());
    scope_ = scope_->enclosing();
  }

  Scope* scope() const {
    MOZ_ASSERT(!done());
    return scope_;
  }

  ScopeKind kind() const {
    MOZ_ASSERT(!done());
    return scope_->kind();
  }

  // Returns the shape of the environment if it is known. It is possible to
  // hasSyntacticEnvironment and to have no known shape, e.g., eval.
  Shape* environmentShape() const { return scope()->environmentShape(); }

  // Returns whether this scope has a syntactic environment (i.e., an
  // Environment that isn't a non-syntactic With or NonSyntacticVariables)
  // on the environment chain.
  bool hasSyntacticEnvironment() const;

  void trace(JSTracer* trc) {
    if (scope_) {
      TraceRoot(trc, &scope_, "scope iter scope");
    }
  }
};

//
// Specializations of Rooted containers for the iterators.
//

template <typename Wrapper>
class WrappedPtrOperations<BindingIter, Wrapper> {
  const BindingIter& iter() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool done() const { return iter().done(); }
  explicit operator bool() const { return !done(); }
  bool isLast() const { return iter().isLast(); }
  bool canHaveArgumentSlots() const { return iter().canHaveArgumentSlots(); }
  bool canHaveFrameSlots() const { return iter().canHaveFrameSlots(); }
  bool canHaveEnvironmentSlots() const {
    return iter().canHaveEnvironmentSlots();
  }
  JSAtom* name() const { return iter().name(); }
  bool closedOver() const { return iter().closedOver(); }
  BindingLocation location() const { return iter().location(); }
  BindingKind kind() const { return iter().kind(); }
  bool isTopLevelFunction() const { return iter().isTopLevelFunction(); }
  bool hasArgumentSlot() const { return iter().hasArgumentSlot(); }
  uint16_t argumentSlot() const { return iter().argumentSlot(); }
  uint32_t nextFrameSlot() const { return iter().nextFrameSlot(); }
  uint32_t nextEnvironmentSlot() const { return iter().nextEnvironmentSlot(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<BindingIter, Wrapper>
    : public WrappedPtrOperations<BindingIter, Wrapper> {
  BindingIter& iter() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void operator++(int) { iter().operator++(1); }
};

template <typename Wrapper>
class WrappedPtrOperations<ScopeIter, Wrapper> {
  const ScopeIter& iter() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool done() const { return iter().done(); }
  explicit operator bool() const { return !done(); }
  Scope* scope() const { return iter().scope(); }
  ScopeKind kind() const { return iter().kind(); }
  Shape* environmentShape() const { return iter().environmentShape(); }
  bool hasSyntacticEnvironment() const {
    return iter().hasSyntacticEnvironment();
  }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<ScopeIter, Wrapper>
    : public WrappedPtrOperations<ScopeIter, Wrapper> {
  ScopeIter& iter() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void operator++(int) { iter().operator++(1); }
};

Shape* CreateEnvironmentShape(JSContext* cx, BindingIter& bi,
                              const JSClass* cls, uint32_t numSlots,
                              uint32_t baseShapeFlags);

Shape* EmptyEnvironmentShape(JSContext* cx, const JSClass* cls,
                             uint32_t numSlots, uint32_t baseShapeFlags);

}  // namespace js

namespace JS {

template <>
struct GCPolicy<js::ScopeKind> : public IgnoreGCPolicy<js::ScopeKind> {};

template <typename T>
struct ScopeDataGCPolicy : public NonGCPointerPolicy<T> {};

#define DEFINE_SCOPE_DATA_GCPOLICY(Data)              \
  template <>                                         \
  struct MapTypeToRootKind<Data*> {                   \
    static const RootKind kind = RootKind::Traceable; \
  };                                                  \
  template <>                                         \
  struct GCPolicy<Data*> : public ScopeDataGCPolicy<Data*> {}

DEFINE_SCOPE_DATA_GCPOLICY(js::LexicalScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::FunctionScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::VarScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::GlobalScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::EvalScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::ModuleScope::Data);
DEFINE_SCOPE_DATA_GCPOLICY(js::WasmFunctionScope::Data);

#undef DEFINE_SCOPE_DATA_GCPOLICY

// Scope data that contain GCPtrs must use the correct DeletePolicy.

template <>
struct DeletePolicy<js::FunctionScope::Data>
    : public js::GCManagedDeletePolicy<js::FunctionScope::Data> {};

template <>
struct DeletePolicy<js::ModuleScope::Data>
    : public js::GCManagedDeletePolicy<js::ModuleScope::Data> {};

template <>
struct DeletePolicy<js::WasmInstanceScope::Data>
    : public js::GCManagedDeletePolicy<js::WasmInstanceScope::Data> {};

namespace ubi {

template <>
class Concrete<js::Scope> : TracerConcrete<js::Scope> {
 protected:
  explicit Concrete(js::Scope* ptr) : TracerConcrete<js::Scope>(ptr) {}

 public:
  static void construct(void* storage, js::Scope* ptr) {
    new (storage) Concrete(ptr);
  }

  CoarseType coarseType() const final { return CoarseType::Script; }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif  // vm_Scope_h
