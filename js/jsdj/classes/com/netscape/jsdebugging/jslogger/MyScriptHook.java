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

class MyScriptHook
    extends ScriptHook
{
    public MyScriptHook(JSLogger logger)
    {
        _logger = logger;
    }

    public void justLoadedScript(Script script)
    {
        try
        {
            if( _shouldLog(script) )
                Log.log(null, "++LOADED SCRIPT:   " + _describe(script) );
        }
        catch(Exception e)
        {
            // eat it;
        }
    }

    public void aboutToUnloadScript(Script script)
    {
        try
        {
            if( _shouldLog(script) )
                Log.log(null, "--UNLOADED SCRIPT: " + _describe(script) );
        }
        catch(Exception e)
        {
            // eat it;
        }
    }

    private boolean _shouldLog(Script script)
    {
        if( ! _enabled(JSLogger.LOGGING_ON) || 
            ! _enabled(JSLogger.TRACE_SCRIPT_LOADING) )
            return false;

        String ignoreFrom = _string(JSLogger.IGNORE_SCRIPT_LOADING_FROM);

        return ignoreFrom == null || ! ignoreFrom.equals(script.getURL());
    }

    private String _describe(Script script)
    {
        String s = script.getURL();
        if( null != script.getFunction() )
            s += "#" + script.getFunction() +"()";
        return s;
    }

    private final String  _string(int option)
    {
        return _logger.getStringOption(option);
    }

    private final boolean _enabled(int option)
    {
        return _logger.getBoolOption(option);
    }

    private JSLogger _logger;
}        


