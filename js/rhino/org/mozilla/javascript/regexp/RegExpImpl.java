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

package org.mozilla.javascript.regexp;

import org.mozilla.javascript.*;
import java.util.Vector;

/**
 * 
 */
public class RegExpImpl implements RegExpProxy {
    
    public RegExpImpl() {
        parens = new Vector(9);
    }
    
    public boolean isRegExp(Object obj) {
        return obj instanceof NativeRegExp;
    }

    public Object executeRegExp(Object regExp, Scriptable scopeObj, 
                                String str, int indexp[], boolean test)
    {
        if (regExp instanceof NativeRegExp)
            return ((NativeRegExp) regExp).executeRegExp(scopeObj, str, 
                                                         indexp, test);
        return null;
    }
    
    
    public Object match(Context cx, Scriptable thisObj, Object[] args, 
                        Function funObj)
        throws JavaScriptException
    {
        MatchData mdata = new MatchData();
        mdata.optarg = 1;
        mdata.mode = GlobData.GLOB_MATCH;
        mdata.parent = ScriptableObject.getTopLevelScope(funObj);
        Object rval = matchOrReplace(cx, thisObj, args, funObj, mdata);
        return mdata.arrayobj == null ? rval : mdata.arrayobj;
    }

    public Object search(Context cx, Scriptable thisObj, Object[] args, 
                         Function funObj)
        throws JavaScriptException
    {
        MatchData mdata = new MatchData();
        mdata.optarg = 1;
        mdata.mode = GlobData.GLOB_SEARCH;
        mdata.parent = ScriptableObject.getTopLevelScope(funObj);
        return matchOrReplace(cx, thisObj, args, funObj, mdata);
    }

    public Object replace(Context cx, Scriptable thisObj, Object[] args, 
                          Function funObj)
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

        ReplaceData rdata = new ReplaceData();
        rdata.optarg = 2;
        rdata.mode = GlobData.GLOB_REPLACE;
        rdata.lambda = lambda;
        rdata.repstr = repstr == null ? null : repstr.toCharArray();
        rdata.dollar = repstr == null ? -1 : repstr.indexOf('$');
        rdata.charArray = null;
        rdata.length = 0;
        rdata.index = 0;
        rdata.leftIndex = 0;
        Object val = matchOrReplace(cx, thisObj, args, funObj, rdata);
        char[] charArray;

