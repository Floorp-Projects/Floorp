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
import com.netscape.jsdebugging.apitests.xml.Tags;


/** 
 * Writes info in the XML format. 
 *
 * @author Alex Rakhlin
 */
public class XMLWriter {

    /**
     * @param filename Name of the file to write into.
     * @param indent Starting indent in the file
     */
    public XMLWriter (String filename, int indent) {
        try {
            _output = new RandomAccessFile (_base_directory+filename, "rw");
            //_output.seek (_output.length());
            _indent = indent;
        } catch (IOException e) { System.out.println ("Error opening "+_base_directory+filename); }
    }
    
    /**
     * Writes string to the file
     */
    public void write (String s){
        try {
            _output.writeBytes (s);
        } catch (IOException e) { System.out.println ("Error"); }
    }

    private void _indent (){
        String s = "";
        for (int i = 0; i<_indent*INDENT_SIZE; i++) s = s + " ";
        write (s);
    }

    /**
     * Writes string and the new-line character
     */
    public void println (String s){ write (s+"\n");  }

    /**
     * Writes an open tag with indent.
     */
    public void startTag (String s){
        _indent();
        write ("<"+s+">");
        _indent ++;
        write ("\n");
    }

    /**
     * Writes a closing tag with indent.
     */
    public void endTag (String s){
        _indent --;
        _indent();
        write ("</"+s+">");
        write ("\n");
    }

    /**
     * Writes an opening tag, value of the tag, and a closing tag.
     */
    public void tag (String tag, String val){
        _indent();
        write ("<"+tag+">");
        write ( XMLEscape.escape(val) ); // write escaped value
        write ("</"+tag+">");
        write ("\n");
    }

    public void tag (String tag, int val){ tag (tag, (new Integer (val)).toString());}

    public void tag (String tag, boolean val){ tag (tag, (new Boolean (val)).toString());}

    /**
     * Close the output file.
     */
    public void close (){
        try {
            _output.close();
        } catch (IOException e) { System.out.println ("Error closing xml document"); }
    }

    // remove this later
    public void prDTDInit (){
        write ("<?xml version=\"1.0\"?>\n");
        write ("<!DOCTYPE DOC SYSTEM \"Tests.dtd\">\n");
    }

    private RandomAccessFile _output;
    private int _indent;

    private final int INDENT_SIZE = 1;
    
    /**
     * Set the base directory for XMLWriter. This base directory is prepended to the filename
     * of all new XMLWriters.
     */
    public static void setBaseDirectory (String s) {
        if (s == null) { _base_directory = ""; return; }
        if (s.equals ("") || s.endsWith ("\\") || s.endsWith ("/")) _base_directory = s; 
        else _base_directory = s + "\\";
    }
    public static String getBaseDirectory () { return _base_directory; }
    private static String _base_directory = "";
}