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

    public final String getSourceName() { return sourceName; }

    public final void setSourceName(String sourceName) {
        this.sourceName = sourceName;
    }

    public final String getEncodedSource() { return encodedSource; }

    public final void setEncodedSource(String encodedSource) {
        this.encodedSource = encodedSource;
    }

    public final String getOriginalSource() { return originalSource; }

    public final void setOriginalSource(String originalSource) {
        this.originalSource = originalSource;
    }

    public final int getBaseLineno() { return baseLineno; }

    public final void setBaseLineno(int lineno) {
        // One time action
        if (lineno < 0 || baseLineno >= 0) Context.codeBug();
        baseLineno = lineno;
    }

    public final int getEndLineno() { return baseLineno; }

    public final void setEndLineno(int lineno) {
        // One time action
        if (lineno < 0 || endLineno >= 0) Context.codeBug();
        endLineno = lineno;
    }

    public final int getFunctionCount() {
        if (functions == null) { return 0; }
        return functions.size();
    }

    public final FunctionNode getFunctionNode(int i) {
        return (FunctionNode)functions.get(i);
    }

    public final void replaceFunctionNode(int i, FunctionNode fnNode) {
        if (fnNode == null) Context.codeBug();
        functions.set(i, fnNode);
    }

    public final int addFunction(FunctionNode fnNode) {
        if (fnNode == null) Context.codeBug();
        if (functions == null) { functions = new ObjArray(); }
        functions.add(fnNode);
        return functions.size() - 1;
    }

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
        if (string == null) Context.codeBug();
        if (regexps == null) { regexps = new ObjArray(); }
        regexps.add(string);
        regexps.add(flags);
        return regexps.size() / 2 - 1;
    }

    public final boolean hasParameterOrVar(String name) {
        if (variableTable == null) { return false; }
        return variableTable.hasVariable(name);
    }

    public final int getParameterOrVarIndex(String name) {
        if (variableTable == null) { return -1; }
        return variableTable.getOrdinal(name);
    }

    public final String getParameterOrVarName(int index) {
        return variableTable.getVariable(index);
    }

    public final int getParameterCount() {
        if (variableTable == null) { return 0; }
        return variableTable.getParameterCount();
    }

    public final int getParameterAndVarCount() {
        if (variableTable == null) { return 0; }
        return variableTable.size();
    }

    public final String[] getParameterAndVarNames() {
        if (variableTable == null) { return new String[0]; }
        return variableTable.getAllVariables();
    }

    public final void addParameter(String name) {
        if (variableTable == null) { variableTable = new VariableTable(); }
        variableTable.addParameter(name);
    }

    public final void addVar(String name) {
        if (variableTable == null) { variableTable = new VariableTable(); }
        variableTable.addLocal(name);
    }

    public final void removeParameterOrVar(String name) {
        if (variableTable == null) { return; }
        variableTable.removeLocal(name);
    }

    public final int getLocalCount() { return localCount; }

    public final void incrementLocalCount() {
        ++localCount;
    }

    protected void finishParsing(IRFactory irFactory) { }

    private String encodedSource;
    private String originalSource;
    private String sourceName;
    private int baseLineno = -1;
    private int endLineno = -1;
    private ObjArray functions;
    private ObjArray regexps;
    private VariableTable variableTable;
    private int localCount;

}
