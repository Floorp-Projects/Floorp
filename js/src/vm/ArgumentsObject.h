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

class ArgumentsObject : public ::JSObject
{
    /*
     * Stores the initial arguments length, plus a flag indicating whether
     * arguments.length has been overwritten. Use initialLength() to access the
     * initial arguments length.
     */
    static const uint32 INITIAL_LENGTH_SLOT = 0;

    /* Stores an ArgumentsData for these arguments; access with data(). */
    static const uint32 DATA_SLOT = 1;

  protected:
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
#endif

    void setInitialLength(uint32 length);

    void setCalleeAndData(JSObject &callee, ArgumentsData *data);

  public:
    /*
     * Create arguments parented to parent, for the given callee function.
     * Is parent redundant with callee->getGlobal()?
     */
    static ArgumentsObject *create(JSContext *cx, JSObject *parent, uint32 argc, JSObject &callee);

    /*
     * Return the initial length of the arguments.  This may differ from the
     * current value of arguments.length!
     */
    inline uint32 initialLength() const;

    /* True iff arguments.length has been assigned or its attributes changed. */
    inline bool hasOverriddenLength() const;
    inline void markLengthOverridden();

    inline js::ArgumentsData *data() const;

    inline const js::Value &element(uint32 i) const;
    inline js::Value *elements() const;
    inline js::Value *addressOfElement(uint32 i);
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
    static js::Class jsClass;

    friend bool JSObject::isNormalArguments() const;
    friend struct EmptyShape; // for EmptyShape::getEmptyArgumentsShape
    friend ArgumentsObject *
    ArgumentsObject::create(JSContext *cx, JSObject *parent, uint32 argc, JSObject &callee);

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
    static js::Class jsClass;

    friend bool JSObject::isStrictArguments() const;
    friend ArgumentsObject *
    ArgumentsObject::create(JSContext *cx, JSObject *parent, uint32 argc, JSObject &callee);
};

} // namespace js

inline bool
JSObject::isNormalArguments() const
{
    return getClass() == &js::NormalArgumentsObject::jsClass;
}

js::NormalArgumentsObject *
JSObject::asNormalArguments()
{
    JS_ASSERT(isNormalArguments());
    return reinterpret_cast<js::NormalArgumentsObject *>(this);
}

inline bool
JSObject::isStrictArguments() const
{
    return getClass() == &js::StrictArgumentsObject::jsClass;
}

js::StrictArgumentsObject *
JSObject::asStrictArguments()
{
    JS_ASSERT(isStrictArguments());
    return reinterpret_cast<js::StrictArgumentsObject *>(this);
}

inline bool
JSObject::isArguments() const
{
    return isNormalArguments() || isStrictArguments();
}

js::ArgumentsObject *
JSObject::asArguments()
{
    JS_ASSERT(isArguments());
    return reinterpret_cast<js::ArgumentsObject *>(this);
}

#endif /* ArgumentsObject_h___ */
