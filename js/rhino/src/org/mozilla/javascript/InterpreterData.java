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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Roger Lawrence
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

import java.io.Serializable;

import org.mozilla.javascript.debug.DebuggableScript;

final class InterpreterData implements Serializable, DebuggableScript
{

    static final long serialVersionUID = 4815333329084415557L;

    static final int INITIAL_MAX_ICODE_LENGTH = 1024;
    static final int INITIAL_STRINGTABLE_SIZE = 64;
    static final int INITIAL_NUMBERTABLE_SIZE = 64;

    InterpreterData(int languageVersion,
                    String sourceFile, String encodedSource)
    {
        this.languageVersion = languageVersion;
        this.itsSourceFile = sourceFile;
        this.encodedSource = encodedSource;

        init();
    }

    InterpreterData(InterpreterData parent)
    {
        this.parentData = parent;
        this.languageVersion = parent.languageVersion;
        this.itsSourceFile = parent.itsSourceFile;
        this.encodedSource = parent.encodedSource;

        init();
    }

    private void init()
    {
        itsICode = new byte[INITIAL_MAX_ICODE_LENGTH];
        itsStringTable = new String[INITIAL_STRINGTABLE_SIZE];
    }

    String itsName;
    String itsSourceFile;
    boolean itsNeedsActivation;
    boolean itsCheckThis;
    int itsFunctionType;

    String[] itsStringTable;
    double[] itsDoubleTable;
    InterpreterData[] itsNestedFunctions;
    Object[] itsRegExpLiterals;

    byte[] itsICode;

    int[] itsExceptionTable;

    int itsMaxVars;
    int itsMaxLocals;
    int itsMaxStack;
    int itsMaxFrameArray;

    // see comments in NativeFuncion for definition of argNames and argCount
    String[] argNames;
    int argCount;

    int itsMaxCalleeArgs;

    String encodedSource;
    int encodedSourceStart;
    int encodedSourceEnd;

    int languageVersion;

    boolean useDynamicScope;

    boolean topLevel;

    Object[] literalIds;

    UintMap longJumps;

    int firstLinePC = -1; // PC for the first LINE icode

    InterpreterData parentData;

    public boolean isTopLevel()
    {
        return topLevel;
    }

    public boolean isFunction()
    {
        return itsFunctionType != 0;
    }

    public String getFunctionName()
    {
        return itsName;
    }

    public String getSourceName()
    {
        return itsSourceFile;
    }

    public boolean isGeneratedScript()
    {
        return ScriptRuntime.isGeneratedScript(itsSourceFile);
    }

    public int[] getLineNumbers()
    {
        return Interpreter.getLineNumbers(this);
    }

    public int getFunctionCount()
    {
        return (itsNestedFunctions == null) ? 0 : itsNestedFunctions.length;
    }

    public DebuggableScript getFunction(int index)
    {
        return itsNestedFunctions[index];
    }

    public DebuggableScript getParent()
    {
         return parentData;
    }

}
