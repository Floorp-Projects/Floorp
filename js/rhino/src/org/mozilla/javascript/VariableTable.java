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

import java.io.*;
import java.util.*;

public class VariableTable {

    public int size()
    {
        return itsVariables.size();
    }

    public int getParameterCount()
    {
        return varStart;
    }
    
    public LocalVariable createLocalVariable(String name, boolean isParameter) 
    {
        return new LocalVariable(name, isParameter);
    }

    public LocalVariable get(int index)
    {
        return (LocalVariable)(itsVariables.elementAt(index));
    }

    public LocalVariable get(String name)
    {
        Integer vIndex = (Integer)(itsVariableNames.get(name));
        if (vIndex != null)
            return (LocalVariable)(itsVariables.elementAt(vIndex.intValue()));
        else
            return null;
    }

    public int getOrdinal(String name) {
        Integer vIndex = (Integer)(itsVariableNames.get(name));
        if (vIndex != null)
            return vIndex.intValue();
        else
            return -1;
    }

    public String getName(int index)
    {
        return ((LocalVariable)(itsVariables.elementAt(index))).getName();
    }

    public void establishIndices()
    {
        for (int i = 0; i < itsVariables.size(); i++) {
            LocalVariable lVar = (LocalVariable)(itsVariables.elementAt(i));
            lVar.setIndex(i);
        }
    }

    public void addParameter(String pName)
    {
        Integer pIndex = (Integer)(itsVariableNames.get(pName));
        if (pIndex != null) {
            LocalVariable p = (LocalVariable)
                                (itsVariables.elementAt(pIndex.intValue()));
            if (p.isParameter()) {
                Object[] errorArgs = { pName };
                String message = Context.getMessage("msg.dup.parms", errorArgs);
                Context.reportWarning(message, null, 0, null, 0);
            }
            else {  // there's a local variable with this name, blow it off
                itsVariables.removeElementAt(pIndex.intValue());
            }
        }
        int curIndex = varStart++;
        LocalVariable lVar = createLocalVariable(pName, true);
        itsVariables.insertElementAt(lVar, curIndex);
        itsVariableNames.put(pName, new Integer(curIndex));
    }

    public void addLocal(String vName)
    {
        Integer vIndex = (Integer)(itsVariableNames.get(vName));
        if (vIndex != null) {
            LocalVariable v = (LocalVariable)
                            (itsVariables.elementAt(vIndex.intValue()));
            if (v.isParameter()) {
                // this is o.k. the parameter subsumes the variable def.
            }
            else {
                return;
            }
        }
        int index = itsVariables.size();
        LocalVariable lVar = createLocalVariable(vName, false);
        itsVariables.addElement(lVar);
        itsVariableNames.put(vName, new Integer(index));
    }

    // a list of the formal parameters and local variables
    protected Vector itsVariables = new Vector();    

    // mapping from name to index in list
    protected Hashtable itsVariableNames = new Hashtable(11);   

    protected int varStart;               // index in list of first variable

}
