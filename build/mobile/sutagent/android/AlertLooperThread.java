/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.util.Timer;

import android.content.ContextWrapper;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;

class AlertLooperThread extends Thread
    {
    public Handler mHandler;
    private Looper looper = null;
    private DoAlert da    = null;
    private Timer alertTimer = null;
    private ContextWrapper contextWrapper = null;

    AlertLooperThread(ContextWrapper ctxW)
        {
        this.contextWrapper = ctxW;
        }

    public Timer getAlertTimer()
        {
        return alertTimer;
        }

    public void term()
        {
        if (da != null)
            da.term();
        }

    public void quit()
        {
        if (looper != null)
            looper.quit();
        }

    public void run()
        {
        Looper.prepare();

        looper = Looper.myLooper();

        mHandler = new Handler()
            {
            public void handleMessage(Message msg)
                {
                // process incoming messages here
                }
            };

        alertTimer = new Timer();
        da = new DoAlert(contextWrapper);
        alertTimer.scheduleAtFixedRate(da, 0, 5000);
        Looper.loop();
        }
    }
