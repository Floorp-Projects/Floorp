/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.util.*;
import java.util.Hashtable;
import java.util.Vector;
import java.io.StringReader;
import java.io.IOException;

public class DebugManager
    implements IDebugManager, DeepBytecodeHook
{
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 1;

    public DebugManager()
    {
        _scriptManager = new ScriptManager(this);
        _instructionHookTable = new Hashtable();
        _evalThreadsVector = new Vector();
    }

    // version?

    // script enum/access/hooks
//    public Enumeration getScripts();
//    public Enumeration getScripts(Context cx);

    // source access
    public SourceTextManager getSourceTextManager() {return _stm;}
    public synchronized SourceTextManager setSourceTextManager(SourceTextManager stm) 
    {
        SourceTextManager result = _stm;
        _stm = stm;
        return  result;
    }

    // context notification
    public void createdContext(Context cx)
    {
        cx.setScriptHook(_scriptManager);
        cx.setCallHook(_scriptManager);
        cx.setExecuteHook(_scriptManager);
        cx.setNewObjectHook(_scriptManager);
        cx.setBytecodeHook(this);
        new ErrorReporterDebug(this, _scriptManager).connect(cx);
    }

    public void abandoningContext(Context cx)
    {
        cx.setScriptHook(null);
        cx.setCallHook(null);
        cx.setExecuteHook(null);
        cx.setNewObjectHook(null);
        cx.setBytecodeHook(null);

        DeepErrorReporterHook h = cx.getErrorReporterHook();
        if(null != h && (h instanceof ErrorReporterDebug))
            ((ErrorReporterDebug)h).disconnect();
    }

    /**********************************************************/

    public IDebugBreakHook setDebugBreakHook(IDebugBreakHook h)
    {
        IDebugBreakHook result = _debugBreakHook;
        _debugBreakHook = h;
        return result;
    }
    public IDebugBreakHook getDebugBreakHook()
    {
        return _debugBreakHook;
    }

    public IErrorReporter setErrorReporter(IErrorReporter er)
    {
        IErrorReporter result = _errorReporter;
        _errorReporter = er;
        return result;
    }
    public IErrorReporter getErrorReporter()
    {
        return _errorReporter;
    }

    /**********************************************************/
    /**********************************************************/
    // stuff to add....
    // script set/clear trap
    // data eval/property iterators
    // watchpoints
    /**********************************************************/
    /**********************************************************/

    /**********************************************************/

    // implements DeepBytecodeHook
    public int trapped(Context cx, int pc, Object[] retVal)
    {
        if(isEvalingOnThisThread())
            return DeepBytecodeHook.CONTINUE;

        if(_interruptSet)
        {
//            System.out.println("---clearing interrupt in trapped");
//            _dumpActiveScripts();
            _interruptSet = false;
            _setOrClearInterruptFlagInActiveScripts(false);
        }

        ThreadState state = new ThreadState(cx, _scriptManager);
        IInstructionHook hook = (IInstructionHook)
                    _instructionHookTable.get(state.getCurrentFrame().getPC());
        if(null != hook)
            hook.aboutToExecute(state);

        return _returnHookValue(state, retVal);
    }

    public int interrupted(Context cx, int pc, Object[] retVal)
    {
        if(isEvalingOnThisThread())
            return DeepBytecodeHook.CONTINUE;

        if(_interruptSet)
        {
//            System.out.println("---clearing interrupt in interrupted");
//            _dumpActiveScripts();
            _interruptSet = false;
            _setOrClearInterruptFlagInActiveScripts(false);
        }

        IInterruptHook hook = _interruptHook;
        if(null == hook)
            return DeepBytecodeHook.CONTINUE;

        ThreadState state = new ThreadState(cx, _scriptManager);
        hook.aboutToExecute(state, state.getCurrentFrame().getPC());

        return _returnHookValue(state, retVal);
    }

    private static int _returnHookValue(ThreadState state, Object[] retVal)
    {
        switch(state.getContinueState())
        {
            default:
            case IThreadState.DEBUG_STATE_RUN:
                return DeepBytecodeHook.CONTINUE;
            case IThreadState.DEBUG_STATE_RETURN:
                // XXX (do conversion here???)
                retVal[0] = state.getReturnValue();
                return DeepBytecodeHook.RETURN_VALUE;
            case IThreadState.DEBUG_STATE_DEAD:
                retVal[0] = null;
                return DeepBytecodeHook.RETURN_VALUE;
            case IThreadState.DEBUG_STATE_THROW:
                retVal[0] = state.getException();
                return DeepBytecodeHook.THROW;
        }
    }

    /**********************************************************/

    public void scriptActivated(Script script)
    {
        if(_interruptSet)
        {
//            System.out.println("       activating "+script);
            NativeFunction fun = script.getWrappedFunction();
            if(fun instanceof INativeFunScript)
                ((INativeFunScript)fun).setInterrupt();
        }
    }

    public void scriptWasCreated(Script script, boolean reactivate)
    {
        if(null != _scriptHook)
            _scriptHook.justLoadedScript(script);
    }

    public void scriptAboutToBeDestroyed(Script script)
    {
        NativeFunction fun = script.getWrappedFunction();
        if(fun instanceof INativeFunScript)
            ((INativeFunScript)fun).clearInterrupt();
        
        if(null != _scriptHook)
            _scriptHook.aboutToUnloadScript(script);
    }

    /**********************************************************/

    public IInterruptHook setInterruptHook(IInterruptHook hook)
    {
        IInterruptHook oldHook = _interruptHook;
        _interruptHook = hook;
        return oldHook;
    }

    public IInterruptHook getInterruptHook()
    {
        return _interruptHook;
    }

    public void sendInterrupt()
    {
//        System.out.println("+++setting interrupt");
//        _dumpActiveScripts();
        _interruptSet = true;
        _setOrClearInterruptFlagInActiveScripts(true);
    }

    private void _setOrClearInterruptFlagInActiveScripts(boolean set)
    {
        ScriptUseTracker tracker = _scriptManager.getScriptUseTracker();
        synchronized(tracker)
        {
            CList list = tracker.getActiveList();
            for(ICListElement e = list.head(); e != list; e = e.getNext())
            {
                NativeFunction fun = ((Script)e).getWrappedFunction();
                if(fun instanceof INativeFunScript)
                {
                    if(set)
                        ((INativeFunScript)fun).setInterrupt();
                    else
                        ((INativeFunScript)fun).clearInterrupt();
                }
            }
        }
    }

    // test code...
//    private void _dumpActiveScripts()
//    {
//        ScriptUseTracker tracker = _scriptManager.getScriptUseTracker();
//        synchronized(tracker)
//        {
//            CList list = tracker.getActiveList();
//            for(ICListElement e = list.head(); e != list; e = e.getNext())
//            {
//                System.out.println("   "+e);
//            }
//        }
//    }


    /**********************************************************/

    public IInstructionHook setInstructionHook(IPC pc, IInstructionHook h)
    {
        synchronized(_instructionHookTable)
        {
            IInstructionHook oldHook = (IInstructionHook)
                        _instructionHookTable.remove(pc);
            if(null == h)
            {
                if(null != oldHook)
                {
                    NativeFunction fun =
                        ((Script)pc.getScript()).getWrappedFunction();
                    if(fun instanceof INativeFunScript)
                        ((INativeFunScript)fun).clearTrap(pc.getPC());
                }
            }
            else
            {
                _instructionHookTable.put(pc, h);
                if(null == oldHook)
                {
                    NativeFunction fun =
                        ((Script)pc.getScript()).getWrappedFunction();
                    if(fun instanceof INativeFunScript)
                        ((INativeFunScript)fun).setTrap(pc.getPC());
                }
            }
            return oldHook;
        }
    }

    public IInstructionHook getInstructionHook(IPC pc)
    {
        synchronized(_instructionHookTable)
        {
            return (IInstructionHook) _instructionHookTable.get(pc);
        }
    }

    /**********************************************************/

    public IScriptHook getScriptHook()
    {
        return _scriptHook;
    }

    public IScriptHook setScriptHook(IScriptHook hook)
    {
        IScriptHook oldHook = _scriptHook;
        _scriptHook = hook;
        return oldHook;
    }

    /**********************************************************/

    public String executeScriptInStackFrame(IStackFrame frame,
                                            String text,
                                            String filename,
                                            int lineno)
    {
        if( null == frame || !frame.isValid() || !(frame instanceof StackFrame))
        {
            if(AS.S)ER.T(false,"bad frame",this);
            return null;
        }

        org.mozilla.javascript.Script script = null;
        Object retObj = null;

        StackFrame framed = (StackFrame) frame;
        Context cx = framed.getThreadState().getContext();
        NativeCall activation = framed.getWrappedFrame(); 
        Thread thisThread = Thread.currentThread();

        // avoid debug tracking while doing this eval
        SourceTextManager oldSTM = cx.getSourceTextManager();
        cx.setSourceTextManager(null);
        _setEvalingOnThisThread(thisThread);
        
        NativeCall savedActivation = ScriptRuntime.getCurrentActivation(cx);
        ScriptRuntime.setCurrentActivation(cx, activation);
        

//        long _baseTime;
//        long _compileElapsedTime = 0;
//        long _runElapsedTime = 0;

        try {
//            _baseTime = System.currentTimeMillis();
            // Compile the reader with opt level of -1 to force interpreter
            // mode.
            int oldOptLevel = cx.getOptimizationLevel();
            cx.setOptimizationLevel(-1);
            script = cx.compileReader(null, new StringReader(text),
                                      filename, lineno, null);
            cx.setOptimizationLevel(oldOptLevel);
//            _compileElapsedTime = System.currentTimeMillis() - _baseTime;
        }
        catch(Throwable t1) {
            // should never happen since we just made the Reader from a String
            // eat it
        }

        if(null != script)
        {
            InterpretedScript is = (InterpretedScript) script;
            Scriptable thisObj = activation.getThisObj();
            try
            {
//                _baseTime = System.currentTimeMillis();
                retObj = is.call(cx, activation, thisObj, null);
//                _runElapsedTime = System.currentTimeMillis() - _baseTime;
            }
            catch(Throwable t2)
            {
                // eat it
            }
                        
            // restore debug tracking 
            _clearEvalingOnThisThread(thisThread);
            cx.setSourceTextManager(oldSTM);
            
            // restore stack state
            ScriptRuntime.setCurrentActivation(cx, savedActivation);
            
        }
//        System.out.println("--- compile time: "+_compileElapsedTime+"\t run time: "+_runElapsedTime);
        if(null != retObj)
            return Context.toString(retObj);
        return null;
    }

    /**********************************************************/

    public int getMajorVersion() {return MAJOR_VERSION;}
    public int getMinorVersion() {return MINOR_VERSION;}

    /**********************************************************/

    public boolean isEvalingOnThisThread()
    {
        return _evalThreadsVector.contains(Thread.currentThread());
    }

    private void _setEvalingOnThisThread(Thread t)
    {
        _evalThreadsVector.addElement(t);
    }

    private  void _setEvalingOnThisThread()
    {
        _setEvalingOnThisThread(Thread.currentThread());
    }

    private void _clearEvalingOnThisThread(Thread t)
    {
        _evalThreadsVector.removeElement(t);
    }

    private  void _clearEvalingOnThisThread()
    {
        _clearEvalingOnThisThread(Thread.currentThread());
    }

    /**********************************************************/
    // temp tests...


//    private void _dumpStack(Context cx, Object fun)
//    {
//        if( null == cx )
//            return;
//        try
//        {
//            String s = "  [CALLEE]-> ";
//            // s += fun +" ";
//            if( fun instanceof NativeFunction )
//                s += _formattedInfo((NativeFunction)fun);
//            else
//                s += "Java method";
//            System.out.println(s);
//
//            Object[] frame = cx.topFrame;
//            while(null != frame)
//            {
//                s = "  [FRAME]-> ";
//                // s += frame +" ";
//
//                Object o = frame[NativeFunction.funObjSlot];
//                    if( o instanceof NativeFunction )
//                    s += _formattedFrameInfo(frame);
//                else
//                    s += "Java method";
//
//                System.out.println(s);
//                frame = (Object[]) frame[NativeFunction.callerSlot];
//            }
//            System.out.println("********************");
//        }
//        catch(Exception e)
//        {
//            System.out.println(e);
//            // eat it...
//        }
//    }
//
//    private String _formattedFrameInfo(Object[] frame)
//    {
//        NativeFunction fun = (NativeFunction)frame[NativeFunction.funObjSlot];
//        String s = _formattedInfo(fun);
//
//        Integer pc = (Integer)frame[NativeFunction.debug_pcSlot];
//        if( null != pc )
//            s += " line " + ((INativeFunScript)fun).pc2lineno(pc.intValue());
//        return s;
//    }
//
//    private static String _formattedInfo(NativeFunction fun)
//    {
//        String s;
//        int debugLevel = NativeDelegate.getDebugLevel(fun);
//        if( debugLevel >= 0 )
//        {
//            s = NativeDelegate.getSourceName(fun);
//            if( null == s )
//                s = "<unknown>";
//        }
//        else
//        {
//            if( fun instanceof NativeJavaMethod )
//                s = "<PlainJava>";
//            else
//                s = "<native>";
//        }
//        s += "::";
//        if( debugLevel >= 0 && NativeDelegate.isFunction(fun) )
//            s += NativeDelegate.getFunctionName(fun);
//        else
//            s += "<top_level>";
//        return s;
//    }

    private ScriptManager _scriptManager;
    private SourceTextManager _stm;
    private IScriptHook _scriptHook;
    private IInterruptHook _interruptHook;
    private boolean _interruptSet;
    private Hashtable _instructionHookTable;
    private Vector _evalThreadsVector;
    private IDebugBreakHook _debugBreakHook;
    private IErrorReporter _errorReporter;
}

