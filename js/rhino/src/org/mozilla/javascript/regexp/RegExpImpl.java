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

    public Object action(Context cx, Scriptable scope,
                         Scriptable thisObj, Object[] args,
                         int actionType)
    {
        GlobData data = new GlobData();
        data.mode = actionType;

        switch (actionType) {
          case RA_MATCH:
            {
                Object rval;
                data.optarg = 1;
                rval = matchOrReplace(cx, scope, thisObj, args,
                                      this, data, false);
                return data.arrayobj == null ? rval : data.arrayobj;
            }

          case RA_SEARCH:
            data.optarg = 1;
            return matchOrReplace(cx, scope, thisObj, args,
                                  this, data, false);

          case RA_REPLACE:
            {
                Object arg1 = args.length < 2 ? Undefined.instance : args[1];
                String repstr = null;
                Function lambda = null;
                if (arg1 instanceof Function) {
                    lambda = (Function) arg1;
                } else {
                    repstr = ScriptRuntime.toString(arg1);
                }

                data.optarg = 2;
                data.lambda = lambda;
                data.repstr = repstr;
                data.dollar = repstr == null ? -1 : repstr.indexOf('$');
                data.charBuf = null;
                data.leftIndex = 0;
                Object val = matchOrReplace(cx, scope, thisObj, args,
                                            this, data, true);
                SubString rc = this.rightContext;

                if (data.charBuf == null) {
                    if (data.global || val == null
                        || !val.equals(Boolean.TRUE))
                    {
                        /* Didn't match even once. */
                        return data.str;
                    }
                    SubString lc = this.leftContext;
                    replace_glob(data, cx, scope, this, lc.index, lc.length);
                }
                data.charBuf.append(rc.charArray, rc.index, rc.length);
                return data.charBuf.toString();
            }

          default:
            throw Kit.codeBug();
        }
    }

    /**
     * Analog of C match_or_replace.
     */
    private static Object matchOrReplace(Context cx, Scriptable scope,
                                         Scriptable thisObj, Object[] args,
                                         RegExpImpl reImpl,
                                         GlobData data, boolean forceFlat)
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
        if (data.mode == RA_SEARCH) {
            result = re.executeRegExp(cx, scope, reImpl,
                                      str, indexp, NativeRegExp.TEST);
            if (result != null && result.equals(Boolean.TRUE))
                result = new Integer(reImpl.leftContext.length);
            else
                result = new Integer(-1);
        } else if (data.global) {
            re.lastIndex = 0;
            for (int count = 0; indexp[0] <= str.length(); count++) {
                result = re.executeRegExp(cx, scope, reImpl,
                                          str, indexp, NativeRegExp.TEST);
                if (result == null || !result.equals(Boolean.TRUE))
                    break;
                if (data.mode == RA_MATCH) {
                    match_glob(data, cx, scope, count, reImpl);
                } else {
                    if (data.mode != RA_REPLACE) Kit.codeBug();
                    SubString lastMatch = reImpl.lastMatch;
                    int leftIndex = data.leftIndex;
                    int leftlen = lastMatch.index - leftIndex;
                    data.leftIndex = lastMatch.index + lastMatch.length;
                    replace_glob(data, cx, scope, reImpl, leftIndex, leftlen);
                }
                if (reImpl.lastMatch.length == 0) {
                    if (indexp[0] == str.length())
                        break;
                    indexp[0]++;
                }
            }
        } else {
            result = re.executeRegExp(cx, scope, reImpl, str, indexp,
                                      ((data.mode == RA_REPLACE)
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
                                     Scriptable scope, RegExpImpl reImpl,
                                     int leftIndex, int leftlen)
    {
        int replen;
        String lambdaStr;
        if (rdata.lambda != null) {
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
            Object result = rdata.lambda.call(cx, parent, parent, args);
            lambdaStr = ScriptRuntime.toString(result);
            replen = lambdaStr.length();
        } else {
            lambdaStr = null;
            replen = rdata.repstr.length();
            if (rdata.dollar >= 0) {
                int[] skip = new int[1];
                int dp = rdata.dollar;
                do {
                    SubString sub = interpretDollar(cx, reImpl, rdata.repstr,
                                                    dp, skip);
                    if (sub != null) {
                        replen += sub.length - skip[0];
                        dp += skip[0];
                    } else {
                        ++dp;
                    }
                    dp = rdata.repstr.indexOf('$', dp);
                } while (dp >= 0);
            }
        }

        int growth = leftlen + replen + reImpl.rightContext.length;
        StringBuffer charBuf = rdata.charBuf;
        if (charBuf == null) {
            charBuf = new StringBuffer(growth);
            rdata.charBuf = charBuf;
        } else {
            charBuf.ensureCapacity(rdata.charBuf.length() + growth);
        }

        charBuf.append(reImpl.leftContext.charArray, leftIndex, leftlen);
        if (rdata.lambda != null) {
            charBuf.append(lambdaStr);
        } else {
            do_replace(rdata, cx, reImpl);
        }
    }

    private static SubString interpretDollar(Context cx, RegExpImpl res,
                                             String da, int dp, int[] skip)
    {
        char dc;
        int num, tmp;

        if (da.charAt(dp) != '$') Kit.codeBug();

        /* Allow a real backslash (literal "\\") to escape "$1" etc. */
        int version = cx.getLanguageVersion();
        if (version != Context.VERSION_DEFAULT
            && version <= Context.VERSION_1_4)
        {
            if (dp > 0 && da.charAt(dp - 1) == '\\')
                return null;
        }
        int daL = da.length();
        if (dp + 1 >= daL)
            return null;
        /* Interpret all Perl match-induced dollar variables. */
        dc = da.charAt(dp + 1);
        if (NativeRegExp.isDigit(dc)) {
            int cp;
            if (version != Context.VERSION_DEFAULT
                && version <= Context.VERSION_1_4)
            {
                if (dc == '0')
                    return null;
                /* Check for overflow to avoid gobbling arbitrary decimal digits. */
                num = 0;
                cp = dp;
                while (++cp < daL && NativeRegExp.isDigit(dc = da.charAt(cp)))
                {
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
                if ((dp + 2) < daL) {
                    dc = da.charAt(dp + 2);
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
            return new SubString("$");
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
     * Analog of do_replace in jsstr.c
     */
    private static void do_replace(GlobData rdata, Context cx,
                                   RegExpImpl regExpImpl)
    {
        StringBuffer charBuf = rdata.charBuf;
        int cp = 0;
        String da = rdata.repstr;
        int dp = rdata.dollar;
        if (dp != -1) {
            int[] skip = new int[1];
            do {
                int len = dp - cp;
                charBuf.append(da.substring(cp, dp));
                cp = dp;
                SubString sub = interpretDollar(cx, regExpImpl, da,
                                                dp, skip);
                if (sub != null) {
                    len = sub.length;
                    if (len > 0) {
                        charBuf.append(sub.charArray, sub.index, len);
                    }
                    cp += skip[0];
                    dp += skip[0];
                } else {
                    ++dp;
                }
                dp = da.indexOf('$', dp);
            } while (dp >= 0);
        }
        int daL = da.length();
        if (daL > cp) {
            charBuf.append(da.substring(cp, daL));
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
    int      mode;      /* input: return index, match object, or void */
    int      optarg;    /* input: index of optional flags argument */
    boolean  global;    /* output: whether regexp was global */
    String   str;       /* output: 'this' parameter object as string */
    NativeRegExp regexp;/* output: regexp parameter object private data */

    // match-specific data

    Scriptable arrayobj;

    // replace-specific data

    Function      lambda;        /* replacement function object or null */
    String        repstr;        /* replacement string */
    int           dollar = -1;   /* -1 or index of first $ in repstr */
    StringBuffer  charBuf;       /* result characters, null initially */
    int           leftIndex;     /* leftContext index, always 0 for JS1.2 */
}
