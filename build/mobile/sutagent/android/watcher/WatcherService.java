/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Android SUTAgent code.
 *
 * The Initial Developer of the Original Code is
 * Bob Moss.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bob Moss <bmoss@mozilla.com>
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
package com.mozilla.watcher;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.BatteryManager;
import android.os.Debug;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.RemoteException;
import android.provider.Settings;
import android.util.Log;
import android.view.Gravity;
import android.widget.Toast;

public class WatcherService extends Service
{
    String sErrorPrefix = "##Installer Error## ";
    String currentDir = "/";
    String sPingTarget = "";
    long    lDelay = 60000;
    long    lPeriod = 300000;
    int        nMaxStrikes = 3;
    Process    pProc;
    Context myContext = null;
    Timer myTimer = null;
    private PowerManager.WakeLock pwl = null;
    public static final int NOTIFICATION_ID = 1964;
    boolean bInstalling = false;

    @SuppressWarnings("unchecked")
    private static final Class<?>[] mSetForegroundSignature = new Class[] {
    boolean.class};
    @SuppressWarnings("unchecked")
    private static final Class[] mStartForegroundSignature = new Class[] {
        int.class, Notification.class};
    @SuppressWarnings("unchecked")
    private static final Class[] mStopForegroundSignature = new Class[] {
        boolean.class};

    private NotificationManager mNM;
    private Method mSetForeground;
    private Method mStartForeground;
    private Method mStopForeground;
    private Object[] mSetForegroundArgs = new Object[1];
    private Object[] mStartForegroundArgs = new Object[2];
    private Object[] mStopForegroundArgs = new Object[1];


    private IWatcherService.Stub stub = new IWatcherService.Stub() {
        public int UpdateApplication(String sAppName, String sFileName, String sOutFile, int bReboot) throws RemoteException
            {
            return UpdtApp(sAppName, sFileName, sOutFile, bReboot);
            }
    };

    @Override
    public IBinder onBind(Intent arg0) {
        return stub;
    }

