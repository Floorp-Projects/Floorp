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
import android.graphics.*;
import android.widget.*;
import android.hardware.*;

import android.util.*;
import android.net.*;

abstract public class GeckoApp
    extends Activity
{
    public static final String ACTION_ALERT_CLICK = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR = "org.mozilla.gecko.ACTION_ALERT_CLEAR";

    public static FrameLayout mainLayout;
    public static GeckoSurfaceView surfaceView;
    public static GeckoApp mAppContext;
    public static boolean mFullscreen = false;
    public static File sGREDir = null;
    static Thread mLibLoadThread = null;

    enum LaunchState {PreLaunch, Launching, WaitButton,
                      Launched, GeckoRunning, GeckoExiting};
    private static LaunchState sLaunchState = LaunchState.PreLaunch;

    
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
            .setPositiveButton(getResources().getString(R.string.exit_label),
                               new DialogInterface.OnClickListener() {
                                   public void onClick(DialogInterface dialog,
                                                       int id)
                                   {
                                       GeckoApp.this.finish();
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
                    getResources().getString(R.string.splash_screen_label);
                surfaceView.drawSplashScreen();
                // unpack files in the components directory
                try {
                    unpackComponents();
                } catch (FileNotFoundException fnfe) {
                    Log.e("GeckoApp", "error unpacking components", fnfe);
                    Looper.prepare();
                    showErrorDialog(getString(R.string.error_loading_file));
                    Looper.loop();
                    return;
                } catch (IOException ie) {
                    Log.e("GeckoApp", "error unpacking components", ie);
                    String msg = ie.getMessage();
                    Looper.prepare();
                    if (msg.equalsIgnoreCase("No space left on device"))
                        showErrorDialog(getString(R.string.no_space_to_start_error));
                    else
                        showErrorDialog(getString(R.string.error_loading_file));
                    Looper.loop();
                    return;
                }
        
                // and then fire us up
                String env = i.getStringExtra("env0");
                GeckoAppShell.runGecko(getApplication().getPackageResourcePath(),
                                       i.getStringExtra("args"),
                                       i.getDataString());
            }
        }.start();
        return true;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        Log.i("GeckoApp", "create");
        super.onCreate(savedInstanceState);

        if (sGREDir == null) {
            // If application.ini already exists in the old dir, use its parent.
            File app_ini =
                new File("/data/data/" + getPackageName() + "/application.ini");
            if (app_ini.exists())
                sGREDir = app_ini.getParentFile();
            else
                sGREDir = this.getDir("gre", 0);
        }

        mAppContext = this;

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

        if (!checkAndSetLaunchState(LaunchState.PreLaunch,
                                    LaunchState.Launching))
            return;

        checkAndLaunchUpdate();
        mLibLoadThread = new Thread(new Runnable() { 
            public void run() {
                GeckoAppShell.loadGeckoLibs(
                    getApplication().getPackageResourcePath());
            }});
        File cacheFile = GeckoAppShell.getCacheDir();
        File libxulFile = new File(cacheFile, "libxul.so");

        if (GeckoAppShell.getFreeSpace() > GeckoAppShell.kFreeSpaceThreshold &&
            (!libxulFile.exists() || 
             new File(getApplication().getPackageResourcePath()).lastModified()
             >= libxulFile.lastModified()))
            surfaceView.mSplashStatusMsg =
                getResources().getString(R.string.splash_screen_installing);
        else
            surfaceView.mSplashStatusMsg =
                getResources().getString(R.string.splash_screen_label);
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
        if ("org.mozilla.gecko.DEBUG".equals(action) &&
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

        if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i("GeckoApp","onNewIntent: "+uri);
        }
        else if (Intent.ACTION_MAIN.equals(action)) {
            Log.i("GeckoApp", "Intent : ACTION_MAIN");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(""));
        }
        else if (action.equals("org.mozilla.fennec.WEBAPP")) {
            String uri = intent.getStringExtra("args");
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i("GeckoApp","Intent : WEBAPP - " + uri);
        }
    }

    @Override
    public void onPause()
    {
        Log.i("GeckoApp", "pause");
        GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_PAUSING));
        // The user is navigating away from this activity, but nothing
        // has come to the foreground yet; for Gecko, we may want to
        // stop repainting, for example.

        // Whatever we do here should be fast, because we're blocking
        // the next activity from showing up until we finish.

        // onPause will be followed by either onResume or onStop.
        super.onPause();
    }

    @Override
    public void onResume()
    {
        Log.i("GeckoApp", "resume");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onResume();
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        // Just in case. Normally we start in onNewIntent
        if (checkLaunchState(LaunchState.PreLaunch) ||
            checkLaunchState(LaunchState.Launching))
            onNewIntent(getIntent());
    }

    @Override
    public void onStop()
    {
        Log.i("GeckoApp", "stop");
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
    }

    @Override
    public void onRestart()
    {
        Log.i("GeckoApp", "restart");
        super.onRestart();
    }

    @Override
    public void onStart()
    {
        Log.i("GeckoApp", "start");
        super.onStart();
    }

    @Override
    public void onDestroy()
    {
        Log.i("GeckoApp", "destroy");
        // Tell Gecko to shutting down; we'll end up calling System.exit()
        // in onXreExit.
        if (isFinishing())
            GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_SHUTDOWN));

        super.onDestroy();
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig)
    {
        Log.i("GeckoApp", "configuration changed");
        // nothing, just ignore
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onLowMemory()
    {
        Log.e("GeckoApp", "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
    }

    abstract public String getPackageName();
    abstract public String getContentProcessName();

    protected void unpackComponents()
        throws IOException, FileNotFoundException
    {
        ZipFile zip;
        InputStream listStream;

        File componentsDir = new File(sGREDir, "components");
        componentsDir.mkdir();
        zip = new ZipFile(getApplication().getPackageResourcePath());

        byte[] buf = new byte[8192];
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

    private void unpackFile(ZipFile zip, byte[] buf, ZipEntry fileEntry,
                            String name)
        throws IOException, FileNotFoundException
    {
        if (fileEntry == null)
            fileEntry = zip.getEntry(name);
        if (fileEntry == null)
            throw new FileNotFoundException("Can't find " + name + " in " +
                                            zip.getName());

        File outFile = new File(sGREDir, name);
        if (outFile.exists() &&
            outFile.lastModified() == fileEntry.getTime() &&
            outFile.length() == fileEntry.getSize())
            return;

        File dir = outFile.getParentFile();
        if (!outFile.exists())
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
            Log.i("GeckoAppJava", intent.toString());
            startActivity(intent);
        } catch (Exception e) {
            Log.i("GeckoAppJava", "error doing restart", e);
        }
        finish();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i("GeckoAppJava", "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR
        File downloadDir = null;
        if (Build.VERSION.SDK_INT >= 8)
            downloadDir = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        else
            downloadDir = new File(Environment.getExternalStorageDirectory().getPath(), "download");

        File updateDir = new File(new File(downloadDir, "updates"),"0");

        File updateFile = new File(updateDir, "update.apk");
        File statusFile = new File(updateDir, "update.status");

        if (!statusFile.exists() || !readUpdateStatus(statusFile).equals("pending"))
            return;

        if (!updateFile.exists())
            return;

        Log.i("GeckoAppJava", "Update is available!");

        // Launch APK
        File updateFileToRun = new File(updateDir + getPackageName() + "-update.apk");
        try {
            if (updateFile.renameTo(updateFileToRun)) {
                String amCmd = "/system/bin/am start -a android.intent.action.VIEW " +
                               "-n com.android.packageinstaller/.PackageInstallerActivity -d file://" +
                               updateFileToRun.getPath();
                Log.i("GeckoAppJava", amCmd);
                Runtime.getRuntime().exec(amCmd);
                statusCode = 0; // OK
            } else {
                Log.i("GeckoAppJava", "Cannot rename the update file!");
                statusCode = 7; // WRITE_ERROR
            }
        } catch (Exception e) {
            Log.i("GeckoAppJava", "error launching installer to update", e);
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
            Log.i("GeckoAppJava", "error writing status file", e);
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
            Log.i("GeckoAppJava", "error reading update status", e);
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
                Intent.createChooser(intent,"choose a file"),
                FILE_PICKER_REQUEST);
        String filePickerResult = "";
        try {
            filePickerResult = mFilePickerResult.take();
        } catch (InterruptedException e) {
            Log.i("GeckoApp", "showing file picker ",  e);
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
                String mimeType = cr.getType(uri);
                String fileExt = "." + 
                    mimeType.substring(mimeType.lastIndexOf('/') + 1);
                File file = 
                    File.createTempFile("tmp_" + 
                                        (int)Math.floor(1000 * Math.random()), 
                                        fileExt, sGREDir);
                
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
                Log.e("GeckoApp", "showing file picker", e);
            }
        }
        try {
            mFilePickerResult.put(filePickerResult);
        } catch (InterruptedException e) {
            Log.i("GeckoApp", "error returning file picker result", e);
        }
    }
}
