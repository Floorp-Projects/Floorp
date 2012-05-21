/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.*;
import android.net.*;

public class GeckoConnectivityReceiver
    extends BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent) {
        String status;
        ConnectivityManager cm = (ConnectivityManager)
            context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null)
            status = "unknown";
        else if (!info.isConnected())
            status = "down";
        else
            status = "up";

        if (GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning))
            GeckoAppShell.onChangeNetworkLinkStatus(status);
    }
}
