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
 * Analyzes a SourceLocation data object
 *
 * @author Alex Rakhlin
 */


 public class AnalyzeSourceLocations extends AnalyzerBase {
    
    public AnalyzeSourceLocations (HTMLWriter h, DataPoolManager dpm1, DataPoolManager dpm2, DataPoolManager common) {
        super (h, dpm1, dpm2, common, "SourceLocation test", "sloc", Tags.source_location_tag);
    }
    
    public boolean check (Data d, DataPoolManager dpm){
        DataSourceLocation dsloc = (DataSourceLocation) d;
        int lineno = dsloc.getLineno();
        String url = dsloc.getURL();

        if (dsloc.getSerialNumber() == -1) {
            link (_known_htmlw, url+" "+lineno+": serial_number is -1. This is usually due to a result from getClosestPC", dpm.getTestInfo().getEngine(), dsloc, "white", "black");
            return true;
        }

        String source = getSource (url, lineno);

        if (source.indexOf ('=') == -1 && source.indexOf ("var ") != -1){
            link (_known_htmlw, url+" "+lineno+": var", dpm.getTestInfo().getEngine(), dsloc, "white", "black");
            return true;
        }
        if (source.trim().equals ("}")){
            link (_known_htmlw, url+" "+lineno+": }", dpm.getTestInfo().getEngine(), dsloc, "white", "black");
            return true;
        }
        
        link (_unknown_htmlw, url+" "+lineno+": Unknown difference", dpm.getTestInfo().getEngine(), dsloc, "red", "white");
        return false;
    }


    public void link (HTMLWriter htmlw, String text, String engine, DataSourceLocation d, String bgcolor, String fgcolor)
    {
        String filename = "sloc"+getIndex()+".html";
        htmlw.twoCellTable (text, engine, filename+"#start", "", bgcolor, fgcolor);

        HTMLWriter h = new HTMLWriter (filename);
        h.oneCellTablePRE (d.toFormattedString(), "", "yellow", "black");
        h.highlight_line (d.getURL(), d.getLineno());
        h.close();
    }
    
    public void list (HTMLWriter htmlw, DataPoolManager dpm){
        DTestInfo dtinf = dpm.getTestInfo();
        DataPool pool = dpm.getPool (Tags.source_location_tag);
        Vector v = new Vector();
        for (int i = 0; i < pool.getPool().size(); i ++){
            DataSourceLocation d = (DataSourceLocation) pool.getPool().elementAt(i);
            if (d.getIgnore ()) continue;
            v.addElement (d.getURL()+" "+d.getLineno());
        }
        htmlw.table (v, new Vector (), "white", "black");
    }    
 }
