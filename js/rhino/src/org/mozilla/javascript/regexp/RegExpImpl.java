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
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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

package org.mozilla.javascript.regexp;

import org.mozilla.javascript.*;

/**
 *
 */
public class RegExpImpl implements RegExpProxy {

    public boolean isRegExp(Scriptable obj) {
        return obj instanceof NativeRegExp;
    }

    public Object compileRegExp(Context cx, String source, String flags)
    {
        return NativeRegExp.compileRE(source, flags, false);
    }

    public Scriptable wrapRegExp(Context cx, Scriptable scope,
                                 Object compiled)
    {
        return new NativeRegExp(scope, compiled);
    }

    public Object match(Context cx, Scriptable scope,
                        Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        GlobData mdata = new GlobData();
        mdata.optarg = 1;
        mdata.mode = GlobData.GLOB_MATCH;
        Object rval = matchOrReplace(cx, scope, thisObj, args,
                                     this, mdata, false);
        return mdata.arrayobj == null ? rval : mdata.arrayobj;
    }

    public Object search(Context cx, Scriptable scope,
                         Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        GlobData mdata = new GlobData();
        mdata.optarg = 1;
        mdata.mode = GlobData.GLOB_SEARCH;
        return matchOrReplace(cx, scope, thisObj, args, this, mdata, false);
    }

    public Object replace(Context cx, Scriptable scope,
                          Scriptable thisObj, Object[] args)
        throws JavaScriptException
    {
        Object arg1 = args.length < 2 ? Undefined.instance : args[1];
        String repstr = null;
        Function lambda = null;
        if (arg1 instanceof Function) {
            lambda = (Function) arg1;
        } else {
            repstr = ScriptRuntime.toString(arg1);
        }

        GlobData rdata = new GlobData();
        rdata.optarg = 2;
        rdata.mode = GlobData.GLOB_REPLACE;
        rdata.lambda = lambda;
        rdata.repstr = repstr == null ? null : repstr.toCharArray();
        rdata.dollar = repstr == null ? -1 : repstr.indexOf('$');
        rdata.charArray = null;
        rdata.length = 0;
        rdata.index = 0;
        rdata.leftIndex = 0;
        Object val = matchOrReplace(cx, scope, thisObj, args,
                                    this, rdata, true);
        char[] charArray;

        if (rdata.charArray == null) {
            if (rdata.global || val == null || !val.equals(Boolean.TRUE)) {
                /* Didn't match even once. */
                return rdata.str;
            }
            int leftlen = this.leftContext.length;
            int length = leftlen + find_replen(rdata, cx, scope, this);
            charArray = new char[length];
            SubString leftContext = this.leftContext;
            System.arraycopy(leftContext.charArray, leftContext.index,
                             charArray, 0, leftlen);
            do_replace(rdata, cx, this, charArray, leftlen);
            rdata.charArray = charArray;
            rdata.length = length;
        }

        SubString rc = this.rightContext;
        int rightlen = rc.length;
        int length = rdata.length + rightlen;
        charArray = new char[length];
        System.arraycopy(rdata.charArray, 0,
                         charArray, 0, rdata.charArray.length);
        System.arraycopy(rc.charArray, rc.index, charArray,
                         rdata.length, rightlen);
        return new String(charArray, 0, length);
    }

