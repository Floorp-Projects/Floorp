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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.nio.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.graphics.*;
import android.widget.*;
import android.hardware.*;
import android.location.*;

import android.util.*;
import android.content.DialogInterface; 
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

class GeckoAppShell
{
    static {
        sGeckoRunning = false;
    }

    // static members only
    private GeckoAppShell() { }

    static boolean sGeckoRunning;

    static private boolean gRestartScheduled = false;

    static private final Timer mIMETimer = new Timer();

    static private final int NOTIFY_IME_RESETINPUTSTATE = 0;
    static private final int NOTIFY_IME_SETOPENSTATE = 1;
    static private final int NOTIFY_IME_SETENABLED = 2;
    static private final int NOTIFY_IME_CANCELCOMPOSITION = 3;
    static private final int NOTIFY_IME_FOCUSCHANGE = 4;

    /* The Android-side API: API methods that Android calls */

    // Initialization methods
    public static native void nativeInit();
    public static native void nativeRun(String args);

    // helper methods
    public static native void setInitialSize(int width, int height);
    public static native void setSurfaceView(GeckoSurfaceView sv);
    public static native void putenv(String map);
    public static native void onResume();
    public static native void onLowMemory();
    public static native void callObserver(String observerKey, String topic, String data);
    public static native void removeObserver(String observerKey);

    // java-side stuff
    public static void loadGeckoLibs() {
        // The package data lib directory isn't placed in ld.so's
        // search path, so we have to manually load libraries that
        // libxul will depend on.  Not ideal.

        // MozAlloc
        System.loadLibrary("mozalloc");
        System.loadLibrary("mozutils");
                
        Intent i = GeckoApp.mAppContext.getIntent();
        String env = i.getStringExtra("env0");
        Log.i("GeckoApp", "env0: "+ env);
        for (int c = 1; env != null; c++) {
            GeckoAppShell.putenv(env);
            env = i.getStringExtra("env" + c);
            Log.i("GeckoApp", "env"+ c +": "+ env);
        }

        File f = new File("/data/data/org.mozilla." + 
                          GeckoApp.mAppContext.getAppName() +"/tmp");
        if (!f.exists())
            f.mkdirs();
        GeckoAppShell.putenv("TMPDIR=" + f.getPath());

        f = Environment.getDownloadCacheDirectory();
        GeckoAppShell.putenv("EXTERNAL_STORAGE" + f.getPath());

        // NSPR
        System.loadLibrary("nspr4");
        System.loadLibrary("plc4");
        System.loadLibrary("plds4");

        // SQLite
        System.loadLibrary("mozsqlite3");

        // NSS
        System.loadLibrary("nssutil3");
        System.loadLibrary("nss3");
        System.loadLibrary("ssl3");
        System.loadLibrary("smime3");

        // XUL
        System.loadLibrary("xul");

        // xpcom glue -- needed to load binary components
        System.loadLibrary("xpcom");                                          

        // Root certs. someday we may teach security/manager/ssl/src/nsNSSComponent.cpp to find ckbi itself
        System.loadLibrary("nssckbi");
        System.loadLibrary("freebl3");
        System.loadLibrary("softokn3");
    }

    public static void runGecko(String apkPath, String args, String url) {
        // run gecko -- it will spawn its own thread
        GeckoAppShell.nativeInit();

        // Tell Gecko where the target surface view is for rendering
        GeckoAppShell.setSurfaceView(GeckoApp.surfaceView);

        sGeckoRunning = true;

        // First argument is the .apk path
        String combinedArgs = apkPath + " -omnijar " + apkPath;
        if (args != null)
            combinedArgs += " " + args;
        if (url != null)
            combinedArgs += " " + url;
        // and go
        GeckoAppShell.nativeRun(combinedArgs);
    }

    private static GeckoEvent mLastDrawEvent;

    public static void sendEventToGecko(GeckoEvent e) {
        if (sGeckoRunning)
            notifyGeckoOfEvent(e);
    }

