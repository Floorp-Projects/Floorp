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
 * This object contains info about the test. Don't put it into a pool.
 *
 * @author Alex Rakhlin
 */

public class DTestInfo extends Data {

    public DTestInfo (TreeNode head, DataPoolManager dpm){
        super (dpm);

        TreeNode h = TreeUtils.getFirstTagImmediate (head, Tags.header_tag);
        _start_date = TreeUtils.getChildStringData (h, Tags.date_start_tag);
        _finish_date = TreeUtils.getChildStringData (h, Tags.date_finish_tag);
        _version = TreeUtils.getChildStringData (h, Tags.version_tag);
        TreeNode m = TreeUtils.getFirstTagImmediate (head, Tags.main_tag);
        _engine = TreeUtils.getChildStringData (m, Tags.engine_tag);

        TreeNode t = TreeUtils.getFirstTagImmediate (m, Tags.tests_tag);
        Vector tests = TreeUtils.findAllTags (t, Tags.test_tag);
        _tests = new Vector();
        for (int i = 0; i < tests.size (); i++)
            _tests.addElement (((TreeNode) tests.elementAt (i)).getText());

        TreeNode s = TreeUtils.getFirstTagImmediate (m, Tags.script_files_tag);
        Vector files = TreeUtils.findAllTags (s, Tags.file_tag);
        _files = new Vector();
        for (int i = 0; i < files.size (); i++)
            _files.addElement (((TreeNode) files.elementAt (i)).getText());

    }

    public String toString () {
        String scr_files = "", tests = "";
        for (int i = 0; i < _files.size(); i++) scr_files = scr_files + (String) _files.elementAt (i) + " ";
        for (int i = 0; i < _tests.size(); i++) tests = tests + (String) _tests.elementAt (i) + " ";
        return "STARTED: " + _start_date + "\n"+
               "ENDED: " + _finish_date + "\n"+
               "ENGINE: " + _engine + "\n"+
               "SCRIPT FILES: " + scr_files + "\n"+
               "TESTS PERFORMED: " + tests;
    }

    public boolean equalsTo (Data d){
        // doesn't make much sense to compare these objects
        return false;
    }

    public String getStartDate () { return _start_date; }
    public String getFinishDate () { return _finish_date; }
    public String getVersion () { return _version; }
    public String getEngine () { return _engine; }
    public Vector getTests () { return _tests; }
    public Vector getFiles () { return _files; }

    private String _start_date;
    private String _finish_date;
    private String _version;
    private String _engine;
    private Vector _tests;
    private Vector _files;

}
