/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