    @Override
    public void onCreate()
        {
        super.onCreate();

        myContext = this;

        getKeyGuardAndWakeLock();

        File dir = getFilesDir();
        File iniFile = new File(dir, "watcher.ini");
        String sIniFile = iniFile.getAbsolutePath();
        String sHold = "";

        this.sPingTarget = GetIniData("watcher", "PingTarget", sIniFile, "www.mozilla.org");
        sHold = GetIniData("watcher", "delay", sIniFile, "60000");
        this.lDelay = Long.parseLong(sHold.trim());
        sHold = GetIniData("watcher", "period", sIniFile,"300000");
        this.lPeriod = Long.parseLong(sHold.trim());
        sHold = GetIniData("watcher", "strikes", sIniFile,"3");
        this.nMaxStrikes = Integer.parseInt(sHold.trim());

        sHold = GetIniData("watcher", "stayon", sIniFile,"0");
        int nStayOn = Integer.parseInt(sHold.trim());
        
        try {
            if (nStayOn != 0) {
                if (!Settings.System.putInt(getContentResolver(), Settings.System.STAY_ON_WHILE_PLUGGED_IN, BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB)) {
                    doToast("Screen couldn't be set to Always On [stay on while plugged in]");
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            String sExcept = e.getMessage();
            doToast("Screen couldn't be set to Always On [exception " + sExcept + "]");
        }

        doToast("WatcherService created");
        }

    public String GetIniData(String sSection, String sKey, String sFile, String sDefault)
        {
        String sRet = sDefault;
        String sComp = "";
        String sLine = "";
        boolean bFound = false;
        BufferedReader in = null;
        String    sTmpFileName = fixFileName(sFile);

        try {
            in = new BufferedReader(new FileReader(sTmpFileName));
            sComp = "[" + sSection + "]";
            while ((sLine = in.readLine()) != null)
                {
                if (sLine.equalsIgnoreCase(sComp))
                    {
                    bFound = true;
                    break;
                    }
                }

            if (bFound)
                {
                sComp = (sKey + " =").toLowerCase();
                while ((sLine = in.readLine()) != null)
                    {
                    if (sLine.toLowerCase().contains(sComp))
                        {
                        String [] temp = null;
                        temp = sLine.split("=");
                        if (temp != null)
                            {
                            if (temp.length > 1)
                                sRet = temp[1].trim();
                            }
                        break;
                        }
                    }
                }
            in.close();
            }
        catch (FileNotFoundException e)
            {
            sComp = e.toString();
            }
        catch (IOException e)
            {
            sComp = e.toString();
            }
        return (sRet);
        }

    private void handleCommand(Intent intent)
        {
        String sCmd = intent.getStringExtra("command");

//        Debug.waitForDebugger();

        if (sCmd != null)
            {
            if (sCmd.equalsIgnoreCase("updt"))
                {
                String sPkgName = intent.getStringExtra("pkgName");
                String sPkgFile = intent.getStringExtra("pkgFile");
                String sOutFile = intent.getStringExtra("outFile");
                boolean bReboot = intent.getBooleanExtra("reboot", true);
                int nReboot = bReboot ? 1 : 0;
                   SendNotification("WatcherService updating " + sPkgName + " using file " + sPkgFile, "WatcherService updating " + sPkgName + " using file " + sPkgFile);

                   UpdateApplication worker = new UpdateApplication(sPkgName, sPkgFile, sOutFile, nReboot);
                }
            else if (sCmd.equalsIgnoreCase("start"))
                {
                doToast("WatcherService started");
                myTimer = new Timer();
                myTimer.scheduleAtFixedRate(new MyTime(), lDelay, lPeriod);
                }
            else
                {
                doToast("WatcherService unknown command");
                }
            }
        else
            doToast("WatcherService created");
        }


    @Override
    public void onStart(Intent intent, int startId) {
        handleCommand(intent);
        return;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        handleCommand(intent);
        return START_STICKY;
        }

    @Override
    public void onDestroy() {
        super.onDestroy();
        doToast("WatcherService destroyed");
        if (pwl != null)
            pwl.release();
        stopForegroundCompat(R.string.foreground_service_started);
    }

    protected void getKeyGuardAndWakeLock()
        {
        // Fire off a thread to do some work that we shouldn't do directly in the UI thread
        Thread t = new Thread() {
            public void run() {
                // Keep phone from locking or remove lock on screen
                KeyguardManager km = (KeyguardManager)getSystemService(Context.KEYGUARD_SERVICE);
                if (km != null)
                    {
                    KeyguardManager.KeyguardLock kl = km.newKeyguardLock("watcher");
                    if (kl != null)
                        kl.disableKeyguard();
                    }

                // No sleeping on the job
                PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                if (pm != null)
                    {
                    pwl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, "watcher");
                    if (pwl != null)
                        pwl.acquire();
                    }

                mNM = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
                try {
                    mStartForeground = getClass().getMethod("startForeground", mStartForegroundSignature);
                    mStopForeground = getClass().getMethod("stopForeground", mStopForegroundSignature);
                    }
                catch (NoSuchMethodException e)
                    {
                    // Running on an older platform.
                    mStartForeground = mStopForeground = null;
                    }
                try {
                    mSetForeground = getClass().getMethod("setForeground", mSetForegroundSignature);
                    }
                catch (NoSuchMethodException e) {
                    mSetForeground = null;
                    }
                Notification notification = new Notification();
                startForegroundCompat(R.string.foreground_service_started, notification);
                }
            };
        t.start();
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
                Log.w("ApiDemos", "Unable to invoke startForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.w("ApiDemos", "Unable to invoke startForeground", e);
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
                Log.w("ApiDemos", "Unable to invoke stopForeground", e);
            } catch (IllegalAccessException e) {
                // Should not happen.
                Log.w("ApiDemos", "Unable to invoke stopForeground", e);
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

    public void doToast(String sMsg)
        {
        Toast toast = Toast.makeText(this, sMsg, Toast.LENGTH_LONG);
        toast.setGravity(Gravity.TOP|Gravity.CENTER_HORIZONTAL, 0, 100);
        toast.show();
        }

    public void CheckMem() {
           System.gc();
        long lFreeMemory = Runtime.getRuntime().freeMemory();
        long lTotMemory = Runtime.getRuntime().totalMemory();
        long lMaxMemory = Runtime.getRuntime().maxMemory();

        SendNotification("Memory Check", "Free: " + lFreeMemory + "Total: " + lTotMemory + "Max: " + lMaxMemory);
    }

    public int UpdtApp(String sPkgName, String sPkgFileName, String sOutFile, int bReboot)
        {
        int nRet = 1;
        int lcv = 0;
        String sRet = "";

//        Debug.waitForDebugger();

        FileOutputStream f = null;

           try {
               SendNotification("Killing " + sPkgName, "Step 1: Kill " + sPkgName + " if running");
            while (!IsProcessDead(sPkgName) && (lcv < 5)) {
                if (KillProcess(sPkgName, null).startsWith("Successfully"))
                    break;
                else
                    lcv++;
                   Thread.sleep(2000);
                   }

               CheckMem();

            if ((sOutFile != null) && (sOutFile.length() > 0)) {
                File outFile = new File(sOutFile);
                if (outFile.exists() && outFile.canWrite()) {
                    f = new FileOutputStream(outFile, true);
                } else {
                       SendNotification("File not found or cannot write to " + sOutFile, "File not found or cannot write to " + sOutFile);
                }
            }
        } catch (InterruptedException e) {
               e.printStackTrace();
        } catch (FileNotFoundException e) {
               SendNotification("File not found " + sOutFile, "Couldn't open " + sOutFile + " " + e.getLocalizedMessage());
            e.printStackTrace();
           } catch (SecurityException e) {
               SendNotification("Security excepetion for " + sOutFile, "Exception message " + e.getLocalizedMessage());
            e.printStackTrace();
           }

        if ((sPkgName != null) && (sPkgName.length() > 0))
            {
               SendNotification("Uninstalling " + sPkgName, "Step 2: Uninstall " + sPkgName);
            sRet = UnInstallApp(sPkgName, null);
               CheckMem();
            if ((sRet.length() > 0) && (f != null))
                {
                try {
                    f.write(sRet.getBytes());
                    f.flush();
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    }
                }
            }

        if ((sPkgFileName != null) && (sPkgFileName.length() > 0))
            {
               SendNotification("Installing " + sPkgFileName, "Step 3: Install " + sPkgFileName);
            sRet = InstallApp(sPkgFileName, null);
               SendNotification("Installed " + sPkgFileName, "" + sRet);
               CheckMem();
            if ((sRet.length() > 0) && (f != null))
                {
                try {
                    f.write(sRet.getBytes());
                    f.flush();
                    f.close();
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    }
                }
            }

        if (bReboot > 0)
            RunReboot(null);

        return(nRet);
        }

    public boolean GetProcessInfo(String sProcName)
        {
        boolean bRet = false;
        ActivityManager aMgr = (ActivityManager) getApplicationContext().getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
        int    nProcs = 0;
        int lcv = 0;
        String strProcName = "";

        if (lProcesses != null)
            nProcs = lProcesses.size();

        for (lcv = 0; lcv < nProcs; lcv++)
            {
            strProcName = lProcesses.get(lcv).processName;
            if (strProcName.contains(sProcName))
                {
                bRet = true;
                }
            }

        return (bRet);
        }

    public String RunReboot(OutputStream out)
        {
        String sRet = "";
        String [] theArgs = new String [3];

        theArgs[0] = "su";
        theArgs[1] = "-c";
        theArgs[2] = "reboot";

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);
            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.join(10000);
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            e.printStackTrace();
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            }

        return (sRet);
        }

    public String KillProcess(String sProcName, OutputStream out)
        {
        String [] theArgs = new String [3];

        theArgs[0] = "su";
        theArgs[1] = "-c";
        theArgs[2] = "kill";

        String sRet = sErrorPrefix + "Unable to kill " + sProcName + "\n";
        ActivityManager aMgr = (ActivityManager) getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
        int lcv = 0;
        String strProcName = "";
        int    nPID = 0;
        int nProcs = 0;

        if (lProcesses != null)
            nProcs = lProcesses.size();

        for (lcv = 0; lcv < nProcs; lcv++)
            {
            if (lProcesses.get(lcv).processName.contains(sProcName))
                {
                strProcName = lProcesses.get(lcv).processName;
                nPID = lProcesses.get(lcv).pid;
                sRet = sErrorPrefix + "Failed to kill " + nPID + " " + strProcName + "\n";

                theArgs[2] += " " + nPID;

                try
                    {
                    pProc = Runtime.getRuntime().exec(theArgs);
                    RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
                    outThrd.start();
                    outThrd.join(5000);
                    }
                catch (IOException e)
                    {
                    sRet = e.getMessage();
                    e.printStackTrace();
                    }
                catch (InterruptedException e)
                    {
                    e.printStackTrace();
                    }

                // Give the messages a chance to be processed
                try {
                    Thread.sleep(2000);
                    }
                catch (InterruptedException e)
                    {
                    e.printStackTrace();
                    }
                break;
                }
            }

        if (nPID > 0)
            {
            sRet = "Successfully killed " + nPID + " " + strProcName + "\n";
            lProcesses = aMgr.getRunningAppProcesses();
            nProcs = 0;
            if (lProcesses != null)
                nProcs = lProcesses.size();
            for (lcv = 0; lcv < nProcs; lcv++)
                {
                if (lProcesses.get(lcv).processName.contains(sProcName))
                    {
                    sRet = sErrorPrefix + "Unable to kill " + nPID + " " + strProcName + "\n";
                    break;
                    }
                }
            }

        return (sRet);
        }

    public String GetAppRoot(String AppName)
        {
        String sRet = sErrorPrefix + " internal error [no context]";
        Context ctx = getApplicationContext();

        if (ctx != null)
            {
            try {
                Context appCtx = ctx.createPackageContext(AppName, 0);
                ContextWrapper appCtxW = new ContextWrapper(appCtx);
                sRet = appCtxW.getPackageResourcePath();
                appCtxW = null;
                appCtx = null;
                ctx = null;
                System.gc();
                }
            catch (NameNotFoundException e)
                {
                e.printStackTrace();
                }
            }
        return(sRet);
        }

    public boolean IsProcessDead(String sProcName)
        {
        boolean bRet = true;
        ActivityManager aMgr = (ActivityManager) getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses(); //    .getProcessesInErrorState();
        int lcv = 0;

        if (lProcesses != null)
            {
            for (lcv = 0; lcv < lProcesses.size(); lcv++)
                {
                if (lProcesses.get(lcv).processName.contentEquals(sProcName))
                    {
                    bRet = false;
                    break;
                    }
                }
            }

        return (bRet);
        }

    public String fixFileName(String fileName)
        {
        String    sRet = "";
        String    sTmpFileName = "";

        sRet = fileName.replace('\\', '/');

        if (sRet.startsWith("/"))
            sTmpFileName = sRet;
        else
            sTmpFileName = currentDir + "/" + sRet;

        sRet = sTmpFileName.replace('\\', '/');
        sTmpFileName = sRet;
        sRet = sTmpFileName.replace("//", "/");

        return(sRet);
        }

    public String GetTmpDir()
        {
        String     sRet = "";
        Context ctx = getApplicationContext();
        File dir = ctx.getFilesDir();
        ctx = null;
        try {
            sRet = dir.getCanonicalPath();
            }
        catch (IOException e)
            {
            e.printStackTrace();
            }
        return(sRet);
        }

    public String UnInstallApp(String sApp, OutputStream out)
        {
        String sRet = "";
        String [] theArgs = new String [3];

        theArgs[0] = "su";
        theArgs[1] = "-c";
        theArgs[2] = "pm uninstall " + sApp + ";exit";

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);

            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.join(60000);
            int nRet = pProc.exitValue();
            sRet = "\nuninst complete [" + nRet + "]";
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            e.printStackTrace();
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            }

