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

class JSDBBreakpoint
{
    public JSDBBreakpoint(String url, int line){
        _url = url;
        _line = line;
        _enabled = true;
        _hook = null;
    }
                
    public void update (String url, int line){
        _url = url;
        _line = line;
    }
    
    public void setHook (JSDBInstructionHook h){
        _hook = h;
        _enabled = true;
    }
    
    public void clearHook (){
        System.out.println ("--- clearing hook --- "+_url+" "+_line);
        _enabled = false;
    }
    
    public JSDBInstructionHook getHook () { return _hook; }   
    public String getURL () { return _url; }
    public int getLine() { return _line; }
    public boolean isEnabled () { return _enabled; }
    
    
    public boolean equalsTo (JSDBBreakpoint b){
        if (_url.equals (b.getURL()) && _line == b.getLine()) return true;
        else return false;
    }  
    
    public String toString (){
        return _url + " " + _line;
    }
    
    
    private String _url;
    private int _line;
    private JSDBInstructionHook _hook;
    private boolean _enabled;
}
