/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

import java.util.Hashtable;

/**
 * This class implements the Function native object.
 * See ECMA 15.3.
 * @author Norris Boyd
 */
public class NativeFunction extends ScriptableObject implements Function {

    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
    {
        // Fix up bootstrapping problem: the prototype of the Function
        // constructor was set to null because Function.prototype
        // was not yet defined.
        ctor.setPrototype(proto);

        ((NativeFunction) proto).functionName = "";
    }

    public String getClassName() {
        return "Function";
    }

    public boolean has(String s, Scriptable start) {
        return s.equals("prototype") || s.equals("arguments") || 
               super.has(s, start);
    }

    public Object get(String s, Scriptable start) {
        Object result = super.get(s, start);
        if (result != NOT_FOUND)
            return result;
        if (s.equals("prototype")) {
            NativeObject obj = new NativeObject();
            final int attr = ScriptableObject.DONTENUM |
                             ScriptableObject.READONLY |
                             ScriptableObject.PERMANENT;
            obj.defineProperty("constructor", this, attr);
            // put the prototype property into the object now, then in the
            // wacky case of a user defining a function Object(), we don't
            // get an infinite loop trying to find the prototype.
            put(s, this, obj);
            Scriptable proto = getObjectPrototype(this);            
            if (proto != obj) // not the one we just made, it must remain grounded
                obj.setPrototype(proto);
            return obj;
        }
        if (s.equals("arguments")) {
            // <Function name>.arguments is deprecated, so we use a slow
            // way of getting it that doesn't add to the invocation cost.
            // TODO: add warning, error based on version
            NativeCall activation = getActivation(Context.getContext());
            return activation == null 
                   ? null 
                   : activation.get("arguments", activation);
        }
        return NOT_FOUND;
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
        if (protoProp instanceof Scriptable && protoProp != Undefined.instance) {
            return ScriptRuntime.jsDelegatesTo(instance, (Scriptable)protoProp);
        }
        throw NativeGlobal.typeError1
            ("msg.instanceof.bad.prototype", functionName, instance);
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

    protected Scriptable getClassPrototype() {
        Object protoVal = get("prototype", this);
        if (protoVal == null 
                    || !(protoVal instanceof Scriptable)
                    || (protoVal == Undefined.instance))
            protoVal = getClassPrototype(this, "Object");
        return (Scriptable) protoVal;
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        Scriptable newInstance = new NativeObject();

        newInstance.setPrototype(getClassPrototype());
        newInstance.setParentScope(getParentScope());

        Object val = call(cx, scope, newInstance, args);
        if (val != null && val != Undefined.instance &&
            val instanceof Scriptable)
        {
            return (Scriptable) val;
        }
        return newInstance;
    }

    private boolean nextIs(int i, int token) {
        if (i + 1 < source.length())
            return source.charAt(i + 1) == token;
        return false;
    }

    // how much to indent
    private final static int OFFSET = 4;

    // less how much for case labels
    private final static int SETBACK = 2;

    // whether to do a debug print of the source information, when
    // decompiling.
    private static final boolean printSource = false;

    /**
     * Decompile the source information associated with this js
     * function/script back into a string.  For the most part, this
     * just means translating tokens back to their string
     * representations; there's a little bit of lookahead logic to
     * decide the proper spacing/indentation.  Most of the work in
     * mapping the original source to the prettyprinted decompiled
     * version is done by the parser.
     *
     * Note that support for Context.decompileFunctionBody is hacked
     * on through special cases; I suspect that js makes a distinction
     * between function header and function body that rhino
     * decompilation does not.
     *
     * @param indent How much to indent the decompiled result
     *
     * @param toplevel Whether this is the first call or a recursive
     * call of decompile.  (Not whether the function is defined at the
     * top level of script evaluation.)
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */

    public String decompile(int indent, boolean toplevel, boolean justbody) {
         if (source == null)
             return "function " + jsGet_name() +
                "() {\n\t[native code]\n}\n";

        // Spew tokens in source, for debugging.
        // as TYPE number char
        if (printSource) {
            System.err.println("length:" + source.length());
            for (int i = 0; i < source.length(); i++) {
                // Note that tokenToName will fail unless Context.printTrees
                // is true.
                String tokenname = TokenStream.tokenToName(source.charAt(i));
                if (tokenname == null)
                    tokenname = "---";
                String pad = tokenname.length() > 7
                    ? "\t"
                    : "\t\t";
                System.err.println
                    (tokenname
                     + pad + (int)source.charAt(i)
                     + "\t'" + ScriptRuntime.escapeString
                     (source.substring(i, i+1))
                     + "'");
            }
            System.err.println();
        }

        StringBuffer result = new StringBuffer();

        int i = 0;

        if (source.length() > 0) {
            /* special-case FUNCTION as the first token; if it is,
             * (and it's not followed by a NAME or LP) then we're
             * decompiling a function (and not the toplevel script.)

             * FUNCTION appearing elsewhere is an escape that means we'll
             * need to call toString of the given function (object).

             * If not at the top level, don't add an initial indent;
             * let the caller do it, so functions as expressions look
             * reasonable.  */

            if (toplevel) {
                // add an initial newline to exactly match js.
                if (!justbody)
                    result.append('\n');
                for (int j = 0; j < indent; j++)
                    result.append(' ');
            }

            if (source.charAt(0) == TokenStream.FUNCTION
                // make sure it's not a script that begins with a
                // reference to a function definition.
                && source.length() > 1
                && (source.charAt(1) == TokenStream.NAME
                    || source.charAt(1) == TokenStream.LP))
            {
                if (!justbody) {
                    result.append("function ");

                    /* version != 1.2 Function constructor behavior - if
                     * there's no function name in the source info, and
                     * the names[0] entry is the empty string, then it must
                     * have been created by the Function constructor;
                     * print 'anonymous' as the function name if the
                     * version (under which the function was compiled) is
                     * less than 1.2... or if it's greater than 1.2, because
                     * we need to be closer to ECMA.  (ToSource, please?)
                     */
                    if (nextIs(i, TokenStream.LP)
                        && this.version != Context.VERSION_1_2
                        && this.functionName != null 
                        && this.functionName.equals("anonymous"))
                        result.append("anonymous");
                    i++;
                } else {
                    /* Skip past the entire function header to the next EOL.
                     * Depends on how NAMEs are encoded.
                     */
                    while (i < source.length()
                           && (source.charAt(i) != TokenStream.EOL
                               // the length char of a NAME sequence
                               // can look like an EOL.
                               || (i > 0
                                   && source.charAt(i-1) == TokenStream.NAME)))
                    {
                        i++;
                    }
                    // Skip past the EOL, too.
                    i++;
                }
            }
        }

        while (i < source.length()) {
            int stop;
            switch(source.charAt(i)) {
            case TokenStream.NAME:
            case TokenStream.OBJECT:  // re-wrapped in '/'s in parser...
                /* NAMEs are encoded as NAME, (char) length, string...
                 * Note that lookahead for detecting labels depends on
                 * this encoding; change there if this changes.

                 * Also change function-header skipping code above,
                 * used when decompling under decompileFunctionBody.
                 */
                i++;
                stop = i + (int)source.charAt(i);
                result.append(source.substring(i + 1, stop + 1));
                i = stop;
                break;

            case TokenStream.NUMBER:
                i++;
                long lbits = 0;
                switch(source.charAt(i)) {
                case 'S':
                    i++;
                    result.append((int)source.charAt(i));
                    break;

                case 'J':
                    i++;
                    lbits |= (long)source.charAt(i++) << 48;
                    lbits |= (long)source.charAt(i++) << 32;
                    lbits |= (long)source.charAt(i++) << 16;
                    lbits |= (long)source.charAt(i);

                    result.append(lbits);
                    break;
                case 'D':
                    i++;

                    lbits |= (long)source.charAt(i++) << 48;
                    lbits |= (long)source.charAt(i++) << 32;
                    lbits |= (long)source.charAt(i++) << 16;
                    lbits |= (long)source.charAt(i);

                    double dval = Double.longBitsToDouble(lbits);
                    result.append(ScriptRuntime.numberToString(dval, 10));
                    break;
                }
                break;

            case TokenStream.STRING:
                i++;
                stop = i + (int)source.charAt(i);
                result.append('"');
                result.append(ScriptRuntime.escapeString
                              (source.substring(i + 1, stop + 1)));
                result.append('"');
                i = stop;
                break;

            case TokenStream.PRIMARY:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.TRUE:
                    result.append("true");
                    break;

                case TokenStream.FALSE:
                    result.append("false");
                    break;

                case TokenStream.NULL:
                    result.append("null");
                    break;

                case TokenStream.THIS:
                    result.append("this");
                    break;

                case TokenStream.TYPEOF:
                    result.append("typeof");
                    break;

                case TokenStream.VOID:
                    result.append("void");
                    break;

                case TokenStream.UNDEFINED:
                    result.append("undefined");
                    break;
                }
                break;

            case TokenStream.FUNCTION: {
                /* decompile a FUNCTION token as an escape; call
                 * toString on the nth enclosed nested function,
                 * where n is given by the byte that follows.
                 */

                i++;
                int functionNumber = source.charAt(i);
                if (nestedFunctions == null
                    || functionNumber > nestedFunctions.length)
                {
                    String message;
                    if (functionName != null && functionName.length() > 0) {
                        message = Context.getMessage2
                            ("msg.no.function.ref.found.in", 
                             new Integer((int)source.charAt(i)), functionName);
                    } else {
                        message = Context.getMessage1
                            ("msg.no.function.ref.found", 
                             new Integer((int)source.charAt(i)));
                    }
                    throw Context.reportRuntimeError(message);
                }
                result.append(nestedFunctions[functionNumber].decompile(indent,
                                                                        false,
                                                                        false));
                break;
            }
            case TokenStream.COMMA:
                result.append(", ");
                break;

            case TokenStream.LC:
                if (nextIs(i, TokenStream.EOL))
                    indent += OFFSET;
                result.append("{");
                break;

            case TokenStream.RC:
                /* don't print the closing RC if it closes the
                 * toplevel function and we're called from
                 * decompileFunctionBody.
                 */
                if (justbody && toplevel && i + 1 == source.length())
                    break;

                if (nextIs(i, TokenStream.EOL))
                    indent -= OFFSET;
                if (nextIs(i, TokenStream.WHILE)
                    || nextIs(i, TokenStream.ELSE)) {
                    indent -= OFFSET;
                    result.append("} ");
                }
                else
                    result.append('}');
                break;

            case TokenStream.LP:
                result.append('(');
                break;

            case TokenStream.RP:
                if (nextIs(i, TokenStream.LC))
                    result.append(") ");
                else
                    result.append(')');
                break;

            case TokenStream.LB:
                result.append('[');
                break;

            case TokenStream.RB:
                result.append(']');
                break;

            case TokenStream.EOL:
                result.append('\n');

                /* add indent if any tokens remain,
                 * less setback if next token is
                 * a label, case or default.
                 */
                if (i + 1 < source.length()) {
                    int less = 0;
                    if (nextIs(i, TokenStream.CASE)
                        || nextIs(i, TokenStream.DEFAULT))
                        less = SETBACK;
                    else if (nextIs(i, TokenStream.RC))
                        less = OFFSET;

                    /* elaborate check against label... skip past a
                     * following inlined NAME and look for a COLON.
                     * Depends on how NAME is encoded.
                     */
                    else if (nextIs(i, TokenStream.NAME)) {
                        int skip = source.charAt(i + 2);
                        if (source.charAt(i + skip + 3) == TokenStream.COLON)
                            less = OFFSET;
                    }

                    for (; less < indent; less++)
                        result.append(' ');
                }
                break;

            case TokenStream.DOT:
                result.append('.');
                break;

            case TokenStream.NEW:
                result.append("new ");
                break;

            case TokenStream.DELPROP:
                result.append("delete ");
                break;

            case TokenStream.IF:
                result.append("if ");
                break;

            case TokenStream.ELSE:
                result.append("else ");
                break;

            case TokenStream.FOR:
                result.append("for ");
                break;

            case TokenStream.IN:
                result.append(" in ");
                break;

            case TokenStream.WITH:
                result.append("with ");
                break;

            case TokenStream.WHILE:
                result.append("while ");
                break;

            case TokenStream.DO:
                result.append("do ");
                break;

            case TokenStream.TRY:
                result.append("try ");
                break;

            case TokenStream.CATCH:
                result.append("catch ");
                break;

            case TokenStream.FINALLY:
                result.append("finally ");
                break;

            case TokenStream.THROW:
                result.append("throw ");
                break;

            case TokenStream.SWITCH:
                result.append("switch ");
                break;

            case TokenStream.BREAK:
                if (nextIs(i, TokenStream.NAME))
                    result.append("break ");
                else
                    result.append("break");
                break;

            case TokenStream.CONTINUE:
                if (nextIs(i, TokenStream.NAME))
                    result.append("continue ");
                else
                    result.append("continue");
                break;

            case TokenStream.CASE:
                result.append("case ");
                break;

            case TokenStream.DEFAULT:
                result.append("default");
                break;

            case TokenStream.RETURN:
                if (nextIs(i, TokenStream.SEMI))
                    result.append("return");
                else
                    result.append("return ");
                break;

            case TokenStream.VAR:
                result.append("var ");
                break;

            case TokenStream.SEMI:
                if (nextIs(i, TokenStream.EOL))
                    // statement termination
                    result.append(";");
                else
                    // separators in FOR
                    result.append("; ");
                break;

            case TokenStream.ASSIGN:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.NOP:
                    result.append(" = ");
                    break;

                case TokenStream.ADD:
                    result.append(" += ");
                    break;

                case TokenStream.SUB:
                    result.append(" -= ");
                    break;

                case TokenStream.MUL:
                    result.append(" *= ");
                    break;

                case TokenStream.DIV:
                    result.append(" /= ");
                    break;

                case TokenStream.MOD:
                    result.append(" %= ");
                    break;

                case TokenStream.BITOR:
                    result.append(" |= ");
                    break;

                case TokenStream.BITXOR:
                    result.append(" ^= ");
                    break;

                case TokenStream.BITAND:
                    result.append(" &= ");
                    break;

                case TokenStream.LSH:
                    result.append(" <<= ");
                    break;

                case TokenStream.RSH:
                    result.append(" >>= ");
                    break;

                case TokenStream.URSH:
                    result.append(" >>>= ");
                    break;
                }
                break;

            case TokenStream.HOOK:
                result.append(" ? ");
                break;

            case TokenStream.OBJLIT:
                // pun OBJLIT to mean colon in objlit property initialization.
                // this needs to be distinct from COLON in the general case
                // to distinguish from the colon in a ternary... which needs
                // different spacing.
                result.append(':');
                break;

            case TokenStream.COLON:
                if (nextIs(i, TokenStream.EOL))
                    // it's the end of a label
                    result.append(":");
                else
                    // it's the middle part of a ternary
                    result.append(" : ");
                break;

            case TokenStream.OR:
                result.append(" || ");
                break;

            case TokenStream.AND:
                result.append(" && ");
                break;

            case TokenStream.BITOR:
                result.append(" | ");
                break;

            case TokenStream.BITXOR:
                result.append(" ^ ");
                break;

            case TokenStream.BITAND:
                result.append(" & ");
                break;

            case TokenStream.EQOP:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.SHEQ:
                    /*
                     * Emulate the C engine; if we're under version
                     * 1.2, then the == operator behaves like the ===
                     * operator (and the source is generated by
                     * decompiling a === opcode), so print the ===
                     * operator as ==.
                     */
                    result.append(this.version == Context.VERSION_1_2 ? " == "
                                  : " === ");
                    break;

                case TokenStream.SHNE:
                    result.append(this.version == Context.VERSION_1_2 ? " != "
                                  : " !== ");
                    break;

                case TokenStream.EQ:
                    result.append(" == ");
                    break;

                case TokenStream.NE:
                    result.append(" != ");
                    break;
                }
                break;

            case TokenStream.RELOP:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.LE:
                    result.append(" <= ");
                    break;

                case TokenStream.LT:
                    result.append(" < ");
                    break;

                case TokenStream.GE:
                    result.append(" >= ");
                    break;

                case TokenStream.GT:
                    result.append(" > ");
                    break;

                case TokenStream.INSTANCEOF:
                    result.append(" instanceof ");
                    break;
                }
                break;

