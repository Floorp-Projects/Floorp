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

import netscape.jsdebug.*;
import netscape.security.PrivilegeManager;
import com.netscape.jsdebugging.ifcui.Log;

class MyInterruptHook
    extends InterruptHook
    implements JSErrorReporter
{
    public MyInterruptHook(JSLogger logger)
    {
        _logger = logger;
    }

    public void aboutToExecute(ThreadStateBase debug, PC pc)
    {
        try
        {
            if( ! _enabled(JSLogger.LOGGING_ON) )
                return;
            PrivilegeManager.enablePrivilege("Debugger");

            if( ! _enabled(JSLogger.TRACE_EXECUTION) )
                return;

            JSThreadState state = (JSThreadState) debug;
            JSPC jspc  = (JSPC) pc;
            // bootstap on the first pass...
            if( null == _lastJSPC )
                _lastJSPC = jspc;

            String fun = jspc.getScript().getFunction();
            if( null == fun )
                fun = _noFun;

            String file = jspc.getScript().getURL();


            if( _enabled(JSLogger.TRACE_FUNCTION_CALLS) )
            {
                boolean ignoreChain = false;
                CallChain newChain = new CallChain(state);

                // bootstap on the first pass...
                if( null == _lastChain )
                {
                    _lastChain = newChain;
                    _lastFunction = fun;
                    _lastJSPC = jspc;
                }

                switch(_lastChain.compare(newChain))
                {
                default:
                case CallChain.EQUAL:
                    ignoreChain = true;
                    break;
                case CallChain.CALLER:
                    // returned
                    Log.log(null, _pad(state)+"call to "+_lastFunction+
                                  " returned to "+fun+" at line: "+
                                  jspc.getSourceLocation().getLine());
                    break;
                case CallChain.CALLEE:
                    // calling deeper
                    Log.log(null, _pad(state)+_lastFunction+" is calling "+
                                  fun+ " from line: "+
                                  _lastJSPC.getSourceLocation().getLine());
                    _lastChain = newChain;
                    break;
                case CallChain.DISJOINT:
                    Log.log(null, _pad(state)+"control jumped to "+fun);
                    _lastChain = newChain;
                    break;
                }
                if( ! ignoreChain )
                {
                    if( _enabled(JSLogger.TRACE_DUMP_CALLSTACKS) )
                        _dumpStack(state);
                    _lastFunction = fun;
                    _lastChain = newChain;
                }
            }

            if( _enabled(JSLogger.TRACE_EACH_LINE) &&
                (null == _string(JSLogger.IGNORE_SCRIPTS_FROM) ||
                 ! _string(JSLogger.IGNORE_SCRIPTS_FROM).equals(file)) &&
                (null == _string(JSLogger.LOG_ONLY_SCRIPTS_FROM) ||
                 _string(JSLogger.LOG_ONLY_SCRIPTS_FROM).equals(file)) &&
                (null == _string(JSLogger.IGNORE_IN_FUNCTION) ||
                 ! _string(JSLogger.IGNORE_IN_FUNCTION).equals(fun)) &&
                (null == _string(JSLogger.LOG_ONLY_IN_FUNCTION) ||
                 _string(JSLogger.LOG_ONLY_IN_FUNCTION).equals(fun)) )
            {
                if( _lastJSPC.getScript() != jspc.getScript() ||
                    _lastJSPC.getSourceLocation().getLine() !=
                     jspc.getSourceLocation().getLine() )
                {
                    // show info about this line
                    String str = _pad(state)+"->"+file+"#"+fun+"#"+
                                 jspc.getSourceLocation().getLine();

                    // evaluate the expression here if indicated
                    String expression = _string(JSLogger.EVAL_EXPRESSION);
                    if( null != expression )
                    {
                        DebugController dc = _logger.getController();

                        JSStackFrameInfo frame = 
                            (JSStackFrameInfo) state.getCurrentFrame();

                        _evalErrorMsg = null;
                        JSErrorReporter oldER = dc.setErrorReporter(this);
                        String result = 
                            dc.executeScriptInStackFrame(frame, 
                                                         expression, 
                                                         "eval", 1);
                        dc.setErrorReporter(oldER);
                        if( null != _evalErrorMsg )
                            result = _evalErrorMsg;

                        str += " +++ EVAL OF: "+expression+" YIELDS: "+result;
                    }
                    Log.log(null, str);
                }
                
            }
            _lastJSPC = jspc;
            _logger.getController().sendInterrupt();
        }
        catch(Throwable t)
        {
            // eat it...
            Log.log(null, "exception thrown " + t );
        }
    }

    // implements JSErrorReporter
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        _evalErrorMsg = "!!ERROR!! "+msg;
        return JSErrorReporter.RETURN;
    }

    private final String  _string(int option)
    {
        return _logger.getStringOption(option);
    }

    private final boolean _enabled(int option)
    {
        return _logger.getBoolOption(option);
    }

    private final String _pad(JSThreadState state)
    {
        try
        {
            StringBuffer b = new StringBuffer();
            int len = state.getStack().length;
            for(int i = 0; i < len; i++)
                b.append("  ");
            return b.toString();
        }
        catch(Exception e)
        {
            // eat it;
        }
        return null;
    }

    private void _dumpStack(JSThreadState state)
    {
        String pad = _pad(state);
        Log.log(null, pad+"++++BEGIN STACKDUMP:");

        try
        {
            StackFrameInfo[] stack = state.getStack();
            for(int i = stack.length-1; i >= 0 ; i--)
            {
                JSPC jspc = (JSPC)stack[i].getPC();
                int line = jspc.getSourceLocation().getLine();
                String file = jspc.getScript().getURL();
                String fun = jspc.getScript().getFunction();
                if( null == fun )
                    fun = _noFun;
                // Log.log(null, pad+"line: "+line+" fun: "+fun+" file: "+file );
                Log.log(null, pad+file+"#"+fun+"#"+line);
            }
        }
        catch(Exception e)
        {
            // eat it;
        }

        Log.log(null, pad+"----END STACKDUMP");
    }

    private JSLogger  _logger;
    private CallChain _lastChain = null;
    private String    _lastFunction = null;
    private JSPC      _lastJSPC = null;
    private String    _evalErrorMsg;
    private static final String _noFun = "_TOP_LEVEL_";
}

