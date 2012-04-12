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
 *   Luke Wagner <luke@mozilla.com>
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

namespace js {

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
    HeapValue   callee;

    /*
     * Pointer to an array of bits indicating, for every argument in 'slots',
     * whether the element has been deleted. See isElementDeleted comment.
     */
    size_t      *deletedBits;

    /*
     * Values of the arguments for this object, or MagicValue(JS_ARGS_HOLE) if
     * the indexed argument has been modified.
     */
    HeapValue   slots[1];
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
 *   STACK_FRAME_SLOT
 *     Stores the function's stack frame for non-strict arguments objects until
 *     the function returns, when it is replaced with null.  When an arguments
 *     object is created on-trace its private is JS_ARGUMENTS_OBJECT_ON_TRACE,
 *     and when the trace exits its private is replaced with the stack frame or
 *     null, as appropriate. This slot is used by strict arguments objects as
 *     well, but the slot is always null. Conceptually it would be better to
 *     remove this oddity, but preserving it allows us to work with arguments
 *     objects of either kind more abstractly, so we keep it for now.
 */
class ArgumentsObject : public JSObject
{
    static const uint32_t INITIAL_LENGTH_SLOT = 0;
    static const uint32_t DATA_SLOT = 1;
    static const uint32_t STACK_FRAME_SLOT = 2;

    /* Lower-order bit stolen from the length slot. */
    static const uint32_t LENGTH_OVERRIDDEN_BIT = 0x1;
    static const uint32_t PACKED_BITS_COUNT = 1;

    void initInitialLength(uint32_t length);
    void initData(ArgumentsData *data);
    static ArgumentsObject *create(JSContext *cx, uint32_t argc, HandleObject callee);

  public:
    static const uint32_t RESERVED_SLOTS = 3;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4;

    /* Create an arguments object for a frame that is expecting them. */
    static ArgumentsObject *create(JSContext *cx, StackFrame *fp);

    /*
     * Purposefully disconnect the returned arguments object from the frame
     * by always creating a new copy that does not alias formal parameters.
     * This allows function-local analysis to determine that formals are
     * not aliased and generally simplifies arguments objects.
     */
    static ArgumentsObject *createUnexpected(JSContext *cx, StackFrame *fp);

    /*
     * Return the initial length of the arguments.  This may differ from the
     * current value of arguments.length!
     */
    inline uint32_t initialLength() const;

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
    inline bool getElement(uint32_t i, js::Value *vp);

    /*
     * Attempt to speedily and efficiently get elements [start, start + count)
     * of this arguments object into the locations starting at |vp|.  Return
     * true if all elements were copied.  Return false if the elements must be
     * gotten more slowly, perhaps using a getProperty or some similar method
     * in a loop.
     *
     * NB: Returning false does not indicate error!
     */
    inline bool getElements(uint32_t start, uint32_t count, js::Value *vp);

    inline js::ArgumentsData *data() const;

    /*
     * Because the arguments object is a real object, its elements may be
     * deleted. This is implemented by setting a 'deleted' flag for the arg
     * which is read by argument object resolve and getter/setter hooks.
     *
     * NB: an element, once deleted, stays deleted. Thus:
     *
     *   function f(x) { delete arguments[0]; arguments[0] = 42; return x }
     *   assertEq(f(1), 1);
     *
     * This works because, once a property is deleted from an arguments object,
     * it gets regular properties with regular getters/setters that don't alias
     * ArgumentsData::slots.
     */
    inline bool isElementDeleted(uint32_t i) const;
    inline bool isAnyElementDeleted() const;
    inline void markElementDeleted(uint32_t i);

    inline const js::Value &element(uint32_t i) const;
    inline void setElement(uint32_t i, const js::Value &v);

    /* The stack frame for this ArgumentsObject, if the frame is still active. */
    inline js::StackFrame *maybeStackFrame() const;
    inline void setStackFrame(js::StackFrame *frame);

    /*
     * Measures things hanging off this ArgumentsObject that are counted by the
     * |miscSize| argument in JSObject::sizeOfExcludingThis().
     */
    inline size_t sizeOfMisc(JSMallocSizeOfFun mallocSizeOf) const;
};

class NormalArgumentsObject : public ArgumentsObject
{
  public:
    /*
     * Stores arguments.callee, or MagicValue(JS_ARGS_HOLE) if the callee has
     * been cleared.
     */
    inline const js::Value &callee() const;

    /* Clear the location storing arguments.callee's initial value. */
    inline void clearCallee();

    /*
     * Return 'arguments[index]' for some unmodified NormalArgumentsObject of
     * 'fp' (the actual instance of 'arguments' doesn't matter so it does not
     * have to be passed or even created).
     */
    static bool optimizedGetElem(JSContext *cx, StackFrame *fp, const Value &elem, Value *vp);
};

class StrictArgumentsObject : public ArgumentsObject
{};

} // namespace js

js::NormalArgumentsObject &
JSObject::asNormalArguments()
{
    JS_ASSERT(isNormalArguments());
    return *static_cast<js::NormalArgumentsObject *>(this);
}

js::StrictArgumentsObject &
JSObject::asStrictArguments()
{
    JS_ASSERT(isStrictArguments());
    return *static_cast<js::StrictArgumentsObject *>(this);
}

js::ArgumentsObject &
JSObject::asArguments()
{
    JS_ASSERT(isArguments());
    return *static_cast<js::ArgumentsObject *>(this);
}

const js::ArgumentsObject &
JSObject::asArguments() const
{
    JS_ASSERT(isArguments());
    return *static_cast<const js::ArgumentsObject *>(this);
}

#endif /* ArgumentsObject_h___ */
