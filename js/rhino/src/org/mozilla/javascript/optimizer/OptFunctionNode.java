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

public class OptFunctionNode extends FunctionNode {

    public OptFunctionNode(String name, Node left, Node right,
                           ClassNameHelper nameHelper)
    {
        super(name, left, right);
        OptClassNameHelper nh = (OptClassNameHelper) nameHelper;
        itsClassName = nh.getJavaScriptClassName(name, false);
    }

    public String getDirectCallParameterSignature() {
        int pCount = itsVariableTable.getParameterCount();
        switch (pCount) {
            case 0: return ZERO_PARAM_SIG;
            case 1: return ONE_PARAM_SIG;
            case 2: return TWO_PARAM_SIG;
        }
        StringBuffer sb = new StringBuffer(ZERO_PARAM_SIG.length()
                                           + pCount * DIRECT_ARG_SIG.length());
        sb.append(BEFORE_DIRECT_SIG);
        for (int i = 0; i != pCount; i++) {
            sb.append(DIRECT_ARG_SIG);
        }
        sb.append(AFTER_DIRECT_SIG);
        return sb.toString();
    }

    public String getClassName() {
        return itsClassName;
    }

    public boolean isTargetOfDirectCall() {
        return itsIsTargetOfDirectCall;
    }

    public void addDirectCallTarget(FunctionNode target) {
        if (itsDirectCallTargets == null)
            itsDirectCallTargets = new ObjArray();
        for (int i = 0; i < itsDirectCallTargets.size(); i++)   // OPT !!
            if (((FunctionNode)itsDirectCallTargets.get(i)) == target)
                return;
        itsDirectCallTargets.add(target);
    }

    public ObjArray getDirectCallTargets() {
        return itsDirectCallTargets;
    }

    public void setIsTargetOfDirectCall() {
        itsIsTargetOfDirectCall = true;
    }

    public void setParameterNumberContext(boolean b) {
        itsParameterNumberContext = b;
    }

    public boolean getParameterNumberContext() {
        return itsParameterNumberContext;
    }

    public boolean containsCalls(int argCount) {
        if ((argCount < itsContainsCallsCount.length) && (argCount >= 0))
            return itsContainsCallsCount[argCount];
        else
            return itsContainsCalls;
    }

    public void setContainsCalls(int argCount) {
        if (argCount < itsContainsCallsCount.length)
            itsContainsCallsCount[argCount] = true;
        itsContainsCalls = true;
    }

    public void incrementLocalCount() {
        int localCount = getIntProp(Node.LOCALCOUNT_PROP, 0);
        putIntProp(Node.LOCALCOUNT_PROP, localCount + 1);
    }

    private static final String
        BEFORE_DIRECT_SIG = "(Lorg/mozilla/javascript/Context;"
                            +"Lorg/mozilla/javascript/Scriptable;"
                            +"Lorg/mozilla/javascript/Scriptable;",
        DIRECT_ARG_SIG    = "Ljava/lang/Object;D",
        AFTER_DIRECT_SIG  = "[Ljava/lang/Object;)";

    private static final String
        ZERO_PARAM_SIG = BEFORE_DIRECT_SIG+AFTER_DIRECT_SIG,
        ONE_PARAM_SIG  = BEFORE_DIRECT_SIG+DIRECT_ARG_SIG+AFTER_DIRECT_SIG,
        TWO_PARAM_SIG  = BEFORE_DIRECT_SIG+DIRECT_ARG_SIG+DIRECT_ARG_SIG
                         +AFTER_DIRECT_SIG;

    private String itsClassName;
    private boolean itsIsTargetOfDirectCall;
    private boolean itsContainsCalls;
    private boolean[] itsContainsCallsCount = new boolean[4];
    private boolean itsParameterNumberContext;
    private ObjArray itsDirectCallTargets;
}
