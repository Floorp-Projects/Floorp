/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.util.*;
import java.util.Hashtable;

public class ScriptManager
    implements DeepScriptHook, DeepCallHook, 
               DeepNewObjectHook, DeepExecuteHook
{
    public ScriptManager(DebugManager debugManager)
    {
        _debugManager = debugManager;
        _nonDebugScripts = new Hashtable();
        _scriptUseTracker = new ScriptUseTracker();
        setFallowGraceCount(DEFAULT_FALLOW_GRACE_COUNT);
        setTimedGCFrequency(DEFAULT_GC_INTERVAL);
    }

    public synchronized void setTimedGCFrequency(int msec)
    {
        if( msec < 0 )
        {
            if(AS.S)ER.T(false,"negative msec passed to setTimedGCFrequency",this);
            return;
        }

        if( 0 == msec )
        {
            if( null != _scriptGCInvoker )
            {
                _scriptGCInvoker.discontinue();
                _scriptGCInvoker = null;
            }
        }
        else
        {
            if( null == _scriptGCInvoker )
            {
                _scriptGCInvoker = new ScriptGCInvoker(this, msec);
                Thread t = new Thread(_scriptGCInvoker);
                t.setDaemon(true);
                t.start();
            }
            else
            {
                _scriptGCInvoker.setInterval(msec);
            }
        }
    }

    public int getFallowGraceCount() {return _fallowGraceCount;}
    public void setFallowGraceCount(int count)
    {
        if( count < 0 )
        {
            if(AS.S)ER.T(false,"negative count passed to setFallowGraceCount",this);
            return;
        }
        _fallowGraceCount = count;
    }

    // Do GC
    public void doGC()
    {
        if( _doingGC || _scriptUseTracker.getFallowCount() <= _fallowGraceCount )
            return;
        _doingGC = true;

        if(SHOW_DEBUG_OUTPUT)
        {
            System.out.println("(((( Starting GC with "+
                               _scriptUseTracker.getActiveCount()+
                               " active, "+
                               _scriptUseTracker.getFallowCount()+
                               " fallow, and "+
                               _untrackedScripts +
                               " untracked scripts");
        }

        while( _scriptUseTracker.getFallowCount() > _fallowGraceCount )
        {
            if(SHOW_DEBUG_OUTPUT)
                System.out.println("About to GC "+_scriptUseTracker.getFallowHead());
            Script script = _revoveAndInvalidateOneFallowScript();
            if(null == script)
                break;
        }

        if(SHOW_DEBUG_OUTPUT)
        {
            System.out.println(")))) Ending GC with "+
                               _scriptUseTracker.getActiveCount()+
                               " active, "+
                               _scriptUseTracker.getFallowCount()+
                               " fallow, and "+
                               _untrackedScripts +
                               " untracked scripts");
        }

        _doingGC = false;
    }

    public Script lockScriptForFunction(NativeFunction fun)
    {
        return _FindOrCreateScript(fun, true);
    }

    public Script lockScript(Script script)
    {
        return _activateScript(script);
    }

    public void releaseScript(Script script, boolean removeHint)
    {
        _deactivateScript(script, removeHint);
    }

    public ScriptUseTracker getScriptUseTracker() {return _scriptUseTracker;}

    /**********************************************************************/

    private void _revoveAndInvalidateScript(Script script)
    {
        if(AS.S)ER.T(0==script.getRefCount(),"inuse script being removed",this);
        _callRemoveScriptDebuggerHook(script);
        _untrackedScripts++ ;
        NativeFunction fun = script.getWrappedFunction();
        if(!(fun instanceof INativeFunScript))
            _nonDebugScripts.remove(fun);
        script.invalidate();
    }

    private Script _revoveAndInvalidateOneFallowScript()
    {
        Script script = null;
        synchronized(_scriptUseTracker)
        {
            script = _scriptUseTracker.getFallowHead();
            if(null != script)
            {
                _scriptUseTracker.removeFallowScript(script);
                _revoveAndInvalidateScript(script);
            }
        }
        return script;
    }

    // hooks...

    // implements DeepScriptHook
    public void loadingScript(Context cx, NativeFunction obj)
    {
        if(_debugManager.isEvalingOnThisThread())
        {
            _untrackedScripts++ ;
            return;
        }

        if(AS.S)ER.T(NativeDelegate.getDebugLevel(obj) >= 0, "non-script passed to loadingScript", this);
        _FindOrCreateScript(obj, false);
    }
    public void unloadingScript(Context cx, NativeFunction obj)
    {
        _untrackedScripts-- ;
        if(SHOW_DEBUG_OUTPUT)
            System.out.println("*** finalized "+obj+" leaving "+_untrackedScripts+" untracked scripts");
    }

    // implemewnts DeepCallHook
    public Object preCall(Context cx, Object callee, Object thisArg, Object[] args)
    {
        if(!(callee instanceof NativeFunction))
            return null;
        if(_debugManager.isEvalingOnThisThread())
            return null;

        NativeFunction fun = (NativeFunction) callee;
        if( NativeDelegate.getDebugLevel(fun) < 0)
            return null;

        return _FindOrCreateScript(fun, true);
    }
    public void postCall(Context cx, Object ref, Object retVal, 
                         JavaScriptException caughtException)
    {
        if(null == ref)
            return;
        _deactivateScript((Script)ref, false);
    }

    // implemewnts DeepExecuteHook
    public Object preExecute(Context cx, NativeFunction fun, Scriptable scope)
    {
        if(_debugManager.isEvalingOnThisThread())
            return null;
        return _FindOrCreateScript(fun, true);
    }
    public void postExecute(Context cx, Object ref, Object retVal, JavaScriptException e)
    {
        if(null == ref)
            return;
        _deactivateScript((Script)ref, true);
    }

    // implemewnts DeepNewObjectHook
    public Object preNewObject(Context cx, Object callee, Object[] args)
    {
        if(!(callee instanceof NativeFunction))
            return null;
        if(_debugManager.isEvalingOnThisThread())
            return null;

        NativeFunction fun = (NativeFunction) callee;
        if( NativeDelegate.getDebugLevel(fun) < 0)
            return null;

        return _FindOrCreateScript(fun, true);
    }
    public void postNewObject(Context cx, Object ref, Scriptable retVal, Exception e)
    {
        if(null == ref)
            return;
        _deactivateScript((Script)ref, false);
    }


    // notifies...

    private void _callNewScriptDebuggerHook(Script script, boolean reactivate)
    {
        if(SHOW_DEBUG_OUTPUT)
            System.out.println("+++loading   "+script);
        _debugManager.scriptWasCreated(script, reactivate);
    }

    private void _callRemoveScriptDebuggerHook(Script script)
    {
        if(SHOW_DEBUG_OUTPUT)
            System.out.println("---unloading   "+script);
        _debugManager.scriptAboutToBeDestroyed(script);
    }


    // script create/destroy
    // call/return

    private Script _FindScript(NativeFunction fun)
    {
        if(fun instanceof INativeFunScript)
            return (Script)((INativeFunScript)fun).get_debug_script();
        return (Script) _nonDebugScripts.get(fun);
    }

    private Script _FindOrCreateScript(NativeFunction fun, boolean activate)
    {
        synchronized(_scriptUseTracker)
        {
            boolean created = false;
            Script script = _FindScript(fun);
            if( null == script)
            {
                script = new Script(fun);
                if(! (fun instanceof INativeFunScript))
                    _nonDebugScripts.put(fun,script);
                _scriptUseTracker.addFallowScript(script);
                created = true;
            }
            if(activate)
                _activateScript(script);
        
            if(created && activate)
            {
                _untrackedScripts-- ;
                if(SHOW_DEBUG_OUTPUT)
                    System.out.println("$$$ - had to reactivate"+script);
            }

            if(created)
                _callNewScriptDebuggerHook(script, activate);
            return script;
        }
    }

    private Script _activateScript(Script script)
    {
        if(AS.S)ER.T(null!=script,"null script handed to _activateScript",this);
        if(AS.S)ER.T(script.isValid(),"invalid script handed to _activateScript",this);

        if(1 == script.incRefCount())
        {
            _scriptUseTracker.moveScriptFromFallowToActive(script);
            _debugManager.scriptActivated(script);
        }
        return script;
    }

    private Script _deactivateScript(Script script, boolean remove)
    {
        if(AS.S)ER.T(null!=script,"null script handed to _deactivateScript",this);
        if(AS.S)ER.T(script.isValid(),"invalid script handed to _deactivateScript",this);

        synchronized(_scriptUseTracker)
        {
            if(0 == script.decRefCount())
            {
                if(remove)
                {
                    _scriptUseTracker.removeActiveScript(script);
                    _revoveAndInvalidateScript(script);
                }
                else
                    _scriptUseTracker.moveScriptFromActiveToFallowTail(script);
            }
        }
        return script;
    }
    
    public static final int DEFAULT_GC_INTERVAL = 10000; // 10 seconds
    public static final int DEFAULT_FALLOW_GRACE_COUNT = 100;
//    public static final int DEFAULT_GC_INTERVAL = 10;
//    public static final int DEFAULT_FALLOW_GRACE_COUNT = 2;

    private static final boolean SHOW_DEBUG_OUTPUT = false;

    private DebugManager _debugManager;
    private Hashtable _nonDebugScripts;
    private ScriptUseTracker _scriptUseTracker;
    private ScriptGCInvoker _scriptGCInvoker;
    private int _fallowGraceCount;
    private int _untrackedScripts;
    private boolean _doingGC;
}

class ScriptGCInvoker
    implements Runnable
{
    ScriptGCInvoker(ScriptManager scriptManager, int interval)
    {
        _scriptManager = scriptManager;
        _interval = interval;
    }

    void setInterval(int interval)
    {
        _interval = interval;
    }
    void discontinue()
    {
        _discontinue = true;
    }

    public void run()
    {
        while(!_discontinue)
        {
            _scriptManager.doGC();
            try
            {
                Thread.sleep(_interval);
            }
            catch(InterruptedException e)
            {
                // eat it...
            }
        }
    }

    private ScriptManager _scriptManager;
    private int _interval;
    private boolean _discontinue;
}
