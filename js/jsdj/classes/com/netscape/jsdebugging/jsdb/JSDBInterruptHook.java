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

/* JavaScript command line Debugger
 * @author Alex Rakhlin
 */

package com.netscape.jsdebugging.jsdb;

import com.netscape.jsdebugging.api.*;
import java.io.*;
import java.util.*;

class JSDBInterruptHook
    extends InterruptHook
{
    public JSDBInterruptHook(DebugController controller, SourceTextProvider stp)
    {
        _stp = stp;
        _frame = null;
        _controller = controller;
        _breakpoints = new JSDBBreakpointsList ((JSDBScriptHook) _controller.getScriptHook(), controller);
        _old_pc = null;
        _stepping = false;
    }

    public void aboutToExecute(ThreadStateBase debug, PC pc)
    {
        if (_stepping && !shouldStop ((JSPC)pc)) {
            _controller.sendInterrupt ();
            return;
        }

        System.out.println ("Stopped at "+pc.getSourceLocation().getLine());

        _resume = false;
        _stepping = false;
        _old_pc = (JSPC) pc;

        JSThreadState state = (JSThreadState) debug;
        try {
            _frame = (JSStackFrameInfo) state.getCurrentFrame();
        }
        catch (InvalidInfoException e){
            //eat it...
        }
        try {
            _length_stack = state.getStack().length;
        }
        catch (InvalidInfoException e){
        }
        _stackpos = _length_stack-1;
        //-------------------- LOOP -----------------
        DataInputStream in = new DataInputStream (System.in);
        String inputLine="";
        while (!_resume){

            System.out.print ("STOPPED >>");
            try {
                inputLine = in.readLine();
            }
            catch (IOException e){
                //eat it...
            }

            processLineSuspended (inputLine, state);
        }
        //--------------------- END -----------------
    }

    public void processLineSuspended (String line, JSThreadState state){
        StringTokenizer st = new StringTokenizer(line);
        if (st.countTokens() == 0) return;
        if (st.countTokens() == 1){
            String first = new String (st.nextToken());
            if (first.equals ("quit") || first.equals ("exit") || first.equals ("resume")) _resume = true;
            else
            if (first.equals ("where")) where(state);
            else
            if (first.equals ("help") || first.equals ("?")) help();
            else
            if (first.equals ("step"))  step();
            else
            if (first.equals ("scripts")) listScripts();
            else
            if (first.equals ("up")) upStack(state);
            else
            if (first.equals ("down")) downStack(state);
            else
            if (first.equals ("bp")) listBreakpoints();
            else
            if (first.equals ("list")){
                try{
                    StackFrameInfo[] stack = state.getStack();
                    JSPC jspc = (JSPC)stack[_stackpos].getPC();
                    printSource (jspc.getScript().getURL(), jspc.getSourceLocation().getLine());
                }
                catch (InvalidInfoException e) {}
            }
            else System.out.println ("Invalid command");
        }
        else
        if (st.countTokens() >= 2){
            String first =new String (st.nextToken());
            String second = new String (st.nextToken());
            if (first.equals ("eval")) 
                 System.out.println(eval (line.substring(5, line.length()), state));
            else
            if (first.equals ("stop") && second.equals ("at")) stopAt (st);
            else
            if (first.equals ("clear") && second.equals ("at")) clearAt (st);
            else
            if (first.equals ("list")) list(st, second);
            else System.out.println ("Invalid command");
        }
        else System.out.println ("Invalid command");

    }

    public void clearAt (StringTokenizer st){
        try {
            String filename = new String (st.nextToken());
            String num = new String (st.nextToken());
            int val = Integer.valueOf (num).intValue ();
            _breakpoints.clearBreakpointsAtLoc (filename, val);
        }
        catch (NoSuchElementException e){
            System.out.println ("Invalid number of parameters");
        }
        catch (NumberFormatException e){
            System.out.println ("not a valid line number");
        }
    }

    public void stopAt (StringTokenizer st){
        try {
            String filename = new String (st.nextToken());
            String num = new String (st.nextToken());
            int val = Integer.valueOf (num).intValue ();
            _breakpoints.createBreakpoint (filename, val);
        }
        catch (NoSuchElementException e){
            System.out.println ("Invalid number of parameters");
        }
        catch (NumberFormatException e){
            System.out.println ("not a valid line number");
        }
    }

    public void list (StringTokenizer st, String filename){
        try {
            String num = new String (st.nextToken());
            int val = Integer.valueOf (num).intValue ();
            printSource (filename, val);
        }
        catch (NoSuchElementException e){
            System.out.println ("Invalid number of parameters");
        }
        catch (NumberFormatException e){
            System.out.println ("not a valid line number");
        }
    }


    public void listScripts (){
        JSDBScriptHook hook = (JSDBScriptHook)_controller.getScriptHook();
        Vector scripts = hook.getScripts();

        int count = scripts.size();
        if (count == 0) {
            System.out.println ("No scripts loaded");
            return;
        }

        Script s;
        for (int i = 0; i < count; i++){
            s = (Script) scripts.elementAt (i);
            String file = s.getURL();
            String fun = s.getFunction ();
            if (null == fun) fun = "_TOP_LEVEL_";
            System.out.println (file+"#"+fun);
        }
    }

    public void listBreakpoints(){
        _breakpoints.listBreakpoints();
    }

    public String eval (String str, JSThreadState state){
        ExecResult result;
        result = _controller.executeScriptInStackFrame (_frame, str, "USER INPUT", 1);
        if(! result.getErrorOccured())
            return result.getResult();
        return "[error] "+result.getErrorMessage();
    }

    public void printSource (String URL, int line){
        _stp.refreshAll();
        SourceTextItem item = _stp.findItem(URL);
        if(null == item) {
            System.out.println("failed to find item for "+URL);
            return;
        }
        String s = item.getText();
        StringTokenizer tokenizer = new StringTokenizer (s, "\n");
        if (line>tokenizer.countTokens()) System.out.println ("Invalid line");
        else {
            String tmp="";
            for (int i=0; i<line; i++) tmp = new String (tokenizer.nextToken());
                System.out.println (tmp);
        }
    }

    public void step (){
        _controller.sendInterrupt();
        _resume = true;
        _stepping = true;
    }

    public void where (JSThreadState state)
    {
        try {
            StackFrameInfo[] stack = state.getStack();
            for(int i = stack.length-1; i >= 0 ; i--)
            {
                JSPC jspc = (JSPC)stack[i].getPC();
                int line = jspc.getSourceLocation().getLine();
                String file = jspc.getScript().getURL();
                String fun = jspc.getScript().getFunction();
                if( null == fun ) fun = "_TOP_LEVEL_";
                if (i == _stackpos) System.out.print ("*");
                else System.out.print (" ");
                System.out.println (file+"#"+fun+"#"+line);
            }
        }
        catch(Exception e) {
            // eat it;
        }
    }


    public void upStack(JSThreadState state){
        if (_stackpos == 0) System.out.println ("This IS the topmost frame.");
        else {
            _stackpos --;
            try {
                _frame = (JSStackFrameInfo)state.getStack()[_stackpos];
            }
            catch (InvalidInfoException e){
            }
        }
    }

    public void downStack(JSThreadState state){
        if (_stackpos == _length_stack-1) System.out.println ("This IS the lowest frame");
        else {
            _stackpos ++;
            try {
                _frame = (JSStackFrameInfo)state.getStack()[_stackpos];
            }
            catch (InvalidInfoException e){
            }
        }
    }

    public boolean shouldStop (JSPC pc){
        if (_old_pc == null) return false;
        if (!pc.getScript().equals(_old_pc.getScript())) return true;
        if (pc.getSourceLocation().getLine() != _old_pc.getSourceLocation().getLine()) return true;
        _old_pc = pc;
        return false;
    }


    public void help(){
        System.out.println ("********************************** HELP **********************************");
        System.out.println ("resume, exit               Resume execution.");
        System.out.println ("stop at <filename> <line>  Set a breakpoint.");
        System.out.println ("clear at <filename> <line> Clear a breakpoint.");
        System.out.println ("bp                         List breakpoints.");
        System.out.println ("list [<filename> <line>]   Print source code. If filename is not specified");
        System.out.println ("                           prints the current line in the current stackframe.");
        System.out.println ("up                         Move up the stack.");
        System.out.println ("down                       Move down the stack.");
        System.out.println ("where                      Dump stack.");
        System.out.println ("step                       Execute current line. [stepping into]");
        System.out.println ("eval <expr>                Evaluate expression in current frame and"); 
        System.out.println ("                           print the result.");
        System.out.println ("**************************************************************************");
    }


    public JSDBBreakpointsList getBreakpoints () { return _breakpoints; }

    private static boolean _resume;
    private static DebugController _controller;
    private static JSDBBreakpointsList _breakpoints;
    private static JSStackFrameInfo _frame;
    private static int _stackpos, _length_stack;
    private static SourceTextProvider _stp;
    private static JSPC _old_pc;
    private static boolean _stepping;
}