class ErrorReporterDebug
    implements DeepErrorReporterHook
{
    ErrorReporterDebug(DebugManager debugManager, ScriptManager scriptManager)
    {
        _debugManager = debugManager;
        _scriptManager = scriptManager;
    }

    void connect(Context cx)
    {
        _cx = cx;
        _errorReporter = _cx.getErrorReporter();
        _cx.setErrorReporterHook(this);
    }

    void disconnect()
    {
        _cx.setErrorReporterHook(null);
        _cx.setErrorReporter(_errorReporter);
        _cx = null;
    }

    // implements DeepErrorReporterHook
    public ErrorReporter setErrorReporter(ErrorReporter reporter)
    {
        ErrorReporter result = _errorReporter;
        _errorReporter = reporter;
        return result;
    }

    public ErrorReporter getErrorReporter()
    {
        return _errorReporter;
    }
    public void warning(String message, String sourceName, int line,
                        String lineSource, int lineOffset)
    {
        // XXX for now we just pass warning through...
        if(null != _errorReporter)
            _errorReporter.warning(message, sourceName, line, 
                                   lineSource, lineOffset);
    }

    public void error(String message, String sourceName, int line,
                      String lineSource, int lineOffset)
    {
        int answer = IErrorReporter.PASS_ALONG;
        IErrorReporter er = _debugManager.getErrorReporter();
        if(null != er)
            answer = er.reportError(message, sourceName, line, 
                                    lineSource, lineOffset);

        switch(answer)
        {
            default:
            case IErrorReporter.PASS_ALONG:
                if(null != _errorReporter)
                    _errorReporter.error(message, sourceName, line,
                                         lineSource, lineOffset);
                break;
            case IErrorReporter.RETURN:
            case IErrorReporter.DEBUG:
        }
        // in the current Rhino implementation the Parser relies on the
        // EvaluatorException being thrown in order to work correctly.
        throw new EvaluatorException(message);
    }

    public EvaluatorException runtimeError(String message, String sourceName, 
                                           int line, String lineSource, 
                                           int lineOffset)
    {
        int answer = IErrorReporter.PASS_ALONG;
        IErrorReporter er = _debugManager.getErrorReporter();
        if(null != er)
            answer = er.reportError(message, sourceName, line,
                                    lineSource, lineOffset);
        switch(answer)
        {
            default:
            case IErrorReporter.PASS_ALONG:
                if(null != _errorReporter)
                    return _errorReporter.runtimeError(message, sourceName, line,
                                                       lineSource, lineOffset);
            case IErrorReporter.RETURN:
                return new EvaluatorException(message);
            case IErrorReporter.DEBUG:
                IDebugBreakHook hook = _debugManager.getDebugBreakHook();
                if(null != hook)
                {
                    ThreadState state = new ThreadState(_cx, _scriptManager);
                    hook.aboutToExecute(state, state.getCurrentFrame().getPC());
                    // ignore the answer...
                }
                return new EvaluatorException(message);
        }
    }

    private Context _cx;
    private DebugManager _debugManager;
    private ScriptManager _scriptManager;
    private ErrorReporter _errorReporter;
}
