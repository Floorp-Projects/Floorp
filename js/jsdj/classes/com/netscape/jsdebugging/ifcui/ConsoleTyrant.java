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

/*
* 'Model' for Console
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.api.*;

public class ConsoleTyrant
    extends Observable
    implements Observer, Target, JSErrorReporter // , SimulatorPrinter // XXX Sim Hack
{
    public ConsoleTyrant(Emperor emperor)
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);


        // XXX Sim Hack...
//        Simulator.setPrinter(this);
    }


    public void setPrinter(ConsolePrinter printer)
    {
        _printer = printer;
        // XXX need to plant this hook (or our own pass through) into JSD
    }

    /*
    * NOTE: this ErrorReporter may be called on a thread other than
    * the IFC UI thread
    */

    // implement JSErrorReporter interface
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        _errorString = msg;
        return JSErrorReporter.RETURN;
    }

    public void eval(String input)
    {
        if( null == input )
            return;

        String eval = null;
        int evalLine = ++_lineno;

        /*
        * In order to allow the user to enter functions on multiple lines,
        * this hack accumulates line which go to the console printer, but
        * are not sent to JSD until the end is reached.
        * The rule is that when not accumulating if a line ends with '{'
        * then it is assumed to be the end of a function definition line and
        * accumulation begins. If accumulating, then if a line
        * has only one char and it is '}' the accumulation ends
        */

        int len = input.length();
        if( null == _accumulator && len > 0 && '{' == input.charAt(len-1) )
        {
            _accumulator = new String(input + "\n");
            _accumulatorLine = evalLine;
        }
        else if( null != _accumulator )
        {
            if( 1 == len && '}' == input.charAt(0) )
            {
                eval = _accumulator + input + "\n";
                evalLine =_accumulatorLine;
                _accumulator = null;
            }
            else
            {
                _accumulator += input + "\n";
            }
        }
        else
            eval = input + "\n";

        // Sent the input to the printer.
        _sendInputToPrinter(input, null == _accumulator);

        if( null == eval )
            return;

        // currently don't allow eval when not stopped.
        if( ControlTyrant.STOPPED != _controlTyrant.getState() )
            return;

        JSStackFrameInfo frame = (JSStackFrameInfo) _stackTyrant.getCurrentFrame();
        if( null == frame )
            return;

        JSSourceLocation loc = _stackTyrant.getCurrentLocation();
        if( null == loc )
            return;

        String filename;
        if( _emperor.isPre40b6() || Emperor.LOCAL != _emperor.getHostMode() )
            filename = "console";
        else
            filename = loc.getURL();

        _emperor.setWaitCursor(true);

        PrivilegeManager.enablePrivilege("Debugger");

        String result = "";
        _errorString = null;

        JSErrorReporter oldER = null;

        if( ! _emperor.isCorbaHostConnection() )
            oldER = _controlTyrant.setErrorReporter(this);

        DebugController dc = _emperor.getDebugController();
        if( null != dc && null != frame )
        {
            ExecResult fullresult =
                dc.executeScriptInStackFrame(frame,eval,filename,evalLine);
            result = fullresult.getResult();

            if( Emperor.REMOTE_SERVER == _emperor.getHostMode() &&
                fullresult.getErrorOccured() )
                {
                    _errorString = fullresult.getErrorMessage();
                }
        }

        if( ! _emperor.isCorbaHostConnection() )
            _controlTyrant.setErrorReporter(oldER);

        // XXX quick test...
//        _emperor.getInspectorTyrant().getPropNamesOfJavaScriptThing(eval);

        if( null != _printer )
        {
            if( null != _errorString )
                _printer.print("[error "+_errorString+"]\n", true);
            else if( null == result )
                _printer.print("[null]\n", true);
            else
                _printer.print(result + "\n", true);
        }

        _emperor.setWaitCursor(false);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
    }

/*
    // implement SimulatorPrinter interface
    public void print(String stringToPrint)
    {
        if(null != _printer)
            _printer.print(stringToPrint, false);
        else
            System.out.print(stringToPrint);
    }
*/

    private static final boolean SHOW_CONSOLE_LINES = false;

    private void _sendInputToPrinter(String input, boolean full)
    {
        if(null != _printer)
        {
            if( SHOW_CONSOLE_LINES )
            {
                String num = ""+_lineno;
                num = Util.TEN_ZEROS.substring(0,4-num.length()) + num;
                _printer.print(num+(full?": ":"+ ")+input+"\n", false);
            }
            else
            {
//                _printer.print("eval> "+input+"\n", false);
                _printer.print(input+"\n", false);
            }
        }
    }


    private Emperor         _emperor;
    private ControlTyrant   _controlTyrant;
    private StackTyrant     _stackTyrant;
    private int             _lineno;
    private String          _accumulator = null;
    private int             _accumulatorLine;
    private ConsolePrinter  _printer;
    private String          _errorString;
}
