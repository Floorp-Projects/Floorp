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

/* JavaScript command line Debugger
 * @author Alex Rakhlin
 */

package com.netscape.jsdebugging.jsdb;

import com.netscape.jsdebugging.api.*;

class JSDBErrorReporter
    implements JSErrorReporter
{
    public JSDBErrorReporter()
    {
    }

    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
            System.out.println ("!!!!!!!!!!!!!! BEGIN ERROR_REPORT !!!!!!!!!!!!!!");
            System.out.println ("!!! msg: " +         msg);
            System.out.println ("!!! file: "+         filename);
            System.out.println ("!!! lineno: "+       lineno);
            System.out.println ("!!! linebuf: "+      linebuf);
            System.out.println ("!!! tokenOffset: "+  tokenOffset);
            System.out.println ("!!!!!!!!!!!!!! END ERROR_REPORT !!!!!!!!!!!!!!");
            return 0;
    }
}