    // Tell the Gecko event loop that an event is available.
    public static native void notifyGeckoOfEvent(GeckoEvent event);

    /*
     *  The Gecko-side API: API methods that Gecko calls
     */
    public static void scheduleRedraw() {
        // Redraw everything
        scheduleRedraw(0, -1, -1, -1, -1);
    }

    public static void scheduleRedraw(int nativeWindow, int x, int y, int w, int h) {
        GeckoEvent e;

        if (x == -1) {
            e = new GeckoEvent(GeckoEvent.DRAW, null);
        } else {
            e = new GeckoEvent(GeckoEvent.DRAW, new Rect(x, y, w, h));
        }

        e.mNativeWindow = nativeWindow;

        sendEventToGecko(e);
    }

    /* Delay updating IME states (see bug 573800) */
    private static final class IMEStateUpdater extends TimerTask
    {
        static private IMEStateUpdater instance;
        private boolean mEnable, mReset;

        static private IMEStateUpdater getInstance() {
            if (instance == null) {
                instance = new IMEStateUpdater();
                mIMETimer.schedule(instance, 200);
            }
            return instance;
        }

        static public synchronized void enableIME() {
            getInstance().mEnable = true;
        }

        static public synchronized void resetIME() {
            getInstance().mReset = true;
        }

        public void run() {
            synchronized(IMEStateUpdater.class) {
                instance = null;
            }

            InputMethodManager imm = (InputMethodManager) 
                GeckoApp.surfaceView.getContext().getSystemService(
                    Context.INPUT_METHOD_SERVICE);
            if (imm == null)
                return;

            if (mReset)
                imm.restartInput(GeckoApp.surfaceView);

            if (!mEnable)
                return;

            if (GeckoApp.surfaceView.mIMEState !=
                    GeckoSurfaceView.IME_STATE_DISABLED)
                imm.showSoftInput(GeckoApp.surfaceView, 0);
            else
                imm.hideSoftInputFromWindow(
                    GeckoApp.surfaceView.getWindowToken(), 0);
        }
    }

    public static void notifyIME(int type, int state) {
        if (GeckoApp.surfaceView == null)
            return;

        switch (type) {
        case NOTIFY_IME_RESETINPUTSTATE:
            IMEStateUpdater.resetIME();
            // keep current enabled state
            IMEStateUpdater.enableIME();
            break;

        case NOTIFY_IME_SETENABLED:
            /* When IME is 'disabled', IME processing is disabled.
                In addition, the IME UI is hidden */
            GeckoApp.surfaceView.mIMEState = state;
            IMEStateUpdater.enableIME();
            break;

        case NOTIFY_IME_CANCELCOMPOSITION:
            IMEStateUpdater.resetIME();
            break;

        case NOTIFY_IME_FOCUSCHANGE:
            GeckoApp.surfaceView.mIMEFocus = state != 0;
            IMEStateUpdater.resetIME();
            break;

        }
    }

    public static void notifyIMEChange(String text, int start, int end, int newEnd) {
        if (GeckoApp.surfaceView == null ||
            GeckoApp.surfaceView.inputConnection == null)
            return;

        InputMethodManager imm = (InputMethodManager) 
            GeckoApp.surfaceView.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (imm == null)
            return;

        if (newEnd < 0)
            GeckoApp.surfaceView.inputConnection.notifySelectionChange(
                imm, start, end);
        else
            GeckoApp.surfaceView.inputConnection.notifyTextChange(
                imm, text, start, end, newEnd);
    }

    public static void enableAccelerometer(boolean enable) {
        SensorManager sm = (SensorManager) 
            GeckoApp.surfaceView.getContext().getSystemService(Context.SENSOR_SERVICE);

        if (enable) {
            Sensor accelSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            if (accelSensor == null)
                return;

            sm.registerListener(GeckoApp.surfaceView, accelSensor, SensorManager.SENSOR_DELAY_GAME);
        } else {
            sm.unregisterListener(GeckoApp.surfaceView);
        }
    }

