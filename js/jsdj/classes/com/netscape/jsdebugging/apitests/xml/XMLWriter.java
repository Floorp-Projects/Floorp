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