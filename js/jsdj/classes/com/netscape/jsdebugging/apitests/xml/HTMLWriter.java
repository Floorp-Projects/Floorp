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

package com.netscape.jsdebugging.apitests.xml;

import java.io.*;
import java.util.*;

/**
 * Extends XMLWriter and provides additional HTML-specific functions.
 *
 * @author Alex Rakhlin
 */

public class HTMLWriter extends XMLWriter {

    public HTMLWriter (String filename) {
        super (filename, 0);
        writeHTMLHeader ();
    }

    public void close (){
        writeHTMLFooter ();
        super.close ();
    }
    
    private void writeHTMLHeader(){ println ("<HTML>\n<BODY BGCOLOR=\"silver\">"); }
    private void writeHTMLFooter(){ endTag ("BODY"); endTag ("HTML"); }

    public void printParagraph (String s){   tag ("P", s);   }
    public void printBoldParagraph (String s){ startTag ("B"); tag ("P", s); endTag ("B"); }
    public void link (String s, String url) { write ("<A HREF=\""+url+"\" target=\"_blank\">"+s+"</A>\n"); }
    
    public void cell (String s, String url, String fgcolor){
        if (s.equals ("")) s = "&nbsp;";
        write ("<TD width=\"50%\">");
        write ("<font color=\""+fgcolor+"\">");
        if (url.equals("")) write(s);
        else link (s, url);
        write ("</font>");
        write ("</TD>");
    }

    public void cellPRE (String s, String url, String fgcolor){
        if (s.equals ("")) s = "&nbsp;";
        write ("<TD width=\"50%\">");
        write ("<font color=\""+fgcolor+"\">");
        if (url.equals("")) write("<PRE>"+s+"</PRE>");
        else link (s, url);
        write ("</font>");
        write ("</TD>");
    }
    
    public void twoCells (String s1, String s2, String url1, String url2, String fgcolor){
        write ("<TR>");
        cell (s1, url1, fgcolor);
        cell (s2, url2, fgcolor);
        write ("</TR>");
    }
    
    public void twoCellsPRE (String s1, String s2, String url1, String url2, String fgcolor){
        write ("<TR>");
        cellPRE (s1, url1, fgcolor);
        cellPRE (s2, url2, fgcolor);
        write ("</TR>");
    }
    
    public void twoCellTable (String s1, String s2, String url1, String url2, String bgcolor, String fgcolor){
        println ("<TABLE border=\"1\" width=\"100%\" bgcolor=\""+bgcolor+"\">");
        twoCells (s1, s2, url1, url2, fgcolor);
        println ("</TABLE>");
    }
    public void oneCellTable (String s, String url, String bgcolor, String fgcolor){
        println ("<TABLE border=\"1\" width=\"100%\" bgcolor=\""+bgcolor+"\">");
        cell (s, url, fgcolor);
        println ("</TABLE>");
    }

    public void twoCellTablePRE (String s1, String s2, String url1, String url2, String bgcolor, String fgcolor){
        println ("<TABLE border=\"1\" width=\"100%\" bgcolor=\""+bgcolor+"\">");
        twoCellsPRE (s1, s2, url1, url2, fgcolor);
        println ("</TABLE>");
    }

    public void oneCellTablePRE (String s, String url, String bgcolor, String fgcolor){
        println ("<TABLE border=\"1\" width=\"100%\" bgcolor=\""+bgcolor+"\">");
        cellPRE (s, url, fgcolor);
        println ("</TABLE>");
    }
        
    public void table (Vector column1, Vector column2, String bgcolor, String fgcolor){
        println("<TABLE border=\"1\" width=\"100%\" bgcolor=\""+bgcolor+"\">");
        System.out.println ("TableBoth "+column1.size()+" "+column2.size());
        int count = (int) java.lang.Math.max (column1.size(), column2.size());
        
        String s1, s2, url1, url2;
        for (int i = 0; i < count; i++){
            s1 = ""; s2 = ""; url1 = ""; url2 = "";
            if (i < column1.size()) s1 = (String) column1.elementAt(i);
            if (i < column2.size()) s2 = (String) column2.elementAt(i);
            StringTokenizer st1 = new StringTokenizer (s1, " ");
            StringTokenizer st2 = new StringTokenizer (s2, " ");
            if (st1.countTokens() == 2) url1 = st1.nextToken() + ".html#" + st1.nextToken();
            if (st2.countTokens() == 2) url2 = st2.nextToken() + ".html#" + st2.nextToken();
            twoCells (s1, s2, url1, url2, fgcolor);
        }
        println ("</TABLE>");
    }
    
