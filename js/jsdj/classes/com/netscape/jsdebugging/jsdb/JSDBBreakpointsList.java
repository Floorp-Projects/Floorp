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