    /**
     * Analog of C match_or_replace.
     */
    private static Object matchOrReplace(Context cx, Scriptable scope,
                                         Scriptable thisObj, Object[] args,
                                         RegExpImpl reImpl,
                                         GlobData data, boolean forceFlat)
        throws JavaScriptException
    {
        NativeRegExp re;

        String str = ScriptRuntime.toString(thisObj);
        data.str = str;
        Scriptable topScope = ScriptableObject.getTopLevelScope(scope);

        if (args.length == 0) {
            Object compiled = NativeRegExp.compileRE("", "", false);
            re = new NativeRegExp(topScope, compiled);
        } else if (args[0] instanceof NativeRegExp) {
            re = (NativeRegExp) args[0];
        } else {
            String src = ScriptRuntime.toString(args[0]);
            String opt;
            if (data.optarg < args.length) {
                args[0] = src;
                opt = ScriptRuntime.toString(args[data.optarg]);
            } else {
                opt = null;
            }
            Object compiled = NativeRegExp.compileRE(src, opt, forceFlat);
            re = new NativeRegExp(topScope, compiled);
        }
        data.regexp = re;

        data.global = (re.getFlags() & NativeRegExp.JSREG_GLOB) != 0;
        int[] indexp = { 0 };
        Object result = null;
        if (data.mode == GlobData.GLOB_SEARCH) {
            result = re.executeRegExp(cx, scope, reImpl,
                                      str, indexp, NativeRegExp.TEST);
            if (result != null && result.equals(Boolean.TRUE))
                result = new Integer(reImpl.leftContext.length);
            else
                result = new Integer(-1);
        } else if (data.global) {
            re.setLastIndex(0);
            for (int count = 0; indexp[0] <= str.length(); count++) {
                result = re.executeRegExp(cx, scope, reImpl,
                                          str, indexp, NativeRegExp.TEST);
                if (result == null || !result.equals(Boolean.TRUE))
                    break;
                if (data.mode == GlobData.GLOB_MATCH) {
                    match_glob(data, cx, scope, count, reImpl);
                } else {
                    replace_glob(data, cx, scope, count, reImpl);
                }
                if (reImpl.lastMatch.length == 0) {
                    if (indexp[0] == str.length())
                        break;
                    indexp[0]++;
                }
            }
        } else {
            result = re.executeRegExp(cx, scope, reImpl, str, indexp,
                                      ((data.mode == GlobData.GLOB_REPLACE)
                                       ? NativeRegExp.TEST
                                       : NativeRegExp.MATCH));
        }

        return result;
    }



    public int find_split(Context cx, Scriptable scope, String target,
                          String separator, Scriptable reObj,
                          int[] ip, int[] matchlen,
                          boolean[] matched, String[][] parensp)
    {
        int i = ip[0];
        int length = target.length();
        int result;

        int version = cx.getLanguageVersion();
        NativeRegExp re = (NativeRegExp) reObj;
        again:
        while (true) {  // imitating C label
            /* JS1.2 deviated from Perl by never matching at end of string. */
            int ipsave = ip[0]; // reuse ip to save object creation
            ip[0] = i;
            Object ret = re.executeRegExp(cx, scope, this, target, ip,
                                          NativeRegExp.TEST);
            if (ret != Boolean.TRUE) {
                // Mismatch: ensure our caller advances i past end of string.
                ip[0] = ipsave;
                matchlen[0] = 1;
                matched[0] = false;
                return length;
            }
            i = ip[0];
            ip[0] = ipsave;
            matched[0] = true;

            SubString sep = this.lastMatch;
            matchlen[0] = sep.length;
            if (matchlen[0] == 0) {
                /*
                 * Empty string match: never split on an empty
                 * match at the start of a find_split cycle.  Same
                 * rule as for an empty global match in
                 * match_or_replace.
                 */
                if (i == ip[0]) {
                    /*
                     * "Bump-along" to avoid sticking at an empty
                     * match, but don't bump past end of string --
                     * our caller must do that by adding
                     * sep->length to our return value.
                     */
                    if (i == length) {
                        if (version == Context.VERSION_1_2) {
                            matchlen[0] = 1;
                            result = i;
                        }
                        else
                            result = -1;
                        break;
                    }
                    i++;
                    continue again; // imitating C goto
                }
            }
            // PR_ASSERT((size_t)i >= sep->length);
            result = i - matchlen[0];
            break;
        }
        int size = (parens == null) ? 0 : parens.length;
        parensp[0] = new String[size];
        for (int num = 0; num < size; num++) {
            SubString parsub = getParenSubString(num);
            parensp[0][num] = parsub.toString();
        }
        return result;
    }

    /**
     * Analog of REGEXP_PAREN_SUBSTRING in C jsregexp.h.
     * Assumes zero-based; i.e., for $3, i==2
     */
    SubString getParenSubString(int i)
    {
        if (parens != null && i < parens.length) {
            SubString parsub = parens[i];
            if (parsub != null) {
                return parsub;
            }
        }
        return SubString.emptySubString;
    }

    /*
     * Analog of match_glob() in jsstr.c
     */
    private static void match_glob(GlobData mdata, Context cx,
                                   Scriptable scope, int count,
                                   RegExpImpl reImpl)
    {
        if (mdata.arrayobj == null) {
            Scriptable s = ScriptableObject.getTopLevelScope(scope);
            mdata.arrayobj = ScriptRuntime.newObject(cx, s, "Array", null);
        }
        SubString matchsub = reImpl.lastMatch;
        String matchstr = matchsub.toString();
        mdata.arrayobj.put(count, mdata.arrayobj, matchstr);
    }