            case TokenStream.SHOP:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.LSH:
                    result.append(" << ");
                    break;

                case TokenStream.RSH:
                    result.append(" >> ");
                    break;

                case TokenStream.URSH:
                    result.append(" >>> ");
                    break;
                }
                break;

            case TokenStream.UNARYOP:
                i++;
                switch(source.charAt(i)) {
                case TokenStream.TYPEOF:
                    result.append("typeof ");
                    break;

                case TokenStream.VOID:
                    result.append("void ");
                    break;

                case TokenStream.NOT:
                    result.append('!');
                    break;

                case TokenStream.BITNOT:
                    result.append('~');
                    break;

                case TokenStream.ADD:
                    result.append('+');
                    break;

                case TokenStream.SUB:
                    result.append('-');
                    break;
                }
                break;

            case TokenStream.INC:
                result.append("++");
                break;

            case TokenStream.DEC:
                result.append("--");
                break;

            case TokenStream.ADD:
                result.append(" + ");
                break;

            case TokenStream.SUB:
                result.append(" - ");
                break;

            case TokenStream.MUL:
                result.append(" * ");
                break;

            case TokenStream.DIV:
                result.append(" / ");
                break;

            case TokenStream.MOD:
                result.append(" % ");
                break;

            default: 
                // If we don't know how to decompile it, raise an exception.
                throw new RuntimeException("Unknown token " + 
                                           source.charAt(i));
            }
            i++;
        }

        // add that trailing newline if it's an outermost function.
        if (toplevel && !justbody)
            result.append('\n');

        return result.toString();
    }

    public static Object jsFunction_toString(Context cx, Scriptable thisObj,
                                             Object[] args, Function funObj)
    {
        Object val = thisObj.getDefaultValue(ScriptRuntime.FunctionClass);
        if (val instanceof NativeFunction) {
            NativeFunction funVal = (NativeFunction)val;
            if (funVal instanceof NativeJavaMethod) {
                return "\nfunction " + funVal.jsGet_name() +
                    "() {/*\n" + val.toString() + "*/}\n";
            }
            int indent = 0;
            if (args.length > 0)
                indent = (int)ScriptRuntime.toNumber(args[0]);

            return funVal.decompile(indent, true, false);
        }
        else if (val instanceof IdFunction) {
            IdFunction funVal = (IdFunction)val;
            return funVal.toStringForScript(cx);
        }
        throw NativeGlobal.typeError1
            ("msg.incompat.call", "toString", thisObj);
    }

    public static Object jsConstructor(Context cx, Object[] args,
                                       Function ctorObj, boolean inNewExpr)
    {
        int arglen = args.length;
        StringBuffer funArgs = new StringBuffer();

        /* Collect the arguments into a string.  */

        int i;
        for (i = 0; i < arglen - 1; i++) {
            if (i > 0)
                funArgs.append(",");
            funArgs.append(ScriptRuntime.toString(args[i]));
        }
        String funBody = arglen == 0 ? "" : ScriptRuntime.toString(args[i]);

        String source = "function (" + funArgs.toString() + ") {" +
                        funBody + "}";
        int[] linep = { 0 };
        String filename = Context.getSourcePositionFromStack(linep);
        if (filename == null) {
            filename = "<eval'ed string>";
            linep[0] = 1;
        }
        Object securityDomain = cx.getSecurityDomainForStackDepth(4);
        Scriptable scope = cx.ctorScope;
        if (scope == null)
            scope = ctorObj;
        Scriptable global = ScriptableObject.getTopLevelScope(scope);
        
        // Compile the function with opt level of -1 to force interpreter
        // mode.
        int oldOptLevel = cx.getOptimizationLevel();
        cx.setOptimizationLevel(-1);
        NativeFunction fn = (NativeFunction) cx.compileFunction(
                                                global, source,
                                                filename, linep[0], 
                                                securityDomain);
        cx.setOptimizationLevel(oldOptLevel);

        fn.functionName = "anonymous";
        fn.setPrototype(getFunctionPrototype(global));
        fn.setParentScope(global);

        return fn;
    }

    /**
     * Function.prototype.apply
     *
     * A proposed ECMA extension for round 2.
     */
    public static Object jsFunction_apply(Context cx, Scriptable thisObj,
                                          Object[] args, Function funObj)
        throws JavaScriptException
    {
        if (args.length != 2)
            return jsFunction_call(cx, thisObj, args, funObj);
        Object val = thisObj.getDefaultValue(ScriptRuntime.FunctionClass);
        Scriptable newThis = args[0] == null
                             ? ScriptableObject.getTopLevelScope(thisObj)
                             : ScriptRuntime.toObject(funObj, args[0]);
        Object[] newArgs;
        if (args.length > 1) {
            if ((args[1] instanceof NativeArray) 
                    || (args[1] instanceof Arguments))
                newArgs = cx.getElements((Scriptable) args[1]);
            else
                throw NativeGlobal.typeError0("msg.arg.isnt.array", thisObj);            
        }
        else
            newArgs = new Object[0];
        return ScriptRuntime.call(cx, val, newThis, newArgs, newThis);
    }

    /**
     * Function.prototype.call
     *
     * A proposed ECMA extension for round 2.
     */
    public static Object jsFunction_call(Context cx, Scriptable thisObj,
                                         Object[] args, Function funObj)
        throws JavaScriptException
    {
        Object val = thisObj.getDefaultValue(ScriptRuntime.FunctionClass);
        if (args.length == 0) {
            Scriptable s = ScriptRuntime.toObject(funObj, val);
            Scriptable scope = s.getParentScope();
            return ScriptRuntime.call(cx, val, scope, ScriptRuntime.emptyArgs, 
                                      scope);
        } else {
            Scriptable newThis = args[0] == null
                                 ? ScriptableObject.getTopLevelScope(thisObj)
                                 : ScriptRuntime.toObject(funObj, args[0]);

            Object[] newArgs = new Object[args.length - 1];
            System.arraycopy(args, 1, newArgs, 0, newArgs.length);
            return ScriptRuntime.call(cx, val, newThis, newArgs, newThis);
        }
    }

    public int jsGet_length() {
        Context cx = Context.getContext();
        if (cx != null && cx.getLanguageVersion() != Context.VERSION_1_2)
            return argCount;
        NativeCall activation = getActivation(cx);
        if (activation == null)
            return argCount;
        return activation.getOriginalArguments().length;
    }

    public int jsGet_arity() {
        return argCount;
    }

    public String jsGet_name() {
        if (functionName == null)
            return "";
        if (functionName.equals("anonymous")) {
            Context cx = Context.getCurrentContext();
            if (cx != null && cx.getLanguageVersion() == Context.VERSION_1_2)
                return "";
        }
        return functionName;
    }

    private NativeCall getActivation(Context cx) {
        NativeCall activation = cx.currentActivation;
        while (activation != null) {
            if (activation.getFunctionObject() == this) 
                return activation;
            activation = activation.caller;
        }
        return null;
    }
        
    protected String functionName;
    /**
     * The "argsNames" array has the following information:
     * argNames[0] through argNames[argCount - 1]: the names of the parameters
     * argNames[argCount] through argNames[args.length-1]: the names of the
     * variables declared in var statements
     */
    protected String[] argNames;
    protected short argCount;
    protected short version;

    /**
     * An encoded representation of the function source, for
     * decompiling.  Needs to be visible (only) to generated
     * subclasses of NativeFunction.
     */
    protected String source;

    /**
     * An array of NativeFunction values for each nested function.
     * Used internally, and also for decompiling nested functions.
     */
    public NativeFunction[] nestedFunctions;

    // For all generated subclass objects debug_level is set to 0 or higher.
    // So, if debug_level remains -1 in some object, then that object is
    // known to have not been generated.
    public int debug_level = -1;
    public String debug_srcName;
}

