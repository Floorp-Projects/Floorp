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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
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
import java.nio.channels.FileChannel;
import java.util.concurrent.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.content.res.*;
import android.graphics.*;
import android.widget.*;
import android.hardware.*;

import android.util.*;
import android.net.*;
import android.database.*;
import android.provider.*;
import android.telephony.*;

abstract public class GeckoApp
    extends Activity
{
    private static final String LOG_FILE_NAME     = "GeckoApp";

    public static final String ACTION_ALERT_CLICK = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR = "org.mozilla.gecko.ACTION_ALERT_CLEAR";
    public static final String ACTION_WEBAPP      = "org.mozilla.gecko.WEBAPP";
    public static final String ACTION_DEBUG       = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_BOOKMARK    = "org.mozilla.gecko.BOOKMARK";

    public static FrameLayout mainLayout;
    public static GeckoSurfaceView surfaceView;
    public static GeckoApp mAppContext;
    public static boolean mFullscreen = false;
    public static File sGREDir = null;
    static Thread mLibLoadThread = null;
    public Handler mMainHandler;
    private IntentFilter mConnectivityFilter;
    private BroadcastReceiver mConnectivityReceiver;
    private PhoneStateListener mPhoneStateListener;

    enum LaunchState {PreLaunch, Launching, WaitButton,
                      Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.PreLaunch;
    private static boolean sTryCatchAttached = false;


    static boolean checkLaunchState(LaunchState checkState) {
        synchronized(sLaunchState) {
            return sLaunchState == checkState;
        }
    }

    static void setLaunchState(LaunchState setState) {
        synchronized(sLaunchState) {
            sLaunchState = setState;
        }
    }

    // if mLaunchState is equal to checkState this sets mLaunchState to setState
    // and return true. Otherwise we return false.
    static boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        synchronized(sLaunchState) {
            if (sLaunchState != checkState)
                return false;
            sLaunchState = setState;
            return true;
        }
    }

    void showErrorDialog(String message)
    {
        new AlertDialog.Builder(this)
            .setMessage(message)
            .setCancelable(false)
            .setPositiveButton(R.string.exit_label,
                               new DialogInterface.OnClickListener() {
                                   public void onClick(DialogInterface dialog,
                                                       int id)
                                   {
                                       GeckoApp.this.finish();
                                       System.exit(0);
                                   }
                               }).show();
    }

    // Returns true when the intent is going to be handled by gecko launch
    boolean launch(Intent intent)
    {
        if (!checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched))
            return false;

        if (intent == null)
            intent = getIntent();
        final Intent i = intent;
        new Thread() {
            public void run() {
                try {
                    if (mLibLoadThread != null)
                        mLibLoadThread.join();
                } catch (InterruptedException ie) {}
                surfaceView.mSplashStatusMsg =
                    getResources().getString(R.string.splash_screen_loading);
                surfaceView.drawSplashScreen();
                // unpack files in the components directory
                try {
                    unpackComponents();
                } catch (FileNotFoundException fnfe) {
                    Log.e(LOG_FILE_NAME, "error unpacking components", fnfe);
                    Looper.prepare();
                    showErrorDialog(getString(R.string.error_loading_file));
                    Looper.loop();
                    return;
                } catch (IOException ie) {
                    Log.e(LOG_FILE_NAME, "error unpacking components", ie);
                    String msg = ie.getMessage();
                    Looper.prepare();
                    if (msg != null && msg.equalsIgnoreCase("No space left on device"))
                        showErrorDialog(getString(R.string.no_space_to_start_error));
                    else
                        showErrorDialog(getString(R.string.error_loading_file));
                    Looper.loop();
                    return;
                }

                // and then fire us up
                try {
                    String env = i.getStringExtra("env0");
                    GeckoAppShell.runGecko(getApplication().getPackageResourcePath(),
                                           i.getStringExtra("args"),
                                           i.getDataString());
                } catch (Exception e) {
                    Log.e(LOG_FILE_NAME, "top level exception", e);
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    GeckoAppShell.reportJavaCrash(sw.toString());
                }
            }
        }.start();
        return true;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        mAppContext = this;
        mMainHandler = new Handler();

        if (!sTryCatchAttached) {
            sTryCatchAttached = true;
            mMainHandler.post(new Runnable() {
                public void run() {
                    try {
                        Looper.loop();
                    } catch (Exception e) {
                        Log.e(LOG_FILE_NAME, "top level exception", e);
                        StringWriter sw = new StringWriter();
                        e.printStackTrace(new PrintWriter(sw));
                        GeckoAppShell.reportJavaCrash(sw.toString());
                    }
                    // resetting this is kinda pointless, but oh well
                    sTryCatchAttached = false;
                }
            });
        }

        SharedPreferences settings = getPreferences(Activity.MODE_PRIVATE);
        String localeCode = settings.getString(getPackageName() + ".locale", "");
        if (localeCode != null && localeCode.length() > 0)
            GeckoAppShell.setSelectedLocale(localeCode);

        Log.i(LOG_FILE_NAME, "create");
        super.onCreate(savedInstanceState);

        if (sGREDir == null)
            sGREDir = new File(this.getApplicationInfo().dataDir);

        getWindow().setFlags(mFullscreen ?
                             WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);

        if (surfaceView == null)
            surfaceView = new GeckoSurfaceView(this);
        else
            mainLayout.removeView(surfaceView);

        mainLayout = new FrameLayout(this);
        mainLayout.addView(surfaceView,
                           new FrameLayout.LayoutParams(FrameLayout.LayoutParams.FILL_PARENT,
                                                        FrameLayout.LayoutParams.FILL_PARENT));

        setContentView(mainLayout,
                       new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT,
                                                  ViewGroup.LayoutParams.FILL_PARENT));

        mConnectivityFilter = new IntentFilter();
        mConnectivityFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        mConnectivityReceiver = new GeckoConnectivityReceiver();

        mPhoneStateListener = new GeckoPhoneStateListener();

        if (!checkAndSetLaunchState(LaunchState.PreLaunch,
                                    LaunchState.Launching))
            return;

        checkAndLaunchUpdate();
        mLibLoadThread = new Thread(new Runnable() {
            public void run() {
                // At some point while loading the gecko libs our default locale gets set
                // so just save it to locale here and reset it as default after the join
                Locale locale = Locale.getDefault();
                GeckoAppShell.loadGeckoLibs(
                    getApplication().getPackageResourcePath());
                Locale.setDefault(locale);
                Resources res = getBaseContext().getResources();
                Configuration config = res.getConfiguration();
                config.locale = locale;
                res.updateConfiguration(config, res.getDisplayMetrics());


            }});
        surfaceView.mSplashStatusMsg =
            getResources().getString(R.string.splash_screen_loading);
        mLibLoadThread.start();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (checkLaunchState(LaunchState.GeckoExiting)) {
            // We're exiting and shouldn't try to do anything else just incase
            // we're hung for some reason we'll force the process to exit
            System.exit(0);
            return;
        }
        final String action = intent.getAction();
        if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitButton)) {
            final Button launchButton = new Button(this);
            launchButton.setText("Launch"); // don't need to localize
            launchButton.setOnClickListener(new Button.OnClickListener() {
                public void onClick (View v) {
                    // hide the button so we can't be launched again
                    mainLayout.removeView(launchButton);
                    setLaunchState(LaunchState.Launching);
                    launch(null);
                }
            });
            mainLayout.addView(launchButton, 300, 200);
            return;
        }
        if (checkLaunchState(LaunchState.WaitButton) || launch(intent))
            return;

        if (Intent.ACTION_MAIN.equals(action)) {
            Log.i(LOG_FILE_NAME, "Intent : ACTION_MAIN");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(""));
        }
        else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOG_FILE_NAME,"onNewIntent: "+uri);
        }
        else if (ACTION_WEBAPP.equals(action)) {
            String uri = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i(LOG_FILE_NAME,"Intent : WEBAPP - " + uri);
        }
        else if (ACTION_BOOKMARK.equals(action)) {
            String args = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(args));
            Log.i(LOG_FILE_NAME,"Intent : BOOKMARK - " + args);
        }
    }

    @Override
    public void onPause()
    {
        Log.i(LOG_FILE_NAME, "pause");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_PAUSING));
        // The user is navigating away from this activity, but nothing
        // has come to the foreground yet; for Gecko, we may want to
        // stop repainting, for example.

        // Whatever we do here should be fast, because we're blocking
        // the next activity from showing up until we finish.

        // onPause will be followed by either onResume or onStop.
        super.onPause();

        unregisterReceiver(mConnectivityReceiver);

        TelephonyManager tm = (TelephonyManager)
            GeckoApp.mAppContext.getSystemService(Context.TELEPHONY_SERVICE);
        tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
    }

    @Override
    public void onResume()
    {
        Log.i(LOG_FILE_NAME, "resume");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onResume();
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        // Just in case. Normally we start in onNewIntent
        if (checkLaunchState(LaunchState.PreLaunch) ||
            checkLaunchState(LaunchState.Launching))
            onNewIntent(getIntent());

        registerReceiver(mConnectivityReceiver, mConnectivityFilter);

        TelephonyManager tm = (TelephonyManager)
            GeckoApp.mAppContext.getSystemService(Context.TELEPHONY_SERVICE);
        tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_DATA_CONNECTION_STATE);

        // Notify if network state changed since we paused
        GeckoAppShell.onNetworkStateChange(true);
    }

    @Override
    public void onStop()
    {
        Log.i(LOG_FILE_NAME, "stop");
        // We're about to be stopped, potentially in preparation for
        // being destroyed.  We're killable after this point -- as I
        // understand it, in extreme cases the process can be terminated
        // without going through onDestroy.
        //
        // We might also get an onRestart after this; not sure what
        // that would mean for Gecko if we were to kill it here.
        // Instead, what we should do here is save prefs, session,
        // etc., and generally mark the profile as 'clean', and then
        // dirty it again if we get an onResume.


        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_STOPPING));
        super.onStop();
        GeckoAppShell.putChildInBackground();
    }

    @Override
    public void onRestart()
    {
        Log.i(LOG_FILE_NAME, "restart");
        GeckoAppShell.putChildInForeground();
        super.onRestart();
    }

    @Override
    public void onStart()
    {
        Log.i(LOG_FILE_NAME, "start");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_START));
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        Log.i(LOG_FILE_NAME, "destroy");
        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_SHUTDOWN));

        super.onDestroy();
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig)
    {
        Log.i(LOG_FILE_NAME, "configuration changed");
        // nothing, just ignore
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onLowMemory()
    {
        Log.e(LOG_FILE_NAME, "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
    }

    abstract public String getPackageName();
    abstract public String getContentProcessName();

    protected void unpackComponents()
        throws IOException, FileNotFoundException
    {
        File applicationPackage = new File(getApplication().getPackageResourcePath());
        File componentsDir = new File(sGREDir, "components");
        if (componentsDir.lastModified() == applicationPackage.lastModified())
            return;

        componentsDir.mkdir();
        componentsDir.setLastModified(applicationPackage.lastModified());

        surfaceView.mSplashStatusMsg =
                    getResources().getString(R.string.splash_firstrun);
        surfaceView.drawSplashScreen();

        GeckoAppShell.killAnyZombies();

        ZipFile zip = new ZipFile(applicationPackage);

        byte[] buf = new byte[32768];
        try {
            if (unpackFile(zip, buf, null, "removed-files"))
                removeFiles();
        } catch (Exception ex) {
            // This file may not be there, so just log any errors and move on
            Log.w(LOG_FILE_NAME, "error removing files", ex);
        }
        unpackFile(zip, buf, null, "application.ini");
        unpackFile(zip, buf, null, getContentProcessName());
        try {
            unpackFile(zip, buf, null, "update.locale");
        } catch (Exception e) {/* this is non-fatal */}

        // copy any .xpi file into an extensions/ directory
        Enumeration<? extends ZipEntry> zipEntries = zip.entries();
        while (zipEntries.hasMoreElements()) {
            ZipEntry entry = zipEntries.nextElement();
            if (entry.getName().startsWith("extensions/") && entry.getName().endsWith(".xpi")) {
                Log.i("GeckoAppJava", "installing extension : " + entry.getName());
                unpackFile(zip, buf, entry, entry.getName());
            }
        }
    }

    void removeFiles() throws IOException {
        BufferedReader reader = new BufferedReader(
            new FileReader(new File(sGREDir, "removed-files")));
        try {
            for (String removedFileName = reader.readLine(); 
                 removedFileName != null; removedFileName = reader.readLine()) {
                File removedFile = new File(sGREDir, removedFileName);
                if (removedFile.exists())
                    removedFile.delete();
            }
        } finally {
            reader.close();
        }
        
    }

    private boolean unpackFile(ZipFile zip, byte[] buf, ZipEntry fileEntry,
                            String name)
        throws IOException, FileNotFoundException
    {
        if (fileEntry == null)
            fileEntry = zip.getEntry(name);
        if (fileEntry == null)
            throw new FileNotFoundException("Can't find " + name + " in " +
                                            zip.getName());

        File outFile = new File(sGREDir, name);
        if (outFile.lastModified() == fileEntry.getTime() &&
            outFile.length() == fileEntry.getSize())
            return false;

        File dir = outFile.getParentFile();
        if (!dir.exists())
            dir.mkdirs();

        InputStream fileStream;
        fileStream = zip.getInputStream(fileEntry);

        OutputStream outStream = new FileOutputStream(outFile);

        while (fileStream.available() > 0) {
            int read = fileStream.read(buf, 0, buf.length);
            outStream.write(buf, 0, read);
        }

        fileStream.close();
        outStream.close();
        outFile.setLastModified(fileEntry.getTime());
        return true;
    }

    public void addEnvToIntent(Intent intent) {
        Map<String,String> envMap = System.getenv();
        Set<Map.Entry<String,String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String,String>> envIter = envSet.iterator();
        StringBuffer envstr = new StringBuffer();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String,String> entry = envIter.next();
            intent.putExtra("env" + c, entry.getKey() + "="
                            + entry.getValue());
            c++;
        }
    }

    public void doRestart() {
        try {
            String action = "org.mozilla.gecko.restart";
            Intent intent = new Intent(action);
            intent.setClassName(getPackageName(),
                                getPackageName() + ".Restarter");
            addEnvToIntent(intent);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                            Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            Log.i(LOG_FILE_NAME, intent.toString());
            GeckoAppShell.killAnyZombies();
            startActivity(intent);
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error doing restart", e);
        }
        finish();
        // Give the restart process time to start before we die
        GeckoAppShell.waitForAnotherGeckoProc();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i(LOG_FILE_NAME, "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR
        File baseUpdateDir = null;
        if (Build.VERSION.SDK_INT >= 8)
            baseUpdateDir = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        else
            baseUpdateDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");

        File updateDir = new File(new File(baseUpdateDir, "updates"),"0");

        File updateFile = new File(updateDir, "update.apk");
        File statusFile = new File(updateDir, "update.status");

        if (!statusFile.exists() || !readUpdateStatus(statusFile).equals("pending"))
            return;

        if (!updateFile.exists())
            return;

        Log.i(LOG_FILE_NAME, "Update is available!");

        // Launch APK
        File updateFileToRun = new File(updateDir, getPackageName() + "-update.apk");
        try {
            if (updateFile.renameTo(updateFileToRun)) {
                String amCmd = "/system/bin/am start -a android.intent.action.VIEW " +
                               "-n com.android.packageinstaller/.PackageInstallerActivity -d file://" +
                               updateFileToRun.getPath();
                Log.i(LOG_FILE_NAME, amCmd);
                Runtime.getRuntime().exec(amCmd);
                statusCode = 0; // OK
            } else {
                Log.i(LOG_FILE_NAME, "Cannot rename the update file!");
                statusCode = 7; // WRITE_ERROR
            }
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error launching installer to update", e);
        }

        // Update the status file
        String status = statusCode == 0 ? "succeeded\n" : "failed: "+ statusCode + "\n";

        OutputStream outStream;
        try {
            byte[] buf = status.getBytes("UTF-8");
            outStream = new FileOutputStream(statusFile);
            outStream.write(buf, 0, buf.length);
            outStream.close();
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error writing status file", e);
        }

        if (statusCode == 0)
            System.exit(0);
    }

    private String readUpdateStatus(File statusFile) {
        String status = "";
        try {
            BufferedReader reader = new BufferedReader(new FileReader(statusFile));
            status = reader.readLine();
            reader.close();
        } catch (Exception e) {
            Log.i(LOG_FILE_NAME, "error reading update status", e);
        }
        return status;
    }

    static final int FILE_PICKER_REQUEST = 1;

    private SynchronousQueue<String> mFilePickerResult = new SynchronousQueue();
    public String showFilePicker(String aMimeType) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(aMimeType);
        GeckoApp.this.
            startActivityForResult(
                Intent.createChooser(intent, getString(R.string.choose_file)),
                FILE_PICKER_REQUEST);
        String filePickerResult = "";

        try {
            while (null == (filePickerResult = mFilePickerResult.poll(1, TimeUnit.MILLISECONDS))) {
                Log.i("GeckoApp", "processing events from showFilePicker ");
                GeckoAppShell.processNextNativeEvent();
            }
        } catch (InterruptedException e) {
            Log.i(LOG_FILE_NAME, "showing file picker ",  e);
        }

        return filePickerResult;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {
        String filePickerResult = "";
        if (data != null && resultCode == RESULT_OK) {
            try {
                ContentResolver cr = getContentResolver();
                Uri uri = data.getData();
                Cursor cursor = GeckoApp.mAppContext.getContentResolver().query(
                    uri, 
                    new String[] { OpenableColumns.DISPLAY_NAME },
                    null, 
                    null, 
                    null);
                String name = null;
                if (cursor != null) {
                    try {
                        if (cursor.moveToNext()) {
                            name = cursor.getString(0);
                        }
                    } finally {
                        cursor.close();
                    }
                }
                String fileName = "tmp_";
                String fileExt = null;
                int period;
                if (name == null || (period = name.lastIndexOf('.')) == -1) {
                    String mimeType = cr.getType(uri);
                    fileExt = "." + GeckoAppShell.getExtensionFromMimeType(mimeType);
                } else {
                    fileExt = name.substring(period);
                    fileName = name.substring(0, period);
                }
                File file = File.createTempFile(fileName, fileExt, sGREDir);

                FileOutputStream fos = new FileOutputStream(file);
                InputStream is = cr.openInputStream(uri);
                byte[] buf = new byte[4096];
                int len = is.read(buf);
                while (len != -1) {
                    fos.write(buf, 0, len);
                    len = is.read(buf);
                }
                fos.close();
                filePickerResult =  file.getAbsolutePath();
            }catch (Exception e) {
                Log.e(LOG_FILE_NAME, "showing file picker", e);
            }
        }
        try {
            mFilePickerResult.put(filePickerResult);
        } catch (InterruptedException e) {
            Log.i(LOG_FILE_NAME, "error returning file picker result", e);
        }
    }
}
