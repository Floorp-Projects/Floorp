/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Igor Bukanov
 * Roger Lawrence
 * Mike McCabe
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

/**
 * The base class for Function objects
 * See ECMA 15.3.
 * @author Norris Boyd
 */
public class BaseFunction extends IdScriptable implements Function {

    static void init(Context cx, Scriptable scope, boolean sealed) {
        BaseFunction obj = new BaseFunction();
        obj.prototypeFlag = true;
        obj.functionName = "";
        obj.prototypePropertyAttrs = DONTENUM | READONLY | PERMANENT;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    protected void fillConstructorProperties
        (Context cx, IdFunction ctor, boolean sealed)
    {
        // Fix up bootstrapping problem: getPrototype of the IdFunction
        // can not return Function.prototype because Function object is not
        // yet defined.
        ctor.setPrototype(this);
    }

    public String getClassName() {
        return "Function";
    }

    /**
     * Implements the instanceof operator for JavaScript Function objects.
     * <p>
     * <code>
     * foo = new Foo();<br>
     * foo instanceof Foo;  // true<br>
     * </code>
     *
     * @param instance The value that appeared on the LHS of the instanceof
     *              operator
     * @return true if the "prototype" property of "this" appears in
     *              value's prototype chain
     *
     */
    public boolean hasInstance(Scriptable instance) {
        Object protoProp = ScriptableObject.getProperty(this, "prototype");
        if (protoProp instanceof Scriptable && protoProp != Undefined.instance)
        {
            return ScriptRuntime.jsDelegatesTo(instance, (Scriptable)protoProp);
        }
        throw NativeGlobal.typeError1
            ("msg.instanceof.bad.prototype", functionName, instance);
    }

    protected int getIdDefaultAttributes(int id) {
        switch (id) {
            case Id_length:
            case Id_arity:
            case Id_name:
                return DONTENUM | READONLY | PERMANENT;
            case Id_prototype:
                return prototypePropertyAttrs;
            case Id_arguments:
                return EMPTY;
        }
        return super.getIdDefaultAttributes(id);
    }

    protected boolean hasIdValue(int id) {
        if (id == Id_prototype) {
            return prototypeProperty != NOT_FOUND;
        }
        else if (id == Id_arguments) {
            // Should after delete Function.arguments its activation still
            // be available during Function call?
            // This code assumes it should not: after default set/deleteIdValue
            // hasIdValue/getIdValue would not be called again
            // To handle the opposite case, set/deleteIdValue should be
            // overwritten as well
            return null != getActivation(Context.getContext());
        }
        return super.hasIdValue(id);
    }

    protected Object getIdValue(int id) {
        switch (id) {
            case Id_length:    return wrap_int(getLength());
            case Id_arity:     return wrap_int(getArity());
            case Id_name:      return getFunctionName();
            case Id_prototype: return getPrototypeProperty();
            case Id_arguments: return getArguments();
        }
        return super.getIdValue(id);
    }

    protected void setIdValue(int id, Object value) {
        if (id == Id_prototype) {
            prototypeProperty = (value != null) ? value : UniqueTag.NULL_VALUE;
            return;
        }
        super.setIdValue(id, value);
    }

    protected void deleteIdValue(int id) {
        if (id == Id_prototype) {
            prototypeProperty = NOT_FOUND;
            return;
        }
        super.deleteIdValue(id);
    }

    public int methodArity(int methodId) {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor: return 1;
                case Id_toString:    return 1;
                case Id_apply:       return 2;
                case Id_call:        return 1;
            }
        }
        return super.methodArity(methodId);
    }

    public Object execMethod(int methodId, IdFunction f, Context cx,
                             Scriptable scope, Scriptable thisObj,
                             Object[] args)
        throws JavaScriptException
    {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor:
                    return jsConstructor(cx, scope, args);

                case Id_toString:
                    return js_toString(cx, thisObj, args);

                case Id_apply:
                    return applyOrCall(true, cx, scope, thisObj, args);

                case Id_call:
                    return applyOrCall(false, cx, scope, thisObj, args);
            }
        }
        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    /**
     * Make value as DontEnum, DontDelete, ReadOnly
     * prototype property of this Function object
     */
    public void setImmunePrototypeProperty(Object value) {
        prototypeProperty = (value != null) ? value : UniqueTag.NULL_VALUE;
        prototypePropertyAttrs = DONTENUM | READONLY | PERMANENT;
    }

    protected Scriptable getClassPrototype() {
        Object protoVal = getPrototypeProperty();
        if (protoVal == null
                    || !(protoVal instanceof Scriptable)
                    || (protoVal == Undefined.instance))
            protoVal = getClassPrototype(this, "Object");
        return (Scriptable) protoVal;
    }

    /**
     * Should be overridden.
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        return Undefined.instance;
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        Scriptable newInstance = createObject(cx, scope);
        Object val = call(cx, scope, newInstance, args);
        if (val instanceof Scriptable && val != Undefined.instance) {
            return (Scriptable) val;
        }
        return newInstance;
    }

    public Scriptable createObject(Context cx, Scriptable scope) {
        Scriptable newInstance = new NativeObject();
        newInstance.setPrototype(getClassPrototype());
        newInstance.setParentScope(getParentScope());
        return newInstance;
    }

    /**
     * Decompile the source information associated with this js
     * function/script back into a string.
     *
     * @param cx Current context
     *
     * @param indent How much to indent the decompiled result
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */

    public String decompile(Context cx, int indent, boolean justbody) {
        StringBuffer sb = new StringBuffer();
        if (!justbody) {
            sb.append("function ");
            sb.append(getFunctionName());
            sb.append("() {\n\t");
        }
        sb.append("[native code, arity=");
        sb.append(getArity());
        sb.append("]\n");
        if (!justbody) {
            sb.append("}\n");
        }
        return sb.toString();
    }

    public int getArity() { return 0; }

    public int getLength() { return 0; }

    public String getFunctionName() {
        if (functionName == null)
            return "";
        return functionName;
    }

    private Object getPrototypeProperty() {
        Object result = prototypeProperty;
        if (result == null) {
            synchronized (this) {
                result = prototypeProperty;
                if (result == null) {
                    setupDefaultPrototype();
                    result = prototypeProperty;
                }
            }
        }
        else if (result == UniqueTag.NULL_VALUE) { result = null; }
        return result;
    }

    private void setupDefaultPrototype() {
        NativeObject obj = new NativeObject();
        final int attr = ScriptableObject.DONTENUM |
                         ScriptableObject.READONLY |
                         ScriptableObject.PERMANENT;
        obj.defineProperty("constructor", this, attr);
        // put the prototype property into the object now, then in the
        // wacky case of a user defining a function Object(), we don't
        // get an infinite loop trying to find the prototype.
        prototypeProperty = obj;
        Scriptable proto = getObjectPrototype(this);
        if (proto != obj) {
            // not the one we just made, it must remain grounded
            obj.setPrototype(proto);
        }
    }

    private Object getArguments() {
        // <Function name>.arguments is deprecated, so we use a slow
        // way of getting it that doesn't add to the invocation cost.
        // TODO: add warning, error based on version
        NativeCall activation = getActivation(Context.getContext());
        return activation == null
               ? null
               : activation.get("arguments", activation);
    }

    NativeCall getActivation(Context cx) {
        NativeCall activation = cx.currentActivation;
        while (activation != null) {
            if (activation.getFunctionObject() == this)
                return activation;
            activation = activation.caller;
        }
        return null;
    }

    private static Object jsConstructor(Context cx, Scriptable scope,
                                        Object[] args)
    {
        int arglen = args.length;
        StringBuffer sourceBuf = new StringBuffer();
        sourceBuf.append("function (");
        // Append arguments as coma separated strings
        for (int i = 0; i < arglen - 1; i++) {
            if (i > 0) {
                sourceBuf.append(',');
            }
            sourceBuf.append(ScriptRuntime.toString(args[i]));
        }
        sourceBuf.append(") {");
        if (arglen != 0) {
            // append function body
            String funBody = ScriptRuntime.toString(args[arglen - 1]);
            sourceBuf.append(funBody);
        }
        sourceBuf.append('}');
        String source = sourceBuf.toString();

        int[] linep = new int[1];
        String filename = Context.getSourcePositionFromStack(linep);
        if (filename == null) {
            filename = "<eval'ed string>";
            linep[0] = 1;
        }

        String sourceName = ScriptRuntime.
            makeUrlForGeneratedScript(false, filename, linep[0]);

        Scriptable global = ScriptableObject.getTopLevelScope(scope);

        // Compile the function with opt level of -1 to force interpreter
        // mode.
        int oldOptLevel = cx.getOptimizationLevel();
        cx.setOptimizationLevel(-1);
        NativeFunction fn;
        try {
            fn = (NativeFunction) cx.compileFunction(global, source,
                                                     sourceName, 1,
                                                     null);
        }
        finally { cx.setOptimizationLevel(oldOptLevel); }

        fn.fromFunctionConstructor = true;
        ScriptRuntime.setFunctionProtoAndParent(global, fn);

        return fn;
    }

    private static Object js_toString(Context cx, Scriptable thisObj,
                                      Object[] args)
    {
        int indent = ScriptRuntime.toInt32(args, 0);
        Object val = thisObj.getDefaultValue(ScriptRuntime.FunctionClass);
        if (val instanceof BaseFunction) {
            return ((BaseFunction)val).decompile(cx, indent, false);
        }
        throw NativeGlobal.typeError1("msg.incompat.call", "toString", thisObj);
    }

    /**
     * Function.prototype.apply and Function.prototype.call
     *
     * See Ecma 15.3.4.[34]
     */
    private static Object applyOrCall(boolean isApply,
                                      Context cx, Scriptable scope,
                                      Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        int L = args.length;
        Object function = thisObj.getDefaultValue(ScriptRuntime.FunctionClass);

        Object callThis;
        if (L == 0 || args[0] == null || args[0] == Undefined.instance) {
            callThis = ScriptableObject.getTopLevelScope(scope);
        } else {
            callThis = ScriptRuntime.toObject(cx, scope, args[0]);
        }

        Object[] callArgs;
        if (isApply) {
            // Follow Ecma 15.3.4.3
            if (L <= 1) {
                callArgs = ScriptRuntime.emptyArgs;
            } else {
                Object arg1 = args[1];
                if (arg1 == null || arg1 == Undefined.instance) {
                    callArgs = ScriptRuntime.emptyArgs;
                } else if (arg1 instanceof NativeArray
                           || arg1 instanceof Arguments)
                {
                    callArgs = cx.getElements((Scriptable) arg1);
                } else {
                    throw NativeGlobal.typeError0("msg.arg.isnt.array", scope);
                }
            }
        } else {
            // Follow Ecma 15.3.4.4
            if (L <= 1) {
                callArgs = ScriptRuntime.emptyArgs;
            } else {
                callArgs = new Object[L - 1];
                System.arraycopy(args, 1, callArgs, 0, L - 1);
            }
        }

        return ScriptRuntime.call(cx, function, callThis, callArgs, scope);
    }

    protected String getIdName(int id) {
        switch (id) {
            case Id_length:       return "length";
            case Id_arity:        return "arity";
            case Id_name:         return "name";
            case Id_prototype:    return "prototype";
            case Id_arguments:    return "arguments";
        }

        if (prototypeFlag) {
            switch (id) {
                case Id_constructor:  return "constructor";
                case Id_toString:     return "toString";
                case Id_apply:        return "apply";
                case Id_call:         return "call";
            }
        }
        return null;
    }

