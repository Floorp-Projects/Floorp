/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package com.netscape.jsdebugging.apitests.analyzing.data;

import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import com.netscape.jsdebugging.apitests.xml.Tags;
import java.util.*;

/**
 * Even though an interrupt is not something you can give a serial number for, we still want to 
 * represent it by an object. Don't put it into a pool.
 *
 * @author Alex Rakhlin
 */

public class DInterrupt extends Data {

    public DInterrupt (TreeNode head, DataPoolManager dpm){
        super (dpm);
        _ignore = false;
        
        TreeNode t = TreeUtils.getFirstTagImmediate (head, Tags.pc_tag);
        _pc = (DataPC) t.getPointer();

        TreeNode s = TreeUtils.getFirstTagImmediate (head, Tags.stack_tag);
        _stack = new DStack (s, dpm);
    }

    public boolean equalsTo (Data d){
        DInterrupt dint = (DInterrupt) d;
        return (dint.getPC().equalsTo (_pc) && dint.getStack().equalsTo (_stack));
    }
    
    public String toString (){
        return " Interrupt at "+_pc;
    }
    
    public boolean getIgnore () { return _ignore; }
    public void ignore () { _ignore = true; }
    
    public DataPC getPC () { return _pc; }
    public DStack getStack () { return _stack; }

    public DTestInfo getTestInfo () { return _dtinf; }
    public void setTestInfo (DTestInfo d) { _dtinf = d; }
    private DTestInfo _dtinf;
    
    private DataPC _pc;
    private DStack _stack;
    
    private boolean _ignore;
}
