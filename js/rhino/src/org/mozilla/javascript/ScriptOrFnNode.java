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

public class ScriptOrFnNode extends Node {

    public ScriptOrFnNode(int nodeType) {
        super(nodeType);
    }

    public final VariableTable getVariableTable() { return variableTable; }

    public final String getEncodedSource() { return encodedSource; }

    public final String getOriginalSource() { return originalSource; }

    public final String getSourceName() { return sourceName; }

    public final int getBaseLineno() { return baseLineno; }

    public final int getEndLineno() { return baseLineno; }

    public final int getRegexpCount() {
        if (regexps == null) { return 0; }
        return regexps.size() / 2;
    }

    public final String getRegexpString(int index) {
        return (String)regexps.get(index * 2);
    }

    public final String getRegexpFlags(int index) {
        return (String)regexps.get(index * 2 + 1);
    }

    public final int addRegexp(String string, String flags) {
        if (regexps == null) { regexps = new ObjArray(); }
        regexps.add(string);
        regexps.add(flags);
        return (regexps.size() - 2) / 2;
    }

    VariableTable variableTable;
    String encodedSource;
    String originalSource;
    String sourceName;
    int baseLineno = -1;
    int endLineno = -1;
    private ObjArray regexps;

}
