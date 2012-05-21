/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package com.mozilla.watcher;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
// import android.os.Debug;

public class WatcherReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
//        Debug.waitForDebugger();
        Intent serviceIntent = new Intent();
        serviceIntent.putExtra("command", "start");
        serviceIntent.setAction("com.mozilla.watcher.LISTENER_SERVICE");
        context.startService(serviceIntent);
    }

}
