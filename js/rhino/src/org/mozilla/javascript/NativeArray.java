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

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeArray obj = new NativeArray();
        obj.prototypeFlag = true;
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    /**
     * Zero-parameter constructor: just used to create Array.prototype
     */
    private NativeArray()
    {
        dense = null;
        this.length = 0;
    }

    public NativeArray(long length)
    {
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

    public NativeArray(Object[] array)
    {
        dense = array;
        this.length = array.length;
    }

    public String getClassName()
    {
        return "Array";
    }

    protected int getIdAttributes(int id)
    {
        if (id == Id_length) {
            return DONTENUM | PERMANENT;
        }
        return super.getIdAttributes(id);
    }

    protected Object getIdValue(int id)
    {
        if (id == Id_length) {
            return wrap_double(length);
        }
        return super.getIdValue(id);
    }

    protected void setIdValue(int id, Object value)
    {
        if (id == Id_length) {
            setLength(value); return;
        }
        super.setIdValue(id, value);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (prototypeFlag) {
            switch (f.methodId) {
                case Id_constructor:
                    return jsConstructor(cx, scope, args, f, thisObj == null);

                case Id_toString:
                    return toStringHelper(cx, scope, thisObj,
                        cx.hasFeature(Context.FEATURE_TO_STRING_AS_SOURCE),
                        false);

                case Id_toLocaleString:
                    return toStringHelper(cx, scope, thisObj, false, true);

                case Id_toSource:
                    return toStringHelper(cx, scope, thisObj, true, false);

                case Id_join:
                    return js_join(cx, thisObj, args);

                case Id_reverse:
                    return js_reverse(cx, thisObj, args);

                case Id_sort:
                    return js_sort(cx, scope, thisObj, args);

                case Id_push:
                    return js_push(cx, thisObj, args);

                case Id_pop:
                    return js_pop(cx, thisObj, args);

                case Id_shift:
                    return js_shift(cx, thisObj, args);

                case Id_unshift:
                    return js_unshift(cx, thisObj, args);

                case Id_splice:
                    return js_splice(cx, scope, thisObj, args);

                case Id_concat:
                    return js_concat(cx, scope, thisObj, args);

                case Id_slice:
                    return js_slice(cx, thisObj, args);
            }
        }
        return super.execMethod(f, cx, scope, thisObj, args);
    }

    public Object get(int index, Scriptable start)
    {
        if (dense != null && 0 <= index && index < dense.length)
            return dense[index];
        return super.get(index, start);
    }

    public boolean has(int index, Scriptable start)
    {
        if (dense != null && 0 <= index && index < dense.length)
            return dense[index] != NOT_FOUND;
        return super.has(index, start);
    }

    // if id is an array index (ECMA 15.4.0), return the number,
    // otherwise return -1L
    private static long toArrayIndex(String id)
    {
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

    public void put(String id, Scriptable start, Object value)
    {
        super.put(id, start, value);
        if (start == this) {
            // If the object is sealed, super will throw exception
            long index = toArrayIndex(id);
            if (index >= length) {
                length = index + 1;
            }
        }
    }

    public void put(int index, Scriptable start, Object value)
    {
        if (start == this && !isSealed()
            && dense != null && 0 <= index && index < dense.length)
        {
            // If start == this && sealed, super will throw exception
            dense[index] = value;
        } else {
            super.put(index, start, value);
        }
        if (start == this) {
            // only set the array length if given an array index (ECMA 15.4.0)
            if (this.length <= index) {
                // avoid overflowing index!
                this.length = (long)index + 1;
            }
        }

    }

    public void delete(int index)
    {
        if (!isSealed()
            && dense != null && 0 <= index && index < dense.length)
        {
            dense[index] = NOT_FOUND;
        } else {
            super.delete(index);
        }
    }

    public Object[] getIds()
    {
        Object[] superIds = super.getIds();
        if (dense == null) { return superIds; }
        int N = dense.length;
        long currentLength = length;
        if (N > currentLength) {
            N = (int)currentLength;
        }
        if (N == 0) { return superIds; }
        int superLength = superIds.length;
        Object[] ids = new Object[N + superLength];
        // Make a copy of dense to be immune to removing
        // of array elems from other thread when calculating presentCount
        System.arraycopy(dense, 0, ids, 0, N);
        int presentCount = 0;
        for (int i = 0; i != N; ++i) {
            // Replace existing elements by their indexes
            if (ids[i] != NOT_FOUND) {
                ids[presentCount] = new Integer(i);
                ++presentCount;
            }
        }
        if (presentCount != N) {
            // dense contains deleted elems, need to shrink the result
            Object[] tmp = new Object[presentCount + superLength];
            System.arraycopy(ids, 0, tmp, 0, presentCount);
            ids = tmp;
        }
        System.arraycopy(superIds, 0, ids, presentCount, superLength);
        return ids;
    }

    public Object getDefaultValue(Class hint)
    {
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
        } else {
            Object arg0 = args[0];
            if (args.length > 1 || !(arg0 instanceof Number)) {
                return new NativeArray(args);
            } else {
                long len = ScriptRuntime.toUint32(arg0);
                if (len != ((Number)arg0).doubleValue())
                    throw Context.reportRuntimeError0("msg.arraylength.bad");
                return new NativeArray(len);
            }
        }
    }

    public long getLength() {
        return length;
    }

    /** @deprecated Use {@link #getLength()} instead. */
    public long jsGet_length() {
        return getLength();
    }

    private void setLength(Object val) {
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
     * property is assumed to be an array.
     * getLengthProperty returns 0 if obj does not have the length property
     * or its value is not convertible to a number.
     */
    static long getLengthProperty(Scriptable obj) {
        // These will both give numeric lengths within Uint32 range.
        if (obj instanceof NativeString) {
            return ((NativeString)obj).getLength();
        } else if (obj instanceof NativeArray) {
            return ((NativeArray)obj).getLength();
        } else if (!(obj instanceof Scriptable)) {
            return 0;
        }
        return ScriptRuntime.toUint32(ScriptRuntime
                                      .getProp(obj, "length", obj));
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
            return ScriptRuntime.getProp(target, id);
        } else {
            return ScriptRuntime.getElem(target, (int)index);
        }
    }

    private static void setElem(Scriptable target, long index, Object value) {
        if (index > Integer.MAX_VALUE) {
            String id = Long.toString(index);
            ScriptRuntime.setProp(target, id, value);
        } else {
            ScriptRuntime.setElem(target, (int)index, value);
        }
    }

    private static String toStringHelper(Context cx, Scriptable scope,
                                         Scriptable thisObj,
                                         boolean toSource, boolean toLocale)
        throws JavaScriptException
    {
        /* It's probably redundant to handle long lengths in this
         * function; StringBuffers are limited to 2^31 in java.
         */

        long length = getLengthProperty(thisObj);

        StringBuffer result = new StringBuffer(256);

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

        boolean toplevel, iterating;
        if (cx.iterating == null) {
            toplevel = true;
            iterating = false;
            cx.iterating = new ObjToIntMap(31);
        } else {
            toplevel = false;
            iterating = cx.iterating.has(thisObj);
        }

        // Make sure cx.iterating is set to null when done
        // so we don't leak memory
        try {
            if (!iterating) {
                cx.iterating.put(thisObj, 0); // stop recursion.
                for (i = 0; i < length; i++) {
                    if (i > 0) result.append(separator);
                    Object elem = getElem(thisObj, i);
                    if (elem == null || elem == Undefined.instance) {
                        haslast = false;
                        continue;
                    }
                    haslast = true;

                    if (toSource) {
                        result.append(ScriptRuntime.uneval(cx, scope, elem));

                    } else if (elem instanceof String) {
                        String s = (String)elem;
                        if (toSource) {
                            result.append('\"');
                            result.append(ScriptRuntime.escapeString(s));
                            result.append('\"');
                        } else {
                            result.append(s);
                        }

                    } else {
                        if (toLocale && elem != Undefined.instance &&
                            elem != null)
                        {
                            Scriptable obj = ScriptRuntime.
                                    toObject(cx, thisObj, elem);
                            Object tls = ScriptRuntime.
                                    getProp(obj, "toLocaleString", thisObj);
                            elem = ScriptRuntime.call(cx, tls, elem,
                                                      ScriptRuntime.emptyArgs);
                        }
                        result.append(ScriptRuntime.toString(elem));
                    }
                }
            }
        } finally {
            if (toplevel) {
                cx.iterating = null;
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
    private static String js_join(Context cx, Scriptable thisObj,
                                  Object[] args)
    {
        String separator;

        long llength = getLengthProperty(thisObj);
        int length = (int)llength;
        if (llength != length) {
            throw Context.reportRuntimeError1(
                "msg.arraylength.too.big", String.valueOf(llength));
        }
        // if no args, use "," as separator
        if (args.length < 1 || args[0] == Undefined.instance) {
            separator = ",";
        } else {
            separator = ScriptRuntime.toString(args[0]);
        }
        if (length == 0) {
            return "";
        }
        String[] buf = new String[length];
        int total_size = 0;
        for (int i = 0; i != length; i++) {
            Object temp = getElem(thisObj, i);
            if (temp != null && temp != Undefined.instance) {
                String str = ScriptRuntime.toString(temp);
                total_size += str.length();
                buf[i] = str;
            }
        }
        total_size += (length - 1) * separator.length();
        StringBuffer sb = new StringBuffer(total_size);
        for (int i = 0; i != length; i++) {
            if (i != 0) {
                sb.append(separator);
            }
            String str = buf[i];
            if (str != null) {
                // str == null for undefined or null
                sb.append(str);
            }
        }
        return sb.toString();
    }

    /**
     * See ECMA 15.4.4.4
     */
    private static Scriptable js_reverse(Context cx, Scriptable thisObj,
                                         Object[] args)
    {
        long len = getLengthProperty(thisObj);

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
    private static Scriptable js_sort(Context cx, Scriptable scope,
                                      Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        long length = getLengthProperty(thisObj);

        if (length <= 1) { return thisObj; }

        Object compare;
        Object[] cmpBuf;

        if (args.length > 0 && Undefined.instance != args[0]) {
            // sort with given compare function
            compare = args[0];
            cmpBuf = new Object[2]; // Buffer for cmp arguments
        } else {
            // sort with default compare
            compare = null;
            cmpBuf = null;
        }

        // Should we use the extended sort function, or the faster one?
        if (length >= Integer.MAX_VALUE) {
            heapsort_extended(cx, scope, thisObj, length, compare, cmpBuf);
        }
        else {
            int ilength = (int)length;
            // copy the JS array into a working array, so it can be
            // sorted cheaply.
            Object[] working = new Object[ilength];
            for (int i = 0; i != ilength; ++i) {
                working[i] = getElem(thisObj, i);
            }

            heapsort(cx, scope, working, ilength, compare, cmpBuf);

            // copy the working array back into thisObj
            for (int i = 0; i != ilength; ++i) {
                setElem(thisObj, i, working[i]);
            }
        }
        return thisObj;
    }

    // Return true only if x > y
    private static boolean isBigger(Context cx, Scriptable scope,
                                    Object x, Object y,
                                    Object cmp, Object[] cmpBuf)
        throws JavaScriptException
    {
        if (check) {
            if (cmp == null) {
                if (cmpBuf != null) Kit.codeBug();
            } else {
                if (cmpBuf == null || cmpBuf.length != 2) Kit.codeBug();
            }
        }

        Object undef = Undefined.instance;

        // sort undefined to end
        if (undef == y) {
            return false; // x can not be bigger then undef
        } else if (undef == x) {
            return true; // y != undef here, so x > y
        }

        if (cmp == null) {
            // if no cmp function supplied, sort lexicographically
            String a = ScriptRuntime.toString(x);
            String b = ScriptRuntime.toString(y);
            return a.compareTo(b) > 0;
        }
        else {
            // assemble args and call supplied JS cmp function
            cmpBuf[0] = x;
            cmpBuf[1] = y;
            Object ret = ScriptRuntime.call(cx, cmp, null, cmpBuf, scope);
            double d = ScriptRuntime.toNumber(ret);

            // XXX what to do when cmp function returns NaN?  ECMA states
            // that it's then not a 'consistent compararison function'... but
            // then what do we do?  Back out and start over with the generic
            // cmp function when we see a NaN?  Throw an error?

            // for now, just ignore it:

            return d > 0;
        }
    }

/** Heapsort implementation.
 * See "Introduction to Algorithms" by Cormen, Leiserson, Rivest for details.
 * Adjusted for zero based indexes.
 */
    private static void heapsort(Context cx, Scriptable scope,
                                 Object[] array, int length,
                                 Object cmp, Object[] cmpBuf)
        throws JavaScriptException
    {
        if (check && length <= 1) Kit.codeBug();

        // Build heap
        for (int i = length / 2; i != 0;) {
            --i;
            Object pivot = array[i];
            heapify(cx, scope, pivot, array, i, length, cmp, cmpBuf);
        }

        // Sort heap
        for (int i = length; i != 1;) {
            --i;
            Object pivot = array[i];
            array[i] = array[0];
            heapify(cx, scope, pivot, array, 0, i, cmp, cmpBuf);
        }
    }

/** pivot and child heaps of i should be made into heap starting at i,
 * original array[i] is never used to have less array access during sorting.
 */
    private static void heapify(Context cx, Scriptable scope,
                                Object pivot, Object[] array, int i, int end,
                                Object cmp, Object[] cmpBuf)
        throws JavaScriptException
    {
        for (;;) {
            int child = i * 2 + 1;
            if (child >= end) {
                break;
            }
            Object childVal = array[child];
            if (child + 1 < end) {
                Object nextVal = array[child + 1];
                if (isBigger(cx, scope, nextVal, childVal, cmp, cmpBuf)) {
                    ++child; childVal = nextVal;
                }
            }
            if (!isBigger(cx, scope, childVal, pivot, cmp, cmpBuf)) {
                break;
            }
            array[i] = childVal;
            i = child;
        }
        array[i] = pivot;
    }

/** Version of heapsort that call getElem/setElem on target to query/assign
 * array elements instead of Java array access
 */
    private static void heapsort_extended(Context cx, Scriptable scope,
                                          Scriptable target, long length,
                                          Object cmp, Object[] cmpBuf)
        throws JavaScriptException
    {
        if (check && length <= 1) Kit.codeBug();

        // Build heap
        for (long i = length / 2; i != 0;) {
            --i;
            Object pivot = getElem(target, i);
            heapify_extended(cx, scope, pivot, target, i, length, cmp, cmpBuf);
        }

        // Sort heap
        for (long i = length; i != 1;) {
            --i;
            Object pivot = getElem(target, i);
            setElem(target, i, getElem(target, 0));
            heapify_extended(cx, scope, pivot, target, 0, i, cmp, cmpBuf);
        }
    }

    private static void heapify_extended(Context cx, Scriptable scope,
                                         Object pivot, Scriptable target,
                                         long i, long end,
                                         Object cmp, Object[] cmpBuf)
        throws JavaScriptException
    {
        for (;;) {
            long child = i * 2 + 1;
            if (child >= end) {
                break;
            }
            Object childVal = getElem(target, child);
            if (child + 1 < end) {
                Object nextVal = getElem(target, child + 1);
                if (isBigger(cx, scope, nextVal, childVal, cmp, cmpBuf)) {
                    ++child; childVal = nextVal;
                }
            }
            if (!isBigger(cx, scope, childVal, pivot, cmp, cmpBuf)) {
                break;
            }
            setElem(target, i, childVal);
            i = child;
        }
        setElem(target, i, pivot);
    }

    /**
     * Non-ECMA methods.
     */

    private static Object js_push(Context cx, Scriptable thisObj,
                                  Object[] args)
    {
        long length = getLengthProperty(thisObj);
        for (int i = 0; i < args.length; i++) {
            setElem(thisObj, length + i, args[i]);
        }

        length += args.length;
        Double lengthObj = new Double(length);
        ScriptRuntime.setProp(thisObj, "length", lengthObj);

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
            return lengthObj;
    }

    private static Object js_pop(Context cx, Scriptable thisObj,
                                 Object[] args)
    {
        Object result;
        long length = getLengthProperty(thisObj);
        if (length > 0) {
            length--;

            // Get the to-be-deleted property's value.
            result = getElem(thisObj, length);

            // We don't need to delete the last property, because
            // setLength does that for us.
        } else {
            result = Context.getUndefinedValue();
        }
        // necessary to match js even when length < 0; js pop will give a
        // length property to any target it is called on.
        ScriptRuntime.setProp(thisObj, "length", new Double(length));

        return result;
    }

    private static Object js_shift(Context cx, Scriptable thisObj,
                                   Object[] args)
    {
        Object result;
        long length = getLengthProperty(thisObj);
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
        ScriptRuntime.setProp(thisObj, "length", new Double(length));
        return result;
    }

    private static Object js_unshift(Context cx, Scriptable thisObj,
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
            ScriptRuntime.setProp(thisObj, "length", new Double(length));
        }
        return new Long((long)length);
    }

    private static Object js_splice(Context cx, Scriptable scope,
                                    Scriptable thisObj, Object[] args)
    {
        /* create an empty Array to return. */
        scope = getTopLevelScope(scope);
        Object result = ScriptRuntime.newObject(cx, scope, "Array", null);
        int argc = args.length;
        if (argc == 0)
            return result;
        long length = getLengthProperty(thisObj);

        /* Convert the first argument into a starting index. */
        long begin = toSliceIndex(ScriptRuntime.toInteger(args[0]), length);
        argc--;

        /* Convert the second argument into count */
        long count;
        if (args.length == 1) {
            count = length - begin;
        } else {
            double dcount = ScriptRuntime.toInteger(args[1]);
            if (dcount < 0) {
                count = 0;
            } else if (dcount > (length - begin)) {
                count = length - begin;
            } else {
                count = (long)dcount;
            }
            argc--;
        }

        long end = begin + count;

        /* If there are elements to remove, put them into the return value. */
        if (count != 0) {
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
                result = getElem(thisObj, begin);
            } else {
                for (long last = begin; last != end; last++) {
                    Scriptable resultArray = (Scriptable)result;
                    Object temp = getElem(thisObj, last);
                    setElem(resultArray, last - begin, temp);
                }
            }
        } else if (count == 0
                   && cx.getLanguageVersion() == Context.VERSION_1_2)
        {
            /* Emulate C JS1.2; if no elements are removed, return undefined. */
            result = Context.getUndefinedValue();
        }

        /* Find the direction (up or down) to copy and make way for argv. */
        long delta = argc - count;

        if (delta > 0) {
            for (long last = length - 1; last >= end; last--) {
                Object temp = getElem(thisObj, last);
                setElem(thisObj, last + delta, temp);
            }
        } else if (delta < 0) {
            for (long last = end; last < length; last++) {
                Object temp = getElem(thisObj, last);
                setElem(thisObj, last + delta, temp);
            }
        }

        /* Copy from argv into the hole to complete the splice. */
        int argoffset = args.length - argc;
        for (int i = 0; i < argc; i++) {
            setElem(thisObj, begin + i, args[i + argoffset]);
        }

        /* Update length in case we deleted elements from the end. */
        ScriptRuntime.setProp(thisObj, "length",
                              new Double(length + delta));
        return result;
    }

    /*
     * See Ecma 262v3 15.4.4.4
     */
    private static Scriptable js_concat(Context cx, Scriptable scope,
                                        Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        // create an empty Array to return.
        scope = getTopLevelScope(scope);
        Function ctor = ScriptRuntime.getExistingCtor(cx, scope, "Array");
        Scriptable result = ctor.construct(cx, scope, ScriptRuntime.emptyArgs);
        long length;
        long slot = 0;

        /* Put the target in the result array; only add it as an array
         * if it looks like one.
         */
        if (ScriptRuntime.instanceOf(thisObj, ctor, scope)) {
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
            if (ScriptRuntime.instanceOf(args[i], ctor, scope)) {
                // ScriptRuntime.instanceOf => instanceof Scriptable
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

    private Scriptable js_slice(Context cx, Scriptable thisObj,
                                Object[] args)
    {
        Scriptable scope = getTopLevelScope(this);
        Scriptable result = ScriptRuntime.newObject(cx, scope, "Array", null);
        long length = getLengthProperty(thisObj);

        long begin, end;
        if (args.length == 0) {
            begin = 0;
            end = length;
        } else {
            begin = toSliceIndex(ScriptRuntime.toInteger(args[0]), length);
            if (args.length == 1) {
                end = length;
            } else {
                end = toSliceIndex(ScriptRuntime.toInteger(args[1]), length);
            }
        }

        for (long slot = begin; slot < end; slot++) {
            Object temp = getElem(thisObj, slot);
            setElem(result, slot - begin, temp);
        }

        return result;
    }

    private static long toSliceIndex(double value, long length) {
        long result;
        if (value < 0.0) {
            if (value + length < 0.0) {
                result = 0;
            } else {
                result = (long)(value + length);
            }
        } else if (value > length) {
            result = length;
        } else {
            result = (long)value;
        }
        return result;
    }

    protected String getIdName(int id)
    {
        if (id == Id_length) { return "length"; }

        if (prototypeFlag) {
            switch (id) {
                case Id_constructor:     return "constructor";
                case Id_toString:        return "toString";
                case Id_toLocaleString:  return "toLocaleString";
                case Id_toSource:        return "toSource";
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

    protected int methodArity(int methodId)
    {
        if (prototypeFlag) {
            switch (methodId) {
                case Id_constructor:     return 1;
                case Id_toString:        return 0;
                case Id_toLocaleString:  return 1;
                case Id_toSource:        return 0;
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

    private static final int
        Id_length        =  1,
        MAX_INSTANCE_ID  =  1;

    { setMaxId(MAX_INSTANCE_ID); }

    protected int mapNameToId(String s)
    {
        if (s.equals("length")) { return Id_length; }
        else if (prototypeFlag) {
            return toPrototypeId(s);
        }
        return 0;
    }

// #string_id_map#

    private static int toPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2004-03-17 13:17:02 CET
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
                else if (c=='s') { X="splice";id=Id_splice; }
                break L;
            case 7: c=s.charAt(0);
                if (c=='r') { X="reverse";id=Id_reverse; }
                else if (c=='u') { X="unshift";id=Id_unshift; }
                break L;
            case 8: c=s.charAt(3);
                if (c=='o') { X="toSource";id=Id_toSource; }
                else if (c=='t') { X="toString";id=Id_toString; }
                break L;
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
        Id_toSource             = MAX_INSTANCE_ID + 4,
        Id_join                 = MAX_INSTANCE_ID + 5,
        Id_reverse              = MAX_INSTANCE_ID + 6,
        Id_sort                 = MAX_INSTANCE_ID + 7,
        Id_push                 = MAX_INSTANCE_ID + 8,
        Id_pop                  = MAX_INSTANCE_ID + 9,
        Id_shift                = MAX_INSTANCE_ID + 10,
        Id_unshift              = MAX_INSTANCE_ID + 11,
        Id_splice               = MAX_INSTANCE_ID + 12,
        Id_concat               = MAX_INSTANCE_ID + 13,
        Id_slice                = MAX_INSTANCE_ID + 14,

        MAX_PROTOTYPE_ID        = MAX_INSTANCE_ID + 14;

// #/string_id_map#

    private long length;
    private Object[] dense;
    private static final int maximumDenseLength = 10000;

    private boolean prototypeFlag;

    private static final boolean check = true && Context.check;
}
