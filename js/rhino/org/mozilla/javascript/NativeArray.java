/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;
import java.util.Hashtable;

/**
 * This class implements the Array native object.
 * @author Norris Boyd
 * @author Mike McCabe
 */
public class NativeArray extends ScriptableObject {

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

    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
    {
        // Set some method length values.
        // See comment for NativeString.finishInit()

        String[] specialLengthNames = { "reverse",
                                        "toString",
                                      };

        short[] specialLengthValues = { 0,
                                        0,
                                      };

        for (int i=0; i < specialLengthNames.length; i++) {
            Object obj = proto.get(specialLengthNames[i], proto);
            ((FunctionObject) obj).setLength(specialLengthValues[i]);
        }
    }
    
    public String getClassName() {
        return "Array";
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

    public void put(String id, Scriptable start, Object value) {
        // only set the array length if given an array index (ECMA 15.4.0)

        // try to get an array index from id
        double d = ScriptRuntime.toNumber(id);

        if (ScriptRuntime.toUint32(d) == d &&
            ScriptRuntime.numberToString(d).equals(id) &&
            this.length <= d && d != 4294967295.0)
            this.length = (long)d + 1;

        super.put(id, start, value);
    }

    public void put(int index, Scriptable start, Object value) {
        // only set the array length if given an array index (ECMA 15.4.0)

        if (this.length <= index) {
            // avoid overflowing index!
            this.length = (long)index + 1;
        }

        if (dense != null && 0 <= index && index < dense.length) {
            dense[index] = value;
            return;
        }

        super.put(index, start, value);
    }

    public void delete(int index) {
        if (dense != null && 0 <= index && index < dense.length) {
            dense[index] = NOT_FOUND;
            return;
        }
        super.delete(index);
    }

    public Object[] getIds() {
        Object[] superIds = super.getIds();
        if (dense == null)
            return superIds;
        int count = 0;
        int last = dense.length;
        if (last > length)
            last = (int) length;
        for (int i=last-1; i >= 0; i--) {
            if (dense[i] != NOT_FOUND)
                count++;
        }
        count += superIds.length;
        Object[] result = new Object[count];
        System.arraycopy(superIds, 0, result, 0, superIds.length);
        for (int i=last-1; i >= 0; i--) {
            if (dense[i] != NOT_FOUND)
                result[--count] = new Integer(i);
        }
        return result;
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
    public static Object js_Array(Context cx, Object[] args, Function ctorObj,
			                      boolean inNewExpr)
        throws JavaScriptException
    {
        if (!inNewExpr) {
            // FunctionObject.construct will set up parent, proto
            return ctorObj.construct(cx, ctorObj.getParentScope(), args);
        }
        if (args.length == 0)
            return new NativeArray();

        // Only use 1 arg as first element for version 1.2; for
        // any other version (including 1.3) follow ECMA and use it as
        // a length.
        if (args.length == 1 && args[0] instanceof Number &&
            cx.getLanguageVersion() != cx.VERSION_1_2)
        {
            return new NativeArray(ScriptRuntime.toUint32(args[0]));
        }

        // initialize array with arguments
        return new NativeArray(args);
    }

    private static final int lengthAttr = ScriptableObject.DONTENUM |
                                          ScriptableObject.PERMANENT;

    public long js_getLength() {
        return length;
    }

    public void js_setLength(Object val) {
        /* XXX do we satisfy this?
         * 15.4.5.1 [[Put]](P, V):
         * 1. Call the [[CanPut]] method of A with name P.
         * 2. If Result(1) is false, return.
         * ?
         */

        long longVal = ScriptRuntime.toUint32(val);

        if (longVal < length) {
            // remove all properties between longVal and length
            if (length - longVal > 0x1000) {
                // assume that the representation is sparse
                Object[] e = getIds(); // will only find in object itself
                for (int i=0; i < e.length; i++) {
                    if (e[i] instanceof String) {
                        // > MAXINT will appear as string
                        String id = (String) e[i];
                        double d = ScriptRuntime.toNumber(id);
                        if (d == d && d < length)
                            delete(id);
                        continue;
                    }
                    int index = ((Number) e[i]).intValue();
                    if (index >= longVal)
                        delete(index);
                }
            } else {
                // assume a dense representation
                for (long i=longVal; i < length; i++) {
                    // only delete if defined in the object itself
                    if (hasElem(this, i))
                        ScriptRuntime.delete(this, new Long(i));
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
            return (double)((NativeString)obj).js_getLength();
        if (obj instanceof NativeArray)
            return (double)((NativeArray)obj).js_getLength();
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
    private static boolean hasElem(Scriptable target, long index) {
        return index > Integer.MAX_VALUE
               ? target.has(Long.toString(index), target)
               : target.has((int)index, target);
    }

    private static Object getElem(Scriptable target, long index) {
        if (index > Integer.MAX_VALUE) {
            String id = Long.toString(index);
            return ScriptRuntime.getElem(target, id, target);
        } else {
            return ScriptRuntime.getElem(target, (int)index);
        }
    }

    private static void setElem(Scriptable target, long index, Object value) {
        if (index > Integer.MAX_VALUE) {
            String id = Long.toString(index);
            ScriptRuntime.setElem(target, id, value, target);
        } else {
            ScriptRuntime.setElem(target, (int)index, value);
        }
    }

    public static String js_toString(Context cx, Scriptable thisObj,
                                     Object[] args, Function funObj)
    {
        return toStringHelper(cx, thisObj, 
                              cx.getLanguageVersion() == cx.VERSION_1_2);
    }
    
    private static String toStringHelper(Context cx, Scriptable thisObj,
                                         boolean toSource)
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
            result.append("[");
            separator = ", ";
        } else {
            separator = ",";
        }

        boolean haslast = false;
        long i = 0;

        if (!iterating) {
            for (i = 0; i < length; i++) {
                if (i > 0)
                    result.append(separator);
                Object elem = getElem(thisObj, i);
                if (elem == null || elem == Undefined.instance) {
                    haslast = false;
                    continue;
                }
                haslast = true;

                if (elem instanceof String) {
                    if (toSource) {
                        result.append("\"");
                        result.append(ScriptRuntime.escapeString
                                      (ScriptRuntime.toString(elem)));
                        result.append("\"");
                    } else {
                        result.append(ScriptRuntime.toString(elem));
                    }
                } else {
                    /* wrap changes to cx.iterating in a try/finally
                     * so that the reference always gets removed, and
                     * we don't leak memory.  Good place for weak
                     * references, if we had them.  */
                    try {
                        // stop recursion.
                        cx.iterating.put(thisObj, Boolean.TRUE);
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
                result.append("]");
        }
        return result.toString();
    }

    /**
     * See ECMA 15.4.4.3
     */
    public static String js_join(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
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
    public static Scriptable js_reverse(Context cx, Scriptable thisObj,
                                        Object[] args, Function funObj)
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
    public static Scriptable js_sort(Context cx, Scriptable thisObj,
                                     Object[] args, Function funObj)
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

            qsort(cx, compare, working, 0, (int)length - 1);

            // copy the working array back into thisObj
            for (int i=0; i<length; i++) {
                setElem(thisObj, i, working[i]);
            }
        }
        return thisObj;
    }

    private static double qsortCompare(Context cx, Object jsCompare, Object x,
                                       Object y)
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
                toNumber(ScriptRuntime.call(cx, jsCompare, null, args));

            return (d == d) ? d : 0;
        }
    }

    private static void qsort(Context cx, Object jsCompare, Object[] working,
                              int lo, int hi)
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
                    if (qsortCompare(cx, jsCompare, working[j], pivot) <= 0)
                        break;
                    j--;
                }
                working[a] = working[b];
                while (i < j && qsortCompare(cx, jsCompare, working[a],
                                             pivot) <= 0)
                {
                    i++;
                    a = i;
                }
                working[b] = working[a];
            }
            working[a] = pivot;
            if (i - lo < hi - i) {
                qsort(cx, jsCompare, working, lo, i - 1);
                lo = i + 1;
            } else {
                qsort(cx, jsCompare, working, i + 1, hi);
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
                                     pivot) <= 0)
                        break;
                    j--;
                }
                setElem(target, a, getElem(target, b));
                while (i < j && qsortCompare(cx, jsCompare,
                                             getElem(target, a), pivot) <= 0)
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

    public static Object js_push(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
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

    public static Object js_pop(Context cx, Scriptable thisObj, Object[] args,
                                Function funObj)
    {
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

    public static Object js_shift(Context cx, Scriptable thisObj, Object[] args,
                                  Function funOjb)
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

    public static Object js_unshift(Context cx, Scriptable thisObj,
                                    Object[] args, Function funOjb)
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

    public static Object js_splice(Context cx, Scriptable thisObj, 
                                   Object[] args, Function funObj)
    {
        /* create an empty Array to return. */
        Scriptable scope = getTopLevelScope(funObj);
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
    public static Scriptable js_concat(Context cx, Scriptable thisObj,
                                       Object[] args, Function funObj)
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
        Scriptable scope = getTopLevelScope(funObj);
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

    public static Scriptable js_slice(Context cx, Scriptable thisObj,
                                      Object[] args, Function funObj)
    {
        Scriptable scope = getTopLevelScope(funObj);
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

    private long length;
    private Object[] dense;
    private static final int maximumDenseLength = 10000;
}
