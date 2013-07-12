#filter substitution

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package @ANDROID_PACKAGE_NAME@;

import android.app.Activity;

import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;

public final class RobocopUtils {
    private static final int MAX_WAIT_MS = 20000;

    private RobocopUtils() {}

    public static void runOnUiThreadSync(Activity activity, final Runnable runnable) {
        final SynchronousQueue syncQueue = new SynchronousQueue();
        activity.runOnUiThread(
            new Runnable() {
                public void run() {
                    runnable.run();
                    try {
                        syncQueue.put(new Object());
                    } catch (InterruptedException e) {
                        FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
                    }
                }
            });
        try {
            // Wait for the UiThread code to finish running
            if (syncQueue.poll(MAX_WAIT_MS, TimeUnit.MILLISECONDS) == null) {
                FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                                       "time-out waiting for UI thread");
                FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);
            }
        } catch (InterruptedException e) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
        }
    }
}
