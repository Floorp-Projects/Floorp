/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.ifcui.Log;
import com.netscape.jsdebugging.ifcui.palomar.util.ER;

/**
 * This is the master control panel for observing events in the VM.
 * Each method setXHook() must be passed an object that extends
 * the interface XHook.  When an event of the specified type
 * occurs, a well-known method on XHook will be called (see the
 * various XHook interfacees for details).  The method call takes place
 * on the same thread that triggered the event in the first place,
 * so that any monitors held by the thread which triggered the hook
 * will still be owned in the hook method.
 */
public class DebugControllerCorba implements DebugController
{
    public long getSupportsFlags()
    {
        return  0;
    }

    public static synchronized DebugControllerCorba getDebugController(String host)
        throws ForbiddenTargetException
    {
        if( null == _controllerCorba )
            _controllerCorba = new DebugControllerCorba(host);
        if( null == _controllerCorba || null == _controllerCorba._remoteController )
        {
            _controllerCorba = null;
            throw new ForbiddenTargetException();
        }
        return _controllerCorba;        
    }

    private DebugControllerCorba(String host)
    {
        _host = host;
        try
        {
            _orb = org.omg.CORBA.ORB.init();
            _boa = _orb.BOA_init();

            PrivilegeManager.enablePrivilege("UniversalConnect");

//            String name = _host+"/NameService/"+wai_name;
//            org.omg.CORBA.Object o = netscape.WAI.Naming.resolve(name);
            org.omg.CORBA.Object o = MyNaming.resolve(_host, wai_name);
            // if(ASS)System.err.println("o = " + o );
            if( null == o )
            {
                // eat it;
                System.err.println("DebugControllerCorba.ctor (null returned by resolve)");
                return;
            }

            _remoteController = com.netscape.jsdebugging.remote.corba.IDebugControllerHelper.narrow(o);
            // if(ASS)System.err.println("_remoteController = " + _remoteController );
            if( null == _remoteController )
            {
                // eat it;
                System.err.println("DebugControllerCorba.ctor (null returned by narrow)");
                return;
            }

        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "ctor");
            _remoteController = null;
            return;
        }

