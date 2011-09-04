/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is SpiderMonkey arguments object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef ArgumentsObject_h___
#define ArgumentsObject_h___

#include "jsfun.h"

#ifdef JS_POLYIC
class GetPropCompiler;
#endif

#define JS_ARGUMENTS_OBJECT_ON_TRACE ((void *)0xa126)

#ifdef JS_TRACER
namespace nanojit {
class ValidateWriter;
}
#endif

namespace js {

#ifdef JS_POLYIC
struct VMFrame;
namespace mjit {
namespace ic {
struct PICInfo;
struct GetElementIC;

/* Aargh, Windows. */
#ifdef GetProp
#undef GetProp
#endif
void JS_FASTCALL GetProp(VMFrame &f, ic::PICInfo *pic);
}
}
#endif

#ifdef JS_TRACER
namespace tjit {
class Writer;
}
#endif

struct EmptyShape;

/*
 * ArgumentsData stores the initial indexed arguments provided to the
 * corresponding and that function itself.  It is used to store arguments[i]
 * and arguments.callee -- up until the corresponding property is modified,
 * when the relevant value is overwritten with MagicValue(JS_ARGS_HOLE) to
 * memorialize the modification.
 */
struct ArgumentsData
{
    /*
     * arguments.callee, or MagicValue(JS_ARGS_HOLE) if arguments.callee has
     * been modified.
     */
    js::Value   callee;

    /*
     * Values of the arguments for this object, or MagicValue(JS_ARGS_HOLE) if
     * the indexed argument has been modified.
     */
    js::Value   slots[1];
};

/*
 * ArgumentsObject instances represent |arguments| objects created to store
 * function arguments when a function is called.  It's expensive to create such
 * objects if they're never used, so they're only created lazily.  (See
 * js::StackFrame::setArgsObj and friends.)
 *
 * Arguments objects are complicated because, for non-strict mode code, they
 * must alias any named arguments which were provided to the function.  Gnarly
 * example:
 *
 *   function f(a, b, c, d)
 *   {
 *     arguments[0] = "seta";
 *     assertEq(a, "seta");
 *     b = "setb";
 *     assertEq(arguments[1], "setb");
 *     c = "setc";
 *     assertEq(arguments[2], undefined);
 *     arguments[3] = "setd";
 *     assertEq(d, undefined);
 *   }
 *   f("arga", "argb");
 *
 * ES5's strict mode behaves more sanely, and named arguments don't alias
 * elements of an arguments object.
 *
 * ArgumentsObject instances use the following reserved slots:
 *
 *   INITIAL_LENGTH_SLOT
 *     Stores the initial value of arguments.length, plus a bit indicating
 *     whether arguments.length has been modified.  Use initialLength() and
 *     hasOverriddenLength() to access these values.  If arguments.length has
 *     been modified, then the current value of arguments.length is stored in
 *     another slot associated with a new property.
 *   DATA_SLOT
 *     Stores an ArgumentsData* storing argument values and the callee, or
 *     sentinels for any of these if the corresponding property is modified.
 *     Use callee() to access the callee/sentinel, and use
 *     element/addressOfElement/setElement to access the values stored in
 *     the ArgumentsData.  If you're simply looking to get arguments[i],
 *     however, use getElement or getElements to avoid spreading arguments
 *     object implementation details around too much.
 */
class ArgumentsObject : public ::JSObject
{
    static const uint32 INITIAL_LENGTH_SLOT = 0;
    static const uint32 DATA_SLOT = 1;

  public:
    static const uint32 RESERVED_SLOTS = 2;

  private:
    /* Lower-order bit stolen from the length slot. */
    static const uint32 LENGTH_OVERRIDDEN_BIT = 0x1;
    static const uint32 PACKED_BITS_COUNT = 1;

#ifdef JS_TRACER
    /*
     * Needs access to INITIAL_LENGTH_SLOT -- technically just getArgsLength,
     * but nanojit's including windows.h makes that difficult.
     */
    friend class tjit::Writer;

