/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.jslogger;

import netscape.application.*;
import netscape.util.*;
import netscape.jsdebug.*;
import netscape.security.PrivilegeManager;
import com.netscape.jsdebugging.ifcui.Env;
import com.netscape.jsdebugging.ifcui.Log;

/**
*  Main entry point and workhorse for JavaScript Logger
*
* @author  John Bandhauer
*/

public class JSLogger
    extends Application
{
    private static final String _logfilename  = "log.txt";
    private static final boolean _interactive = true;

    // boolean options
    public static final int LOGGING_ON                 = 0;
    public static final int LOG_DATE                   = 1;
    public static final int LOG_THREADNAME             = 2;
    public static final int LOG_TO_FILE                = 3;
    public static final int LOG_TO_CONSOLE             = 4;
    public static final int TRACE_SCRIPT_LOADING       = 5;
    public static final int TRACE_EXECUTION            = 6;
    public static final int TRACE_EACH_LINE            = 7;
    public static final int TRACE_FUNCTION_CALLS       = 8;
    public static final int TRACE_DUMP_CALLSTACKS      = 9;
    public static final int BOOLEAN_LIMIT              = 10;

    // String options
    public static final int EVAL_EXPRESSION            = 0;
    public static final int LOG_ONLY_SCRIPTS_FROM      = 1;
    public static final int IGNORE_SCRIPTS_FROM        = 2;
    public static final int LOG_ONLY_IN_FUNCTION       = 3;
    public static final int IGNORE_IN_FUNCTION         = 4;
    public static final int IGNORE_SCRIPT_LOADING_FROM = 5;
    public static final int STRING_LIMIT               = 6;

    /**
    * Entry point for running as an Application
    */
    public static void main(String[] args)
    {
        System.out.println("Launching JSLogger...");
        new Thread(new JSLogger()).start();
    }

    /**
    * override Application.init()
    */
    public void init()
    {
        Env.Init();
//        Log.setEnabled(true);
//        Log.setFilename(_logfilename, true);
        System.out.println("JSLogger output to : "+Log.getFilename());

        // defaults can be changed here...
        setBoolOption(LOGGING_ON            , true);
        setBoolOption(LOG_DATE              , true);
        setBoolOption(LOG_THREADNAME        , false);
        setBoolOption(LOG_TO_FILE           , false);
        setBoolOption(LOG_TO_CONSOLE        , true);
        setBoolOption(TRACE_SCRIPT_LOADING  , true);
        setBoolOption(TRACE_EXECUTION       , true);
        setBoolOption(TRACE_EACH_LINE       , true);
        setBoolOption(TRACE_FUNCTION_CALLS  , true);
        setBoolOption(TRACE_DUMP_CALLSTACKS , false);

        setStringOption(EVAL_EXPRESSION            , null);
        setStringOption(LOG_ONLY_SCRIPTS_FROM      , null);
        setStringOption(IGNORE_SCRIPTS_FROM        , null);
        setStringOption(LOG_ONLY_IN_FUNCTION       , null);
        setStringOption(IGNORE_IN_FUNCTION         , null);
        setStringOption(IGNORE_SCRIPT_LOADING_FROM , null);

        Log.setShowThreadName(getBoolOption(LOG_THREADNAME));
        Log.setLogToFile(getBoolOption(LOG_TO_FILE));
        Log.setLogToConsole(getBoolOption(LOG_TO_CONSOLE));

        Log.setShowDate(true);
        Log.log(null, "========== session start ==========" );
        Log.setShowDate(getBoolOption(LOG_DATE));

        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            _controller = DebugController.getDebugController();
            if( null == _controller || 0 == _controller.getNativeContext() )
            {
                Log.log(null, "FAILED to Init DebugController");
                return;
            }
            _controller.setScriptHook(new MyScriptHook(this));
            _controller.setInterruptHook(new MyInterruptHook(this));
            _controller.setErrorReporter( new MyErrorReporter(this));
            _controller.sendInterrupt();

            if( _interactive )
            {
                _controlPanel = ControlPanel.create(this);
            }
        }
        catch(Throwable t)
        {
            Log.log(null, "exception thrown " + t );
        }
    }

    private void _update()
    {
        Log.setShowThreadName(getBoolOption(LOG_THREADNAME));
        Log.setLogToFile(getBoolOption(LOG_TO_FILE));
        Log.setLogToConsole(getBoolOption(LOG_TO_CONSOLE));
        Log.setShowDate(getBoolOption(LOG_DATE));

        if( null != _controller )
        {
            try
            {
                PrivilegeManager.enablePrivilege("Debugger");
                _controller.sendInterrupt();
            }
            catch(Throwable t)
            {
                Log.log(null, "exception thrown " + t );
            }
        }
    }

    public void setBoolOption(int option, boolean b)
    {
        _boolOptions[option] = b;
        _update();
    }

    public boolean getBoolOption(int option)
    {
        return _boolOptions[option];
    }

    public void setStringOption(int option, String s)
    {
        _stringOptions[option] = s;
        _update();
    }

    public String getStringOption(int option)
    {
        return _stringOptions[option];
    }

    public DebugController getController()    {return _controller;}

    private DebugController _controller;
    private ControlPanel    _controlPanel;
    private boolean[] _boolOptions = new boolean[BOOLEAN_LIMIT];
    private String[]  _stringOptions = new String[STRING_LIMIT];
}

