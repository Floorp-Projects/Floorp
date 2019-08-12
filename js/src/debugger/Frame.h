/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Frame_h
#define debugger_Frame_h

#include "mozilla/Attributes.h"  // for MOZ_MUST_USE
#include "mozilla/Maybe.h"       // for Maybe
#include "mozilla/Range.h"       // for Range
#include "mozilla/Result.h"      // for Result

#include <stddef.h>  // for size_t

#include "jsapi.h"  // for JSContext, CallArgs

#include "NamespaceImports.h"   // for Value, MutableHandleValue, HandleObject
#include "debugger/DebugAPI.h"  // for ResumeMode
#include "debugger/Debugger.h"  // for ResumeMode, Handler, Debugger
#include "gc/Barrier.h"         // for HeapPtr
#include "gc/Rooting.h"         // for HandleDebuggerFrame, HandleNativeObject
#include "vm/JSObject.h"        // for JSObject
#include "vm/NativeObject.h"    // for NativeObject
#include "vm/Stack.h"           // for FrameIter, AbstractFramePtr

namespace js {

class AbstractGeneratorObject;
class FreeOp;
class GlobalObject;

/*
 * An OnStepHandler represents a handler function that is called when a small
 * amount of progress is made in a frame.
 */
struct OnStepHandler : Handler {
  /*
   * If we have made a small amount of progress in a frame, this method is
   * called with the frame as argument. If succesful, this method should
   * return true, with `resumeMode` and `vp` set to a resumption value
   * specifiying how execution should continue.
   */
  virtual bool onStep(JSContext* cx, HandleDebuggerFrame frame,
                      ResumeMode& resumeMode, MutableHandleValue vp) = 0;
};

class ScriptedOnStepHandler final : public OnStepHandler {
 public:
  explicit ScriptedOnStepHandler(JSObject* object);
  virtual JSObject* object() const override;
  virtual void hold(JSObject* owner) override;
  virtual void drop(js::FreeOp* fop, JSObject* owner) override;
  virtual void trace(JSTracer* tracer) override;
  virtual size_t allocSize() const override;
  virtual bool onStep(JSContext* cx, HandleDebuggerFrame frame,
                      ResumeMode& resumeMode, MutableHandleValue vp) override;

 private:
  HeapPtr<JSObject*> object_;
};

/*
 * An OnPopHandler represents a handler function that is called just before a
 * frame is popped.
 */
struct OnPopHandler : Handler {
  /*
   * The given `frame` is about to be popped; `completion` explains why.
   *
   * When this method returns true, it must set `resumeMode` and `vp` to a
   * resumption value specifying how execution should continue.
   *
   * When this method returns false, it should set an exception on `cx`.
   */
  virtual bool onPop(JSContext* cx, HandleDebuggerFrame frame,
                     const Completion& completion, ResumeMode& resumeMode,
                     MutableHandleValue vp) = 0;
};

class ScriptedOnPopHandler final : public OnPopHandler {
 public:
  explicit ScriptedOnPopHandler(JSObject* object);
  virtual JSObject* object() const override;
  virtual void hold(JSObject* owner) override;
  virtual void drop(js::FreeOp* fop, JSObject* owner) override;
  virtual void trace(JSTracer* tracer) override;
  virtual size_t allocSize() const override;
  virtual bool onPop(JSContext* cx, HandleDebuggerFrame frame,
                     const Completion& completion, ResumeMode& resumeMode,
                     MutableHandleValue vp) override;

 private:
  HeapPtr<JSObject*> object_;
};

enum class DebuggerFrameType { Eval, Global, Call, Module, WasmCall };

enum class DebuggerFrameImplementation { Interpreter, Baseline, Ion, Wasm };

class DebuggerArguments : public NativeObject {
 public:
  static const Class class_;

  static DebuggerArguments* create(JSContext* cx, HandleObject proto,
                                   HandleDebuggerFrame frame);

 private:
  enum { FRAME_SLOT };

