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
import java.lang.reflect.Method;

/**
 * This class implements the RegExp constructor native object.
 *
 * Revision History:
 * Implementation in C by Brendan Eich
 * Initial port to Java by Norris Boyd from jsregexp.c version 1.36
 * Merged up to version 1.38, which included Unicode support.
 * Merged bug fixes in version 1.39.
 * Merged JSFUN13_BRANCH changes up to 1.32.2.11
 *
 * @author Brendan Eich
 * @author Norris Boyd
 */
public class NativeRegExpCtor extends NativeFunction {

    public NativeRegExpCtor() {
        functionName = "RegExp";
    }

    public String getClassName() {
        return "Function";
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
    {
        if (args.length > 0 && args[0] instanceof NativeRegExp &&
            (args.length == 1 || args[1] == Undefined.instance))
          {
            return args[0];
        }
        return construct(cx, scope, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
    {
        NativeRegExp re = new NativeRegExp();
        re.compile(cx, scope, args);
        re.setPrototype(getClassPrototype(scope, "RegExp"));
        re.setParentScope(getParentScope());
        return re;
    }

    static RegExpImpl getImpl()
    {
        Context cx = Context.getCurrentContext();
        return (RegExpImpl) ScriptRuntime.getRegExpProxy(cx);
    }

    private static String stringResult(Object obj)
    {
        return (obj == null) ? "" : obj.toString();
    }

// #string_id_map#

    private static final int
        Id_multiline     = 1,
        Id_STAR          = 2,  // #string=$*#

        Id_input         = 3,
        Id_UNDERSCORE    = 4,  // #string=$_#

        Id_lastMatch     = 5,
        Id_AMPERSAND     = 6,  // #string=$&#

        Id_lastParen     = 7,
        Id_PLUS          = 8,  // #string=$+#

        Id_leftContext   = 9,
        Id_BACK_QUOTE    = 10, // #string=$`#

        Id_rightContext  = 11,
        Id_QUOTE         = 12, // #string=$'#

        DOLLAR_ID_BASE   = 12;

    private static final int
        Id_DOLLAR_1 = DOLLAR_ID_BASE + 1, // #string=$1#
        Id_DOLLAR_2 = DOLLAR_ID_BASE + 2, // #string=$2#
        Id_DOLLAR_3 = DOLLAR_ID_BASE + 3, // #string=$3#
        Id_DOLLAR_4 = DOLLAR_ID_BASE + 4, // #string=$4#
        Id_DOLLAR_5 = DOLLAR_ID_BASE + 5, // #string=$5#
        Id_DOLLAR_6 = DOLLAR_ID_BASE + 6, // #string=$6#
        Id_DOLLAR_7 = DOLLAR_ID_BASE + 7, // #string=$7#
        Id_DOLLAR_8 = DOLLAR_ID_BASE + 8, // #string=$8#
        Id_DOLLAR_9 = DOLLAR_ID_BASE + 9, // #string=$9#

        MAX_INSTANCE_ID = DOLLAR_ID_BASE + 9;

// maxId for superclass
    private static int idBase = -1;

    {
        if (idBase < 0) idBase = getMaxInstanceId();
        setMaxInstanceId(idBase, idBase + MAX_INSTANCE_ID);
    }

    protected int findInstanceIdInfo(String s) {
        int id;
// #generated# Last update: 2001-05-24 16:09:31 GMT+02:00
        L0: { id = 0; String X = null; int c;
            L: switch (s.length()) {
            case 2: switch (s.charAt(1)) {
                case '&': if (s.charAt(0)=='$') {id=Id_AMPERSAND; break L0;} break L;
                case '\'': if (s.charAt(0)=='$') {id=Id_QUOTE; break L0;} break L;
                case '*': if (s.charAt(0)=='$') {id=Id_STAR; break L0;} break L;
                case '+': if (s.charAt(0)=='$') {id=Id_PLUS; break L0;} break L;
                case '1': if (s.charAt(0)=='$') {id=Id_DOLLAR_1; break L0;} break L;
                case '2': if (s.charAt(0)=='$') {id=Id_DOLLAR_2; break L0;} break L;
                case '3': if (s.charAt(0)=='$') {id=Id_DOLLAR_3; break L0;} break L;
                case '4': if (s.charAt(0)=='$') {id=Id_DOLLAR_4; break L0;} break L;
                case '5': if (s.charAt(0)=='$') {id=Id_DOLLAR_5; break L0;} break L;
                case '6': if (s.charAt(0)=='$') {id=Id_DOLLAR_6; break L0;} break L;
                case '7': if (s.charAt(0)=='$') {id=Id_DOLLAR_7; break L0;} break L;
                case '8': if (s.charAt(0)=='$') {id=Id_DOLLAR_8; break L0;} break L;
                case '9': if (s.charAt(0)=='$') {id=Id_DOLLAR_9; break L0;} break L;
                case '_': if (s.charAt(0)=='$') {id=Id_UNDERSCORE; break L0;} break L;
                case '`': if (s.charAt(0)=='$') {id=Id_BACK_QUOTE; break L0;} break L;
                } break L;
            case 5: X="input";id=Id_input; break L;
            case 9: c=s.charAt(4);
                if (c=='M') { X="lastMatch";id=Id_lastMatch; }
                else if (c=='P') { X="lastParen";id=Id_lastParen; }
                else if (c=='i') { X="multiline";id=Id_multiline; }
                break L;
            case 11: X="leftContext";id=Id_leftContext; break L;
            case 12: X="rightContext";id=Id_rightContext; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#

        if (id == 0) return super.findInstanceIdInfo(s);

        int attr;
        switch (id) {
          case Id_multiline:
          case Id_STAR:
          case Id_input:
          case Id_UNDERSCORE:
            attr = PERMANENT;
            break;
          default:
            attr = PERMANENT | READONLY;
            break;
        }

        return instanceIdInfo(attr, idBase + id);
    }

// #/string_id_map#

    protected String getInstanceIdName(int id)
    {
        int shifted = id - idBase;
        if (1 <= shifted && shifted <= MAX_INSTANCE_ID) {
            switch (shifted) {
                case Id_multiline:    return "multiline";
                case Id_STAR:         return "$*";

                case Id_input:        return "input";
                case Id_UNDERSCORE:   return "$_";

                case Id_lastMatch:    return "lastMatch";
                case Id_AMPERSAND:    return "$&";

                case Id_lastParen:    return "lastParen";
                case Id_PLUS:         return "$+";

                case Id_leftContext:  return "leftContext";
                case Id_BACK_QUOTE:   return "$`";

                case Id_rightContext: return "rightContext";
                case Id_QUOTE:        return "$'";
            }
            // Must be one of $1..$9, convert to 0..8
            int substring_number = shifted - DOLLAR_ID_BASE - 1;
            char[] buf = { '$', (char)('1' + substring_number) };
            return new String(buf);
        }
        return super.getInstanceIdName(id);
    }

    protected Object getInstanceIdValue(int id)
    {
        int shifted = id - idBase;
        if (1 <= shifted && shifted <= MAX_INSTANCE_ID) {
            RegExpImpl impl = getImpl();
            switch (shifted) {
                case Id_multiline:
                case Id_STAR:
                    return ScriptRuntime.wrapBoolean(impl.multiline);

                case Id_input:
                case Id_UNDERSCORE:
                    return stringResult(impl.input);

                case Id_lastMatch:
                case Id_AMPERSAND:
                    return stringResult(impl.lastMatch);

                case Id_lastParen:
                case Id_PLUS:
                    return stringResult(impl.lastParen);

                case Id_leftContext:
                case Id_BACK_QUOTE:
                    return stringResult(impl.leftContext);

                case Id_rightContext:
                case Id_QUOTE:
                    return stringResult(impl.rightContext);
            }
            // Must be one of $1..$9, convert to 0..8
            int substring_number = shifted - DOLLAR_ID_BASE - 1;
            return impl.getParenSubString(substring_number).toString();
        }
        return super.getInstanceIdValue(id);
    }

    protected void setInstanceIdValue(int id, Object value)
    {
        switch (id - idBase) {
            case Id_multiline:
            case Id_STAR:
                getImpl().multiline = ScriptRuntime.toBoolean(value);
                return;

            case Id_input:
            case Id_UNDERSCORE:
                getImpl().input = ScriptRuntime.toString(value);
                return;
        }
        super.setInstanceIdValue(id, value);
    }

}