        if (rdata.charArray == null) {
            if (rdata.global || val == null || !val.equals(Boolean.TRUE)) {
                /* Didn't match even once. */
                return rdata.str;
            }
            int leftlen = this.leftContext.length;
            int length = leftlen + rdata.findReplen(this);
            charArray = new char[length];
            SubString leftContext = this.leftContext;
            System.arraycopy(leftContext.charArray, leftContext.index,
                             charArray, 0, leftlen);
            rdata.doReplace(this, charArray, leftlen);
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
    private static Object matchOrReplace(Context cx, Scriptable thisObj,
                                         Object[] args, Function funObj,
                                         GlobData data)
        throws JavaScriptException
    {
        NativeRegExp re;

        String str = ScriptRuntime.toString(thisObj);
        data.str = str;
        RegExpImpl reImpl = RegExpImpl.getRegExpImpl(cx);

        if (args[0] instanceof NativeRegExp) {
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
            Scriptable scope = ScriptableObject.getTopLevelScope(funObj);
            re = new NativeRegExp(scope, src, opt);
        }
        data.regexp = re;

        data.global = (re.getFlags() & NativeRegExp.GLOB) != 0;
        int[] indexp = { 0 };
        Object result = null;
        if (data.mode == GlobData.GLOB_SEARCH) {
            result = re.executeRegExp(funObj, str, indexp, true);
            if (result != null && result.equals(Boolean.TRUE))
                result = new Integer(reImpl.leftContext.length);
            else
                result = new Integer(-1);
        } else if (data.global) {
            re.setLastIndex(0);
            for (int count = 0; indexp[0] <= str.length(); count++) {
                result = re.executeRegExp(funObj, str, indexp, true);
                if (result == null || !result.equals(Boolean.TRUE))
                    break;
                data.doGlobal(funObj, count);
                if (reImpl.lastMatch.length == 0) {
                    if (indexp[0] == str.length())
                        break;
                    indexp[0]++;
                }
            }
        } else {
            result = re.executeRegExp(funObj, str, indexp,
                                      data.mode == GlobData.GLOB_REPLACE);
        }

        return result;
    } 
    
    
    
    public int find_split(Function funObj, String target, String separator, 
                          Object reObj, int[] ip, int[] matchlen, 
                          boolean[] matched, String[][] parensp)
    {
        int i = ip[0];
        int length = target.length();
        int result;
        Context cx = Context.getCurrentContext();
        int version = cx.getLanguageVersion();
        NativeRegExp re = (NativeRegExp) reObj;
        again:
        while (true) {  // imitating C label
            /* JS1.2 deviated from Perl by never matching at end of string. */
            int ipsave = ip[0]; // reuse ip to save object creation
            ip[0] = i;
            if (re.executeRegExp(funObj, target, ip, true) != Boolean.TRUE) 
            {
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
        int size = parens.size();
        parensp[0] = new String[size];
        for (int num = 0; num < size; num++) {
            SubString parsub = getParenSubString(num);
            parensp[0][num] = parsub.toString();
        }
        return result;
    }
    
    static RegExpImpl getRegExpImpl(Context cx) { 
        return (RegExpImpl) ScriptRuntime.getRegExpProxy(cx);
    }
        
    /**
     * Analog of REGEXP_PAREN_SUBSTRING in C jsregexp.h.
     * Assumes zero-based; i.e., for $3, i==2
     */
    SubString getParenSubString(int i) {
        if (i >= parens.size())
            return SubString.emptySubString;
        return (SubString) parens.elementAt(i);
    }

    String          input;         /* input string to match (perl $_, GC root) */
    boolean         multiline;     /* whether input contains newlines (perl $*) */
    Vector          parens;        /* Vector of SubString; last set of parens
                                      matched (perl $1, $2) */
    SubString       lastMatch;     /* last string matched (perl $&) */
    SubString       lastParen;     /* last paren matched (perl $+) */
    SubString       leftContext;   /* input to left of last match (perl $`) */
    SubString       rightContext;  /* input to right of last match (perl $') */
}


abstract class GlobData {
    static final int GLOB_MATCH =      1;
    static final int GLOB_REPLACE =    2;
    static final int GLOB_SEARCH =     3;

    abstract void doGlobal(Function funObj, int count) 
        throws JavaScriptException;

    byte     mode;      /* input: return index, match object, or void */
    int      optarg;    /* input: index of optional flags argument */
    boolean  global;    /* output: whether regexp was global */
    String   str;       /* output: 'this' parameter object as string */
    NativeRegExp regexp;/* output: regexp parameter object private data */
    Scriptable parent;
}


class MatchData extends GlobData {

    /*
     * Analog of match_glob() in jsstr.c
     */
    void doGlobal(Function funObj, int count) throws JavaScriptException {
        MatchData mdata;
        Object v;

        mdata = this;
        Context cx = Context.getCurrentContext();
        RegExpImpl reImpl = RegExpImpl.getRegExpImpl(cx);
        if (arrayobj == null) {
            Scriptable s = ScriptableObject.getTopLevelScope(funObj);
            arrayobj = ScriptRuntime.newObject(cx, s, "Array", null);
        }
        SubString matchsub = reImpl.lastMatch;
        String matchstr = matchsub.toString();
        arrayobj.put(count, arrayobj, matchstr);
    }

    Scriptable arrayobj;
}


class ReplaceData extends GlobData {

    ReplaceData() {
        dollar = -1;
    }

    /*
     * Analog of replace_glob() in jsstr.c
     */
    void doGlobal(Function funObj, int count)
        throws JavaScriptException
    {
        ReplaceData rdata = this;

        Context cx = Context.getCurrentContext();
        RegExpImpl reImpl = RegExpImpl.getRegExpImpl(cx);
        SubString lc = reImpl.leftContext;

        char[] leftArray = lc.charArray;
        int leftIndex = rdata.leftIndex;
        
        int leftlen = reImpl.lastMatch.index - leftIndex;
        rdata.leftIndex = reImpl.lastMatch.index + reImpl.lastMatch.length;
        int replen = findReplen(reImpl);
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
        doReplace(reImpl, charArray, index);
    }


    static SubString interpretDollar(RegExpImpl res, char[] da, int dp,
                                     int bp, int[] skip)
    {
        char[] ca;
        int cp;
        char dc;
        int num, tmp;

        /* Allow a real backslash (literal "\\") to escape "$1" etc. */
        if (da[dp] != '$')
            throw new RuntimeException();
        if (dp > bp && da[dp-1] == '\\')
            return null;

        /* Interpret all Perl match-induced dollar variables. */
        dc = da[dp+1];
        if (NativeRegExp.isDigit(dc)) {
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

            /* Adjust num from 1 $n-origin to 0 array-index-origin. */
            num--;
            skip[0] = cp - dp;
            return res.getParenSubString(num);
        }

        skip[0] = 2;
        switch (dc) {
          case '&':
            return res.lastMatch;
          case '+':
            return res.lastParen;
          case '`':
            Context cx = Context.getCurrentContext();
            if (cx.getLanguageVersion() == Context.VERSION_1_2) {
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
    int findReplen(RegExpImpl reImpl)
        throws JavaScriptException
    {
        if (lambda != null) {
            // invoke lambda function with args lastMatch, $1, $2, ... $n,
            // leftContext.length, whole string.
            Context cx = Context.getCurrentContext();
            Vector parens = reImpl.parens;
            int parenCount = parens.size();
            Object[] args = new Object[parenCount + 3];
            args[0] = reImpl.lastMatch.toString();
            for (int i=0; i < parenCount; i++) {
                SubString sub = (SubString) parens.elementAt(i);
                args[i+1] = sub.toString();
            }
            args[parenCount+1] = new Integer(reImpl.leftContext.length);
            args[parenCount+2] = str;
            Scriptable parent = lambda.getParentScope();
            Object result = lambda.call(cx, parent, parent, args);

            this.repstr = ScriptRuntime.toString(result).toCharArray();
            return this.repstr.length;
        }

        int replen = this.repstr.length;
        if (dollar == -1)
            return replen;

        int bp = 0;
        for (int dp = dollar; dp < this.repstr.length ; dp++) {
            char c = this.repstr[dp];
            if (c != '$')
                continue;
            int[] skip = { 0 };
            SubString sub = interpretDollar(reImpl, this.repstr, dp,
                                            bp, skip);
            if (sub != null)
                replen += sub.length - skip[0];
        }
        return replen;
    }

    /**
     * Analog of do_replace in jsstr.c
     */
    void doReplace(RegExpImpl regExpImpl, char[] charArray,
                   int arrayIndex)
    {
        int cp = 0;
        char[] da = repstr;
        int dp = this.dollar;
        int bp = cp;
        if (dp != -1) {
          outer:
            for (;;) {
                int len = dp - cp;
                System.arraycopy(repstr, cp, charArray, arrayIndex,
                                 len);
                arrayIndex += len;
                cp = dp;
                int[] skip = { 0 };
                SubString sub = interpretDollar(regExpImpl, da,
                                                dp, bp, skip);
                if (sub != null) {
                    len = sub.length;
                    if (len > 0) {
                        System.arraycopy(sub.charArray, sub.index, charArray,
                                         arrayIndex, len);
                    }
                    arrayIndex += len;
                    cp += skip[0];
                }
                dp++;
                do {
                    if (dp >= charArray.length)
                        break outer;
                } while (charArray[dp++] != '$');
            }
        }
        if (repstr.length > cp) {
            System.arraycopy(repstr, cp, charArray, arrayIndex,
                             repstr.length - cp);
        }
    }

    Function    lambda;        /* replacement function object or null */
    char[]      repstr;        /* replacement string */
    int         dollar;        /* -1 or index of first $ in repstr */
    char[]      charArray;     /* result characters, null initially */
    int         length;        /* result length, 0 initially */
    int         index;         /* index in result of next replacement */
    int         leftIndex;     /* leftContext index, always 0 for JS1.2 */
}