  static const unsigned RESERVED_SLOTS = 1;
};

class DebuggerFrame : public NativeObject {
  friend class DebuggerArguments;
  friend class ScriptedOnStepHandler;
  friend class ScriptedOnPopHandler;

 public:
  static const Class class_;

  enum {
    OWNER_SLOT = 0,
    ARGUMENTS_SLOT,
    ONSTEP_HANDLER_SLOT,
    ONPOP_HANDLER_SLOT,

    // If this is a frame for a generator call, and the generator object has
    // been created (which doesn't happen until after default argument
    // evaluation and destructuring), then this is a PrivateValue pointing to a
    // GeneratorInfo struct that points to the call's AbstractGeneratorObject.
    // This allows us to implement Debugger.Frame methods even while the call is
    // suspended, and we have no FrameIter::Data.
    //
    // While Debugger::generatorFrames maps an AbstractGeneratorObject to its
    // Debugger.Frame, this link represents the reverse relation, from a
    // Debugger.Frame to its generator object. This slot is set if and only if
    // there is a corresponding entry in generatorFrames.
    GENERATOR_INFO_SLOT,

    RESERVED_SLOTS,
  };

  static void trace(JSTracer* trc, JSObject* obj);

  static NativeObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                                 HandleObject dbgCtor);
  static DebuggerFrame* create(JSContext* cx, HandleObject proto,
                               const FrameIter& iter,
                               HandleNativeObject debugger);

