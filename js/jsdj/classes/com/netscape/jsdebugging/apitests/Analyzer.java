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

 package com.netscape.jsdebugging.apitests;

 import java.io.*;
 import java.util.*;
 import com.netscape.jsdebugging.apitests.xml.*;
 import com.netscape.jsdebugging.apitests.analyzing.data.*;
 import com.netscape.jsdebugging.apitests.analyzing.tree.*;
 import com.netscape.jsdebugging.apitests.analyzing.analyzers.*;

/**
 * Analyzes two log files by building and processing trees.
 *
 * @author Alex Rakhlin
 */

 public class Analyzer {

    public static void main(String args[]){

        Tags.init();
        if (!_processArguments (args)) return;

        try {
            System.setErr (new PrintStream (new FileOutputStream ("error_analyzer.log")));
        } catch (IOException e) { System.out.println ("Error redirecting error stream"); }
        try {
            System.setOut (new PrintStream (new FileOutputStream ("output_analyzer.log")));
        } catch (IOException e) { System.out.println ("Error redirecting error stream"); }

        HTMLWriter.setBaseDirectory (_base_dir);

        _htmlw = new HTMLWriter (_outfilename);
        check_for_error_log (_htmlw, "error_main_RHINO.log");
        check_for_error_log (_htmlw, "error_main_SPIDER_MONKEY.log");

        Date start = new Date();
        analyze ();
        Date finish = new Date();

        _htmlw.oneCellTablePRE ("ANALYZER \nSTARTED: "+start+"\nENDED: "+finish, "", "yellow", "blue");

        check_for_error_log (_htmlw, "error_analyzer.log");
        _htmlw.close();

    }

    private static boolean _processArguments (String args[]){
        if (args.length < 6) {
            usage();
            return false;
        }
        for (int i=0; i < args.length; i++) {
            String arg = args[i];
            if (arg.equals("-f1")) _filename1 = args [i+1];
            if (arg.equals("-f2")) _filename2 = args [i+1];
            if (arg.equals("-out")) _outfilename = args [i+1];
            if (arg.equals("-outdir")) _base_dir = args [i+1];
        }
        return true;
    }


    public static void analyze (){
        /* parse first file */
        _parser = new XMLParser ();
        _parser.parse (_filename1);
        _head1 = _parser.getTree ();
        /* parse second file */
        _parser = new XMLParser ();
        _parser.parse (_filename2);
        _head2 = _parser.getTree ();
        /* get descriptions */
        _dpm1 = new DataPoolManager();
        _dpm1.getDataFromTree (_head1);
        /* get descriptions */
        _dpm2 = new DataPoolManager();
        _dpm2.getDataFromTree (_head2);
        /* get test information */
        DTestInfo dtinf1 = new DTestInfo (TreeUtils.searchDepthFirst (_head1, Tags.doc_tag), _dpm1);
        DTestInfo dtinf2 = new DTestInfo (TreeUtils.searchDepthFirst (_head2, Tags.doc_tag), _dpm2);
        /* set test information field of the DataPoolManager */
        _dpm1.setTestInfo (dtinf1);
        _dpm2.setTestInfo (dtinf2);

        _htmlw.oneCellTablePRE (dtinf1.toString(), "", "yellow", "black");
        _htmlw.oneCellTablePRE (dtinf2.toString(), "", "yellow", "black");

        /* remove duplicate descriptions and redirect tree pointers to new data */
        _dpm1.removeDuplicatesAndFixTree (_head1);
        _dpm2.removeDuplicatesAndFixTree (_head2);

        /* intersect two pool managers */
        DataPoolManager _intersect = DataPoolManager.intersectPools (_dpm1, _dpm2);


        /* NEED TO DO THIS BEFORE OTHER TESTS: create html files from .js with anchors */
        AnalyzeScriptPools.makeHTMLFilesFromScripts (_dpm1.getPool (Tags.script_tag));
        AnalyzeScriptPools.makeHTMLFilesFromScripts (_dpm2.getPool (Tags.script_tag));
        AnalyzeScriptPools.makeHTMLFilesFromScripts (_intersect.getPool (Tags.script_tag));

        AnalyzeScriptPools ascr = new AnalyzeScriptPools (_htmlw, _dpm1, _dpm2, _intersect);
        ascr.analyze();

        AnalyzeSourceLocations asl = new AnalyzeSourceLocations (_htmlw, _dpm1, _dpm2, _intersect);
        asl.analyze();

        // !!!!! check if actually doing interrupt test !!!
        // better do interrupt test after analyzing pc's, evals, errors
        AnalyzeInterrupts ai = new AnalyzeInterrupts (_htmlw, _head1, _head2, _dpm1, _dpm2);
        ai.analyze ();

        AnalyzeEvals ae = new AnalyzeEvals (_htmlw, _dpm1, _dpm2, _intersect);
        ae.analyze ();

        AnalyzeErrorReports aerr = new AnalyzeErrorReports (_htmlw, _dpm1, _dpm2, _intersect);
        aerr.analyze ();
    }

    public static void check_for_error_log (HTMLWriter htmlw, String filename){
         try {
            BufferedInputStream f = new BufferedInputStream (new FileInputStream (filename));
            int b = f.read ();
            if (b != -1) htmlw.oneCellTablePRE ("Error log:     "+filename, filename, "red", "white");
         }
         catch (FileNotFoundException e) {}
         catch (IOException ee) {}
    }
    
    public static void usage () {
        System.out.println ("Usage: Analyzer -f1 [filename1] -f2 [filename2] -out [filename_out] -outdir [dir]");
        System.out.println (" -f1     = first log to analyze ");
        System.out.println (" -f2     = second log to analyze ");
        System.out.println (" -out    = output html file ");
        System.out.println (" -outdir = base directory for the output ");
    }

    private static XMLParser _parser;
    private static String _filename1;
    private static String _filename2;
    private static String _outfilename;
    private static String _base_dir;
    private static HTMLWriter _htmlw;

    private static TreeNode _head1;
    private static TreeNode _head2;
    private static DataPoolManager _dpm1;
    private static DataPoolManager _dpm2;
 }
