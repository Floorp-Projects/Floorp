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

import java.util.*;

/**
 * tags definitions
 *
 * @author Alex Rakhlin
 */

public class Tags {

    public static final String baselineno_tag = "BASE_LINENO";
    public static final String closest_pc_tag = "CLOSEST_PC";
    public static final String count_tag = "COUNT";
    public static final String date_start_tag = "START_DATE";
    public static final String date_finish_tag = "FINISH_DATE";
    public static final String description_tag = "DESCRIPTION";
    public static final String doc_tag = "DOC";
    public static final String engine_tag = "ENGINE";
    public static final String error_tag = "ERROR";
    public static final String eval_tag = "EVAL";
    public static final String eval_string_tag = "EVAL_STRING";
    public static final String file_tag = "FILE";
    public static final String frame_tag = "FRAME";
    public static final String function_tag = "FUNCTION";
    public static final String header_tag = "HEADER";
    public static final String interrupt_tag = "INTERRUPT";
    public static final String is_valid_tag = "IS_VALID";
    public static final String linebuf_tag = "LINE_BUF";
    public static final String lineno_tag = "LINENO";
    public static final String line_tag = "LINE";
    public static final String lines_tag = "LINES";
    public static final String line_extent_tag = "LINE_EXTENT";
    public static final String main_tag = "MAIN";
    public static final String message_tag = "MESSAGE";
    public static final String pc_tag = "PC";
    public static final String result_tag = "RESULT";
    public static final String results_tag = "RESULTS";
    public static final String script_tag = "SCRIPT";
    public static final String script_files_tag = "SCRIPT_FILES";
    public static final String script_loaded_tag = "SCRIPT_LOADED";
    public static final String script_unloaded_tag = "SCRIPT_UNLOADED";
    public static final String section_list_tag = "SECTION_LIST";
    public static final String section_tag = "SECTION";
    public static final String serial_number_tag = "SERIAL_NUMBER";
    public static final String source_location_tag = "SOURCE_LOCATION";
    public static final String stack_tag = "STACK";
    public static final String step_tag = "STEP";
    public static final String test_tag = "TEST";
    public static final String tests_tag = "TESTS";
    public static final String thread_state_tag = "THREAD_STATE";
    public static final String token_offset_tag = "TOKEN_OFFSET";
    public static final String type_tag = "TYPE";
    public static final String url_tag = "URL";
    public static final String version_tag = "VERSION";
    
    /**
     * Init vector of tags. This function must be called before encode() and decode().
     */
    public static void init (){
        _tags = new Vector ();
        _tags.addElement (baselineno_tag);
        _tags.addElement (closest_pc_tag);
        _tags.addElement (count_tag);
        _tags.addElement (date_start_tag);
        _tags.addElement (date_finish_tag);
        _tags.addElement (description_tag);
        _tags.addElement (doc_tag);
        _tags.addElement (engine_tag);
        _tags.addElement (error_tag);
        _tags.addElement (eval_tag);
        _tags.addElement (eval_string_tag);
        _tags.addElement (file_tag);
        _tags.addElement (frame_tag);
        _tags.addElement (function_tag);
        _tags.addElement (header_tag);
        _tags.addElement (interrupt_tag);
        _tags.addElement (is_valid_tag);
        _tags.addElement (linebuf_tag);
        _tags.addElement (lineno_tag);
        _tags.addElement (line_tag);
        _tags.addElement (lines_tag);
        _tags.addElement (line_extent_tag);
        _tags.addElement (main_tag);
        _tags.addElement (message_tag);
        _tags.addElement (pc_tag);
        _tags.addElement (result_tag);
        _tags.addElement (results_tag);
        _tags.addElement (script_tag);
        _tags.addElement (script_files_tag);
        _tags.addElement (script_loaded_tag);
        _tags.addElement (script_unloaded_tag);
        _tags.addElement (section_list_tag);
        _tags.addElement (section_tag);
        _tags.addElement (serial_number_tag);
        _tags.addElement (source_location_tag);
        _tags.addElement (stack_tag);
        _tags.addElement (step_tag);
        _tags.addElement (test_tag);
        _tags.addElement (tests_tag);
        _tags.addElement (thread_state_tag);
        _tags.addElement (token_offset_tag);
        _tags.addElement (type_tag);
        _tags.addElement (url_tag);
        _tags.addElement (version_tag);
    }
    
    /**
     * Get a number given a tag.
     */
    public static int encode (String st){        
        return _tags.indexOf (st);
    }
    
    /**
     * Get a tag given a number.
     */
    public static String decode (int i){
        if (i == -1) return "";
        else return (String) _tags.elementAt (i);
    }
    
    
    private static Vector _tags;
}
