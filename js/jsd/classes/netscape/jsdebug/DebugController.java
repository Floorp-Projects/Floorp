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

package netscape.jsdebug;

import netscape.util.Hashtable;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;

/**
* This is the master control panel for observing events in the VM.
* Each method setXHook() must be passed an object that extends
* the class XHook.  When an event of the specified type
* occurs, a well-known method on XHook will be called (see the
* various XHook classes for details).  The method call takes place
* on the same thread that triggered the event in the first place,
* so that any monitors held by the thread which triggered the hook
* will still be owned in the hook method.
* <p>
* This class is meant to be a singleton and has a private constructor.
* Call the static <code>getDebugController()</code> to get this object.
* <p>
* Note that all functions use netscape.security.PrivilegeManager to verify 
* that the caller has the "Debugger" privilege. The exception 
* netscape.security.ForbiddenTargetException will be throw if this is 
* not enabled.
*
* @author  John Bandhauer
* @author  Nick Thompson
* @version 1.0
* @since   1.0
* @see netscape.security.PrivilegeManager
* @see netscape.security.ForbiddenTargetException
*/
public final class DebugController {

    private static final int majorVersion = 1;
    private static final int minorVersion = 0;

    private static DebugController controller;
    private ScriptHook scriptHook;
    private Hashtable instructionHookTable;
    private InterruptHook interruptHook;
    private DebugBreakHook debugBreakHook;
    private JSErrorReporter errorReporter;

    /**
     * Get the DebugController object for the current VM.
     * <p>
     * @return the singleton DebugController
     */
    public static synchronized DebugController getDebugController() 
        throws ForbiddenTargetException
    {
        try {
            PrivilegeManager.checkPrivilegeEnabled("Debugger");
            if (controller == null)
                controller = new DebugController();
            return controller;
    	} catch (ForbiddenTargetException e) {
            System.out.println("failed in check Priv in DebugController.getDebugController()");
            e.printStackTrace(System.out);
            throw e;
    	}
    }

    private DebugController()
    {
        scriptTable = new Hashtable();
        _setController(true);
    }



