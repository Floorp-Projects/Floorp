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
 import com.netscape.jsdebugging.apitests.analyzing.tree.*;

/**
 * Base class for analyzers
 *
 * @author Alex Rakhlin
 */


 public abstract class AnalyzerBase {

    public AnalyzerBase (HTMLWriter h, DataPoolManager dpm1, DataPoolManager dpm2, DataPoolManager common, 
                         String test_name, String prefix, String tag){
        _pass = true;
        _htmlw_main = h;
        _dpm1 = dpm1;
        _dpm2 = dpm2;
        _common = common;
        _test_name = test_name;
        _prefix = prefix;
        _tag = tag;
        _main_filename = _prefix + "_main.html";
        _known_filename = _prefix + "_known.html";
        _unknown_filename = _prefix + "_unknown.html";
        _common_filename = _prefix + "_intersect.html";

        _main_htmlw = new HTMLWriter (_main_filename);
        _known_htmlw = new HTMLWriter (_known_filename);
        _unknown_htmlw = new HTMLWriter (_unknown_filename);
        _common_htmlw = new HTMLWriter (_common_filename);

    }

    public void done () {
        _main_htmlw.close();
        _known_htmlw.close();
        _unknown_htmlw.close();
        _common_htmlw.close();
    }
    
    public void analyze (){
        analyzePool (_dpm1);
        analyzePool (_dpm2);
        list (_common_htmlw, _common);
        link_to_files ();
        done ();
    }
    
    public void link_to_files (){
        if (_number_of_unknown_diffs > 0) 
            _main_htmlw.oneCellTable ("UNKNOWN DIFFERENCES", _unknown_filename, "red", "white");
        _main_htmlw.oneCellTable ("KNOWN DIFFERENCES", _known_filename, "white", "black");
        _main_htmlw.oneCellTable ("COMMON", _common_filename, "white", "black");

        _htmlw_main.twoCellTable (_test_name, HTMLWriter.pass_fail (_number_of_known_diffs, _number_of_unknown_diffs),
                                  _main_filename, "", "white", "black");       
    }
    
    /**
     * This function should be implemented by each test. It checks if this data is a known difference.
     * This function is also responsible for writing output to known_htmlw, unknown_htmlw, etc.
     */
    public boolean check (Data d, DataPoolManager dpm){ 
        return false; 
    }

    public void analyzePool (DataPoolManager dpm){
        DTestInfo dtinf = dpm.getTestInfo();
        DataPool pool = dpm.getPool (_tag);

        for (int i = 0; i < pool.getPool().size(); i ++){
            DataSerializable d = (DataSerializable) pool.getPool().elementAt(i);
            if (d.getIgnore ()) continue;

            if (check (d, dpm)) {
                d.ignore ();
                _number_of_known_diffs ++;
                continue;
            }
            else {
                _number_of_unknown_diffs ++;
            }
        }
    }

    /**
     * This function lists (writes out) a pool corresponding to _tag in the manager
     * Overwrite this for specific output.
     */
    public void list (HTMLWriter htmlw, DataPoolManager dpm){
        DataPool pool = dpm.getPool (_tag);
        for (int i = 0; i < pool.getPool().size(); i ++){
            Data d = (Data) pool.getPool().elementAt(i);
            if (d.getIgnore ()) continue;

            htmlw.oneCellTablePRE (d.toFormattedString(), "", "white", "black");
        }
    }
    
    public static String getSource (String url, int line){
        DataInputStream din = null;
        String st;
        try {
            din = new DataInputStream (new FileInputStream (url));
            int i = 1;
            while (null != (st = din.readLine())) {
                if (i == line) return st;
                i ++;
            }
        }
        catch (FileNotFoundException e){ System.out.println ("File not found: "+url); }
        catch (IOException e){ System.out.println ("Error reading from file "+url); }

        return "";
    }
    

    // index is used for making unique filenames.
    public int getIndex () { return _index ++; }
    private int _index = 0;    

    /* pass/fail result of the whole test */
    protected boolean _pass;
    protected int _number_of_known_diffs = 0;
    protected int _number_of_unknown_diffs = 0;

    protected DataPoolManager _dpm1;
    protected DataPoolManager _dpm2;
    protected DataPoolManager _common;
    protected HTMLWriter _htmlw_main;
    protected String _test_name;
    protected String _prefix;
    protected String _tag;

    protected String _main_filename;
    protected String _known_filename;
    protected String _unknown_filename;
    protected String _common_filename;

    protected HTMLWriter _main_htmlw;
    protected HTMLWriter _known_htmlw;
    protected HTMLWriter _unknown_htmlw;
    protected HTMLWriter _common_htmlw;

 }