    public void table (Vector column1, Vector column2, int start1, int start2, int end1, int end2, String bgcolor, String fgcolor){
        System.out.println ("table "+start1+" "+start2+" " + end1+" "+end2+" ");
        Vector v1 = new Vector ();
        Vector v2 = new Vector ();
        if (start1 <= 0) start1 = 1; 
        if (start2 <= 0) start2 = 1;
        for (int i = start1-1; i < end1-1; i++) v1.addElement ((String) column1.elementAt (i));
        for (int i = start2-1; i < end2-1; i++) v2.addElement ((String) column2.elementAt (i));
        table (v1, v2, bgcolor, fgcolor);
    }
    
    public static String pass_fail (int num_known, int num_unknown) {
        if (num_unknown == 0) return "<B><FONT color=\"green\"><PRE>PASS  Known: "+num_known+". Unknown: "+num_unknown+"</PRE></FONT></B>";
        return "<B><FONT color=\"red\"><PRE>FAIL  Known: "+num_known+". Unknown: "+num_unknown+"</PRE></FONT></B>";
    }
    
    public void highlight (String filename, int start1, int start2, int end1, int end2){
        if (filename.equals ("")) return;
        println ("<TABLE BGCOLOR=silver CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\" COLS=2>");
        String st = "";
        DataInputStream din = null;
        try {
            din = new DataInputStream (new FileInputStream (filename));
            int i = 1;
            while (null != (st = din.readLine())) {
                if (st.equals ("")) st = " ";
                if (i == start1) println ("<A NAME=start1></A>");
                if (i == start2) println ("<A NAME=start2></A>");
                write ("<TR>");
                if (i >= start1 && i <= end1) write ("<TD BGCOLOR=red><PRE><FONT FACE='Lucida Console' color=white SIZE=-1>"+st); 
                else write ("<TD><PRE><FONT FACE='Lucida Console' SIZE=-1>"+st);
                if (i >= start2 && i <= end2) println ("<TD BGCOLOR=red><PRE><FONT FACE='Lucida Console' color=white SIZE=-1>"+st); 
                else println ("<TD><PRE><FONT FACE='Lucida Console' SIZE=-1>"+st);
                i ++;
            }
        } catch (FileNotFoundException e){
            System.out.println ("File not found: "+filename);
        } catch (IOException e){
            System.out.println ("Error reading from file "+filename);
        }
        println ("</TABLE>");
    }
    
    public void highlight_line (String filename, int lineno){
        if (filename.equals ("")) return;
        write ("<TABLE BGCOLOR=silver CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\" COLS=1>");
        String st = "";
        DataInputStream din = null;
        try {
            din = new DataInputStream (new FileInputStream (filename));
            int i = 1;
            while (null != (st = din.readLine())) {
                if (st.equals ("")) st = " ";
                if (i == lineno) println ("<A NAME=start></A>");
                write ("<TR>");
                if (i == lineno) println ("<TD BGCOLOR=red><PRE><FONT FACE='Lucida Console' color=white SIZE=-1>"+st);
                else println ("<TD><PRE><FONT FACE='Lucida Console' SIZE=-1>"+st); 
                i ++;
            }
        } catch (FileNotFoundException e){
            System.out.println ("File not found: "+filename);
        } catch (IOException e){
            System.out.println ("Error reading from file "+filename);
        }
        println ("</TABLE>");
    }
    
    public void getLinesFromScript (String name){
        try {
            DataInputStream din = new DataInputStream (new FileInputStream (name));
            String st;
            int i = 1;
            //startTag ("PRE");
            while (null != (st = din.readLine())){
                write ("<A NAME=\""+i+"\">");
                println ("<XMP>"+i+":  "+st+"</XMP>");
                
                i++;
            }
            //endTag ("PRE");
        }
        catch (IOException e) { System.out.println ("Error reading/writing file "+name+"xx");}
    }
    
    public static void makeHTMLFromScript (String name){
        HTMLWriter h = new HTMLWriter (name+".html");
        h.getLinesFromScript (name);
        h.close();
    }

}
