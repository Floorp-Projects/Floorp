/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

public final class IJSThreadState
{
    /**
     * partial list of thread states from sun.debug.ThreadInfo.
     * XXX some of these don't apply.
     */
    public final static int THR_STATUS_UNKNOWN		= 0x01;
    public final static int THR_STATUS_ZOMBIE		= 0x02;
    public final static int THR_STATUS_RUNNING		= 0x03;
    public final static int THR_STATUS_SLEEPING		= 0x04;
    public final static int THR_STATUS_MONWAIT		= 0x05;
    public final static int THR_STATUS_CONDWAIT		= 0x06;
    public final static int THR_STATUS_SUSPENDED	= 0x07;
    public final static int THR_STATUS_BREAK		= 0x08;


    public final static int DEBUG_STATE_DEAD		= 0x01;

    /**
     * if the continueState is RUN, the thread will
     * proceed to the next program counter value when it resumes.
     */
    public final static int DEBUG_STATE_RUN		= 0x02;

    /**
     * if the continueState is RETURN, the thread will
     * return from the current method with the value in getReturnValue()
     * when it resumes.
     */
    public final static int DEBUG_STATE_RETURN		= 0x03;

    /**
     * if the continueState is THROW, the thread will
     * throw an exception (accessible with getException()) when it
     * resumes.
     */
    public final static int DEBUG_STATE_THROW		= 0x04;


    public IJSStackFrameInfo[]  stack;
    public int                  continueState;
    public String               returnValue;
    public int                  status;
    public int                  jsdthreadstate;
    public int                  id;  // used for referencing this object
}    

