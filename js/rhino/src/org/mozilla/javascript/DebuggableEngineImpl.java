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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
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

// API class

package org.mozilla.javascript;

import org.mozilla.javascript.debug.*;

public class DebuggableEngineImpl implements DebuggableEngine {
  
    public DebuggableEngineImpl(Context cx) {
        this.cx = cx;
    }

    /**
     * Set whether the engine should break when it encounters
     * the next line.
     * <p>
     * The engine will call the attached debugger's handleBreakpointHit
     * method on the next line it executes if isLineStep is true.
     * May be used from another thread to interrupt execution.
     * 
     * @param isLineStep if true, break next line
     */
    public void setBreakNextLine(boolean isLineStep) {
        cx.inLineStepMode = isLineStep;
    }
    
    /**
     * Return the value of the breakNextLine flag.
     * @return true if the engine will break on execution of the 
     * next line.
     */
    public boolean getBreakNextLine() {
        return cx.inLineStepMode;
    }

    /**
     * Set the associated debugger.
     * @param debugger the debugger to be used on callbacks from
     * the engine.
     */
    public void setDebugger(Debugger debugger) {
        cx.debugger = debugger;
    }
    
    /**
     * Return the current debugger.
     * @return the debugger, or null if none is attached.
     */
    public Debugger getDebugger() {
        return cx.debugger;
    }
    
    /**
     * Return the number of frames in current execution.
     * @return the count of current frames
     */
    public int getFrameCount() {
        return cx.frameStack == null ? 0 : cx.frameStack.size();
    }
    
    /**
     * Return a frame from the current execution.
     * Frames are numbered starting from 0 for the innermost
     * frame.
     * @param frameNumber the number of the frame in the range
     *        [0,frameCount-1]
     * @return the relevant DebugFrame, or null if frameNumber is out
     *         of range or the engine isn't currently saving 
     *         frames
     */
    public DebugFrame getFrame(int frameNumber) {
        return (DebugFrame) cx.frameStack.elementAt(cx.frameStack.size() - frameNumber - 1);
    }
    
    private Context cx;
}
