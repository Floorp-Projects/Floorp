/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package netscape.jslogger;

import netscape.jsdebug.*;
import netscape.security.PrivilegeManager;
import netscape.jsdebugger.Log;

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