    /*
     * Needs access to DATA_SLOT -- technically just checkAccSet needs it, but
     * that's private, and exposing turns into a mess.
     */
    friend class ::nanojit::ValidateWriter;
#endif

    /*
     * Need access to DATA_SLOT, INITIAL_LENGTH_SLOT, LENGTH_OVERRIDDEN_BIT, and
     * PACKED_BIT_COUNT.
     */
#ifdef JS_TRACER
    friend class TraceRecorder;
#endif
#ifdef JS_POLYIC
    friend class ::GetPropCompiler;
    friend struct mjit::ic::GetElementIC;
#endif

    void setInitialLength(uint32 length);

    void setCalleeAndData(JSObject &callee, ArgumentsData *data);

  public:
    /* Create an arguments object for the given callee function. */
    static ArgumentsObject *create(JSContext *cx, uint32 argc, JSObject &callee);

    /*
     * Return the initial length of the arguments.  This may differ from the
     * current value of arguments.length!
     */
    inline uint32 initialLength() const;

    /* True iff arguments.length has been assigned or its attributes changed. */
    inline bool hasOverriddenLength() const;
    inline void markLengthOverridden();

    /*
     * Attempt to speedily and efficiently access the i-th element of this
     * arguments object.  Return true if the element was speedily returned.
     * Return false if the element must be looked up more slowly using
     * getProperty or some similar method.
     *
     * NB: Returning false does not indicate error!
     */
    inline bool getElement(uint32 i, js::Value *vp);

    /*
     * Attempt to speedily and efficiently get elements [start, start + count)
     * of this arguments object into the locations starting at |vp|.  Return
     * true if all elements were copied.  Return false if the elements must be
     * gotten more slowly, perhaps using a getProperty or some similar method
     * in a loop.
     *
     * NB: Returning false does not indicate error!
     */
    inline bool getElements(uint32 start, uint32 count, js::Value *vp);

    inline js::ArgumentsData *data() const;

    inline const js::Value &element(uint32 i) const;
    inline const js::Value *elements() const;
    inline void setElement(uint32 i, const js::Value &v);
};

/*
 * Non-strict arguments have a private: the function's stack frame until the
 * function returns, when it is replaced with null.  When an arguments object
 * is created on-trace its private is JS_ARGUMENTS_OBJECT_ON_TRACE, and when
 * the trace exits its private is replaced with the stack frame or null, as
 * appropriate.
 */
class NormalArgumentsObject : public ArgumentsObject
{
    friend bool JSObject::isNormalArguments() const;
    friend struct EmptyShape; // for EmptyShape::getEmptyArgumentsShape
    friend ArgumentsObject *
    ArgumentsObject::create(JSContext *cx, uint32 argc, JSObject &callee);

  public:
    /*
     * Stores arguments.callee, or MagicValue(JS_ARGS_HOLE) if the callee has
     * been cleared.
     */
    inline const js::Value &callee() const;

    /* Clear the location storing arguments.callee's initial value. */
    inline void clearCallee();
};

/*
 * Technically strict arguments have a private, but it's always null.
 * Conceptually it would be better to remove this oddity, but preserving it
 * allows us to work with arguments objects of either kind more abstractly,
 * so we keep it for now.
 */
class StrictArgumentsObject : public ArgumentsObject
{
    friend bool JSObject::isStrictArguments() const;
    friend ArgumentsObject *
    ArgumentsObject::create(JSContext *cx, uint32 argc, JSObject &callee);
};

} // namespace js

js::NormalArgumentsObject *
JSObject::asNormalArguments()
{
    JS_ASSERT(isNormalArguments());
    return reinterpret_cast<js::NormalArgumentsObject *>(this);
}

js::StrictArgumentsObject *
JSObject::asStrictArguments()
{
    JS_ASSERT(isStrictArguments());
    return reinterpret_cast<js::StrictArgumentsObject *>(this);
}

js::ArgumentsObject *
JSObject::asArguments()
{
    JS_ASSERT(isArguments());
    return reinterpret_cast<js::ArgumentsObject *>(this);
}

#endif /* ArgumentsObject_h___ */
