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

import java.util.*;
import com.netscape.jsdebugging.api.*;

class JSDBBreakpointsList
{

    public JSDBBreakpointsList (JSDBScriptHook hook, DebugController controller){
        _list = new Vector();
        _scriptHook = hook;
        _controller = controller;
    }

    private void _addBreakpoint (String URL, int line){
        JSDBBreakpoint b = new JSDBBreakpoint (URL, line);
        _addBreakpoint (b);
    }

    private void _removeBreakpoint (String URL, int line){
        JSDBBreakpoint b = new JSDBBreakpoint (URL, line);
        _removeBreakpoint (b);
    }

    private void _addBreakpoint (JSDBBreakpoint b){
        _list.addElement (b);
    }

    private void _removeBreakpoint (JSDBBreakpoint b){
        int count = _list.size();
        for (int i = 0; i<count; i++)
            if (_list.elementAt(i).equals (b)) _list.removeElement (_list.elementAt(i));
    }


    public Script getRealScriptForLoc (String URL, int line){
        Vector scripts = _scriptHook.getScripts();
        int count = scripts.size();
        Script result = null;
        Script curr;
        for (int i = 0; i<count; i++){
            curr = (Script) scripts.elementAt(i);
            // if URL matches
            if (URL.equals (curr.getURL())){
                int base = curr.getBaseLineNumber();
                int extent = curr.getLineExtent();
                // and if bp falls inside the script
                if (line >= base && line <= base+extent) {
                    // if it's a function -- done
                    if (curr.getFunction()!=null) return curr;
                    else result = curr;
                }
            }
        }
        return result;
    }

    public boolean equalScripts (Script s1, Script s2){
        // TODO : use script.equals() ??
        if (s1 == null || s2 == null) return false;
        if (!s1.getURL().equals(s2.getURL())) return false;
        if (s1.getFunction() == null && s2.getFunction() == null) return true;
        if (s1.getFunction() == null || s2.getFunction() == null) return false;
        if (s1.getFunction().equals(s2.getFunction())) return true;
        return false;
    }


    public Vector getBreakpointsForScript (Script script){
        Vector vec = new Vector();
        int count = _list.size();
        for (int i=0; i<count; i++){
            JSDBBreakpoint b = (JSDBBreakpoint) _list.elementAt(i);
            Script belongsTo = getRealScriptForLoc (b.getURL(), b.getLine());
            if (belongsTo == null) continue;
            if (b.isEnabled() && equalScripts(script, belongsTo))
                vec.addElement (b);
        }
        return vec;
    }


    public void setHooksForLoadedScript (Script script) {
        Vector vec = getBreakpointsForScript (script);
        int count = vec.size();
        if (count == 0) return;

        for (int i = 0; i<count; i++){
            JSDBBreakpoint b = (JSDBBreakpoint) vec.elementAt (i);
            hookBreakpoint (b, script);
        }
    }

    public void clearHooksForScript(Script script){
        int count = _list.size();
        if (count == 0) return;

        for (int i = 0; i<count; i++){
            JSDBBreakpoint b = (JSDBBreakpoint) _list.elementAt(i);

            if (script == getRealScriptForLoc (b.getURL(), b.getLine()))
                b.clearHook();
        }
    }

    public void hookBreakpoint (JSDBBreakpoint b, Script script){
        JSPC pc = script.getClosestPC (b.getLine());
        if (pc == null) {
            System.out.println ("Breakpoint location is invalid: "+b);
            return;
        }

        b.update (script.getURL(), pc.getSourceLocation().getLine());
        JSDBInstructionHook h = new JSDBInstructionHook(pc, _controller, b);
        b.setHook (h);
        _controller.setInstructionHook(b.getHook().getPC(), b.getHook());
    }

    public void createBreakpoint (String URL, int line){
        JSDBBreakpoint b = new JSDBBreakpoint (URL, line);
        _addBreakpoint(b);

        Script s = getRealScriptForLoc (URL, line);
        if (s == null) {
            System.out.println ("Script not loaded. Breakpoint will be set once the script is loaded");
            return;
        }

        hookBreakpoint (b, s);
    }

    public void clearBreakpointsAtLoc (String URL, int line){
        Vector vec = new Vector();
        int count = _list.size();
        for (int i=0; i<count; i++){
            JSDBBreakpoint b = (JSDBBreakpoint) _list.elementAt(i);
            if (b.getURL().equals(URL) && b.getLine() == line){
                b.clearHook ();
            }
        }
    }


    public void listBreakpoints(){
        int count = _list.size();
        if (count == 0) {
            System.out.println ("No breakpoints set");
            return;
        }
        for (int i = 0; i<count; i++){
            JSDBBreakpoint b =(JSDBBreakpoint)_list.elementAt (i);
            if (b.isEnabled()) System.out.print ("  ");
            else System.out.print ("- ");
            System.out.println (b);
        }
    }


    private Vector _list;
    private JSDBScriptHook _scriptHook;
    private DebugController _controller;
}