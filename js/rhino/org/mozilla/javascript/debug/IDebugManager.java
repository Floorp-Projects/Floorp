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

// DEBUG API class

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;

/**
 * This interface represents the center of the high-level debug interface.
 * <p>
 * The default implementation of this interface is 
 * org.mozilla.javascript.debug.DebugManager. When starting up the debug support, 
 * that is the class which should be instantiated. This interface allows 
 * for interaction with the debug manager while allowing for the posibility 
 * that it's functionality might be implemented by some other class.
 *
 * @author John Bandhauer
 */

public interface IDebugManager
{
    /**
    * Notify the debug manager that a new context will be used.
    * <p>
    * This tells the debug manager to enable debugging in the given context. 
    * The debug manager will make its low-level connections to this new 
    * context.
    * 
    * @param cx the new Context
    * @see org.mozilla.javascript.debug.IDebugManager#abandoningContext
    * @see org.mozilla.javascript.Context
    */
    public void createdContext(Context cx);

    /**
    * Notify the debug manager that a context will no longer be used.
    * <p>
    * This tells the debug manager to disable debugging in the given context. 
    * The debug manager will remove its low-level connections to this context.
    * 
    * @param cx the Context from which debug hooks will be removed
    * @see org.mozilla.javascript.debug.IDebugManager#createdContext
    * @see org.mozilla.javascript.Context
    */
    public void abandoningContext(Context cx);

    /**
    * Get the SourceTextManager associated with this debug manager.
    * <p>
    * This is simply a convenience method to allow keeping track of 
    * both a debug manager and a source text manager by having a refernece
    * to only the debug manager.
    * 
    * @return current source text manager
    * @see org.mozilla.javascript.debug.IDebugManager#setSourceTextManager
    * @see org.mozilla.javascript.SourceTextManager
    */
    public SourceTextManager getSourceTextManager();

    /**
    * Set the SourceTextManager associated with this debug manager.
    * <p>
    * This is simply a convenience method to allow keeping track of 
    * both a debug manager and a source text manager by having a refernece
    * to only the debug manager.
    * 
    * @param stm the source text manager to associate with this debug manager
    * @return previous source text manager
    * @see org.mozilla.javascript.debug.IDebugManager#getSourceTextManager
    * @see org.mozilla.javascript.SourceTextManager
    */
    public SourceTextManager setSourceTextManager(SourceTextManager stm);

    /**
    * Get the script hook for this debug manager.
    * 
    * @return currrent script hook
    * @see org.mozilla.javascript.debug.IDebugManager#setScriptHook
    * @see org.mozilla.javascript.debug.IScriptHook
    */
    public IScriptHook getScriptHook();

    /**
    * Set the script hook for this debug manager.
    * <p>
    * The script hook is called when scripts (and functions) are loaded 
    * and unloaded.
    * 
    * @param hook the hook to set
    * @return previous script hook
    * @see org.mozilla.javascript.debug.IDebugManager#getScriptHook
    * @see org.mozilla.javascript.debug.IScriptHook
    */
    public IScriptHook setScriptHook(IScriptHook hook);

    /**
    * Get the interrupt hook for this debug manager.
    * 
    * @return currrent hook
    * @see org.mozilla.javascript.debug.IDebugManager#setInterruptHook
    * @see org.mozilla.javascript.debug.IDebugManager#sendInterrupt
    * @see org.mozilla.javascript.debug.IInterruptHook
    */
    public IInterruptHook getInterruptHook();

    /**
    * Set the interrupt hook for this debug manager.
    * <p>
    * The interrupt hook is called some time after a call to 
    * <i>sendInterrupt</i>. The interrupt hook can be set and left in 
    * place. <i>sendInterrupt</i> is a one shot.
    * 
    * @param hook the hook to set
    * @return previous hook
    * @see org.mozilla.javascript.debug.IDebugManager#getInterruptHook
    * @see org.mozilla.javascript.debug.IDebugManager#sendInterrupt
    * @see org.mozilla.javascript.debug.IInterruptHook
    */
    public IInterruptHook setInterruptHook(IInterruptHook hook);

