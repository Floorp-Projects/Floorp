/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.concurrent.atomic.AtomicBoolean;

import android.app.Activity;

public final class RobocopUtils {
    private static final int MAX_WAIT_MS = 20000;

    private RobocopUtils() {}

    public static void runOnUiThreadSync(Activity activity, final Runnable runnable) {
        final AtomicBoolean sentinel = new AtomicBoolean(false);

        // On the UI thread, run the Runnable, then set sentinel to true and wake this thread.
        activity.runOnUiThread(
            new Runnable() {
                @Override
                public void run() {
                    runnable.run();

                    synchronized (sentinel) {
                        sentinel.set(true);
                        sentinel.notifyAll();
                    }
                }
            }
        );


        // Suspend this thread, until the other thread completes its work or until a timeout is
        // reached.
        long startTimestamp = System.currentTimeMillis();

        synchronized (sentinel) {
            while (!sentinel.get()) {
                try {
                    sentinel.wait(MAX_WAIT_MS);
                } catch (InterruptedException e) {
                    FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR, e);
                }

                // Abort if we woke up due to timeout (instead of spuriously).
                if (System.currentTimeMillis() - startTimestamp >= MAX_WAIT_MS) {
                    FennecNativeDriver.log(FennecNativeDriver.LogLevel.ERROR,
                                                  "time-out waiting for UI thread");
                    FennecNativeDriver.logAllStackTraces(FennecNativeDriver.LogLevel.ERROR);

                    return;
                }
            }
        }
    }
}
