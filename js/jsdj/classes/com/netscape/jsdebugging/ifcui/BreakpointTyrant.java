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
* 'Model' that manages breakpoints
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
import com.netscape.jsdebugging.api.*;
import netscape.security.PrivilegeManager;

// scripts stored in a Vector with newest scripts at the high end

public class BreakpointTyrant
    extends Observable
    implements Observer, PrefsSupport
{
    public BreakpointTyrant(Emperor emperor)
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);

        PrivilegeManager.enablePrivilege("Debugger");
        _dc = _emperor.getDebugController();

        // set script hook
        _scriptHook = new BPTyrantScriptHook(this);
        _scriptHook.setNextHook(_dc.setScriptHook(_scriptHook));

        _breakpoints = new Hashtable();
        _scripts     = new Vector();

        _controlTyrant.addObserver(this);

        if(AS.DEBUG)
        {
            _uiThreadForAssertCheck = Thread.currentThread();
        }

        _enabled = true;
        // process the scripts currently known by the controller
        _dc.iterateScripts(_scriptHook);
    }

    public Breakpoint findBreakpoint( Location loc )
    {
        return (Breakpoint) _breakpoints.get(loc);
    }

    public Breakpoint findBreakpoint( Breakpoint bp )
    {
        return (Breakpoint) _breakpoints.get(bp.getLocation());
    }


    public synchronized Breakpoint addBreakpoint( Location loc )
    {
        Breakpoint bp;

        if( null != (bp = findBreakpoint(loc)) )
        {
            if(AS.S)ER.T(false,"attempted to add existing Breakpoint: "+bp,this);
            return bp;
        }

        loc = _getBestBPLocationForLocation(loc,null);
        bp = new Breakpoint(loc);
        _breakpoints.put(loc,bp);
        _notifyObservers( BreakpointTyrantUpdate.ADD_ONE, bp );
        _setHooksForBP(bp);

        return bp;
    }


    public synchronized void removeBreakpoint( Breakpoint bp )
    {
        if( null == findBreakpoint(bp) )
        {
            if(AS.S)ER.T(false,"attempted to remove non-existant Breakpoint: "+bp,this);
            return;
        }

        _clearHooksForBP(bp);
        _breakpoints.remove(bp.getLocation());
        _notifyObservers( BreakpointTyrantUpdate.REMOVE_ONE, bp );
    }

    public synchronized void removeAllBreakpoints()
    {
        Enumeration e = _breakpoints.elements();

        while( e.hasMoreElements() )
        {
            Breakpoint bp = (Breakpoint) e.nextElement();
            _clearHooksForBP(bp);
        }

        _breakpoints.clear();
        _notifyObservers( BreakpointTyrantUpdate.REMOVE_ALL, null );
    }

    public synchronized void removeAllBreakpointsForURL( String url )
    {

        Vector vec = getBreakpointsForURL(url);
        int count = vec.count();
        if( 0 == count )
            return;

        for( int i = 0; i < count; i++ )
        {
            Breakpoint bp = (Breakpoint) vec.elementAt(i);
            _clearHooksForBP(bp);
            _breakpoints.remove(bp.getLocation());
        }
        _notifyObservers( BreakpointTyrantUpdate.REMOVE_ALL_FOR_URL, new Breakpoint(url,0) );
    }

    public Vector getBreakpointsForURL( String url )
    {
        Vector vret = new Vector();

        Enumeration e = _breakpoints.elements();
        while( e.hasMoreElements() )
        {
            Breakpoint bp = (Breakpoint) e.nextElement();
            if( url.equals(bp.getURL()) )
                vret.addElement(bp);
        }
        return vret;
    }


    public Object[] getAllBreakpoints()
    {
        return _breakpoints.elementsArray();
    }

    public Object[] getAllScripts()
    {
        int count = _scripts.count();
        if( 0 == count )
            return null;
        Object[] a = new Object[count];

        // we reverse the list so that newest scripts are first

        for( int i = count-1; i >= 0; i-- )
            a[count-(i+1)] = _scripts.elementAt(i);
        return a;
    }

    public Vector getScriptsVectorForURL(String url)
    {
        Vector vret = new Vector();

        // we reverse the list so that newest scripts are first

        int count = _scripts.count();
        for( int i = count-1; i >= 0; i-- )
        {
            Script script = (Script) _scripts.elementAt(i);
            if( url.equals(script.getURL()) )
                vret.addElement(script);
        }
        return vret;
    }

    public Object[] getScriptsForURL(String url)
    {
        return getScriptsVectorForURL(url).elementArray();
    }

    public void modifiedBreakpoint( Breakpoint bp )
    {
        _notifyObservers(BreakpointTyrantUpdate.MODIFIED_ONE, bp);
    }

    /*******************************/

    private boolean _isNonShadowedLocationInScript(Location loc, Script script)
    {
        if( ! _isLocationInScript(loc, script) )
            return false;

        if( null != script.getFunction() )
            return true;

        boolean oursSeen = false;
        int count = _scripts.count();
        // we reverse the list so that newest scripts are first
        for( int i = count-1; i >= 0; i-- )
        {
            Script scriptCur = (Script) _scripts.elementAt(i);
            if( script.equals(scriptCur) )
            {
                oursSeen = true;
                continue;
            }
            if( ! oursSeen )
                continue;

            if( _isLocationInScript(loc, scriptCur) )
            {
                if( null != scriptCur.getFunction() )
                    return false;
                return true;
            }
        }
        return true;
    }

    private boolean _isLocationInScript(Location loc, Script script)
    {
        if( ! loc.getURL().equals(script.getURL()) )
            return false;

        int line = loc.getLine();
        ScriptSection[] sections = script.getSections();
        for( int i = 0; i < sections.length; i++ )
        {
            int base   = sections[i].getBaseLineNumber();
            int extent = sections[i].getLineExtent();
            if( line >= base && line < base+extent )
                return true;
        }
        return false;
    }

    private Script _getNewestScriptForLocation( Location loc )
    {
        Script scriptTopLevel = null;
        Script scriptBest     = null;

        int count = _scripts.count();
        for( int i = count-1; i >= 0; i-- )
        {
            Script script = (Script) _scripts.elementAt(i);
            if( _isLocationInScript(loc, script) )
            {
                if( null != script.getFunction() )
                    scriptBest = script;
                else if( null != scriptTopLevel )
                    break;
                else
                    scriptBest = scriptTopLevel = script;
            }
        }
        return scriptBest;
    }

    // 'script' can be null
    private Location _getBestBPLocationForLocation(Location loc, Script script)
    {
        if( null == script )
            script = _getNewestScriptForLocation(loc);

        if( null == script )
            return loc;

        JSPC pc = script.getClosestPC(loc.getLine());
        if( null == pc )
            return loc;

        JSSourceLocation sloc = (JSSourceLocation) pc.getSourceLocation();
        if( null == sloc )
            return loc;

        if( sloc.getLine() == loc.getLine() )
            return loc;

        return new Location( loc.getURL(), sloc.getLine() );
    }

    private synchronized void _setHooksForBP(Breakpoint bp)
    {
        PrivilegeManager.enablePrivilege("Debugger");
        Location loc = bp.getLocation();
        boolean setAny = false;

        int count = _scripts.count();
        for( int i = 0; i < count; i++ )
        {
            Script script = (Script) _scripts.elementAt(i);
            if( ! _isNonShadowedLocationInScript(loc, script) )
                continue;

            JSPC pc = script.getClosestPC(bp.getLine());
//            if(AS.S)ER.T(null==pc,"null returned from script.getClosestPC",this);
            if( null == pc )
                continue;

            // generate a hook (doublelinked to BP)
            BreakpointHook hook = new BreakpointHook(_emperor,pc,_controlTyrant,bp);
            bp.putHook(script, hook);

            // set hook (with chaining)
            hook.setNextHook( _dc.setInstructionHook(hook.getPC(),hook) );
            setAny = true;
        }
        if( setAny )
            _notifyObservers(BreakpointTyrantUpdate.ACTIVATED_ONE, bp);
    }

    private synchronized void _clearHooksForBP(Breakpoint bp)
    {
        boolean clearedAny = false;

        Hashtable hookTable = bp.getHooks();
        Enumeration e = hookTable.elements();
        while( e.hasMoreElements() )
        {
            BreakpointHook hook = (BreakpointHook) e.nextElement();

            // somewhat dubious chaining...
            PrivilegeManager.enablePrivilege("Debugger");
            _dc.setInstructionHook(hook.getPC(), hook.getNextHook());
            hook.setTyrant(null);
            hook.setBreakpoint(null);
            clearedAny = true;
        }
        hookTable.clear();

        if( clearedAny )
            _notifyObservers(BreakpointTyrantUpdate.DEACTIVATED_ONE, bp);
    }

    private Vector _getBreakpointsForScript(Script script)
    {
        Vector vret = new Vector();

        Enumeration e = _breakpoints.elements();
        while( e.hasMoreElements() )
        {
            Breakpoint bp = (Breakpoint) e.nextElement();
            if( _isNonShadowedLocationInScript(bp.getLocation(), script) )
                vret.addElement(bp);
        }
        return vret;
    }

    private synchronized void _setHooksForScript(Script script)
    {
        PrivilegeManager.enablePrivilege("Debugger");
        Vector vec = _getBreakpointsForScript(script);

        int count = vec.count();
        if( 0 == count )
            return;

        for( int i = 0; i < count; i++ )
        {
            Breakpoint bp = (Breakpoint) vec.elementAt(i);

            JSPC pc = script.getClosestPC(bp.getLine());
//            if(AS.S)ER.T(null==pc,"null returned from script.getClosestPC",this);
            if( null == pc )
                continue;

            // generate a hook (doublelinked to BP)
            BreakpointHook hook = new BreakpointHook(_emperor,pc,_controlTyrant,bp);
            bp.putHook(script, hook);
            // set hook (with chaining)
            hook.setNextHook( _dc.setInstructionHook(hook.getPC(),hook) );

        // this is called on native thread, can not do notify!!!
//            _notifyObservers(BreakpointTyrantUpdate.ACTIVATED_ONE, bp);
        }
    }

    private synchronized void _clearHooksForScript(Script script)
    {
        Vector vec = _getBreakpointsForScript(script);

        int count = vec.count();
        if( 0 == count )
            return;

        for( int i = 0; i < count; i++ )
        {
            Breakpoint bp = (Breakpoint) vec.elementAt(i);
            BreakpointHook hook = (BreakpointHook) bp.getHook(script);
            if( null == hook )
                continue;
            // somewhat dubious chaining...
            PrivilegeManager.enablePrivilege("Debugger");
            _dc.setInstructionHook(hook.getPC(), hook.getNextHook());

            hook.setTyrant(null);
            hook.setBreakpoint(null);
            bp.removeHook(script);

        // this is called on native thread, can not do notify!!!
//            _notifyObservers(BreakpointTyrantUpdate.DEACTIVATED_ONE, bp);
        }
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if( o == _controlTyrant )
        {
            int type = ((ControlTyrantUpdate)arg).type;
            if( ControlTyrantUpdate.DEBUGGER_DISABLED == type )
            {
                _enabled = false;
                removeAllBreakpoints();
            }
        }
    }

    // helper
    private void _notifyObservers( int type, Breakpoint bp )
    {
        if(AS.S)ER.T(Thread.currentThread()==_uiThreadForAssertCheck,"_notifyObservers called on thread other than UI thread",this);
        setChanged();
        notifyObservers( new BreakpointTyrantUpdate(type,bp) );
    }

    // these are called by our script hook
    public void justLoadedScript(Script script)
    {
//        if(AS.DEBUG)System.out.println( "loaded script: " + script );
        if( ! _enabled )
            return;
        if( ! script.isValid() )
            return;

        synchronized(this)
        {
            // add to the table of scripts
            _scripts.addElement(script);
            _setHooksForScript(script);
        }
    }
    public void aboutToUnloadScript(Script script)
    {
//        if(AS.DEBUG)System.out.println( "unloaded script: " + script );
        if( ! _enabled )
            return;
        synchronized(this)
        {
            _scripts.removeElement(script);
            _clearHooksForScript(script);
        }
    }

    // implement PrefsSupport interface
    public void prefsWrite(Archiver archiver)    throws CodingException
    {
        BreakpointSaver bs = new BreakpointSaver();
        bs.getFromTyrant(this);
        archiver.archiveRootObject(bs);
    }
    public void prefsRead(Unarchiver unarchiver) throws CodingException
    {
        int id = Util.RootIdentifierForClassName(unarchiver.archive(),
                                      "com.netscape.jsdebugging.ifcui.BreakpointSaver" );
        if( -1 != id )
        {
            BreakpointSaver bs = (BreakpointSaver)
                                    unarchiver.unarchiveIdentifier(id);
            bs.sendToTyrant(this);
        }
    }


    // data...

    private Emperor             _emperor;
    private ControlTyrant       _controlTyrant;
    private DebugController     _dc;

    private Hashtable           _breakpoints;
    private Vector              _scripts;

    private BPTyrantScriptHook  _scriptHook;
    private boolean             _enabled = false;
    private Thread              _uiThreadForAssertCheck = null;
}

// used here only...
class BPTyrantScriptHook
    extends ScriptHook
    implements ChainableHook
{
    public BPTyrantScriptHook(BreakpointTyrant bpTyrant)
    {
        setTyrant(bpTyrant);
    }

    public void justLoadedScript(Script script)
    {
        // for safety we make sure not to throw anything at native caller.
        try {
            if( null != _bpTyrant )
                _bpTyrant.justLoadedScript(script);
            if( null != _nextHook )
                _nextHook.justLoadedScript(script);
        } catch(Throwable t){} // eat it.
    }

    public void aboutToUnloadScript(Script script)
    {
        // for safety we make sure not to throw anything at native caller.
        try {
            if( null != _bpTyrant )
                _bpTyrant.aboutToUnloadScript(script);
            if( null != _nextHook )
                _nextHook.aboutToUnloadScript(script);
        } catch(Throwable t){} // eat it.
    }

    // implement ChainableHook
    public void setTyrant(Object tyrant) {_bpTyrant = (BreakpointTyrant) tyrant;}
    public void setNextHook(Hook nextHook) {_nextHook = (ScriptHook) nextHook;}

    private BreakpointTyrant _bpTyrant;
    private ScriptHook _nextHook;

}
