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

 package com.netscape.jsdebugging.apitests.analyzing.analyzers;

 import java.io.*;
 import java.util.*;
 import com.netscape.jsdebugging.apitests.xml.*;
 import com.netscape.jsdebugging.apitests.analyzing.data.*;

/**
 * Analyzes eval data object
 *
 * @author Alex Rakhlin
 */


 public class AnalyzeEvals extends AnalyzerBase {

    public AnalyzeEvals (HTMLWriter h, DataPoolManager dpm1, DataPoolManager dpm2, DataPoolManager common) {
        super (h, dpm1, dpm2, common, "Eval test", "eval", Tags.eval_tag);
    }

    public boolean check (Data d, DataPoolManager dpm){
        DataSourceLocation sloc = ((DataEval) d).getFrame().getPC().getSourceLocation ();
        if (sloc.getIgnore()) {
            _known_htmlw.twoCellTablePRE (d.toFormattedString(), dpm.getTestInfo().getEngine(), "", "", "white", "blue");            
            return true;
        }
        
        _unknown_htmlw.twoCellTablePRE (d.toFormattedString(), dpm.getTestInfo().getEngine(), "", "", "red", "white");
        return false;
    }
 }
