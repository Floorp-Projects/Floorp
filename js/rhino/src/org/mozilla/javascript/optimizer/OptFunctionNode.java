/*
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


package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;
import java.util.*;

final class OptFunctionNode
{
    OptFunctionNode(FunctionNode fnode)
    {
        this.fnode = fnode;
        int N = fnode.getParamAndVarCount();
        int parameterCount = fnode.getParamCount();
        optVars = new OptLocalVariable[N];
        for (int i = 0; i != N; ++i) {
            optVars[i] = new OptLocalVariable(i < parameterCount);
        }
        fnode.setCompilerData(this);
    }

    static OptFunctionNode get(ScriptOrFnNode scriptOrFn, int i)
    {
        FunctionNode fnode = scriptOrFn.getFunctionNode(i);
        return (OptFunctionNode)fnode.getCompilerData();
    }

    static OptFunctionNode get(ScriptOrFnNode scriptOrFn)
    {
        return (OptFunctionNode)scriptOrFn.getCompilerData();
    }

    boolean isTargetOfDirectCall()
    {
        return directTargetIndex >= 0;
    }

    int getDirectTargetIndex()
    {
        return directTargetIndex;
    }

    void setDirectTargetIndex(int directTargetIndex)
    {
        // One time action
        if (directTargetIndex < 0 || this.directTargetIndex >= 0)
            Kit.codeBug();
        this.directTargetIndex = directTargetIndex;
    }

    void setParameterNumberContext(boolean b)
    {
        itsParameterNumberContext = b;
    }

    boolean getParameterNumberContext()
    {
        return itsParameterNumberContext;
    }

    int getVarCount()
    {
        return optVars.length;
    }

    OptLocalVariable getVar(int index)
    {
        return optVars[index];
    }

    OptLocalVariable getVar(String name)
    {
        int index = fnode.getParamOrVarIndex(name);
        if (index < 0) { return null; }
        return optVars[index];
    }

    int getVarIndex(Node n)
    {
        int index = n.getIntProp(Node.VARIABLE_PROP, -1);
        if (index == -1) {
            String name;
            int type = n.getType();
            if (type == Token.GETVAR) {
                name = n.getString();
            } else if (type == Token.SETVAR) {
                name = n.getFirstChild().getString();
            } else {
                throw Kit.codeBug();
            }
            index = fnode.getParamOrVarIndex(name);
            if (index < 0) throw Kit.codeBug();
            n.putIntProp(Node.VARIABLE_PROP, index);
        }
        return index;
    }

    OptLocalVariable getVar(Node n)
    {
        int index = getVarIndex(n);
        return optVars[index];
    }

    OptLocalVariable[] getVarsArray()
    {
        return optVars;
    }

    FunctionNode fnode;
    private OptLocalVariable[] optVars;
    private int directTargetIndex = -1;
    private boolean itsParameterNumberContext;
    boolean itsContainsCalls0;
    boolean itsContainsCalls1;
}
