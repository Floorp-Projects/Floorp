/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

package org.mozilla.javascript;

import java.util.*;

public class FunctionNode extends Node {

    public FunctionNode(String name, Node left, Node right) {
        super(TokenStream.FUNCTION, left, right, name);
        itsVariableTable = new VariableTable();
    }

    public String getFunctionName() {
        return getString();
    }

    public VariableTable getVariableTable() {
        return itsVariableTable;
    }

    public boolean requiresActivation() {
        return itsNeedsActivation;
    }

    public boolean setRequiresActivation(boolean b) {
        return itsNeedsActivation = b;
    }
    
    /**
     * There are three types of functions that can be defined. The first
     * is a function statement. This is a function appearing as a top-level
     * statement (i.e., not nested inside some other statement) in either a
     * script or a function.
     * 
     * The second is a function expression, which is a function appearing in
     * an expression except for the third type, which is...
     * 
     * The third type is a function expression where the expression is the 
     * top-level expression in an expression statement.
     * 
     * The three types of functions have different treatment and must be 
     * distinquished.
     */
    public static final byte FUNCTION_STATEMENT            = 1;
    public static final byte FUNCTION_EXPRESSION           = 2;
    public static final byte FUNCTION_EXPRESSION_STATEMENT = 3;
    
    public byte getFunctionType() {
        return itsFunctionType;
    }

    public void setFunctionType(byte functionType) {
        itsFunctionType = functionType;
    }

    protected VariableTable itsVariableTable;
    protected boolean itsNeedsActivation;
    protected byte itsFunctionType;
}
