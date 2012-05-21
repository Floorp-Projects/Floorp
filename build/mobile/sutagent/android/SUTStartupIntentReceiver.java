/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class SUTStartupIntentReceiver extends BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent)
        {
        Intent mySUTAgentIntent = new Intent(context, SUTAgentAndroid.class);
        mySUTAgentIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(mySUTAgentIntent);
        }
}