    /*
     * Analog of replace_glob() in jsstr.c
     */
    private static void replace_glob(GlobData rdata, Context cx,
                                     Scriptable scope, int count,
                                     RegExpImpl reImpl)
        throws JavaScriptException
    {
        SubString lc = reImpl.leftContext;

        char[] leftArray = lc.charArray;
        int leftIndex = rdata.leftIndex;

        int leftlen = reImpl.lastMatch.index - leftIndex;
        rdata.leftIndex = reImpl.lastMatch.index + reImpl.lastMatch.length;
        int replen = find_replen(rdata, cx, scope, reImpl);
        int growth = leftlen + replen;
        char[] charArray;
        if (rdata.charArray != null) {
            charArray = new char[rdata.length + growth];
            System.arraycopy(rdata.charArray, 0, charArray, 0, rdata.length);
        } else {
            charArray = new char[growth];
        }

        rdata.charArray = charArray;
        rdata.length += growth;
        int index = rdata.index;
        rdata.index += growth;
        System.arraycopy(leftArray, leftIndex, charArray, index, leftlen);
        index += leftlen;
        do_replace(rdata, cx, reImpl, charArray, index);
    }

    private static SubString interpretDollar(Context cx, RegExpImpl res,
                                             char[] da, int dp, int bp,
                                             int[] skip)
    {
        char[] ca;
        int cp;
        char dc;
        int num, tmp;

        /* Allow a real backslash (literal "\\") to escape "$1" etc. */
        if (da[dp] != '$') Kit.codeBug();
        int version = cx.getLanguageVersion();
        if (version != Context.VERSION_DEFAULT
            && version <= Context.VERSION_1_4)
        {
            if (dp > bp && da[dp-1] == '\\')
                return null;
        }
        if (dp+1 >= da.length)
            return null;
        /* Interpret all Perl match-induced dollar variables. */
        dc = da[dp+1];
        if (NativeRegExp.isDigit(dc)) {
            if (version != Context.VERSION_DEFAULT
                && version <= Context.VERSION_1_4)
            {
                if (dc == '0')
                    return null;
                /* Check for overflow to avoid gobbling arbitrary decimal digits. */
                num = 0;
                ca = da;
                cp = dp;
                while (++cp < ca.length && NativeRegExp.isDigit(dc = ca[cp])) {
                    tmp = 10 * num + NativeRegExp.unDigit(dc);
                    if (tmp < num)
                        break;
                    num = tmp;
                }
            }
            else {  /* ECMA 3, 1-9 or 01-99 */
                int parenCount = (res.parens == null) ? 0 : res.parens.length;
                num = NativeRegExp.unDigit(dc);
                if (num > parenCount)
                    return null;
                cp = dp + 2;
                if ((dp + 2) < da.length) {
                    dc = da[dp + 2];
                    if (NativeRegExp.isDigit(dc)) {
                        tmp = 10 * num + NativeRegExp.unDigit(dc);
                        if (tmp <= parenCount) {
                            cp++;
                            num = tmp;
                        }
                    }
                }
                if (num == 0) return null;  /* $0 or $00 is not valid */
            }
            /* Adjust num from 1 $n-origin to 0 array-index-origin. */
            num--;
            skip[0] = cp - dp;
            return res.getParenSubString(num);
        }

        skip[0] = 2;
        switch (dc) {
          case '$':
            return new SubString(da, dp, 1);
          case '&':
            return res.lastMatch;
          case '+':
            return res.lastParen;
          case '`':
            if (version == Context.VERSION_1_2) {
                /*
                 * JS1.2 imitated the Perl4 bug where left context at each step
                 * in an iterative use of a global regexp started from last match,
                 * not from the start of the target string.  But Perl4 does start
                 * $` at the beginning of the target string when it is used in a
                 * substitution, so we emulate that special case here.
                 */
                res.leftContext.index = 0;
                res.leftContext.length = res.lastMatch.index;
            }
            return res.leftContext;
          case '\'':
            return res.rightContext;
        }
        return null;
    }

