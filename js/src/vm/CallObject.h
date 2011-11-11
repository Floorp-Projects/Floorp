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
 * The Original Code is SpiderMonkey call object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Biggar <pbiggar@mozilla.com> (original author)
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

#ifndef CallObject_h___
#define CallObject_h___

namespace js {

class CallObject : public ::JSObject
{
    /*
     * Reserved slot structure for Call objects:
     *
     * private               - the stack frame corresponding to the Call object
     *                         until js_PutCallObject or its on-trace analog
     *                         is called, null thereafter
     * JSSLOT_CALL_CALLEE    - callee function for the stack frame, or null if
     *                         the stack frame is for strict mode eval code
     * JSSLOT_CALL_ARGUMENTS - arguments object for non-strict mode eval stack
     *                         frames (not valid for strict mode eval frames)
     */
    static const uintN CALLEE_SLOT = 0;
    static const uintN ARGUMENTS_SLOT = 1;

  public:
    static const uintN RESERVED_SLOTS = 2;

    /* Create a CallObject for the given callee function. */
    static CallObject *
    create(JSContext *cx, JSScript *script, JSObject &scopeChain, JSObject *callee);

    /* True if this is for a strict mode eval frame or for a function call. */
    inline bool isForEval() const;

    /* The stack frame for this CallObject, if the frame is still active. */
    inline js::StackFrame *maybeStackFrame() const;
    inline void setStackFrame(js::StackFrame *frame);

    /*
     * The callee function if this CallObject was created for a function
     * invocation, or null if it was created for a strict mode eval frame.
     */
    inline JSObject *getCallee() const;
    inline JSFunction *getCalleeFunction() const;
    inline void setCallee(JSObject *callee);
    inline void initCallee(JSObject *callee);

    /* Returns the callee's arguments object. */
    inline const js::Value &getArguments() const;
    inline void setArguments(const js::Value &v);
    inline void initArguments(const js::Value &v);

    /* Returns the formal argument at the given index. */
    inline const js::Value &arg(uintN i) const;
    inline void setArg(uintN i, const js::Value &v);
    inline void initArgUnchecked(uintN i, const js::Value &v);

    /* Returns the variable at the given index. */
    inline const js::Value &var(uintN i) const;
    inline void setVar(uintN i, const js::Value &v);
    inline void initVarUnchecked(uintN i, const js::Value &v);

    /*
     * Get the actual arrays of arguments and variables. Only call if type
     * inference is enabled, where we ensure that call object variables are in
     * contiguous slots (see NewCallObject).
     */
    inline js::HeapValueArray argArray();
    inline js::HeapValueArray varArray();

    inline void copyValues(uintN nargs, Value *argv, uintN nvars, Value *slots);
};

}

js::CallObject &
JSObject::asCall()
{
    JS_ASSERT(isCall());
    return *reinterpret_cast<js::CallObject *>(this);
}

#endif /* CallObject_h___ */