    /**
    * Request interrupt.
    * <p>
    * Sets a low level hook in all contexts to interrupt execution of 
    * the executing JavaScript. The code in the first context to hit 
    * that hook will call the interrupt hook as set in 
    * <i>setInterruptHook</i>. This is a one shot; i.e. the low-level hook 
    * is automatically removed and the high level hook is called only once. 
    * It is possible for the interrupt hook to be called before this
    * <i>sendInterrupt</i> call returns. If no JavaScript code is 
    * currently executing, then the hook might not be called for some time.
    * 
    * @see org.mozilla.javascript.debug.IDebugManager#setInterruptHook
    * @see org.mozilla.javascript.debug.IInterruptHook
    */
    public void sendInterrupt();

    /**
    * Get the instruction hook for given program counter location.
    * 
    * @param pc program counter location for hook
    * @return currrent hook
    * @see org.mozilla.javascript.debug.IDebugManager#setInstructionHook
    * @see org.mozilla.javascript.debug.IInstructionHook
    */
    public IInstructionHook getInstructionHook(IPC pc);

    /**
    * Set the instruction hook for a given program counter location.
    * <p>
    * Sets a trap to be called when JavaScript code in any context is about
    * to execute the code at the given program counter location.
    * 
    * @param pc program counter location for hook
    * @param hook the hook to set
    * @return previous hook
    * @see org.mozilla.javascript.debug.IDebugManager#getInstructionHook
    * @see org.mozilla.javascript.debug.IInstructionHook
    */
    public IInstructionHook setInstructionHook(IPC pc, IInstructionHook hook);

    /**
    * Get the debug break hook for this debug manager.
    * 
    * @return currrent hook
    * @see org.mozilla.javascript.debug.IDebugManager#setDebugBreakHook
    * @see org.mozilla.javascript.debug.IDebugBreakHook
    */
    public IDebugBreakHook getDebugBreakHook();

    /**
    * Set the debug break hook for this debug manager.
    * <p>
    * The debug break hook is called when the error reporter requests
    * that execution be stopped and thrown into the debugger.
    * 
    * @param hook the hook to set
    * @return previous hook
    * @see org.mozilla.javascript.debug.IDebugManager#getDebugBreakHook
    * @see org.mozilla.javascript.debug.IDebugBreakHook
    * @see org.mozilla.javascript.debug.IErrorReporter
    */
    public IDebugBreakHook setDebugBreakHook(IDebugBreakHook hook);

    /**
    * Evaluate a script in the current stack frame.
    * <p>
    * Evaluates the text passed in within the context of the given stack frame. 
    * That frame must be valid; i.e. the thread must be stopped on a hook. This  
    * method must be called on the same thread on which the execution was 
    * stopped. If this evaluation causes an error, then the regular error 
    * reporting mechanism is called. Often a debugger would temporarily insert
    * a special error reporter to catch errors during this evaluation to 
    * distinguish then from errors in the 'real' target JavaScript code.
    * 
    * @param frame the stack frame in which to do the evaluation
    * @param text the textual source of the code to be evaluated
    * @param filename filename to pass to the compiler (for error reporting)
    * @param lineno base line number to pass to the compiler (for error reporting)
    * @return execution return value converted to a String
    * @see org.mozilla.javascript.debug.IStackFrame
    * @see org.mozilla.javascript.debug.IErrorReporter
    */
    public String executeScriptInStackFrame(IStackFrame frame,
                                            String text,
                                            String filename,
                                            int lineno);

    /**
    * Get the error reporter hook for this debug manager.
    * 
    * @return currrent hook
    * @see org.mozilla.javascript.debug.IDebugManager#setErrorReporter
    * @see org.mozilla.javascript.debug.IErrorReporter
    */
    public IErrorReporter getErrorReporter();

    /**
    * Set the error reporter hook for this debug manager.
    * 
    * @param er new error reporter
    * @return previous hook
    * @see org.mozilla.javascript.debug.IDebugManager#getErrorReporter
    * @see org.mozilla.javascript.debug.IErrorReporter
    */
    public IErrorReporter setErrorReporter(IErrorReporter er);

    /**
    * Get Major version number for this debug manager
    * 
    * @return major version number
    */
    public int getMajorVersion();

    /**
    * Get Minor version number for this debug manager
    * 
    * @return minor version number
    */
    public int getMinorVersion();
}    
