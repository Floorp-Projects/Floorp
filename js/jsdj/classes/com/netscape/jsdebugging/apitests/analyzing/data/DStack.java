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
 * Even though Stack is not something you can give a serial number for, we still want to 
 * represent it by an object. Don't put it into a pool.
 *
 * @author Alex Rakhlin
 */

public class DStack extends Data {

    public DStack (TreeNode head, DataPoolManager dpm){
        super (dpm);
        _count = TreeUtils.getChildIntegerData (head, Tags.count_tag).intValue();
        _thread_state = TreeUtils.getChildIntegerData (head, Tags.count_tag).intValue();
        
        Vector frames = TreeUtils.findAllTags (head, Tags.frame_tag);
        _frames = new Vector();
        for (int i = 0; i < frames.size(); i++){
            TreeNode t = (TreeNode) frames.elementAt (i);
            _frames.addElement (new DFrame (t, dpm));
        }
    }

    public boolean equalsTo (Data d){
        DStack ds = (DStack) d;
        if (ds.getCount() != _count) return false;
        if (ds.getThreadState() != _thread_state) return false;
        Vector fr = ds.getFrames();
        if (fr.size() != _frames.size()) return false;
        for (int i = 0; i < fr.size(); i++) {
            DFrame df1 = (DFrame) fr.elementAt (i);
            DFrame df2 = (DFrame) _frames.elementAt (i);
            if (! df1.equalsTo (df2)) return false;
        }
        return true;
    }

    public Vector getFrames () { return _frames; }
    public int getCount () { return _count; }
    public int getThreadState () { return _thread_state; }

    private Vector _frames;

    private int _count;
    private int _thread_state;
}