        return (sRet);
        }

    public String InstallApp(String sApp, OutputStream out)
        {
        String sRet = "";
        String sHold = "";
        String [] theArgs = new String [3];

        theArgs[0] = "su";
        theArgs[1] = "-c";
        theArgs[2] = "pm install -r " + sApp + ";exit";

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);

            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.join(180000);
            int nRet = pProc.exitValue();
            sRet += "\ninstall complete [" + nRet + "]";
            sHold = outThrd.strOutput;
            sRet += "\nSuccess";
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            e.printStackTrace();
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            }

        return (sRet);
        }

    private String SendPing(String sIPAddr)
        {
        Process    pProc;
        String sRet = "";
        String [] theArgs = new String [4];
        boolean bStillRunning = true;
        int    nBytesOut = 0;
        int nBytesErr = 0;
        int nBytesRead = 0;
        byte[] buffer = new byte[1024];

        theArgs[0] = "ping";
        theArgs[1] = "-c";
        theArgs[2] = "3";
        theArgs[3] = sIPAddr;

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);

            InputStream sutOut = pProc.getInputStream();
            InputStream sutErr = pProc.getErrorStream();

            while (bStillRunning)
                {
                try
                    {
                    if ((nBytesOut = sutOut.available()) > 0)
                        {
                        if (nBytesOut > buffer.length)
                            {
                            buffer = null;
                            System.gc();
                            buffer = new byte[nBytesOut];
                            }
                        nBytesRead = sutOut.read(buffer, 0, nBytesOut);
                        if (nBytesRead == -1)
                            bStillRunning = false;
                        else
                            {
                            String sRep = new String(buffer,0,nBytesRead).replace("\n", "\r\n");
                            sRet += sRep;
                            sRep = null;
                            }
                        }

                    if ((nBytesErr = sutErr.available()) > 0)
                        {
                        if (nBytesErr > buffer.length)
                            {
                            buffer = null;
                            System.gc();
                            buffer = new byte[nBytesErr];
                            }
                        nBytesRead = sutErr.read(buffer, 0, nBytesErr);
                        if (nBytesRead == -1)
                            bStillRunning = false;
                        else
                            {
                            String sRep = new String(buffer,0,nBytesRead).replace("\n", "\r\n");
                            sRet += sRep;
                            sRep = null;
                            }
                        }

                    bStillRunning = (IsProcRunning(pProc) || (sutOut.available() > 0) || (sutErr.available() > 0));
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    }

                if ((bStillRunning == true) && (nBytesErr == 0) && (nBytesOut == 0))
                    {
                    try {
                        Thread.sleep(2000);
                        }
                    catch (InterruptedException e) {
                        e.printStackTrace();
                        }
                    }
                }

            pProc.destroy();
            pProc = null;
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            e.printStackTrace();
            }

        return (sRet);
        }

    private boolean IsProcRunning(Process pProc)
        {
        boolean bRet = false;
        @SuppressWarnings("unused")
        int nExitCode = 0;

        try
            {
            nExitCode = pProc.exitValue();
            }
        catch (IllegalThreadStateException z)
            {
            bRet = true;
            }
        catch (Exception e)
            {
            e.printStackTrace();
            }

        return(bRet);
        }

    private class UpdateApplication implements Runnable {
        Thread    runner;
        String    msPkgName = "";
        String    msPkgFileName = "";
        String    msOutFile = "";
        int        mbReboot = 0;

        public UpdateApplication(String sPkgName, String sPkgFileName, String sOutFile, int bReboot) {
            runner = new Thread(this);
            msPkgName = sPkgName;
            msPkgFileName = sPkgFileName;
            msOutFile = sOutFile;
            mbReboot = bReboot;
            runner.start();
        }

        public void run() {
               bInstalling = true;
            UpdtApp(msPkgName, msPkgFileName, msOutFile, mbReboot);
               bInstalling = false;
        }
    }

    private class MyTime extends TimerTask
        {
        int    nStrikes = 0;

        public MyTime()
            {
            }

        @Override
        public void run()
            {
            if (bInstalling)
                return;

            // See if the network is up, if not after three failures reboot
            String sRet = SendPing(sPingTarget);
            if (!sRet.contains("3 received"))
                {
                if (nMaxStrikes > 0)
                    {
                    if (++nStrikes >= nMaxStrikes)
                        RunReboot(null);
                    }
                }
            else
                {
                nStrikes = 0;
                }
            sRet = null;

            String sProgramName = "com.mozilla.SUTAgentAndroid";
            PackageManager pm = myContext.getPackageManager();

//            Debug.waitForDebugger();

            if (!GetProcessInfo(sProgramName))
                {
                Intent agentIntent = new Intent();
                agentIntent.setPackage(sProgramName);
                agentIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                agentIntent.setAction(Intent.ACTION_MAIN);
                try {
                    PackageInfo pi = pm.getPackageInfo(sProgramName, PackageManager.GET_ACTIVITIES | PackageManager.GET_INTENT_FILTERS);
                    ActivityInfo [] ai = pi.activities;
                    for (int i = 0; i < ai.length; i++)
                        {
                        ActivityInfo a = ai[i];
                        if (a.name.length() > 0)
                            {
                            agentIntent.setClassName(a.packageName, a.name);
                            break;
                            }
                        }
                    }
                catch (NameNotFoundException e)
                    {
                    e.printStackTrace();
                    }
                try
                    {
                    myContext.startActivity(agentIntent);
                    }
                catch(ActivityNotFoundException anf)
                    {
                    anf.printStackTrace();
                    }
                }
            }
        }

    private void SendNotification(String tickerText, String expandedText) {
        NotificationManager notificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        int icon = R.drawable.ateamlogo;
        long when = System.currentTimeMillis();

        Notification notification = new Notification(icon, tickerText, when);

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        notification.defaults |= Notification.DEFAULT_SOUND;
//        notification.defaults |= Notification.DEFAULT_VIBRATE;
        notification.defaults |= Notification.DEFAULT_LIGHTS;

        Context context = getApplicationContext();

        // Intent to launch an activity when the extended text is clicked
        Intent intent = new Intent(this, WatcherService.class);
        PendingIntent launchIntent = PendingIntent.getActivity(context, 0, intent, 0);

        notification.setLatestEventInfo(context, tickerText, expandedText, launchIntent);

        notificationManager.notify(NOTIFICATION_ID, notification);
    }

    private void CancelNotification() {
        NotificationManager notificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(NOTIFICATION_ID);
    }

}
