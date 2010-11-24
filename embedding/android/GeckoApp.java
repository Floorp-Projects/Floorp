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
    ProgressDialog mProgressDialog;

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
            .setPositiveButton("Exit",
                               new DialogInterface.OnClickListener() {
                                   public void onClick(DialogInterface dialog,
                                                       int id)
                                   {
                                       GeckoApp.this.finish();
                                   }
                               }).show();
    }

    // Returns true when the intent is going to be handled by gecko launch
    boolean launch(Intent i)
    {
        if (!checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched))
            return false;

        // unpack files in the components directory
        try {
            unpackComponents();
        } catch (FileNotFoundException fnfe) {
            showErrorDialog(getString(R.string.error_loading_file));
            return false;
        } catch (IOException ie) {
            String msg = ie.getMessage();
            if (msg.equalsIgnoreCase("No space left on device"))
                showErrorDialog(getString(R.string.no_space_to_start_error));
            else
                showErrorDialog(getString(R.string.error_loading_file));
            return false;
        }

        mProgressDialog = 
            ProgressDialog.show(GeckoApp.this, "",
                                getString(R.string.splash_screen_label),
                                true);

        // and then fire us up
        if (i == null)
            i = getIntent();
        String env = i.getStringExtra("env0");
        GeckoAppShell.runGecko(getApplication().getPackageResourcePath(),
                               i.getStringExtra("args"),
                               i.getDataString());
        return true;
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        Log.i("GeckoApp", "create");
        super.onCreate(savedInstanceState);

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

        if (!checkCPUCompatability())
            return;
        // Load our JNI libs
        GeckoAppShell.loadGeckoLibs(getApplication().getPackageResourcePath());
    }


    boolean checkCPUCompatability() {
        try {
            BufferedReader reader =
                new BufferedReader(new FileReader("/proc/cpuinfo"));
            String line;
            while ((line = reader.readLine()) != null) {
                int index = line.indexOf("Processor");
                if (index == -1)
                    continue;

                int version = 5;
                if (line.indexOf("(v8l)") != -1)
                    version = 8;
                if (line.indexOf("(v7l)") != -1)
                    version = 7;
                if (line.indexOf("(v6l)") != -1)
                    version = 6;
                
                if (version < getMinCPUVersion()) {
                    showErrorDialog(
                        getString(R.string.incompatable_cpu_error));
                    return false;
                }
                else {
                    break;
                }
            }
        } catch (Exception ex) {
            // Not much we can do here, just continue assuming we're okay
            Log.i("GeckoApp", "exception: " + ex);
        }
        return true;
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
        if (action.equals("org.mozilla.gecko.DEBUG") &&
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

        // XXX do the above.

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
            GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.ACTIVITY_STOPPING));

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
        Log.i("GeckoApp", "low memory");
        if (checkLaunchState(LaunchState.GeckoRunning))
            GeckoAppShell.onLowMemory();
        super.onLowMemory();
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                if (event.getRepeatCount() == 0) {
                    event.startTracking();
                    return true;
                } else {
                    return false;
                }
            case KeyEvent.KEYCODE_MENU:
                if (event.getRepeatCount() == 0) {
                    event.startTracking();
                    break;
                } else if ((event.getFlags() & KeyEvent.FLAG_LONG_PRESS) != 0) {
                    break;
                }
                // Ignore repeats for KEYCODE_MENU; they confuse the widget code.
                return false;
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_SEARCH:
                return false;
            case KeyEvent.KEYCODE_DEL:
                // See comments in GeckoInputConnection.onKeyDel
                if (surfaceView != null &&
                    surfaceView.inputConnection != null &&
                    surfaceView.inputConnection.onKeyDel()) {
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_ENTER:
                if ((event.getFlags() & KeyEvent.FLAG_EDITOR_ACTION) != 0 &&
                    surfaceView.mIMEActionHint.equalsIgnoreCase("next"))
                    event = new KeyEvent(event.getAction(), KeyEvent.KEYCODE_TAB);
                break;
            default:
                break;
        }
        // KeyListener returns true if it handled the event for us.
        if (GeckoApp.surfaceView.mIMEState == GeckoSurfaceView.IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            !GeckoApp.surfaceView.mKeyListener.onKeyDown(GeckoApp.surfaceView, GeckoApp.surfaceView.mEditable, keyCode, event))
            GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                if (!event.isTracking() || event.isCanceled())
                    return false;
                break;
            default:
                break;
        }
        if (GeckoApp.surfaceView.mIMEState == GeckoSurfaceView.IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            !GeckoApp.surfaceView.mKeyListener.onKeyUp(GeckoApp.surfaceView, GeckoApp.surfaceView.mEditable, keyCode, event))
            GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
                GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
                return true;
            case KeyEvent.KEYCODE_MENU:
                InputMethodManager imm = (InputMethodManager)
                    surfaceView.getContext().getSystemService(
                            Context.INPUT_METHOD_SERVICE);
                imm.toggleSoftInputFromWindow(surfaceView.getWindowToken(),
                                              imm.SHOW_FORCED, 0);
                return true;
            default:
                break;
        }
        return false;
    }

    abstract public String getAppName();
    abstract public String getContentProcessName();
    abstract public int getMinCPUVersion();

    protected void unpackComponents()
        throws IOException, FileNotFoundException
    {
        ZipFile zip;
        InputStream listStream;

        File componentsDir = new File("/data/data/org.mozilla." + getAppName() +
                                      "/components");
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

        File outFile = new File("/data/data/org.mozilla." + getAppName() +
                                "/" + name);
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
            String action = "org.mozilla.gecko.restart" + getAppName();
            Intent intent = new Intent(action);
            intent.setClassName("org.mozilla." + getAppName(),
                                "org.mozilla." + getAppName() + ".Restarter");
            addEnvToIntent(intent);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            Log.i("GeckoAppJava", intent.toString());
            startActivity(intent);
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
        }
        finish();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i("GeckoAppJava", "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR

        String updateDir = Environment.getExternalStorageDirectory().getPath() + "/downloads/updates/0/";
        File updateFile = new File(updateDir + "update.apk");
        File statusFile = new File(updateDir + "update.status");

        if (!statusFile.exists() || !readUpdateStatus(statusFile).equals("pending"))
            return;

        if (!updateFile.exists())
            return;

        Log.i("GeckoAppJava", "Update is available!");

        // Launch APK
        File updateFileToRun = new File(updateDir + getAppName() + "-update.apk");
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
            Log.i("GeckoAppJava", e.toString());
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
            Log.i("GeckoAppJava", e.toString());
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
            Log.i("GeckoAppJava", e.toString());
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
            Log.i("GeckoApp", "error: " + e);
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
                                        fileExt, 
                                        new File("/data/data/org.mozilla." +
                                                 getAppName()));
                
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
                Log.e("GeckoApp", "error : "+ e);
            }
        }
        try {
            mFilePickerResult.put(filePickerResult);
        } catch (InterruptedException e) {
            Log.i("GeckoApp", "error: " + e);
        }
    }
}
