/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.ServerSocket;
import java.util.Timer;

import com.mozilla.SUTAgentAndroid.R;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.Gravity;
import android.widget.Toast;

public class ASMozStub extends android.app.Service {

    private ServerSocket cmdChnl = null;
    private ServerSocket dataChnl = null;
    private Handler handler = new Handler();
    RunCmdThread runCmdThrd = null;
    RunDataThread runDataThrd = null;
    Thread monitor = null;
    Timer timer = null;

    @SuppressWarnings("unchecked")
    private static final Class<?>[] mSetForegroundSignature = new Class[] {
    boolean.class};
    @SuppressWarnings("unchecked")
    private static final Class<?>[] mStartForegroundSignature = new Class[] {
        int.class, Notification.class};
    @SuppressWarnings("unchecked")
    private static final Class<?>[] mStopForegroundSignature = new Class[] {
        boolean.class};

    private NotificationManager mNM;
    private Method mSetForeground;
    private Method mStartForeground;
    private Method mStopForeground;
    private Object[] mSetForegroundArgs = new Object[1];
    private Object[] mStartForegroundArgs = new Object[2];
    private Object[] mStopForegroundArgs = new Object[1];

    @Override
    public IBinder onBind(Intent intent)
        {
        return null;
        }

    @Override
    public void onCreate() {
        super.onCreate();

        mNM = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        try {
            mStartForeground = getClass().getMethod("startForeground", mStartForegroundSignature);
            mStopForeground = getClass().getMethod("stopForeground", mStopForegroundSignature);
            }
        catch (NoSuchMethodException e) {
            // Running on an older platform.
            mStartForeground = mStopForeground = null;
            }

        try {
            mSetForeground = getClass().getMethod("setForeground", mSetForegroundSignature);
            }
        catch (NoSuchMethodException e) {
            mSetForeground = null;
            }

        doToast("Listener Service created...");
        }

    public void onStart(Intent intent, int startId) {
        super.onStart(intent, startId);

        try {
            cmdChnl = new ServerSocket(20701);
            runCmdThrd = new RunCmdThread(cmdChnl, this, handler);
            runCmdThrd.start();
            doToast("Command channel port 20701 ...");

            dataChnl = new ServerSocket(20700);
            runDataThrd = new RunDataThread(dataChnl, this);
            runDataThrd.start();
            doToast("Data channel port 20700 ...");

            Notification notification = new Notification();
            startForegroundCompat(R.string.foreground_service_started, notification);
            }
        catch (Exception e) {
            doToast(e.toString());
//            Toast.makeText(getApplication().getApplicationContext(), e.toString(), Toast.LENGTH_LONG).show();
            }

        return;
        }

    public void onDestroy()
        {
        super.onDestroy();

        if (runCmdThrd.isAlive())
            {
            runCmdThrd.StopListening();
            }

        if (runDataThrd.isAlive())
            {
            runDataThrd.StopListening();
            }

        NotificationManager notificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(1959);

        stopForegroundCompat(R.string.foreground_service_started);

        doToast("Listener Service destroyed...");

        System.exit(0);
        }

    public void SendToDataChannel(String strToSend)
        {
        if (runDataThrd.isAlive())
            runDataThrd.SendToDataChannel(strToSend);
        }

    public void doToast(String sMsg) {
        Toast toast = Toast.makeText(this, sMsg, Toast.LENGTH_LONG);
        toast.setGravity(Gravity.TOP|Gravity.CENTER_HORIZONTAL, 0, 100);
        toast.show();
    }

    /**
     * This is a wrapper around the new startForeground method, using the older
     * APIs if it is not available.
     */
    void startForegroundCompat(int id, Notification notification) {
        // If we have the new startForeground API, then use it.
        if (mStartForeground != null) {
            mStartForegroundArgs[0] = Integer.valueOf(id);
            mStartForegroundArgs[1] = notification;
            try {
                mStartForeground.invoke(this, mStartForegroundArgs);
            } catch (InvocationTargetException e) {
                // Should not happen.
                Log.w("ScreenOnWidget", "Unable to invoke startForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.w("ScreenOnWidget", "Unable to invoke startForeground", e);
            }
            return;
        }

        // Fall back on the old API.
        if  (mSetForeground != null) {
            try {
                mSetForegroundArgs[0] = Boolean.TRUE;
                mSetForeground.invoke(this, mSetForegroundArgs);
            } catch (IllegalArgumentException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            }
        }
        mNM.notify(id, notification);
    }

    /**
     * This is a wrapper around the new stopForeground method, using the older
     * APIs if it is not available.
     */
    void stopForegroundCompat(int id) {
        // If we have the new stopForeground API, then use it.
        if (mStopForeground != null) {
            mStopForegroundArgs[0] = Boolean.TRUE;
            try {
                mStopForeground.invoke(this, mStopForegroundArgs);
            } catch (InvocationTargetException e) {
                // Should not happen.
                Log.w("ScreenOnWidget", "Unable to invoke stopForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.w("ScreenOnWidget", "Unable to invoke stopForeground", e);
            }
            return;
        }

        // Fall back on the old API.  Note to cancel BEFORE changing the
        // foreground state, since we could be killed at that point.
        mNM.cancel(id);
        if  (mSetForeground != null) {
            try {
                mSetForegroundArgs[0] = Boolean.FALSE;
                mSetForeground.invoke(this, mSetForegroundArgs);
            } catch (IllegalArgumentException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                e.printStackTrace();
            }
        }
    }
}
