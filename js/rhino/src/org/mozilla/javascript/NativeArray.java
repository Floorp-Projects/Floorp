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
 * Mike McCabe
 * Igor Bukanov
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
 * This class implements the Array native object.
 * @author Norris Boyd
 * @author Mike McCabe
 */
public class NativeArray extends IdScriptable {

    /*
     * Optimization possibilities and open issues:
     * - Long vs. double schizophrenia.  I suspect it might be better
     * to use double throughout.

     * - Most array operations go through getElem or setElem (defined
     * in this file) to handle the full 2^32 range; it might be faster
     * to have versions of most of the loops in this file for the
     * (infinitely more common) case of indices < 2^31.

     * - Functions that need a new Array call "new Array" in the
     * current scope rather than using a hardwired constructor;
     * "Array" could be redefined.  It turns out that js calls the
     * equivalent of "new Array" in the current scope, except that it
     * always gets at least an object back, even when Array == null.
     */

    public static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeArray obj = new NativeArray();
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    /**
     * Zero-parameter constructor: just used to create Array.prototype
     */
    public NativeArray() {
        dense = null;
        this.length = 0;
    }

    public NativeArray(long length) {
        int intLength = (int) length;
        if (intLength == length && intLength > 0) {
            if (intLength > maximumDenseLength)
                intLength = maximumDenseLength;
            dense = new Object[intLength];
            for (int i=0; i < intLength; i++)
                dense[i] = NOT_FOUND;
        }
        this.length = length;
    }

    public NativeArray(Object[] array) {
        dense = array;
        this.length = array.length;
    }

    public String getClassName() {
        return "Array";
    }

    protected int getIdDefaultAttributes(int id) {
        if (id == Id_length) {
            return DONTENUM | PERMANENT;
        }
        return super.getIdDefaultAttributes(id);
    }

    protected Object getIdValue(int id) {
        if (id == Id_length) {
            return wrap_double(length);
        }
        return super.getIdValue(id);
    }
    
    protected void setIdValue(int id, Object value) {
        if (id == Id_length) {
            jsSet_length(value); return;
        }
        super.setIdValue(id, value);
    }
    
