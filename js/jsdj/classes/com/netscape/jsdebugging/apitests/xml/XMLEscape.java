/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.apitests.xml;

import java.io.*;
import java.util.*;
 
/** 
 * Provides functions for escaping characters.
 *
 * @author Alex Rakhlin
 */

public class XMLEscape {

    /**
     * Replaces characters, like "<", ">", etc. with the corresponding "&lt", "&gt", etc.
     */
    public static String escape (String text){
        if (text == null) return null;
        text = _replace ("<", "&lt;", text);
        text = _replace (">", "&gt;", text);
        
        return text;
    }

    /**
     * Undoes escape ()
     */
    public static String unescape (String text){
        if (text == null) return null;
        text = _replace ("&lt;", "<", text);
        text = _replace ("&gt;", ">", text);
        
        return text;
    }
    
    private static String _replace (String oldst, String newst, String target){
        String result = target;
        int j;
        int i = 0;
        while (-1 != (j = result.indexOf (oldst, i))){
            result = result.substring (0, j) + newst + result.substring (j+oldst.length());
            i = j;
        }
        return result;
    }
}