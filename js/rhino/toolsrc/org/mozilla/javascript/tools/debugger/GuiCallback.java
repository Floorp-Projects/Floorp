/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov, igor@fastmail.fm
 *   Cameron McCormack
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */
package org.mozilla.javascript.tools.debugger;

/**
 * Interface for communication between the debugger and its GUI.  This
 * should be implemented by the GUI.
 */
public interface GuiCallback {

    /**
     * Called when the source text of some script has been changed.
     */
    void updateSourceText(Dim.SourceInfo sourceInfo);

    /**
     * Called when the interrupt loop has been entered.
     */
    void enterInterrupt(Dim.StackFrame lastFrame,
                        String threadTitle,
                        String alertMessage);

    /**
     * Returns whether the current thread is the GUI's event thread.
     * This information is required to avoid blocking the event thread
     * from the debugger.
     */
    boolean isGuiEventThread();

    /**
     * Processes the next GUI event.  This manual pumping of GUI events
     * is necessary when the GUI event thread itself has been stopped.
     */
    void dispatchNextGuiEvent() throws InterruptedException;
}