    public int methodArity(int methodId) {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor:     return 1;
                case Id_toString:        return 0;
                case Id_toLocaleString:  return 1;
                case Id_join:            return 1;
                case Id_reverse:         return 0;
                case Id_sort:            return 1;
                case Id_push:            return 1;
                case Id_pop:             return 1;
                case Id_shift:           return 1;
                case Id_unshift:         return 1;
                case Id_splice:          return 1;
                case Id_concat:          return 1;
                case Id_slice:           return 1;
            }
        }
        return super.methodArity(methodId);
    }

    public Object execMethod
        (int methodId, IdFunction f,
         Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor:
                    return jsConstructor(cx, scope, args, f, thisObj == null);

                case Id_toString:
                    return jsFunction_toString(cx, thisObj, args);

                case Id_toLocaleString:
                    return jsFunction_toLocaleString(cx, thisObj, args);

                case Id_join:
                    return jsFunction_join(cx, thisObj, args);

                case Id_reverse:
                    return jsFunction_reverse(cx, thisObj, args);

                case Id_sort:
                    return jsFunction_sort(cx, scope, thisObj, args);

                case Id_push:
                    return jsFunction_push(cx, thisObj, args);

                case Id_pop:
                    return jsFunction_pop(cx, thisObj, args);

                case Id_shift:
                    return jsFunction_shift(cx, thisObj, args);

                case Id_unshift:
                    return jsFunction_unshift(cx, thisObj, args);

                case Id_splice:
                    return jsFunction_splice(cx, scope, thisObj, args);

                case Id_concat:
                    return jsFunction_concat(cx, scope, thisObj, args);

                case Id_slice:
                    return jsFunction_slice(cx, thisObj, args);
            }
        }
        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    public Object get(int index, Scriptable start) {
        if (dense != null && 0 <= index && index < dense.length)
            return dense[index];
        return super.get(index, start);
    }

    public boolean has(int index, Scriptable start) {
        if (dense != null && 0 <= index && index < dense.length)
            return dense[index] != NOT_FOUND;
        return super.has(index, start);
    }
    
    // if id is an array index (ECMA 15.4.0), return the number, 
    // otherwise return -1L
    private static long toArrayIndex(String id) {
        double d = ScriptRuntime.toNumber(id);
        if (d == d) {
            long index = ScriptRuntime.toUint32(d);
            if (index == d && index != 4294967295L) {
                // Assume that ScriptRuntime.toString(index) is the same 
                // as java.lang.Long.toString(index) for long
                if (Long.toString(index).equals(id)) {
                    return index;
                }
            }
        }
        return -1;
    }

    public void put(String id, Scriptable start, Object value) {
        if (start == this) {
            long index = toArrayIndex(id);
            if (index >= length) {
                length = index + 1;
            }
        }
        super.put(id, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        if (start == this) {
            // only set the array length if given an array index (ECMA 15.4.0)
            if (this.length <= index) {
                // avoid overflowing index!
                this.length = (long)index + 1;
            }

            if (dense != null && 0 <= index && index < dense.length) {
                dense[index] = value;
                return;
            }
        }
        super.put(index, start, value);
    }

    public void delete(int index) {
        if (!isSealed()) {
            if (dense != null && 0 <= index && index < dense.length) {
                dense[index] = NOT_FOUND;
                return;
            }
        }
        super.delete(index);
    }

    public Object[] getIds() {
        Object[] superIds = super.getIds();
        if (dense == null) { return superIds; }
        int N = dense.length;
        long currentLength = length;
        if (N > currentLength) {
            N = (int)currentLength;
        }
        if (N == 0) { return superIds; }
        int shift = superIds.length;
        Object[] ids = new Object[shift + N];
        // Make a copy of dense to be immune to removing 
        // of array elems from other thread when calculating presentCount
        System.arraycopy(dense, 0, ids, shift, N);
        int presentCount = 0;
        for (int i = 0; i != N; ++i) {
            // Replace existing elements by their indexes
            if (ids[shift + i] != NOT_FOUND) {
                ids[shift + presentCount] = new Integer(i);
                ++presentCount;
            }
        }
        if (presentCount != N) {
            // dense contains deleted elems, need to shrink the result
            Object[] tmp = new Object[shift + presentCount];
            System.arraycopy(ids, shift, tmp, shift, presentCount);
            ids = tmp;
        }
        System.arraycopy(superIds, 0, ids, 0, shift);
        return ids;
    }
    
    public Object getDefaultValue(Class hint) {
        if (hint == ScriptRuntime.NumberClass) {
            Context cx = Context.getContext();
            if (cx.getLanguageVersion() == Context.VERSION_1_2)
                return new Long(length);
        }
        return super.getDefaultValue(hint);
    }

    /**
     * See ECMA 15.4.1,2
     */
    private static Object jsConstructor(Context cx, Scriptable scope, 
                                        Object[] args, IdFunction ctorObj,
                                        boolean inNewExpr)
        throws JavaScriptException
    {
        if (!inNewExpr) {
            // FunctionObject.construct will set up parent, proto
            return ctorObj.construct(cx, scope, args);
        }
        if (args.length == 0)
            return new NativeArray();

        // Only use 1 arg as first element for version 1.2; for
        // any other version (including 1.3) follow ECMA and use it as
        // a length.
        if (cx.getLanguageVersion() == cx.VERSION_1_2) {
            return new NativeArray(args);
        }
        else {
            if ((args.length > 1) || (!(args[0] instanceof Number)))
                return new NativeArray(args);
            else {
                long len = ScriptRuntime.toUint32(args[0]);
                if (len != (((Number)(args[0])).doubleValue()))
                    throw Context.reportRuntimeError0("msg.arraylength.bad");
                return new NativeArray(len);
            }
        }
    }

    public long jsGet_length() {
        return length;
    }

    private void jsSet_length(Object val) {
        /* XXX do we satisfy this?
         * 15.4.5.1 [[Put]](P, V):
         * 1. Call the [[CanPut]] method of A with name P.
         * 2. If Result(1) is false, return.
         * ?
         */

        double d = ScriptRuntime.toNumber(val);
        long longVal = ScriptRuntime.toUint32(d);
        if (longVal != d)
            throw Context.reportRuntimeError0("msg.arraylength.bad");

        if (longVal < length) {
            // remove all properties between longVal and length
            if (length - longVal > 0x1000) {
                // assume that the representation is sparse
                Object[] e = getIds(); // will only find in object itself
                for (int i=0; i < e.length; i++) {
                    Object id = e[i]; 
                    if (id instanceof String) {
                        // > MAXINT will appear as string
                        String strId = (String)id;
                        long index = toArrayIndex(strId);
                        if (index >= longVal)
                            delete(strId);
                    } else {
                        int index = ((Integer)id).intValue();
                        if (index >= longVal)
                            delete(index);
                    }
                }
            } else {
                // assume a dense representation
                for (long i = longVal; i < length; i++) {
                    deleteElem(this, i);
                }
            }
        }
        length = longVal;
    }

    /* Support for generic Array-ish objects.  Most of the Array
     * functions try to be generic; anything that has a length
     * property is assumed to be an array.  hasLengthProperty is
     * needed in addition to getLengthProperty, because
     * getLengthProperty always succeeds - tries to convert strings, etc.
     */
    static double getLengthProperty(Scriptable obj) {
        // These will both give numeric lengths within Uint32 range.
        if (obj instanceof NativeString)
            return (double)((NativeString)obj).jsGet_length();
        if (obj instanceof NativeArray)
            return (double)((NativeArray)obj).jsGet_length();
        return ScriptRuntime.toUint32(ScriptRuntime
                                      .getProp(obj, "length", obj));
    }

    static boolean hasLengthProperty(Object obj) {
        if (!(obj instanceof Scriptable) || obj == Context.getUndefinedValue())
            return false;
        if (obj instanceof NativeString || obj instanceof NativeArray)
            return true;
        Scriptable sobj = (Scriptable)obj;

        // XXX some confusion as to whether or not to walk to get the length
        // property.  Pending review of js/[new ecma submission] treatment
        // of 'arrayness'.

        Object property = ScriptRuntime.getProp(sobj, "length", sobj);
        return property instanceof Number;
    }

    /* Utility functions to encapsulate index > Integer.MAX_VALUE
     * handling.  Also avoids unnecessary object creation that would
     * be necessary to use the general ScriptRuntime.get/setElem
     * functions... though this is probably premature optimization.
     */
    private static void deleteElem(Scriptable target, long index) {
        int i = (int)index;
        if (i == index) { target.delete(i); }
        else { target.delete(Long.toString(index)); }
    }

    private static Object getElem(Scriptable target, long index) {
        if (index > Integer.MAX_VALUE) {
            String id = Long.toString(index);
            return ScriptRuntime.getStrIdElem(target, id);
        } else {
            return ScriptRuntime.getElem(target, (int)index);
        }
    }

    private static void setElem(Scriptable target, long index, Object value) {
        if (index > Integer.MAX_VALUE) {
            String id = Long.toString(index);
            ScriptRuntime.setStrIdElem(target, id, value, target);
        } else {
            ScriptRuntime.setElem(target, (int)index, value);
        }
    }

    private static String jsFunction_toString(Context cx, Scriptable thisObj,
                                              Object[] args)
        throws JavaScriptException
    {
        return toStringHelper(cx, thisObj, 
                              cx.getLanguageVersion() == cx.VERSION_1_2,
                              false);
    }
    
    private static String jsFunction_toLocaleString(Context cx, 
                                                    Scriptable thisObj,
                                                    Object[] args)
        throws JavaScriptException
    {
        return toStringHelper(cx, thisObj, false, true);
    }
    
    private static String toStringHelper(Context cx, Scriptable thisObj,
                                         boolean toSource, boolean toLocale)
        throws JavaScriptException
    {
        /* It's probably redundant to handle long lengths in this
         * function; StringBuffers are limited to 2^31 in java.
         */

        long length = (long)getLengthProperty(thisObj);

        StringBuffer result = new StringBuffer();

        if (cx.iterating == null)
            cx.iterating = new Hashtable(31);
        boolean iterating = cx.iterating.get(thisObj) == Boolean.TRUE;

        // whether to return '4,unquoted,5' or '[4, "quoted", 5]'
        String separator;

        if (toSource) {
            result.append('[');
            separator = ", ";
        } else {
            separator = ",";
        }

        boolean haslast = false;
        long i = 0;

        if (!iterating) {
            for (i = 0; i < length; i++) {
                if (i > 0) result.append(separator);
                Object elem = getElem(thisObj, i);
                if (elem == null || elem == Undefined.instance) {
                    haslast = false;
                    continue;
                }
                haslast = true;

                if (elem instanceof String) {
                    String s = (String)elem;
                    if (toSource) {
                        result.append('\"');
                        result.append(ScriptRuntime.escapeString(s));
                        result.append('\"');
                    } else {
                        result.append(s);
                    }
                } else {
                    /* wrap changes to cx.iterating in a try/finally
                     * so that the reference always gets removed, and
                     * we don't leak memory.  Good place for weak
                     * references, if we had them.  */
                    try {
                        // stop recursion.
                        cx.iterating.put(thisObj, Boolean.TRUE);
                        if (toLocale && elem != Undefined.instance && 
                            elem != null) 
                        {
                            Scriptable obj = cx.toObject(elem, thisObj);
                            Object tls = ScriptRuntime.getProp(obj, 
                                             "toLocaleString", thisObj);
                            elem = ScriptRuntime.call(cx, tls, elem, 
                                                      ScriptRuntime.emptyArgs);
                        }
                        result.append(ScriptRuntime.toString(elem));
                    } finally {
                        cx.iterating.remove(thisObj);
                    }
                }
            }
        }

        if (toSource) {
            //for [,,].length behavior; we want toString to be symmetric.
            if (!haslast && i > 0)
                result.append(", ]");
            else
                result.append(']');
        }
        return result.toString();
    }

    /**
     * See ECMA 15.4.4.3
     */
    private static String jsFunction_join(Context cx, Scriptable thisObj,
                                          Object[] args) 
    {
        StringBuffer result = new StringBuffer();
        String separator;

        double length = getLengthProperty(thisObj);

        // if no args, use "," as separator
        if (args.length < 1) {
            separator = ",";
        } else {
            separator = ScriptRuntime.toString(args[0]);
        }
        for (long i=0; i < length; i++) {
            if (i > 0)
                result.append(separator);
            Object temp = getElem(thisObj, i);
            if (temp == null || temp == Undefined.instance)
                continue;
            result.append(ScriptRuntime.toString(temp));
        }
        return result.toString();
    }

    /**
     * See ECMA 15.4.4.4
     */
    private static Scriptable jsFunction_reverse(Context cx, 
                                                 Scriptable thisObj, 
                                                 Object[] args) 
    {
        long len = (long)getLengthProperty(thisObj);

        long half = len / 2;
        for(long i=0; i < half; i++) {
            long j = len - i - 1;
            Object temp1 = getElem(thisObj, i);
            Object temp2 = getElem(thisObj, j);
            setElem(thisObj, i, temp2);
            setElem(thisObj, j, temp1);
        }
        return thisObj;
    }

    /**
     * See ECMA 15.4.4.5
     */
    private static Scriptable jsFunction_sort(Context cx, Scriptable scope,
                                              Scriptable thisObj, 
                                              Object[] args)
        throws JavaScriptException
    {
        long length = (long)getLengthProperty(thisObj);

        Object compare;
        if (args.length > 0 && Undefined.instance != args[0])
            // sort with given compare function
            compare = args[0];
        else
            // sort with default compare
            compare = null;


        // OPT: Would it make sense to use the extended sort for very small
        // arrays?

        // Should we use the extended sort function, or the faster one?
        if (length >= Integer.MAX_VALUE) {
            qsort_extended(cx, compare, thisObj, 0, length - 1);
        } else {
            // copy the JS array into a working array, so it can be
            // sorted cheaply.
            Object[] working = new Object[(int)length];
            for (int i=0; i<length; i++) {
                working[i] = getElem(thisObj, i);
            }

            qsort(cx, compare, working, 0, (int)length - 1, scope);

            // copy the working array back into thisObj
            for (int i=0; i<length; i++) {
                setElem(thisObj, i, working[i]);
            }
        }
        return thisObj;
    }

    private static double qsortCompare(Context cx, Object jsCompare, Object x,
                                       Object y, Scriptable scope)
        throws JavaScriptException
    {
        Object undef = Undefined.instance;

        // sort undefined to end
        if (undef == x || undef == y) {
            if (undef != x)
                return -1;
            if (undef != y)
                return 1;
            return 0;
        }

        if (jsCompare == null) {
            // if no compare function supplied, sort lexicographically
            String a = ScriptRuntime.toString(x);
            String b = ScriptRuntime.toString(y);

            return a.compareTo(b);
        } else {
            // assemble args and call supplied JS compare function
            // OPT: put this argument creation in the caller and reuse it.
            // XXX what to do when compare function returns NaN?  ECMA states
            // that it's then not a 'consistent compararison function'... but
            // then what do we do?  Back out and start over with the generic
            // compare function when we see a NaN?  Throw an error?
            Object[] args = {x, y};
//             return ScriptRuntime.toNumber(ScriptRuntime.call(jsCompare, null,
//                                                              args));
            // for now, just ignore it:
            double d = ScriptRuntime.
                toNumber(ScriptRuntime.call(cx, jsCompare, null, args, scope));

            return (d == d) ? d : 0;
        }
    }

    private static void qsort(Context cx, Object jsCompare, Object[] working,
                              int lo, int hi, Scriptable scope)
        throws JavaScriptException
    {
        Object pivot;
        int i, j;
        int a, b;

        while (lo < hi) {
            i = lo;
            j = hi;
            a = i;
            pivot = working[a];
            while (i < j) {
                for(;;) {
                    b = j;
                    if (qsortCompare(cx, jsCompare, working[j], pivot, 
                                     scope) <= 0)
                        break;
                    j--;
                }
                working[a] = working[b];
                while (i < j && qsortCompare(cx, jsCompare, working[a],
                                             pivot, scope) <= 0)
                {
                    i++;
                    a = i;
                }
                working[b] = working[a];
            }
            working[a] = pivot;
            if (i - lo < hi - i) {
                qsort(cx, jsCompare, working, lo, i - 1, scope);
                lo = i + 1;
            } else {
                qsort(cx, jsCompare, working, i + 1, hi, scope);
                hi = i - 1;
            }
        }
    }

    // A version that knows about long indices and doesn't use
    // a working array.  Probably will never be used.
    private static void qsort_extended(Context cx, Object jsCompare,
                                       Scriptable target, long lo, long hi)
        throws JavaScriptException
    {
        Object pivot;
        long i, j;
        long a, b;

        while (lo < hi) {
            i = lo;
            j = hi;
            a = i;
            pivot = getElem(target, a);
            while (i < j) {
                for(;;) {
                    b = j;
                    if (qsortCompare(cx, jsCompare, getElem(target, j),
                                     pivot, target) <= 0)
                        break;
                    j--;
                }
                setElem(target, a, getElem(target, b));
                while (i < j && qsortCompare(cx, jsCompare,
                                             getElem(target, a), 
                                             pivot, target) <= 0)
                {
                    i++;
                    a = i;
                }
                setElem(target, b, getElem(target, a));
            }
            setElem(target, a, pivot);
            if (i - lo < hi - i) {
                qsort_extended(cx, jsCompare, target, lo, i - 1);
                lo = i + 1;
            } else {
                qsort_extended(cx, jsCompare, target, i + 1, hi);
                hi = i - 1;
            }
        }
    }

    /**
     * Non-ECMA methods.
     */

    private static Object jsFunction_push(Context cx, Scriptable thisObj,
                                          Object[] args)
    {
        double length = getLengthProperty(thisObj);
        for (int i = 0; i < args.length; i++) {
            setElem(thisObj, (long)length + i, args[i]);
        }

        length += args.length;
        ScriptRuntime.setProp(thisObj, "length", new Double(length), thisObj);

        /*
         * If JS1.2, follow Perl4 by returning the last thing pushed.
         * Otherwise, return the new array length.
         */
        if (cx.getLanguageVersion() == Context.VERSION_1_2)
            // if JS1.2 && no arguments, return undefined.
            return args.length == 0
                ? Context.getUndefinedValue()
                : args[args.length - 1];

        else
            return new Long((long)length);
    }

    private static Object jsFunction_pop(Context cx, Scriptable thisObj,
                                         Object[] args) {
        Object result;
        double length = getLengthProperty(thisObj);
        if (length > 0) {
            length--;

            // Get the to-be-deleted property's value.
            result = getElem(thisObj, (long)length);

            // We don't need to delete the last property, because
            // setLength does that for us.
        } else {
            result = Context.getUndefinedValue();
        }
        // necessary to match js even when length < 0; js pop will give a
        // length property to any target it is called on.
        ScriptRuntime.setProp(thisObj, "length", new Double(length), thisObj);

        return result;
    }

    private static Object jsFunction_shift(Context cx, Scriptable thisObj,
                                           Object[] args)
    {
        Object result;
        double length = getLengthProperty(thisObj);
        if (length > 0) {
            long i = 0;
            length--;

            // Get the to-be-deleted property's value.
            result = getElem(thisObj, i);

            /*
             * Slide down the array above the first element.  Leave i
             * set to point to the last element.
             */
            if (length > 0) {
                for (i = 1; i <= length; i++) {
                    Object temp = getElem(thisObj, i);
                    setElem(thisObj, i - 1, temp);
                }
            }
            // We don't need to delete the last property, because
            // setLength does that for us.
        } else {
            result = Context.getUndefinedValue();
        }
        ScriptRuntime.setProp(thisObj, "length", new Double(length), thisObj);
        return result;
    }

    private static Object jsFunction_unshift(Context cx, Scriptable thisObj,
                                             Object[] args)
    {
        Object result;
        double length = (double)getLengthProperty(thisObj);
        int argc = args.length;

        if (args.length > 0) {
            /*  Slide up the array to make room for args at the bottom */
            if (length > 0) {
                for (long last = (long)length - 1; last >= 0; last--) {
                    Object temp = getElem(thisObj, last);
                    setElem(thisObj, last + argc, temp);
                }
            }

            /* Copy from argv to the bottom of the array. */
            for (int i = 0; i < args.length; i++) {
                setElem(thisObj, i, args[i]);
            }

            /* Follow Perl by returning the new array length. */
            length += args.length;
            ScriptRuntime.setProp(thisObj, "length",
                                  new Double(length), thisObj);
        }
        return new Long((long)length);
    }

    private static Object jsFunction_splice(Context cx, Scriptable scope,
                                            Scriptable thisObj, Object[] args)
    {
        /* create an empty Array to return. */
        scope = getTopLevelScope(scope);
        Object result = ScriptRuntime.newObject(cx, scope, "Array", null);
        int argc = args.length;
        if (argc == 0)
            return result;
        double length = getLengthProperty(thisObj);

        /* Convert the first argument into a starting index. */
        double begin = ScriptRuntime.toInteger(args[0]);
        double end;
        double delta;
        double count;

        if (begin < 0) {
            begin += length;
            if (begin < 0)
                begin = 0;
        } else if (begin > length) {
            begin = length;
        }
        argc--;

        /* Convert the second argument from a count into a fencepost index. */
        delta = length - begin;

        if (args.length == 1) {
            count = delta;
            end = length;
        } else {
            count = ScriptRuntime.toInteger(args[1]);
            if (count < 0)
                count = 0;
            else if (count > delta)
                count = delta;
            end = begin + count;

            argc--;
        }

        long lbegin = (long)begin;
        long lend = (long)end;

        /* If there are elements to remove, put them into the return value. */
        if (count > 0) {
            if (count == 1
                && (cx.getLanguageVersion() == Context.VERSION_1_2))
            {
                /*
                 * JS lacks "list context", whereby in Perl one turns the
                 * single scalar that's spliced out into an array just by
                 * assigning it to @single instead of $single, or by using it
                 * as Perl push's first argument, for instance.
                 *
                 * JS1.2 emulated Perl too closely and returned a non-Array for
                 * the single-splice-out case, requiring callers to test and
                 * wrap in [] if necessary.  So JS1.3, default, and other
                 * versions all return an array of length 1 for uniformity.
                 */
                result = getElem(thisObj, lbegin);
            } else {
                for (long last = lbegin; last < lend; last++) {
                    Scriptable resultArray = (Scriptable)result;
                    Object temp = getElem(thisObj, last);
                    setElem(resultArray, last - lbegin, temp);
                }
            }
        } else if (count == 0
                   && cx.getLanguageVersion() == Context.VERSION_1_2)
        {
            /* Emulate C JS1.2; if no elements are removed, return undefined. */
            result = Context.getUndefinedValue();
        }

        /* Find the direction (up or down) to copy and make way for argv. */
        delta = argc - count;

        if (delta > 0) {
            for (long last = (long)length - 1; last >= lend; last--) {
                Object temp = getElem(thisObj, last);
                setElem(thisObj, last + (long)delta, temp);
            }
        } else if (delta < 0) {
            for (long last = lend; last < length; last++) {
                Object temp = getElem(thisObj, last);
                setElem(thisObj, last + (long)delta, temp);
            }
        }

        /* Copy from argv into the hole to complete the splice. */
        int argoffset = args.length - argc;
        for (int i = 0; i < argc; i++) {
            setElem(thisObj, lbegin + i, args[i + argoffset]);
        }

        /* Update length in case we deleted elements from the end. */
        ScriptRuntime.setProp(thisObj, "length",
                              new Double(length + delta), thisObj);
        return result;
    }

    /*
     * Python-esque sequence operations.
     */
    private static Scriptable jsFunction_concat(Context cx, Scriptable scope, 
                                                Scriptable thisObj,
                                                Object[] args)
    {
        /* Concat tries to keep the definition of an array as general
         * as possible; if it finds that an object has a numeric
         * 'length' property, then it treats that object as an array.
         * This treats string atoms and string objects differently; as
         * string objects have a length property and are accessible by
         * index, they get exploded into arrays when added, while
         * atomic strings are just added as strings.
         */

        // create an empty Array to return.
        scope = getTopLevelScope(scope);
        Scriptable result = ScriptRuntime.newObject(cx, scope, "Array", null);
        double length;
        long slot = 0;

        /* Put the target in the result array; only add it as an array
         * if it looks like one.
         */
        if (hasLengthProperty(thisObj)) {
            length = getLengthProperty(thisObj);

            // Copy from the target object into the result
            for (slot = 0; slot < length; slot++) {
                Object temp = getElem(thisObj, slot);
                setElem(result, slot, temp);
            }
        } else {
            setElem(result, slot++, thisObj);
        }

        /* Copy from the arguments into the result.  If any argument
         * has a numeric length property, treat it as an array and add
         * elements separately; otherwise, just copy the argument.
         */
        for (int i = 0; i < args.length; i++) {
            if (hasLengthProperty(args[i])) {
                // hasLengthProperty => instanceOf Scriptable.
                Scriptable arg = (Scriptable)args[i];
                length = getLengthProperty(arg);
                for (long j = 0; j < length; j++, slot++) {
                    Object temp = getElem(arg, j);
                    setElem(result, slot, temp);
                }
            } else {
                setElem(result, slot++, args[i]);
            }
        }
        return result;
    }

    private Scriptable jsFunction_slice(Context cx, Scriptable thisObj,
                                        Object[] args)
    {
        Scriptable scope = getTopLevelScope(this);
        Scriptable result = ScriptRuntime.newObject(cx, scope, "Array", null);
        double length = getLengthProperty(thisObj);

        double begin = 0;
        double end = length;

        if (args.length > 0) {
            begin = ScriptRuntime.toInteger(args[0]);
            if (begin < 0) {
                begin += length;
                if (begin < 0)
                    begin = 0;
            } else if (begin > length) {
                begin = length;
            }

            if (args.length > 1) {
                end = ScriptRuntime.toInteger(args[1]);
                if (end < 0) {
                    end += length;
                    if (end < 0)
                        end = 0;
                } else if (end > length) {
                    end = length;
                }
            }
        }

        long lbegin = (long)begin;
        long lend = (long)end;
        for (long slot = lbegin; slot < lend; slot++) {
            Object temp = getElem(thisObj, slot);
            setElem(result, slot - lbegin, temp);
        }

        return result;
    }

    protected int maxInstanceId() { return MAX_INSTANCE_ID; }

    protected String getIdName(int id) {
        if (id == Id_length) { return "length"; }

        if (prototypeFlag) {
            switch (id) {
                case Id_constructor:     return "constructor";
                case Id_toString:        return "toString";
                case Id_toLocaleString:  return "toLocaleString";
                case Id_join:            return "join";
                case Id_reverse:         return "reverse";
                case Id_sort:            return "sort";
                case Id_push:            return "push";
                case Id_pop:             return "pop";
                case Id_shift:           return "shift";
                case Id_unshift:         return "unshift";
                case Id_splice:          return "splice";
                case Id_concat:          return "concat";
                case Id_slice:           return "slice";
            }
        }
        return null;
    }

    private static final int
        Id_length        =  1,
        MAX_INSTANCE_ID  =  1;

    protected int mapNameToId(String s) {
        if (s.equals("length")) { return Id_length; }
        else if (prototypeFlag) { 
            return toPrototypeId(s); 
        }
        return 0;
    }

// #string_id_map#

    private static int toPrototypeId(String s) {
        int id;
// #generated# Last update: 2001-04-23 11:46:01 GMT+02:00
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 3: X="pop";id=Id_pop; break L;
            case 4: c=s.charAt(0);
                if (c=='j') { X="join";id=Id_join; }
                else if (c=='p') { X="push";id=Id_push; }
                else if (c=='s') { X="sort";id=Id_sort; }
                break L;
            case 5: c=s.charAt(1);
                if (c=='h') { X="shift";id=Id_shift; }
                else if (c=='l') { X="slice";id=Id_slice; }
                break L;
            case 6: c=s.charAt(0);
                if (c=='c') { X="concat";id=Id_concat; }
                else if (c=='l') { X="length";id=Id_length; }
                else if (c=='s') { X="splice";id=Id_splice; }
                break L;
            case 7: c=s.charAt(0);
                if (c=='r') { X="reverse";id=Id_reverse; }
                else if (c=='u') { X="unshift";id=Id_unshift; }
                break L;
            case 8: X="toString";id=Id_toString; break L;
            case 11: X="constructor";id=Id_constructor; break L;
            case 14: X="toLocaleString";id=Id_toLocaleString; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor          = MAX_INSTANCE_ID + 1,
        Id_toString             = MAX_INSTANCE_ID + 2,
        Id_toLocaleString       = MAX_INSTANCE_ID + 3,
        Id_join                 = MAX_INSTANCE_ID + 4,
        Id_reverse              = MAX_INSTANCE_ID + 5,
        Id_sort                 = MAX_INSTANCE_ID + 6,
        Id_push                 = MAX_INSTANCE_ID + 7,
        Id_pop                  = MAX_INSTANCE_ID + 8,
        Id_shift                = MAX_INSTANCE_ID + 9,
        Id_unshift              = MAX_INSTANCE_ID + 10,
        Id_splice               = MAX_INSTANCE_ID + 11,
        Id_concat               = MAX_INSTANCE_ID + 12,
        Id_slice                = MAX_INSTANCE_ID + 13,

        MAX_PROTOTYPE_ID        = MAX_INSTANCE_ID + 13;

// #/string_id_map#

    private long length;
    private Object[] dense;
    private static final int maximumDenseLength = 10000;
    
    private boolean prototypeFlag;
}
