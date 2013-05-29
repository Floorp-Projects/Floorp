/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.util.Timer;

import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;
import com.mozilla.SUTAgentAndroid.R;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.Gravity;
import android.widget.Toast;

import javax.jmdns.JmDNS;
import javax.jmdns.ServiceInfo;

public class ASMozStub extends android.app.Service {
    private final static int COMMAND_PORT = 20701;
    private final static int DATA_PORT = 20700;

    private ServerSocket cmdChnl = null;
    private ServerSocket dataChnl = null;
    private Handler handler = new Handler();
    RunCmdThread runCmdThrd = null;
    RunDataThread runDataThrd = null;
    Thread monitor = null;
    Timer timer = null;
    boolean doZeroConfig = false;

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
            // Might be running on an older platform.
            mStartForeground = mStopForeground = null;
            Log.w("SUTAgent", "unable to find start/stopForeground method(s) -- older platform?");
            }

        try {
            mSetForeground = getClass().getMethod("setForeground", mSetForegroundSignature);
            }
        catch (NoSuchMethodException e) {
            mSetForeground = null;
            Log.e("SUTAgent", "unable to find setForeground method!");
            }

        doToast("Listener Service created...");
        }

    WifiManager.MulticastLock multicastLock;
    JmDNS jmdns;

    void startZeroConf() {
        if (multicastLock == null) {
            WifiManager wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
            multicastLock = wifi.createMulticastLock("SUTAgent");
            multicastLock.setReferenceCounted(true);
        }

        multicastLock.acquire();

        try {
            InetAddress inetAddress = SUTAgentAndroid.getLocalInetAddress();

            if (jmdns == null) {
                jmdns = JmDNS.create(inetAddress, null);
            }

            if (jmdns != null) {
                String name = "SUTAgent";

                String hwid = SUTAgentAndroid.getHWID(this);
                if (hwid != null) {
                    name += " [hwid:" + hwid + "]";
                }

                // multicast reception is broken for some reason, so
                // this service can't be resolved; it can only be
                // broadcast.  So, we cheat -- we put the IP address
                // in the broadcast that we can pull out later.
                // However, periods aren't legal, so replace them.
                // The IP address will show up as [ip:127_0_0_1]
                name += " [ip:" + inetAddress.getHostAddress().toString().replace('.', '_') + "]";

                final ServiceInfo serviceInfo = ServiceInfo.create("_sutagent._tcp.local.",
                                                                   name,
                                                                   COMMAND_PORT,
                                                                   "Android SUTAgent");
                final JmDNS dns = jmdns;
                // we want to call registerService on a new thread, because it can block
                // for a little while.
                Thread registerThread = new Thread() {
                        public void run() {
                            try {
                                dns.registerService(serviceInfo);
                            } catch (IOException e) {
                                Log.e("SUTAgent", "Failed to register JmDNS service!", e);
                            }
                        }
                    };
                registerThread.setDaemon(true);
                registerThread.start();
            }
        } catch (IOException e) {
            Log.e("SUTAgent", "Failed to register JmDNS service!", e);
        }
    }

    void stopZeroConf() {
        if (jmdns != null) {
            try {
                jmdns.unregisterAllServices();
                jmdns.close();
            } catch (IOException e) {
                Log.e("SUTAgent", "Failed to close JmDNS service!", e);
            }
            jmdns = null;
        }

        if (multicastLock != null) {
            multicastLock.release();
            multicastLock = null;
        }
    }

    public void onStart(Intent intent, int startId) {
        super.onStart(intent, startId);

        try {
            cmdChnl = new ServerSocket(COMMAND_PORT);
            runCmdThrd = new RunCmdThread(cmdChnl, this, handler);
            runCmdThrd.start();
            doToast(String.format("Command channel port %d ...", COMMAND_PORT));

            dataChnl = new ServerSocket(DATA_PORT);
            runDataThrd = new RunDataThread(dataChnl, this);
            runDataThrd.start();
            doToast(String.format("Data channel port %d ...", DATA_PORT));

            DoCommand tmpdc = new DoCommand(getApplication());
            File dir = getFilesDir();
            File iniFile = new File(dir, "SUTAgent.ini");
            String sIniFile = iniFile.getAbsolutePath();
            String zeroconf = tmpdc.GetIniData("General", "ZeroConfig", sIniFile);
            if (zeroconf != "" && Integer.parseInt(zeroconf) == 1) {
                this.doZeroConfig = true;
            }

            if (this.doZeroConfig) {
                startZeroConf();
            }

            Notification notification = new Notification();
            startForegroundCompat(R.string.foreground_service_started, notification);
            }
        catch (Exception e) {
            doToast(e.toString());
            }

        return;
        }

    public void onDestroy()
        {
        super.onDestroy();

        if (this.doZeroConfig) {
            stopZeroConf();
        }

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
                Log.e("SUTAgent", "Unable to invoke startForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.e("SUTAgent", "Unable to invoke startForeground", e);
            }
            return;
        }

        // Fall back on the old API.
        if  (mSetForeground != null) {
            try {
                mSetForegroundArgs[0] = Boolean.TRUE;
                mSetForeground.invoke(this, mSetForegroundArgs);
            } catch (IllegalArgumentException e) {
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
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
                Log.e("SUTAgent", "Unable to invoke stopForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.e("SUTAgent", "Unable to invoke stopForeground", e);
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
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                Log.e("SUTAgent", "Unable to invoke setForeground", e);
                e.printStackTrace();
            }
        }
    }
}