  static MOZ_MUST_USE bool getScript(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool getArguments(JSContext* cx,
                                        HandleDebuggerFrame frame,
                                        MutableHandleDebuggerArguments result);
  static MOZ_MUST_USE bool getCallee(JSContext* cx, HandleDebuggerFrame frame,
                                     MutableHandleDebuggerObject result);
  static MOZ_MUST_USE bool getIsConstructing(JSContext* cx,
                                             HandleDebuggerFrame frame,
                                             bool& result);
  static MOZ_MUST_USE bool getEnvironment(
      JSContext* cx, HandleDebuggerFrame frame,
      MutableHandleDebuggerEnvironment result);
  static bool getIsGenerator(HandleDebuggerFrame frame);
  static MOZ_MUST_USE bool getOffset(JSContext* cx, HandleDebuggerFrame frame,
                                     size_t& result);
  static MOZ_MUST_USE bool getOlder(JSContext* cx, HandleDebuggerFrame frame,
                                    MutableHandleDebuggerFrame result);
  static MOZ_MUST_USE bool getThis(JSContext* cx, HandleDebuggerFrame frame,
                                   MutableHandleValue result);
  static DebuggerFrameType getType(HandleDebuggerFrame frame);
  static DebuggerFrameImplementation getImplementation(
      HandleDebuggerFrame frame);
  static MOZ_MUST_USE bool setOnStepHandler(JSContext* cx,
                                            HandleDebuggerFrame frame,
                                            OnStepHandler* handler);

  static MOZ_MUST_USE JS::Result<Completion> eval(
      JSContext* cx, HandleDebuggerFrame frame,
      mozilla::Range<const char16_t> chars, HandleObject bindings,
      const EvalOptions& options);

  MOZ_MUST_USE bool requireLive(JSContext* cx);
  static MOZ_MUST_USE DebuggerFrame* checkThis(JSContext* cx,
                                               const CallArgs& args,
                                               const char* fnname,
                                               bool checkLive);

  bool isLive() const;
  OnStepHandler* onStepHandler() const;
  OnPopHandler* onPopHandler() const;
  void setOnPopHandler(JSContext* cx, OnPopHandler* handler);

  inline bool hasGenerator() const;

  // If hasGenerator(), return an direct cross-compartment reference to this
  // Debugger.Frame's generator object.
  AbstractGeneratorObject& unwrappedGenerator() const;

#ifdef DEBUG
  JSScript* generatorScript() const;
#endif

  /*
   * Associate the generator object genObj with this Debugger.Frame. This
   * association allows the Debugger.Frame to track the generator's execution
   * across suspensions and resumptions, and to implement some methods even
   * while the generator is suspended.
   *
   * The context `cx` must be in the Debugger.Frame's realm, and `genObj` must
   * be in a debuggee realm.
   *
   * Technically, the generator activation need not actually be on the stack
   * right now; it's okay to call this method on a Debugger.Frame that has no
   * ScriptFrameIter::Data at present. However, this function has no way to
   * verify that genObj really is the generator associated with the call for
   * which this Debugger.Frame was originally created, so it's best to make the
   * association while the call is on the stack, and the relationships are easy
   * to discern.
   */
  MOZ_MUST_USE bool setGenerator(JSContext* cx,
                                 Handle<AbstractGeneratorObject*> genObj);

  /*
   * Undo the effects of a prior call to setGenerator.
   *
   * If provided, owner must be the Debugger to which this Debugger.Frame
   * belongs; remove this frame's entry from its generatorFrames map, and clean
   * up its cross-compartment wrapper table entry. The owner must be passed
   * unless this method is being called from the Debugger.Frame's finalizer. (In
   * that case, the owner is not reliably available, and is not actually
   * necessary.)
   *
   * If maybeGeneratorFramesEnum is non-null, use it to remove this frame's
   * entry from the Debugger's generatorFrames weak map. In this case, this
   * function will not otherwise disturb generatorFrames. Passing the enum
   * allows this function to be used while iterating over generatorFrames.
   */
  void clearGenerator(FreeOp* fop);
  void clearGenerator(
      FreeOp* fop, Debugger* owner,
      Debugger::GeneratorWeakMap::Enum* maybeGeneratorFramesEnum = nullptr);

  /*
   * Called after a generator/async frame is resumed, before exposing this
   * Debugger.Frame object to any hooks.
   */
  bool resume(const FrameIter& iter);

  bool hasAnyLiveHooks() const;

 private:
  static const ClassOps classOps_;

  static const JSPropertySpec properties_[];
  static const JSFunctionSpec methods_[];

  static void finalize(FreeOp* fop, JSObject* obj);

  static AbstractFramePtr getReferent(HandleDebuggerFrame frame);
  static MOZ_MUST_USE bool getFrameIter(JSContext* cx,
                                        HandleDebuggerFrame frame,
                                        mozilla::Maybe<FrameIter>& result);
  static MOZ_MUST_USE bool requireScriptReferent(JSContext* cx,
                                                 HandleDebuggerFrame frame);

  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  static MOZ_MUST_USE bool argumentsGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool calleeGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool constructingGetter(JSContext* cx, unsigned argc,
                                              Value* vp);
  static MOZ_MUST_USE bool environmentGetter(JSContext* cx, unsigned argc,
                                             Value* vp);
  static MOZ_MUST_USE bool generatorGetter(JSContext* cx, unsigned argc,
                                           Value* vp);
  static MOZ_MUST_USE bool liveGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool offsetGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool olderGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool thisGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool typeGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool implementationGetter(JSContext* cx, unsigned argc,
                                                Value* vp);
  static MOZ_MUST_USE bool onStepGetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool onStepSetter(JSContext* cx, unsigned argc,
                                        Value* vp);
  static MOZ_MUST_USE bool onPopGetter(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool onPopSetter(JSContext* cx, unsigned argc, Value* vp);

  static MOZ_MUST_USE bool evalMethod(JSContext* cx, unsigned argc, Value* vp);
  static MOZ_MUST_USE bool evalWithBindingsMethod(JSContext* cx, unsigned argc,
                                                  Value* vp);

  Debugger* owner() const;

 public:
  FrameIter::Data* frameIterData() const;
  void setFrameIterData(FrameIter::Data*);
  void freeFrameIterData(FreeOp* fop);
  void maybeDecrementFrameScriptStepperCount(FreeOp* fop,
                                             AbstractFramePtr frame);

  class GeneratorInfo;
  inline GeneratorInfo* generatorInfo() const;
};

} /* namespace js */

#endif /* debugger_Frame_h */
