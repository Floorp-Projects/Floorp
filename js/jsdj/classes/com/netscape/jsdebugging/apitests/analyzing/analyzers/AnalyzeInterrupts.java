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
 * Analyzes interrupts. This requires much more work and doesn't fit AnalyzerBase structure.
 *
 * @author Alex Rakhlin
 */


 public class AnalyzeInterrupts {
    
    
    public AnalyzeInterrupts (HTMLWriter h, TreeNode head1, TreeNode head2, DataPoolManager dpm1, DataPoolManager dpm2){
        _pass = true;
        
        _dtinf1 = dpm1.getTestInfo();
        _dtinf2 = dpm2.getTestInfo();
        
        _sloc_pool1 = dpm1.getPool (Tags.source_location_tag);
        _sloc_pool2 = dpm2.getPool (Tags.source_location_tag);
        
        _htmlw_main = h;
        
        Vector i1 = TreeUtils.findAllTags (head1, Tags.interrupt_tag);
        Vector i2 = TreeUtils.findAllTags (head2, Tags.interrupt_tag);
        
        _dinterrupt_list1 = new Vector ();
        _dinterrupt_list2 = new Vector ();
        
        _unknown_differences = new Vector ();
        
        for (int i = 0; i < i1.size(); i++) {
            DInterrupt d = new DInterrupt ((TreeNode) i1.elementAt (i), dpm1);
            d.setTestInfo (dpm1.getTestInfo());
            _dinterrupt_list1.addElement (d);
        }
        for (int i = 0; i < i2.size(); i++) {
            DInterrupt d = new DInterrupt ((TreeNode) i2.elementAt (i), dpm2);
            d.setTestInfo (dpm2.getTestInfo());
            _dinterrupt_list2.addElement (d);
        }
        
    }

    public void analyze (){
        
        String main_filename = "int_main.html";
        String all_filename = "int_all.html";
        String unknown_filename = "int_unknown.html";
        
        HTMLWriter main_htmlw = new HTMLWriter (main_filename);
        HTMLWriter all_htmlw = new HTMLWriter (all_filename);
        HTMLWriter unknown_htmlw = new HTMLWriter (unknown_filename);
        
        all_htmlw.twoCellTable ("TESTS ON "+_dtinf1.getEngine(), "TESTS ON "+_dtinf2.getEngine(), "", "", "yellow", "black");    
             
        if (_remove_invalid && !_output_everything){
            removeInvalid (_dinterrupt_list1, _sloc_pool1);
            removeInvalid (_dinterrupt_list2, _sloc_pool2);
        }
        
        if (!_output_everything){
            removeSequences (_dinterrupt_list1);
            removeSequences (_dinterrupt_list2);
        }
        
        DataOutputStream d1, d2; 
        String file1 = "interrupt1.log";
        String file2 = "interrupt2.log";
        String diff = "difference.log";
        try {
            d1 = new DataOutputStream (new FileOutputStream (file1));
            d2 = new DataOutputStream (new FileOutputStream (file2));
                        
            _strings1 = writeOutInterrupts (d1, _dinterrupt_list1, _sloc_pool1);
            _strings2 = writeOutInterrupts (d2, _dinterrupt_list2, _sloc_pool2);
        
            d1.close();
            d2.close();
            
        }
        catch (IOException e) { System.out.println ("Error initing/writing to files "); }
                
        _differences = runDiff (file1, file2, diff);

        output_all_interrupts (all_htmlw);
        list (unknown_htmlw, _unknown_differences);
    
        if (!_pass) main_htmlw.oneCellTable ("UNKNOWN DIFFERENCES", unknown_filename, "red", "white");
        main_htmlw.oneCellTable ("ALL INTERRUPTS", all_filename, "white", "black");
        
        
        main_htmlw.close();
        all_htmlw.close();
        unknown_htmlw.close();
        
        _htmlw_main.twoCellTable ("Interrupts test", HTMLWriter.pass_fail (_number_of_known_diffs, _number_of_unknown_diffs), 
                                  main_filename, "", "white", "black");
        
    }
    
    
    public void outputTableBoth (int start1, int start2, int end1, int end2, String bgcolor, HTMLWriter htmlw){
        htmlw.table (_strings1, _strings2, start1, start2, end1, end2, bgcolor, "black");
    }

    public void outputLeftColumnTable (int start, int end, String bgcolor, HTMLWriter htmlw){
        Vector v = removeKnownDifferences (_strings1, start, end, _dinterrupt_list1, _sloc_pool1);
        htmlw.table (v, new Vector(), bgcolor, "black");
    }
    
    public void outputRightColumnTable (int start, int end, String bgcolor, HTMLWriter htmlw){
        Vector v = removeKnownDifferences (_strings2, start, end, _dinterrupt_list2, _sloc_pool2);
        htmlw.table (v, new Vector(), bgcolor, "black");
    }

    public void outputTableBothToNewFile (int start1, int start2, int end1, int end2, String color, HTMLWriter htmlw){
        if (end1 - start1 < 1 && end2 - start2 < 1) return;
        _counter ++;
        String filename = "common"+_counter+".html";
        htmlw.link ("Common section", filename);
        HTMLWriter h = new HTMLWriter (filename);
        outputTableBoth (start1, start2, end1, end2, color, h);
        h.close();
    }
    
    public void output_all_interrupts (HTMLWriter htmlw){
        int max; 
        if (_strings1.size() > _strings2.size()) max = _strings1.size();
        else max = _strings2.size();
        
        int index1 = 1, index2 = 1;
        
        for (int i = 0; i < _differences.size(); i++){
            String diff = (String) _differences.elementAt (i);
            int d = diff.indexOf ('d');
            int a = diff.indexOf ('a');
            int c = diff.indexOf ('c');
            int position;
            if (d != -1) position = d;
            else if (a != -1) position = a;
            else if (c != -1) position  = c;
            else continue;
            
            String before = new String (diff.substring (0, position));
            String after = new String (diff.substring (position+1));                    
            int comma1 = before.indexOf (',');
            int comma2 = after.indexOf (',');
            int from1, to1, from2, to2;
            if (comma1 != -1) {
                from1 = Integer.valueOf (before.substring (0, comma1)).intValue();
                to1  = Integer.valueOf (before.substring (comma1+1)).intValue();
            } else {
                from1 = Integer.valueOf (before).intValue();
                to1 = -1;
            }
            if (comma2 != -1) {
                from2 = Integer.valueOf (after.substring (0, comma2)).intValue();
                to2  = Integer.valueOf (after.substring (comma2+1)).intValue();
            } else {
                from2 = Integer.valueOf (after).intValue();
                to2 = -1;
            }
            int max1 = (int) java.lang.Math.max (from1, to1);
            int max2 = (int) java.lang.Math.max (from2, to2);
            
            if (d != -1) {
                outputTableBothToNewFile (index1, index2, from1, from2 + 1, _color_normal, htmlw);                    
                outputLeftColumnTable (from1, max1 + 1, _color_d, htmlw);
            }
            else 
            if (a != -1) {
                outputTableBothToNewFile (index1, index2, from1 + 1 , from2, _color_normal, htmlw);
                outputRightColumnTable (from2, max2 + 1, _color_a, htmlw);
            }
            else
            if (c != -1) {
                outputTableBothToNewFile (index1, index2, from1, from2, _color_normal, htmlw);
                outputTableBoth (from1, from2, max1 + 1, max2 + 1, _color_c, htmlw);
            }
            
            index1 = max1 + 1;
            index2 = max2 + 1;
       }
    }
        
    
    public Vector runDiff (String file1, String file2, String diff){
        Vector differences = new Vector();
        try {
            Runtime rm = java.lang.Runtime.getRuntime();
            Process p = rm.exec ("diff "+file1+" "+file2); 
            BufferedReader din = new BufferedReader(new InputStreamReader(p.getInputStream()));
            
            String st = "";
            while (null != (st = din.readLine()))
                if (st.charAt (0) != '>' && st.charAt (0) != '<' && st.charAt (0) != '-' )  
                        differences.addElement (st);
 
            din.close();
        }
        catch (IOException e) { System.out.println ("ERROR EXECUTING PROGRAM "+e.getMessage()); }
        
        return differences;
    }
    
    public Vector writeOutInterrupts (DataOutputStream out, Vector interrupt, DataPool pool) 
      throws IOException {
        System.out.println ("Size: "+interrupt.size());
        Vector strings = new Vector();
        int i = 0;
        while (i < interrupt.size()){
            DInterrupt d = (DInterrupt) interrupt.elementAt (i);
            if (_output_everything || ((!_remove_invalid || pool.findFirst (d.getPC()) == null) && !d.getIgnore())) {
                String st = d.getPC().getSourceLocation().getURL()+" "+d.getPC().getSourceLocation().getLineno();
                strings.addElement (st);
                out.writeBytes (st + "\n");
                i++;
            }
            else {
                interrupt.removeElementAt (i);
                System.out.println ("removing "+d.getPC().getSourceLocation().getLineno());
            }
        }        
        return strings;
    }
    
    /** If there are several interrupts with the same pc consequently, remove them, leave only one
     */
    public void removeSequences (Vector interrupt){
        if (interrupt.size() == 0) return;
        DInterrupt old = (DInterrupt) interrupt.elementAt (0);
        int i = 1;
        DInterrupt cur;
        while (i < interrupt.size()){
            cur = (DInterrupt) interrupt.elementAt (i);
            if (cur.getPC().equalsTo (old.getPC())) interrupt.removeElement (cur);
            else {
                old = cur;
                i ++;
            }
        }
    }
    
    /** 
     * Remove interrupts with PC which  is not in the other pool or was set to be ignored.
     */
    public void removeInvalid (Vector interrupt, DataPool pool){
        if (interrupt.size() == 0) return;
        int i = 0;
        DInterrupt cur;
        while (i < interrupt.size()){
            cur = (DInterrupt) interrupt.elementAt (i);
            if (cur.getPC().getSourceLocation().getIgnore() || pool.findFirst (cur.getPC().getSourceLocation()) == null)
                    interrupt.removeElementAt (i);
            else i ++;
        }
    }
    
    public Vector removeKnownDifferences (Vector strings, int start, int end, Vector interrupts, DataPool pool){
        Vector result = new Vector ();
        if (strings.size() == 0) return result;
        String old = "";
        boolean known_difference;
        if (start <= 0) start = 1;
        
        for (int i = start - 1; i < end-1; i++) {
            DInterrupt d = (DInterrupt) interrupts.elementAt (i);
            
            String url = d.getPC().getSourceLocation().getURL();
            int lineno = d.getPC().getSourceLocation().getLineno();
            
            if  (d.getIgnore() || d.getPC().getSourceLocation().getIgnore () || pool.findFirst (d.getPC().getSourceLocation()) == null ||
                (_remove_single_curly_braces && AnalyzerBase.getSource (url, lineno).trim().equals ("}")) || 
                (_remove_single_curly_braces && AnalyzerBase.getSource (url, lineno).trim().equals ("{")) ||
                (_remove_spaces && AnalyzerBase.getSource (url, lineno).trim().equals ("")) || 
                (_remove_for_loops && AnalyzerBase.getSource (url, lineno).indexOf ("for") != -1)||
                (_remove_function_declarations && AnalyzerBase.getSource (url, lineno).indexOf ("function") != -1))
                
                {
                    known_difference = true;
                    _number_of_known_diffs ++;
                }
                else {
                    _number_of_unknown_diffs ++;
                    known_difference = false;
                }
            
            if (! known_difference ) {
                _pass = false;
                Enumeration e = _unknown_differences.elements ();
                boolean found = false;
                while (e.hasMoreElements () && !found) if (((DInterrupt)e.nextElement()).equalsTo (d)) found = true;
                if (!found) _unknown_differences.addElement (d);
                
            }
            if (! known_difference || _output_everything) {
                /* make sure we don't add two equal strings one after another */
                String st = (String) strings.elementAt (i);
                if (! st.equals (old) || i == start - 1) {
                    result.addElement (st);
                    old = st;
                }
            }
            
        }
        
        return result;
    }
    
    public void list (HTMLWriter h, Vector v){        
        Vector strings = new Vector ();
        Vector info = new Vector ();
        for (int i = 0; i < v.size(); i++){
            DInterrupt d = (DInterrupt) v.elementAt (i);
            strings.addElement (d.getPC().getSourceLocation().getURL()+" "+d.getPC().getSourceLocation().getLineno());
            info.addElement (d.getTestInfo().getEngine());
        }
        
        h.table (strings, info, "red", "white");
    }
        
    private DTestInfo _dtinf1;
    private DTestInfo _dtinf2;        
    private DataPool _sloc_pool1;
    private DataPool _sloc_pool2;
    private HTMLWriter _htmlw_main;
    private Vector _dinterrupt_list1;
    private Vector _dinterrupt_list2;
    private Vector _strings1;
    private Vector _strings2;
    private Vector _differences; // strings like "1,7d2" -- results of diff
    private Vector _unknown_differences;
    
    /* pass/fail result of the whole test */
    private boolean _pass;
    private int _number_of_known_diffs = 0;
    private int _number_of_unknown_diffs = 0;

    private int _counter = 0;
    
    /* Do we want to output every step of stepping to the file ? */
    private boolean _output_everything = true;
    
    /* KNWON DIFFERENCES FLAGS. MAX INFORMATION WILL BE OBTAINTED BY SETTING ALL THESE TO FALSE */
    /* should we remove where it stoped at sourcelocations not present in the other engine run ?*/
    private boolean _remove_invalid = true;
    /* should ignore all single curly braces ? */
    private boolean _remove_single_curly_braces = true;
    /* should ignore spaces ? */
    private boolean _remove_spaces = true;
    /* should ignore comments ? */
    private boolean _remove_comments = true;
    /* should ignore "for" loops */ 
    private boolean _remove_for_loops = true;
    /* should ignore function declarations */ 
    private boolean _remove_function_declarations = true;
    
    
    
    
    
    
    
    private static final String _color_normal = "#FFFFFF";
    private static final String _color_a = "#FFF0F0";
    private static final String _color_d = "#FFF0F0";
    private static final String _color_c = "#FFB0B0";
 }