    /**
     * Request notification of Script loading events.
     * <p>
     * Whenever a Script is loaded into or unloaded from the VM 
     * the appropriate method of  the ScriptHook argument will be called. 
     * Callers are responsible for chaining hooks if chaining is required.
     *
     * @param h new script hook
     * @return the previous hook object (null if none)
     */
    public synchronized ScriptHook setScriptHook(ScriptHook h)
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        ScriptHook oldHook = scriptHook;
        scriptHook = h;
        return oldHook;
    }

    /**
     * Get the current observer of Script events.
     * <p>
     * @return the current script hook (null if none)
     */
    public ScriptHook getScriptHook()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return scriptHook;
    }

    /**
     * Set a hook at the given program counter value.  
     * <p>
     * When a thread reaches that instruction, a ThreadState 
     * object will be created and the appropriate method 
     * of the hook object will be called. Callers are responsible
     * for chaining hooks if chaining is required.
     * 
     * @param pc pc at which hook should be set
     * @param h new hook for this pc
     * @return the previous hook object (null if none)
     */
    public synchronized InstructionHook setInstructionHook(
        PC pc,
        InstructionHook h) 
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        InstructionHook oldHook;
        if (instructionHookTable == null) {
            instructionHookTable = new Hashtable();
        }
        oldHook = (InstructionHook) instructionHookTable.get(pc);
        instructionHookTable.put(pc, h);
        setInstructionHook0(pc);
        return oldHook;
    }

    private native void setInstructionHook0(PC pc);

    /**
     * Get the hook at the given program counter value.
     * <p>
     * @param pc pc for which hook should be found
     * @return the hook (null if none)
     */
    public InstructionHook getInstructionHook(PC pc)
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return getInstructionHook0(pc);
    }

    // called by native function
    private InstructionHook getInstructionHook0(PC pc)
    {
        if (instructionHookTable == null)
            return null;
        else
            return (InstructionHook) instructionHookTable.get(pc);
    }

    /**************************************************************/

    /**
     * Set the hook at to be called when interrupts occur.
     * <p>
     * The next instruction which starts to execute after 
     * <code>sendInterrupt()</code> has been called will 
     * trigger a call to this hook. A ThreadState 
     * object will be created and the appropriate method 
     * of the hook object will be called. Callers are responsible
     * for chaining hooks if chaining is required.
     * 
     * @param h new hook
     * @return the previous hook object (null if none)
     * @see netscape.jsdebug.DebugController#sendInterrupt
     */
    public synchronized InterruptHook setInterruptHook( InterruptHook h )
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        InterruptHook oldHook = interruptHook;
        interruptHook = h;
        return oldHook;
    }

    /**
     * Get the current hook to be called on interrupt
     * <p>
     * @return the hook (null if none)
     */
    public InterruptHook getInterruptHook()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return interruptHook;
    }

    /**
     * Cause the interrupt hook to be called when the next 
     * JavaScript instruction starts to execute.
     * <p>
     * The interrupt is self clearing
     * @see netscape.jsdebug.DebugController#setInterruptHook
     */
    public void sendInterrupt()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        sendInterrupt0();
    }

    private native void sendInterrupt0();


    /**************************************************************/

    /**
     * Set the hook at to be called when a <i>debug break</i> is requested
     * <p>
     * Set the hook to be called when <i>JSErrorReporter.DEBUG</i> is returned
     * by the <i>error reporter</i> hook. When that happens a ThreadState 
     * object will be created and the appropriate method 
     * of the hook object will be called. Callers are responsible
     * for chaining hooks if chaining is required.
     * 
     * @param h new hook
     * @return the previous hook object (null if none)
     * @see netscape.jsdebug.DebugController#setErrorReporter
     * @see netscape.jsdebug.JSErrorReporter
     */
    public synchronized DebugBreakHook setDebugBreakHook( DebugBreakHook h )
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        DebugBreakHook oldHook = debugBreakHook;
        debugBreakHook = h;
        return oldHook;
    }

    /**
     * Get the current hook to be called on debug break
     * <p>
     * @return the hook (null if none)
     */
    public DebugBreakHook getDebugBreakHook()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return debugBreakHook;
    }


    /**************************************************************/
    /**
     * Get the 'handle' which cooresponds to the native code representing the
     * instance of the underlying JavaScript Debugger context.
     * <p>
     * This would not normally be useful in java. Some of the other classes
     * in this package need this. It remains public mostly for historical 
     * reasons. It serves as a check to see that the native classes have been
     * loaded and the built-in native JavaScript Debugger support has been 
     * initialized. This DebugController is not valid (or useful) when it is
     * in a state where this native context equals 0.
     *
     * @return the native context (0 if none)
     */
    public int getNativeContext() 
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
//        System.out.println( "nativecontext = " + _nativeContext + "\n" );
        return _nativeContext;
    }

    private native void _setController( boolean set );
    private Hashtable scriptTable;
    private int _nativeContext;

    /**
     * Execute a string as a JavaScript script within a stack frame
     * <p>
     * This method can be used to execute arbitrary sets of statements on a 
     * stopped thread. It is useful for inspecting and modifying data.
     * <p>
     * This method can only be called while the JavaScript thread is stopped 
     * - i.e. as part of the code responding to a hook. Thgis method 
     * <b>must</b> be called on the same thread as was executing when the 
     * hook was called.
     * <p>
     * If an error occurs while execuing this code, then the error 
     * reporter hook will be called if present.
     * 
     * @param frame the frame context in which to evaluate this script
     * @param text the script text
     * @param filename where to tell the JavaScript engine this code came 
     * from (it is usually best to make this the same as the filename of
     * code represented by the frame)
     * @param lineno the line number to pass to JS ( >=1 )
     * @return The result of the script execution converted to a string.
     * (null if the result was null or void)
     */
    public String executeScriptInStackFrame( JSStackFrameInfo frame,
                                             String text,
                                             String filename,
                                             int lineno )
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return executeScriptInStackFrame0( frame, text, filename, lineno );
    }

    private native String executeScriptInStackFrame0( JSStackFrameInfo frame,
                                                     String text,
                                                     String filename,
                                                     int lineno );


    /**
     * Set the hook at to be called when a JavaScript error occurs
     * <p>
     * @param er new error reporter hook
     * @return the previous hook object (null if none)
     * @see netscape.jsdebug.JSErrorReporter
     */
    public JSErrorReporter setErrorReporter(JSErrorReporter er)
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        JSErrorReporter old = errorReporter;
        errorReporter = er;
        return old;
    }

    /**
     * Get the hook at to be called when a JavaScript error occurs
     * <p>
     * @return the hook object (null if none)
     * @see netscape.jsdebug.JSErrorReporter
     */
    public JSErrorReporter getErrorReporter()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return errorReporter;
    }

    /**
     * Get the major version number of this module
     * <p>
     * @return the version number
     */
    public static int getMajorVersion() {return majorVersion;}
    /**
     * Get the minor version number of this module
     * <p>
     * @return the version number
     */
    public static int getMinorVersion() {return minorVersion;}

    private static native int getNativeMajorVersion();
    private static native int getNativeMinorVersion();
}