        // Do a real call to the remote controller. If communication
        // is not going to work, then we want to know it now.
        try
        {
            _remoteController.getMajorVersion();
        }
        catch( Exception e )
        {
            // eat it
            System.err.println("/************************************************************");
            System.err.println("* JSD connected to server, but failed when making remote call");
            System.err.println("* Server has probably been stopped or restarted");
            System.err.println("* You should verify server is running and restart Navigator");
            System.err.println("************************************************************/");
            _printException(e, "ctor");
            _remoteController = null;
            return;
        }
    }

    public String getHostName() {return _host;}
    public com.netscape.jsdebugging.remote.corba.IDebugController getRemoteController() 
            {return _remoteController;}
    public org.omg.CORBA.ORB getORB() {return _orb;}
    public org.omg.CORBA.BOA getBOA() {return _boa;}

    /**
     * Request notification of Script loading events.  Whenever a Script
     * is loaded into or unloaded from the VM the appropriate method of 
     * the ScriptHook argument will be called.
     * returns the previous hook object.
     */
    public ScriptHook setScriptHook(ScriptHook h)
    {
        try
        {
            ScriptHookCorba newWrapper = null;
            if( null != h )
            {
                PrivilegeManager.enablePrivilege("UniversalConnect");
                newWrapper = new ScriptHookCorba(this, h);
                _boa.obj_is_ready(newWrapper);
            }
            com.netscape.jsdebugging.remote.corba.IScriptHook oldHook = 
                _remoteController.setScriptHook(newWrapper);
            if( null != oldHook && oldHook instanceof ScriptHookCorba )
                return ((ScriptHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "setScriptHook");
        }
        return null;
    }

    /**
     * Find the current observer of Script events, or return null if there
     * is none.
     */
    public ScriptHook getScriptHook()
    {
        try
        {
            com.netscape.jsdebugging.remote.corba.IScriptHook oldHook = 
                _remoteController.getScriptHook();
            if( null != oldHook && oldHook instanceof ScriptHookCorba )
                return ((ScriptHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getScriptHook");
        }
        return null;
    }

    /**
     * Set a hook at the given program counter value.  When
     * a thread reaches that instruction, a ThreadState object will be
     * created and the appropriate method of the hook object
     * will be called.
     * returns the previous hook object.
     */

    public InstructionHook setInstructionHook(
        PC pc,
        InstructionHook h) 
    {
        try
        {
            com.netscape.jsdebugging.remote.corba.IJSPC newWrappedPC = null;
            if( null != pc )
                newWrappedPC = ((JSPCCorba)pc).getWrappedJSPC();
            
            InstructionHookCorba newWrapper = null;
            if( null != h )
            {
                PrivilegeManager.enablePrivilege("UniversalConnect");
                newWrapper = new InstructionHookCorba(this, h);
                _boa.obj_is_ready(newWrapper);
            }
            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.setInstructionHook(newWrapper, newWrappedPC);
            if( null != oldHook && oldHook instanceof InstructionHookCorba )
                return ((InstructionHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "setInstructionHook");
        }
        return null;
    }

    /**
     * Get the hook at the given program counter value, or return
     * null if there is none.
     */
    public InstructionHook getInstructionHook(PC pc)
    {
        if( null == pc )
            return null;
        try
        {
            com.netscape.jsdebugging.remote.corba.IJSPC newWrappedPC = ((JSPCCorba)pc).getWrappedJSPC();

            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.getInstructionHook(newWrappedPC);
            if( null != oldHook && oldHook instanceof InstructionHookCorba )
                return ((InstructionHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getInstructionHook");
        }
        return null;
    }

    public InterruptHook setInterruptHook( InterruptHook h )
    {
        try
        {
            InterruptHookCorba newWrapper = null;
            if( null != h )
            {
                PrivilegeManager.enablePrivilege("UniversalConnect");
                newWrapper = new InterruptHookCorba(this, h);
                _boa.obj_is_ready(newWrapper);
            }
            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.setInterruptHook(newWrapper);
            if( null != oldHook && oldHook instanceof InterruptHookCorba )
                return ((InterruptHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "setInterruptHook");
        }
        return null;
    }

    public InterruptHook getInterruptHook()
    {
        try
        {
            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.getInterruptHook();
            if( null != oldHook && oldHook instanceof InterruptHookCorba )
                return ((InterruptHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getInterruptHook");
        }
        return null;
    }

    public void sendInterrupt()
    {
        try
        {
            if(ASS)Log.trace("_remoteController.sendInterrupt()", "+++");
            _remoteController.sendInterrupt();
            if(ASS)Log.trace("_remoteController.sendInterrupt()", "---");
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "sendInterrupt");
        }
    }

    public void sendInterruptStepInto(ThreadStateBase debug)
    {
        try
        {
            _remoteController.sendInterruptStepInto(
                        ((JSThreadStateCorba)debug).getWrappedThreadState().id);
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "sendInterruptStepInto");
        }
    }

    public void sendInterruptStepOver(ThreadStateBase debug)
    {
        try
        {
            _remoteController.sendInterruptStepOver(
                        ((JSThreadStateCorba)debug).getWrappedThreadState().id);
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "sendInterruptStepOver");
        }
    }

    public void sendInterruptStepOut(ThreadStateBase debug)
    {
        try
        {
            _remoteController.sendInterruptStepOut(
                        ((JSThreadStateCorba)debug).getWrappedThreadState().id);
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "sendInterruptStepOut");
        }
    }

    public void reinstateStepper(ThreadStateBase debug)
    {
        try
        {
            _remoteController.reinstateStepper(
                        ((JSThreadStateCorba)debug).getWrappedThreadState().id);
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "reinstateStepper");
        }
    }

    public DebugBreakHook setDebugBreakHook( DebugBreakHook h )
    {
        try
        {
            DebugBreakHookCorba newWrapper = null;
            if( null != h )
            {
                PrivilegeManager.enablePrivilege("UniversalConnect");
                newWrapper = new DebugBreakHookCorba(this, h);
                _boa.obj_is_ready(newWrapper);
            }
            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.setDebugBreakHook(newWrapper);
            if( null != oldHook && oldHook instanceof DebugBreakHookCorba )
                return ((DebugBreakHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "setDebugBreakHook");
        }
        return null;
    }

    public DebugBreakHook getDebugBreakHook()
    {
        try
        {
            com.netscape.jsdebugging.remote.corba.IJSExecutionHook oldHook = 
                _remoteController.getDebugBreakHook();
            if( null != oldHook && oldHook instanceof DebugBreakHookCorba )
                return ((DebugBreakHookCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getDebugBreakHook");
        }
        return null;
    }

    public ExecResult executeScriptInStackFrame( JSStackFrameInfo frame,
                                             String text,
                                             String filename,
                                             int lineno )
    {
        JSStackFrameInfoCorba framec = (JSStackFrameInfoCorba) frame;
        JSThreadStateCorba ts = (JSThreadStateCorba) framec.getThreadState();
        int id = ts.getWrappedThreadState().id;

        try
        {
            return new ExecResultCorba(_remoteController.executeScriptInStackFrame( 
                                                id, framec.getWrappedInfo(),
                                                text, filename, lineno));
        }
        catch(Exception e)
        {
            e.printStackTrace();
            System.out.println("exeception while calling remote controler to do exec");
            return null;
        }
    }

    public ExecResult executeScriptInStackFrameValue( JSStackFrameInfo frame,
                                                      String text,
                                                      String filename,
                                                      int lineno )
        throws ForbiddenTargetException
    {
        return null; // XXX implement this...
    }

    public JSErrorReporter setErrorReporter(JSErrorReporter h)
    {
        try
        {
            JSErrorReporterCorba newWrapper = null;
            if( null != h )
            {
                PrivilegeManager.enablePrivilege("UniversalConnect");
                newWrapper = new JSErrorReporterCorba(h);
                _boa.obj_is_ready(newWrapper);
            }
            com.netscape.jsdebugging.remote.corba.IJSErrorReporter oldHook = 
                _remoteController.setErrorReporter(newWrapper);
            if( null != oldHook && oldHook instanceof JSErrorReporterCorba )
                return ((JSErrorReporterCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "setErrorReporter");
        }
        return null;
    }

    public JSErrorReporter getErrorReporter()
    {
        try
        {
            com.netscape.jsdebugging.remote.corba.IJSErrorReporter oldHook = 
                _remoteController.getErrorReporter();
            if( null != oldHook && oldHook instanceof JSErrorReporterCorba )
                return ((JSErrorReporterCorba)oldHook).getWrappedHook();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getErrorReporter");
        }
        return null;
    }

    public void iterateScripts(ScriptHook h)
    {
        try
        {
            PrivilegeManager.enablePrivilege("UniversalConnect");
            ScriptHookCorba newWrapper = new ScriptHookCorba(this, h);
            _boa.obj_is_ready(newWrapper);
            _remoteController.iterateScripts(newWrapper);
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "iterateScripts");
        }
    }

    public int getMajorVersion()
    {
        try
        {
            return _remoteController.getMajorVersion();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getMajorVersion");
        }
        return 0;
    }
    public int getMinorVersion()
    {
        try
        {
            return _remoteController.getMinorVersion();
        }
        catch( Exception e )
        {
            // eat it
            _printException(e, "getMinorVersion");
        }
        return 0;
    }

    private void _printException(Exception e, String funname)
    {
        e.printStackTrace();
        System.out.println("error in DebugControllerCorba." + funname );
    }

//    private int _callsToRemote       = 0;
//    private int _callbacksFromRemote = 0;
//
//    private boolean _callsToRemoteAllowed = true;
//
//    public synchronized void beginCallbackFromRemote()
//    {
//        _callbacksFromRemote++ ;
//    }
//    public synchronized void endCallbackFromRemote()
//    {
//        _callbacksFromRemote-- ;
//    }
//
//    public synchronized void beginCallToRemote()
//    {
//        if(ASS)ER.T(_callsToRemoteAllowed,"beginCallToRemote() when ! _callsToRemoteAllowed",this);
//        _callsToRemote++ ;
//        if(ASS)ER.T(_callsToRemote >= 0,"_callsToRemote >= 0 failed in beginCallToRemote()",this);
//    }
//
//    public synchronized void endCallToRemote()
//    {
//        if(ASS)ER.T(_callsToRemoteAllowed,"endCallToRemote() when ! _callsToRemoteAllowed",this);
//        _callsToRemote-- ;
//        if(ASS)ER.T(_callsToRemote >= 0,"_callsToRemote >= 0 failed in endCallToRemote()",this);
//    }
//
//    public synchronized void setCallsToRemoteAllowed(boolean b)
//    {
//        if(ASS)ER.T(_callsToRemoteAllowed != b,"setNoCallToRemoteAllowed() called with bad arg",this);
//        _callsToRemoteAllowed = b;
//    }


    // data...

    private com.netscape.jsdebugging.remote.corba.IDebugController _remoteController = null;
    private org.omg.CORBA.ORB _orb;
    private org.omg.CORBA.BOA _boa;
    private String _host;

    private static DebugControllerCorba _controllerCorba = null;

    private static final String wai_name = "JSDebugController";
    private static final boolean ASS = true; // enable ASSERT support?
}


