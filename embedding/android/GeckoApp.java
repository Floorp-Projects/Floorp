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

abstract public class GeckoApp
    extends Activity
{
    public static final String ACTION_ALERT_CLICK = "org.mozilla.gecko.ACTION_ALERT_CLICK";
    public static final String ACTION_ALERT_CLEAR = "org.mozilla.gecko.ACTION_ALERT_CLEAR";

    public static FrameLayout mainLayout;
    public static GeckoSurfaceView surfaceView;
    public static GeckoApp mAppContext;
    ProgressDialog mProgressDialog;

    void launch()
    {
        // unpack files in the components directory
        unpackComponents();
        // and then fire us up
        Intent i = getIntent();
        String env = i.getStringExtra("env0");
        GeckoAppShell.runGecko(getApplication().getPackageResourcePath(),
                               i.getStringExtra("args"),
                               i.getDataString());
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        Log.i("GeckoApp", "create");
        super.onCreate(savedInstanceState);

        mAppContext = this;

        // hide our window's title, we don't want it
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        checkAndLaunchUpdate();

        surfaceView = new GeckoSurfaceView(this);
        
        mainLayout = new FrameLayout(this);
        mainLayout.addView(surfaceView,
                           new FrameLayout.LayoutParams(FrameLayout.LayoutParams.FILL_PARENT,
                                                        FrameLayout.LayoutParams.FILL_PARENT));

        boolean useLaunchButton = false;

        String intentAction = getIntent().getAction();
        if (intentAction != null && intentAction.equals("org.mozilla.gecko.DEBUG"))
            useLaunchButton = true;

        setContentView(mainLayout,
                       new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT,
                                                  ViewGroup.LayoutParams.FILL_PARENT));

        if (!GeckoAppShell.sGeckoRunning) {
            
            if (!useLaunchButton)
                mProgressDialog = 
                    ProgressDialog.show(GeckoApp.this, "", getAppName() + 
                                        " is loading", true);
            // Load our JNI libs; we need to do this before launch() because
            // setInitialSize will be called even before Gecko is actually up
            // and running.
            GeckoAppShell.loadGeckoLibs();

            if (useLaunchButton) {
                final Button b = new Button(this);
                b.setText("Launch");
                b.setOnClickListener(new Button.OnClickListener() {
                        public void onClick (View v) {
                            // hide the button so we can't be launched again
                            mainLayout.removeView(b);
                            launch();
                        }
                    });
                mainLayout.addView(b, 300, 200);
            } else {
                launch();
            }
        }

        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        final String action = intent.getAction();
        if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            GeckoAppShell.sendEventToGecko(new GeckoEvent(uri));
            Log.i("GeckoApp","onNewIntent: "+uri);
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
        GeckoAppShell.onResume();
        if (surfaceView != null)
            surfaceView.mSurfaceNeedsRedraw = true;
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();
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
        if (GeckoAppShell.sGeckoRunning)
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
            default:
                break;
        }
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch(keyCode) {
            case KeyEvent.KEYCODE_BACK:
                if (!event.isTracking() || event.isCanceled())
                    return false;
                break;
            default:
                break;
        }
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    abstract public String getAppName();
    abstract public String getContentProcessName();

    protected void unpackComponents()
    {
        ZipFile zip;
        InputStream listStream;

        try {
            File componentsDir = new File("/data/data/org.mozilla." + getAppName() +"/components");
            componentsDir.mkdir();
            zip = new ZipFile(getApplication().getPackageResourcePath());
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
            return;
        }

        byte[] buf = new byte[8192];
        unpackFile(zip, buf, null, "application.ini");
        unpackFile(zip, buf, null, getContentProcessName());
        unpackFile(zip, buf, null, "update.locale");

        try {
            ZipEntry componentsList = zip.getEntry("components/components.manifest");
            if (componentsList == null) {
                Log.i("GeckoAppJava", "Can't find components.manifest!");
                return;
            }

            listStream = new BufferedInputStream(zip.getInputStream(componentsList));
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
            return;
        }

        StreamTokenizer tkn = new StreamTokenizer(new InputStreamReader(listStream));
        String line = "components/";
        int status;
        boolean addnext = false;
        tkn.eolIsSignificant(true);
        do {
            try {
                status = tkn.nextToken();
            } catch (IOException e) {
                Log.i("GeckoAppJava", e.toString());
                return;
            }
            switch (status) {
            case StreamTokenizer.TT_WORD:
                if (tkn.sval.equals("binary-component"))
                    addnext = true;
                else if (addnext) {
                    line += tkn.sval;
                    addnext = false;
                }
                break;
            case StreamTokenizer.TT_NUMBER:
                break;
            case StreamTokenizer.TT_EOF:
            case StreamTokenizer.TT_EOL:
                unpackFile(zip, buf, null, line);
                line = "components/";
                break;
            }
        } while (status != StreamTokenizer.TT_EOF);
    }

    private void unpackFile(ZipFile zip, byte[] buf, ZipEntry fileEntry, String name)
    {
        if (fileEntry == null)
            fileEntry = zip.getEntry(name);
        if (fileEntry == null) {
            Log.i("GeckoAppJava", "Can't find " + name + " in " + zip.getName());
            return;
        }

        File outFile = new File("/data/data/org.mozilla." + getAppName() + "/" + name);
        if (outFile.exists() &&
            outFile.lastModified() >= fileEntry.getTime() &&
            outFile.length() == fileEntry.getSize())
            return;

        try {
            File dir = outFile.getParentFile();
            if (!outFile.exists())
                dir.mkdirs();
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
            return;
        }

        InputStream fileStream;
        try {
            fileStream = zip.getInputStream(fileEntry);
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
            return;
        }

        OutputStream outStream;
        try {
            outStream = new FileOutputStream(outFile);

            while (fileStream.available() > 0) {
                int read = fileStream.read(buf, 0, buf.length);
                outStream.write(buf, 0, read);
            }

            fileStream.close();
            outStream.close();
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
            return;
        }

        outFile.setLastModified(fileEntry.getTime());
    }
    
    public String getEnvString() {
        Map<String,String> envMap = System.getenv();
        Set<Map.Entry<String,String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String,String>> envIter = envSet.iterator();
        StringBuffer envstr = new StringBuffer();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String,String> entry = envIter.next();
            // No need to pass env vars that we know the system provides
            // Unnecessary vars need to be trimmed since amount of data
            // we can pass this way is limited
            if (!entry.getKey().equals("BOOTCLASSPATH") &&
                !entry.getKey().equals("ANDROID_SOCKET_zygote") && 
                !entry.getKey().equals("TMPDIR") &&
                !entry.getKey().equals("ANDROID_BOOTLOGO") &&
                !entry.getKey().equals("EXTERNAL_STORAGE") &&
                !entry.getKey().equals("ANDROID_ASSETS") &&
                !entry.getKey().equals("PATH") &&
                !entry.getKey().equals("TERMINFO") &&
                !entry.getKey().equals("LD_LIBRARY_PATH") &&
                !entry.getKey().equals("ANDROID_DATA") &&
                !entry.getKey().equals("ANDROID_PROPERTY_WORKSPACE") &&
                !entry.getKey().equals("ANDROID_ROOT")) {
                envstr.append(" --es env" + c + " " + entry.getKey() + "=" 
                              + entry.getValue());
                c++;
            }
        }
        return envstr.toString();        
    }

    public void doRestart() {
        try {
            String action = "org.mozilla.gecko.restart" + getAppName();
            String amCmd = "/system/bin/am broadcast -a " + action + getEnvString() + " -n org.mozilla." + getAppName() + "/org.mozilla." + getAppName() + ".Restarter";
            Log.i("GeckoAppJava", amCmd);
            Runtime.getRuntime().exec(amCmd);
        } catch (Exception e) {
            Log.i("GeckoAppJava", e.toString());
        }
        System.exit(0);
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        GeckoAppShell.handleNotification(action, alertName, alertCookie);
    }

    private void checkAndLaunchUpdate() {
        Log.i("GeckoAppJava", "Checking for an update");

        int statusCode = 8; // UNEXPECTED_ERROR

        String updateDir = "/data/data/org.mozilla." + getAppName() + "/updates/0/";
        File updateFile = new File(updateDir + "update.apk");

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

        File statusFile = new File(updateDir + "update.status");
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
}