    /**
     * Corresponds to find_replen in jsstr.c. rdata is 'this', and
     * the result parameter sizep is the return value (errors are
     * propagated with exceptions).
     */
    private static int find_replen(GlobData rdata, Context cx,
                                   Scriptable scope,  RegExpImpl reImpl)
        throws JavaScriptException
    {
        Function lambda = rdata.lambda;
        if (lambda != null) {
            // invoke lambda function with args lastMatch, $1, $2, ... $n,
            // leftContext.length, whole string.
            SubString[] parens = reImpl.parens;
            int parenCount = (parens == null) ? 0 : parens.length;
            Object[] args = new Object[parenCount + 3];
            args[0] = reImpl.lastMatch.toString();
            for (int i=0; i < parenCount; i++) {
                SubString sub = parens[i];
                if (sub != null) {
                    args[i+1] = sub.toString();
                } else {
                    args[i+1] = Undefined.instance;
                }
            }
            args[parenCount+1] = new Integer(reImpl.leftContext.length);
            args[parenCount+2] = rdata.str;
            Scriptable parent = ScriptableObject.getTopLevelScope(scope);
            Object result = lambda.call(cx, parent, parent, args);
            rdata.repstr = ScriptRuntime.toString(result).toCharArray();
            return rdata.repstr.length;
        }

        int replen = rdata.repstr.length;
        if (rdata.dollar == -1)
            return replen;

        int bp = 0;
        for (int dp = rdata.dollar; dp < rdata.repstr.length ; ) {
            char c = rdata.repstr[dp];
            if (c != '$') {
                dp++;
                continue;
            }
            int[] skip = { 0 };
            SubString sub = interpretDollar(cx, reImpl, rdata.repstr, dp,
                                            bp, skip);
            if (sub != null) {
                replen += sub.length - skip[0];
                dp += skip[0];
            }
            else
                dp++;
        }
        return replen;
    }

    /**
     * Analog of do_replace in jsstr.c
     */
    private static void do_replace(GlobData rdata, Context cx,
                                   RegExpImpl regExpImpl,
                                   char[] charArray, int arrayIndex)
    {
        int cp = 0;
        char[] da = rdata.repstr;
        int dp = rdata.dollar;
        int bp = cp;
        if (dp != -1) {
          outer:
            for (;;) {
                int len = dp - cp;
                System.arraycopy(da, cp, charArray, arrayIndex, len);
                arrayIndex += len;
                cp = dp;
                int[] skip = { 0 };
                SubString sub = interpretDollar(cx, regExpImpl, da,
                                                dp, bp, skip);
                if (sub != null) {
                    len = sub.length;
                    if (len > 0) {
                        System.arraycopy(sub.charArray, sub.index, charArray,
                                         arrayIndex, len);
                    }
                    arrayIndex += len;
                    cp += skip[0];
                    dp += skip[0];
                }
                else
                    dp++;
                if (dp >= da.length) break;
                while (da[dp] != '$') {
                    dp++;
                    if (dp >= da.length) break outer;
                }
            }
        }
        if (da.length > cp) {
            System.arraycopy(da, cp, charArray, arrayIndex,
                             da.length - cp);
        }
    }

    String          input;         /* input string to match (perl $_, GC root) */
    boolean         multiline;     /* whether input contains newlines (perl $*) */
    SubString[]     parens;        /* Vector of SubString; last set of parens
                                      matched (perl $1, $2) */
    SubString       lastMatch;     /* last string matched (perl $&) */
    SubString       lastParen;     /* last paren matched (perl $+) */
    SubString       leftContext;   /* input to left of last match (perl $`) */
    SubString       rightContext;  /* input to right of last match (perl $') */
}


final class GlobData
{
    static final int GLOB_MATCH   =    1;
    static final int GLOB_REPLACE =    2;
    static final int GLOB_SEARCH  =    3;

    byte     mode;      /* input: return index, match object, or void */
    int      optarg;    /* input: index of optional flags argument */
    boolean  global;    /* output: whether regexp was global */
    String   str;       /* output: 'this' parameter object as string */
    NativeRegExp regexp;/* output: regexp parameter object private data */

    // match-specific data

    Scriptable arrayobj;

    // replace-specific data

    Function    lambda;        /* replacement function object or null */
    char[]      repstr;        /* replacement string */
    int         dollar = -1;   /* -1 or index of first $ in repstr */
    char[]      charArray;     /* result characters, null initially */
    int         length;        /* result length, 0 initially */
    int         index;         /* index in result of next replacement */
    int         leftIndex;     /* leftContext index, always 0 for JS1.2 */
}
