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
 * Analyzes two Script Pools
 *
 * @author Alex Rakhlin
 */


 public class AnalyzeScriptPools extends AnalyzerBase {
    
    public AnalyzeScriptPools (HTMLWriter h, DataPoolManager dpm1, DataPoolManager dpm2, DataPoolManager common) {
        super (h, dpm1, dpm2, common, "Scripts test", "scr", Tags.script_tag);
    }

    /**
     * overwrite analyze method. analyzing pools is a bit different: we want to find a "match"
     * for the script, not just iterate through the list and print out each difference 
     */
    public void analyze (){
        analyzePools ();
        list (_common_htmlw, _common);
        
        link_to_files ();
        done ();
    }

    public void analyzePools (){
        DataPool _script_pool1 = _dpm1.getPool (_tag);
        DataPool _script_pool2 = _dpm2.getPool (_tag);
        
        _check_first_against_second (_script_pool1, _script_pool2);
        _check_first_against_second (_script_pool2, _script_pool1);
    }

    private void _check_first_against_second (DataPool _script_pool1, DataPool _script_pool2){
        for (int i = 0; i < _script_pool1.getPool().size(); i++){
            
            DataScript d1 = (DataScript) _script_pool1.getPool().elementAt (i);
            if (d1.getIgnore()) continue;
            
            String url = d1.getURL();
            String fun = d1.getFunction();
            DataScript d2 = null; //script corresponding to pool1 [i]
            int j = 0;
            while (j < _script_pool2.getPool().size()){
                DataScript e = (DataScript) _script_pool2.getPool().elementAt (j);
                if (e.getIgnore()) { j ++; continue;}
                if (e.getURL().equals (url) && e.getFunction().equals (fun)){
                    d2 = e;
                    break;
                }
                j ++;
            }
            if ( d2 == null ) {
                link (_unknown_htmlw, "No match for script", d1, d2, _dpm1.getTestInfo(), _dpm2.getTestInfo()); 
                d1.ignore();
                _number_of_unknown_diffs ++;
            }
            else {
                link (_known_htmlw, "scope", d1, d2, _dpm1.getTestInfo(), _dpm2.getTestInfo()); 
                _number_of_known_diffs ++;
                d1.ignore ();
                d2.ignore ();
            }
        }
    }

    private void _describe_scripts (String filename, DataScript d1, DataScript d2, DTestInfo dtinf1, DTestInfo dtinf2){
        HTMLWriter h = new HTMLWriter (filename);
        int start1 = 0, start2 = 0, end1 = 0, end2 = 0;
        String description1 = "";
        String description2 = "";
        String url = "";
        if (d1 != null) {
            description1 = d1.toFormattedString();
            start1 = d1.getBaselineno();
            end1 = d1.getBaselineno()+d1.getLineExtent()-1;
            url = d1.getURL();
        }
        if (d2 != null) {
            description2 = d2.toFormattedString();
            start2 = d2.getBaselineno();
            end2 = d2.getBaselineno()+d2.getLineExtent()-1;
        }
        h.twoCellTablePRE (description1, description2, "", "", "yellow", "black");
        h.twoCellTable ("ENGINE: " + dtinf1.getEngine(), "ENGINE: " + dtinf2.getEngine(), "", "", "white", "black");
        h.highlight (url, start1, start2, end1, end2);
        
        h.close();
    }


    public void link (HTMLWriter htmlw, String text, DataScript d1, DataScript d2, DTestInfo dtinf1, DTestInfo dtinf2)
    {
        String filename  = "scr"+getIndex()+".html";
        htmlw.println ("<TABLE border=\"1\" width=\"100%\" bgcolor=\"#FFF0F0\">");
        htmlw.startTag ("TD");
        htmlw.link (text, filename+"#start1");
        htmlw.endTag ("TD");
        htmlw.endTag ("TABLE");
        
        _describe_scripts (filename, d1, d2, dtinf1, dtinf2);
    }
    
    public static void makeHTMLFilesFromScripts (DataPool script_pool) {
        for (int i = 0; i < script_pool.getPool().size(); i++){
            
            DataScript d = (DataScript) script_pool.getPool().elementAt (i);
            if (d.getIgnore()) continue;
            
            String url = d.getURL();
            String fun = d.getFunction();
            
            if (fun.equals ("none")) {
                if (! new File (HTMLWriter.getBaseDirectory ()+url+".html").exists()){
                    HTMLWriter.makeHTMLFromScript (url);
                }
            }
            
        }
        
    }
        

 }
