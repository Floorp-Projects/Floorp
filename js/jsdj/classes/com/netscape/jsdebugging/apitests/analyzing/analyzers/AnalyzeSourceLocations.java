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
