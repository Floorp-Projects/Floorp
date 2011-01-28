/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

package org.mozilla.gecko;

import android.os.*;
import android.app.*;
import android.app.ActivityManager.*;
import android.util.*;


// Memory Watcher
//
// the onLowMemory method is not called on the foreground
// activity.  Hence we need something that monitors memory
// growth, and broadcasts onLowMemory when we get low on
// resources.  We can also use this to test to see if the
// system is responsive.  For example, if a call to get
// available memory takes too much time, we can assume that
// the system is unstable and we should try our best to reduce
// resources.  Much of this is a hack and should only be used
// on devices that don't kill us during low memory.  Forgive
// me.

public class MemoryWatcher extends Handler
{
    private static final long MEMORY_WATCHER_INTERVAL = 2000;
    private static final long MEMORY_WATCHER_INTERVAL_DELAY_FACTOR = 5;
    private static final long MEMORY_WATCHER_CRITICAL_RESPONSE_THRESHOLD = 200; // in ms

    private Handler mMemoryWatcherHandler;
    private ActivityManager mActivityManager;
    private MemoryInfo mMemoryInfo;
    private boolean mMemoryWatcherKeepGoing;
    private boolean mMemoryWatcherEnabled = false;

    public MemoryWatcher(GeckoApp app) {
        if (android.os.Build.MODEL.equals("Nexus S") == false)
            return;
        mMemoryWatcherEnabled = true;

        mMemoryWatcherKeepGoing = true;
        mMemoryInfo = new MemoryInfo();
        mActivityManager = (ActivityManager) app.getSystemService("activity");
    }


    @Override
    public void handleMessage(Message msg) {
        long startTime = System.currentTimeMillis();
        mActivityManager.getMemoryInfo(mMemoryInfo);
        long took = System.currentTimeMillis() - startTime;

        /*
          Log.w("GeckoApp", String.format("OOM_CHECKER %d %d %b %s\n",
          mMemoryInfo.availMem,
          mMemoryInfo.threshold,
          mMemoryInfo.lowMemory,
          ("took " + took + "ms")));
        */

        // We will adjust the next time this is called if
        // we fire a memory event.
        long nextInterval = MEMORY_WATCHER_INTERVAL;

        // if this call too long, something is very
        // wrong with the device.  fire a critical
        // notification and hope things get better.
        if (took > MEMORY_WATCHER_CRITICAL_RESPONSE_THRESHOLD) {
            GeckoAppShell.onCriticalOOM();
            nextInterval *= MEMORY_WATCHER_INTERVAL_DELAY_FACTOR;
        }
        else if (mMemoryInfo.lowMemory) {
            GeckoAppShell.onLowMemory();
            nextInterval *= MEMORY_WATCHER_INTERVAL_DELAY_FACTOR;
        }

        if (mMemoryWatcherKeepGoing == true)
            this.sendEmptyMessageDelayed(0, nextInterval);
    }

    public void StartMemoryWatcher() {
        if (mMemoryWatcherEnabled == false)
            return;
        mMemoryWatcherKeepGoing = true;
        sendEmptyMessageDelayed(0, MEMORY_WATCHER_INTERVAL);
    }

    public void StopMemoryWatcher() {
        if (mMemoryWatcherEnabled == false)
            return;
        mMemoryWatcherKeepGoing = false;
        removeMessages(0);
    }
}
