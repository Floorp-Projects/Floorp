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

    protected VariableTable itsVariableTable;
    protected boolean itsNeedsActivation;
}
