/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