    public static void enableLocation(boolean enable) {
        LocationManager lm = (LocationManager)
            GeckoApp.surfaceView.getContext().getSystemService(Context.LOCATION_SERVICE);

        if (enable) {
            Criteria crit = new Criteria();
            crit.setAccuracy(Criteria.ACCURACY_FINE);
            String provider = lm.getBestProvider(crit, true);
            if (provider == null)
                return;

            Location loc = lm.getLastKnownLocation(provider);
            if (loc != null)
                sendEventToGecko(new GeckoEvent(loc));
            lm.requestLocationUpdates(provider, 100, (float).5, GeckoApp.surfaceView, Looper.getMainLooper());
        } else {
            lm.removeUpdates(GeckoApp.surfaceView);
        }
    }

    public static void moveTaskToBack() {
        GeckoApp.mAppContext.moveTaskToBack(true);
    }

    public static void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        GeckoApp.surfaceView.inputConnection.mSelectionStart = selectionStart;
        GeckoApp.surfaceView.inputConnection.mSelectionLength = selectionLength;
        try {
            GeckoApp.surfaceView.inputConnection.mQueryResult.put(result);
        } catch (InterruptedException e) {
        }
    }

    static void onXreExit() {
        sGeckoRunning = false;
        Log.i("GeckoAppJava", "XRE exited");
        if (gRestartScheduled) {
            GeckoApp.mAppContext.doRestart();
        } else {
            Log.i("GeckoAppJava", "we're done, good bye");
            System.exit(0);
        }

    }
    static void scheduleRestart() {
        Log.i("GeckoAppJava", "scheduling restart");
        gRestartScheduled = true;        
    }
    
    static String[] getHandlersForMimeType(String aMimeType) {
        PackageManager pm = 
            GeckoApp.surfaceView.getContext().getPackageManager();
        Intent intent = new Intent();
        intent.setType(aMimeType);
        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        int numAttr = 4;
        String[] ret = new String[list.size() * numAttr];
        for (int i = 0; i < list.size(); i++) {
            ResolveInfo resolveInfo = list.get(i);
            ret[i * numAttr] = resolveInfo.loadLabel(pm).toString();
            if (resolveInfo.isDefault)
                ret[i * numAttr + 1] = "default";
            else
                ret[i * numAttr + 1] = "";
            ret[i * numAttr + 2] = resolveInfo.activityInfo.applicationInfo.packageName;
            ret[i * numAttr + 3] = resolveInfo.activityInfo.name;
            
        }
        return ret;
    }

    static String[] getHandlersForProtocol(String aScheme) {
        PackageManager pm = 
            GeckoApp.surfaceView.getContext().getPackageManager();
        Intent intent = new Intent();
        Uri uri = new Uri.Builder().scheme(aScheme).build();
        intent.setData(uri);
        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        int numAttr = 4;
        String[] ret = new String[list.size() * numAttr];
        for (int i = 0; i < list.size(); i++) {
            ResolveInfo resolveInfo = list.get(i);
                ret[i * numAttr] = resolveInfo.loadLabel(pm).toString();
            if (resolveInfo.isDefault)
                ret[i * numAttr + 1] = "default";
            else
                ret[i * numAttr + 1] = "";
            ret[i * numAttr + 2] = resolveInfo.activityInfo.applicationInfo.packageName;
            ret[i * numAttr + 3] = resolveInfo.activityInfo.name;

        }
        return ret;
    }

    static String getMimeTypeFromExtension(String aFileExt) {
        return android.webkit.MimeTypeMap.getSingleton().getMimeTypeFromExtension(aFileExt);
    }

    static boolean openUriExternal(String aUriSpec, String aMimeType, 
                                   String aPackageName, String aClassName) {
        // XXX: It's not clear if we should set the action to view or leave it open
        Intent intent = new Intent(Intent.ACTION_VIEW);
        if (aMimeType.length() > 0)
            intent.setDataAndType(Uri.parse(aUriSpec), aMimeType);
        else
            intent.setData(Uri.parse(aUriSpec));
        if (aPackageName.length() > 0 && aClassName.length() > 0)
            intent.setClassName(aPackageName, aClassName);

        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        try {
            GeckoApp.surfaceView.getContext().startActivity(intent);
            return true;
        } catch(ActivityNotFoundException e) {
            return false;
        }
    }

    static String getClipboardText() {
        Context context = GeckoApp.surfaceView.getContext();
        ClipboardManager cm = (ClipboardManager)
            context.getSystemService(Context.CLIPBOARD_SERVICE);
        if (!cm.hasText())
            return null;
        return cm.getText().toString();
    }

    static void setClipboardText(String text) {
        Context context = GeckoApp.surfaceView.getContext();
        ClipboardManager cm = (ClipboardManager)
            context.getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setText(text);
    }

    static void showAlertNotification(String imageUrl, String alertTitle, String alertText,
                                      String alertCookie, String alertName) {
        Log.i("GeckoAppJava", "GeckoAppShell.showAlertNotification\n" +
              "- image = '" + imageUrl + "'\n" +
              "- title = '" + alertTitle + "'\n" +
              "- text = '" + alertText +"'\n" +
              "- cookie = '" + alertCookie +"'\n" +
              "- name = '" + alertName + "'");

        int icon = R.drawable.icon; // Just use the app icon by default

        Uri imageUri = Uri.parse(imageUrl);
        String scheme = imageUri.getScheme();

        if ("drawable".equals(scheme)) {
            String resource = imageUri.getSchemeSpecificPart();
            if ("//alertdownloads".equals(resource))
                icon = R.drawable.alertdownloads;
            else if ("//alertaddons".equals(resource))
                icon = R.drawable.alertaddons;
        }

        int notificationID = alertName.hashCode();

        Notification notification = new Notification(icon, alertTitle, System.currentTimeMillis());

        // The intent to launch when the user clicks the expanded notification
        Intent notificationIntent = new Intent(GeckoApp.ACTION_ALERT_CLICK);
        notificationIntent.setClassName(GeckoApp.mAppContext,
                                        "org.mozilla." + GeckoApp.mAppContext.getAppName() + ".NotificationHandler");

        // Put the strings into the intent as an URI "alert:<name>#<cookie>"
        Uri dataUri = Uri.fromParts("alert", alertName, alertCookie);
        notificationIntent.setData(dataUri);

        PendingIntent contentIntent = PendingIntent.getActivity(GeckoApp.mAppContext, 0, notificationIntent, 0);
        notification.setLatestEventInfo(GeckoApp.mAppContext, alertTitle, alertText, contentIntent);

        // The intent to execute when the status entry is deleted by the user with the "Clear All Notifications" button
        Intent clearNotificationIntent = new Intent(GeckoApp.ACTION_ALERT_CLEAR);
        clearNotificationIntent.setClassName(GeckoApp.mAppContext,
                                        "org.mozilla." + GeckoApp.mAppContext.getAppName() + ".NotificationHandler");
        clearNotificationIntent.setData(dataUri);

        PendingIntent pendingClearIntent = PendingIntent.getActivity(GeckoApp.mAppContext, 0, clearNotificationIntent, 0);
        notification.deleteIntent = pendingClearIntent;

        // Show the notification
        NotificationManager notificationManager = (NotificationManager)
            GeckoApp.mAppContext.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(notificationID, notification);
        Log.i("GeckoAppJava", "Created notification ID " + notificationID);
    }

    public static void handleNotification(String action, String alertName, String alertCookie) {
        if (GeckoApp.ACTION_ALERT_CLICK.equals(action)) {
            Log.i("GeckoAppJava", "GeckoAppShell.handleNotification: callObserver(alertclickcallback)");
            callObserver(alertName, "alertclickcallback", alertCookie);
        }

        Log.i("GeckoAppJava", "GeckoAppShell.handleNotification: callObserver(alertfinished)");
        callObserver(alertName, "alertfinished", alertCookie);
        removeObserver(alertName);
    }
}
