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

/*
* Static util functions...
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.io.*;
import netscape.application.*;
import netscape.util.*;

public class Util
{
    public static String expandTabs(String aString, int tabstops)
    {
        if( -1 == aString.indexOf('\t') )
            return aString;

        StringBuffer sb = new StringBuffer();
        int i,c;
        char ch;
        int newi;

        for(i=0,newi=0,c=aString.length(); i < c ; i++, newi++ )
        {
            ch = aString.charAt(i);
            if( ch == '\t' )
            {
                int spaces = ((newi+(tabstops-1))%tabstops)+1;
                for( int k = 0; k < spaces; k++ )
                    sb.append(" ");
                newi += spaces-1;
            }
            else
                sb.append(ch);
        }
        return sb.toString();
    }


    /**
    * Searches IFC Archive for root with given classname.
    * returns rootidentifier on success, -1 on failure
    *
    * NOTE: this makes the assupmtion that there will only be one 
    *       rootidentifier for a given classname. I don't think IFC
    *       enforces this. If you want to make that convention and
    *       use this method, then beware.
    *
    */

    public static int RootIdentifierForClassName( Archive a, String n )
    {
        int[] rootIdentifiers = a.rootIdentifiers();
        if( null != rootIdentifiers && 0 != rootIdentifiers.length )
        {
            for( int i = 0; i < rootIdentifiers.length; i++ )
            {
                int id = rootIdentifiers[i];
                if( n.equals(a.classTableForIdentifier(id).className()) )
                    return id;
            }
        }
        return -1;
    }


    public static final String  TEN_ZEROS = "0000000000";
}    
