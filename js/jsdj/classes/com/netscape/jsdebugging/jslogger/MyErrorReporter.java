/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.jslogger;

import netscape.jsdebug.*;
import netscape.security.PrivilegeManager;
import com.netscape.jsdebugging.ifcui.Log;

class MyErrorReporter
    implements JSErrorReporter
{
    public MyErrorReporter(JSLogger logger)
    {
        _logger = logger;
    }

    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        if( _logger.getBoolOption(JSLogger.LOGGING_ON) )
        {
            Log.log(null, "!!!!!!!!!!!!!! BEGIN ERROR_REPORT !!!!!!!!!!!!!!");
            Log.log(null, "!!! msg: " +         msg);
            Log.log(null, "!!! file: "+         filename);
            Log.log(null, "!!! lineno: "+       lineno);
            Log.log(null, "!!! linebuf: "+      linebuf);
            Log.log(null, "!!! tokenOffset: "+  tokenOffset);
            Log.log(null, "!!!!!!!!!!!!!! END ERROR_REPORT !!!!!!!!!!!!!!");
        }

        return JSErrorReporter.PASS_ALONG;
    }
    private JSLogger  _logger;
}        

