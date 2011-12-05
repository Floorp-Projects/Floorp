/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Android SUTAgent code.
 *
 * The Initial Developer of the Original Code is
 * Bob Moss.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bob Moss <bmoss@mozilla.com>
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

package com.mozilla.SUTAgentAndroid.service;

import java.util.List;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ContextWrapper;

public class FindProcThread extends Thread {
    ContextWrapper    contextWrapper = null;
    String sProcNameToFind = "";
    boolean bFoundIt = false;
    boolean bStillRunning = true;

    public FindProcThread(ContextWrapper ctx, String sProcessName) {
        this.contextWrapper = ctx;
        this.sProcNameToFind = sProcessName;
        this.bFoundIt = false;
    }

    public void run() {
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses;
        int lcv = 0;
        int nNumLoops = 0;
        String strProcName = "";
        int    nPID = 0;

        if (aMgr == null)
            return;


        // While we are still looping looking for the process in the list and we haven't found it
        while (bStillRunning && !bFoundIt) {
            lProcesses = aMgr.getRunningAppProcesses();
            if (lProcesses != null) {
                for (lcv = 0; lcv < lProcesses.size(); lcv++) {
                    if (lProcesses.get(lcv).processName.contains(sProcNameToFind)) {
                        strProcName = lProcesses.get(lcv).processName;
                        nPID = lProcesses.get(lcv).pid;
                        bFoundIt = true;
                        break;
                    }
                }
                if (bFoundIt)             // This saves you half a second of wait time if we've found it in the list
                    continue;
            }
            try {
                Thread.sleep(500);         // Sleep half a second
                if (++nNumLoops > 10) { // loop up to 10 times
                    bStillRunning = false;
                }
                lProcesses = null;
                System.gc();
                Thread.yield();
            } catch (InterruptedException e) {
                e.printStackTrace();
                bStillRunning = false;
            }
        }
    }

}