// #string_id_map#

    private static final int
        Id_length       = 1,
        Id_arity        = 2,
        Id_name         = 3,
        Id_prototype    = 4,
        Id_arguments    = 5,

        MAX_INSTANCE_ID = 5;

    { setMaxId(MAX_INSTANCE_ID); }

    protected int mapNameToId(String s) {
        int id;
// #generated# Last update: 2001-05-20 00:12:12 GMT+02:00
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 4: X="name";id=Id_name; break L;
            case 5: X="arity";id=Id_arity; break L;
            case 6: X="length";id=Id_length; break L;
            case 9: c=s.charAt(0);
                if (c=='a') { X="arguments";id=Id_arguments; }
                else if (c=='p') { X="prototype";id=Id_prototype; }
                break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
// #/string_id_map#

        if (id != 0 || !prototypeFlag) { return id; }

// #string_id_map#
// #generated# Last update: 2001-05-20 00:12:12 GMT+02:00
        L0: { id = 0; String X = null;
            L: switch (s.length()) {
            case 4: X="call";id=Id_call; break L;
            case 5: X="apply";id=Id_apply; break L;
            case 8: X="toString";id=Id_toString; break L;
            case 11: X="constructor";id=Id_constructor; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor    = MAX_INSTANCE_ID + 1,
        Id_toString       = MAX_INSTANCE_ID + 2,
        Id_apply          = MAX_INSTANCE_ID + 3,
        Id_call           = MAX_INSTANCE_ID + 4,

        MAX_PROTOTYPE_ID  = MAX_INSTANCE_ID + 4;

// #/string_id_map#

    protected String functionName;

    private Object prototypeProperty;
    private int prototypePropertyAttrs = DONTENUM;

    private boolean prototypeFlag;
}

