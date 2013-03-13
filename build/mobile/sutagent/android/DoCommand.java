/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.TimeZone;
import java.util.zip.Adler32;
import java.util.zip.CheckedInputStream;
import java.util.zip.CheckedOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.apache.commons.net.ftp.FTP;
import org.apache.commons.net.ftp.FTPClient;
import org.apache.commons.net.ftp.FTPFile;
import org.apache.commons.net.ftp.FTPReply;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;

import com.mozilla.SUTAgentAndroid.R;
import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Debug;
import android.os.Environment;
import android.os.StatFs;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

public class DoCommand {

    String lineSep = System.getProperty("line.separator");
    Process    pProc;
    OutputStream sutIn;
    InputStream    sutErr;
    InputStream    sutOut;
    AlertLooperThread alrt = null;
    ContextWrapper    contextWrapper = null;

    String    currentDir = "/";
    String    sErrorPrefix = "##AGENT-WARNING## ";
    boolean bTraceOn = false;

    String ffxProvider = "org.mozilla.ffxcp";
    String fenProvider = "org.mozilla.fencp";

    private final String prgVersion = "SUTAgentAndroid Version 1.16";

    public enum Command
        {
        RUN ("run"),
        EXEC ("exec"),
        EXECSU ("execsu"),
        EXECCWD ("execcwd"),
        EXECCWDSU ("execcwdsu"),
        ENVRUN ("envrun"),
        KILL ("kill"),
        PS ("ps"),
        DEVINFO ("info"),
        OS ("os"),
        ID ("id"),
        UPTIME ("uptime"),
        UPTIMEMILLIS ("uptimemillis"),
        SETTIME ("settime"),
        SYSTIME ("systime"),
        SCREEN ("screen"),
        ROTATION ("rotation"),
        MEMORY ("memory"),
        POWER ("power"),
        PROCESS ("process"),
        SUTUSERINFO ("sutuserinfo"),
        TEMPERATURE ("temperature"),
        GETAPPROOT ("getapproot"),
        TESTROOT ("testroot"),
        ALRT ("alrt"),
        DISK ("disk"),
        CP ("cp"),
        TIME ("time"),
        HASH ("hash"),
        CD ("cd"),
        CAT ("cat"),
        CWD ("cwd"),
        MV ("mv"),
        PUSH ("push"),
        PULL ("pull"),
        RM ("rm"),
        PRUNE ("rmdr"),
        MKDR ("mkdr"),
        DIRWRITABLE ("dirw"),
        ISDIR ("isdir"),
        DEAD ("dead"),
        MEMS ("mems"),
        LS ("ls"),
        TMPD ("tmpd"),
        PING ("ping"),
        REBT ("rebt"),
        UNZP ("unzp"),
        ZIP ("zip"),
        CLOK ("clok"),
        STAT ("stat"),
        QUIT ("quit"),
        EXIT ("exit"),
        HELP ("help"),
        FTPG ("ftpg"),
        FTPP ("ftpp"),
        INST ("inst"),
        UPDT ("updt"),
        UNINST ("uninst"),
        UNINSTALL ("uninstall"),
        TEST ("test"),
        DBG ("dbg"),
        TRACE ("trace"),
        VER ("ver"),
        TZGET ("tzget"),
        TZSET ("tzset"),
        ADB ("adb"),
        CHMOD ("chmod"),
        UNKNOWN ("unknown");

        private final String theCmd;

        Command(String theCmd) { this.theCmd = theCmd; }

        public String theCmd() {return theCmd;}

        public static Command getCmd(String sCmd)
            {
            Command retCmd = UNKNOWN;
            for (Command cmd : Command.values())
                {
                if (cmd.theCmd().equalsIgnoreCase(sCmd))
                    {
                    retCmd = cmd;
                    break;
                    }
                }
            return (retCmd);
            }
        }

    public DoCommand(ContextWrapper service)
        {
        this.contextWrapper = service;
        }

    public String processCommand(String theCmdLine, PrintWriter out, BufferedInputStream in, OutputStream cmdOut)
        {
        String     strReturn = "";
        Command    cCmd = null;
        Command cSubCmd = null;

        if (bTraceOn)
            ((ASMozStub)this.contextWrapper).SendToDataChannel(theCmdLine);

        String [] Argv = parseCmdLine2(theCmdLine);

        int Argc = Argv.length;

        cCmd = Command.getCmd(Argv[0]);

        switch(cCmd)
            {
            case TRACE:
                if (Argc == 2)
                    bTraceOn = (Argv[1].equalsIgnoreCase("on") ? true : false);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for trace command!";
                break;

            case VER:
                strReturn = prgVersion;
                break;

            case CLOK:
                strReturn = GetClok();
                break;

            case TZGET:
                strReturn = GetTimeZone();
                break;

            case TZSET:
                if (Argc == 2)
                    strReturn = SetTimeZone(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for settz command!";
                break;

            case UPDT:
                if (Argc >= 2)
                    strReturn = StrtUpdtOMatic(Argv[1], Argv[2], (Argc > 3 ? Argv[3] : null), (Argc > 4 ? Argv[4] : null));
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for updt command!";
                break;

            case SETTIME:
                strReturn = SetSystemTime(Argv[1], (Argc > 2 ? Argv[2] : null), cmdOut);
                break;

            case CWD:
                try {
                    strReturn = new java.io.File(currentDir).getCanonicalPath();
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    }
                break;

            case CD:
                if (Argc == 2)
                    strReturn = changeDir(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for cd command!";
                break;

            case LS:
                strReturn = PrintDir(((Argc > 1) ? Argv[1] : currentDir));
                break;

            case GETAPPROOT:
                if (Argc == 2)
                    strReturn = GetAppRoot(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for getapproot command!";
                break;

            case ISDIR:
                if (Argc == 2)
                    strReturn = isDirectory(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for isdir command!";
                break;

            case TESTROOT:
                strReturn = GetTestRoot();
                break;

            case DEAD:
                if (Argc == 2)
                    strReturn = (IsProcessDead(Argv[1]) ? (Argv[1] + " is hung or unresponsive") : (Argv[1] + " is ok"));
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for dead command!";
                break;

            case PS:
                strReturn = GetProcessInfo();
                break;

            case PULL:
                if (Argc >= 2) {
                    long lOff = 0;
                    long lLen = -1;
                    if (Argc > 2) {
                        try {
                            lOff = Long.parseLong(Argv[2].trim());
                        } catch (NumberFormatException nfe) {
                            lOff = 0;
                            System.out.println("NumberFormatException: " + nfe.getMessage());
                        }
                    }
                    if (Argc == 4) {
                        try {
                            lLen = Long.parseLong(Argv[3].trim());
                        } catch (NumberFormatException nfe) {
                            lLen = -1;
                            System.out.println("NumberFormatException: " + nfe.getMessage());
                        }
                    }
                    strReturn = Pull(Argv[1], lOff, lLen, cmdOut);
                } else {
                    strReturn = sErrorPrefix + "Wrong number of arguments for pull command!";
                }
                break;

            case PUSH:
                if (Argc == 3)
                    {
                    long lArg = 0;
                    try
                        {
                        lArg = Long.parseLong(Argv[2].trim());
                        }
                    catch (NumberFormatException nfe)
                        {
                        System.out.println("NumberFormatException: " + nfe.getMessage());
                        }

                    strReturn = Push(Argv[1], in, lArg);
                    }
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for push command!";
                break;

            case INST:
                if (Argc >= 2)
                    strReturn = InstallApp(Argv[1], cmdOut);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for inst command!";
                break;

            case UNINST:
                if (Argc >= 2)
                    strReturn = UnInstallApp(Argv[1], cmdOut, true);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for uninst command!";
                break;

            case UNINSTALL:
                if (Argc >= 2)
                    strReturn = UnInstallApp(Argv[1], cmdOut, false);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for uninstall command!";
                break;

            case ALRT:
                if (Argc > 1)
                    {
                    if (Argv[1].contentEquals("on"))
                        {
                        String sTitle = "Agent Alert";
                        String sMsg = "The Agent Alert System has been activated!";
                        if (Argc == 3) {
                            sTitle = Argv[2];
                            sMsg = "";
                        } else if (Argc == 4) {
                            sTitle = Argv[2];
                            sMsg = Argv[3];
                        }
                        StartAlert(sTitle, sMsg);
                        }
                    else
                        {
                        StopAlert();
                        }
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for alrt command!";
                    }
                break;

            case REBT:
                if (Argc >= 1)
                    strReturn = RunReboot(cmdOut, (Argc > 1 ? Argv[1] : null), (Argc > 2 ? Argv[2] : null));
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for rebt command!";
//                RunReboot(cmdOut);
                break;

            case TMPD:
                strReturn = GetTmpDir();
                break;

            case DEVINFO:
                if (Argc == 1)
                    {
                    strReturn += SUTAgentAndroid.sUniqueID;
                    strReturn += "\n";
                    strReturn += GetOSInfo();
                    strReturn += "\n";
                    strReturn += GetSystemTime();
                    strReturn += "\n";
                    strReturn += GetUptime();
                    strReturn += "\n";
                    strReturn += GetUptimeMillis();
                    strReturn += "\n";
                    strReturn += GetScreenInfo();
                    strReturn += "\n";
                    strReturn += GetRotationInfo();
                    strReturn += "\n";
                    strReturn += GetMemoryInfo();
                    strReturn += "\n";
                    strReturn += GetPowerInfo();
                    strReturn += "\n";
                    strReturn += GetTemperatureInfo();
                    strReturn += "\n";
                    strReturn += GetProcessInfo();
                    strReturn += "\n";
                    strReturn += GetSutUserInfo();
                    }
                else
                    {
                    cSubCmd = Command.getCmd(Argv[1]);
                    switch(cSubCmd)
                        {
                        case ID:
                            strReturn = SUTAgentAndroid.sUniqueID;
                            break;

                        case SCREEN:
                            strReturn = GetScreenInfo();
                            break;

                        case ROTATION:
                            strReturn = GetRotationInfo();
                            break;

                        case PROCESS:
                            strReturn = GetProcessInfo();
                            break;

                        case OS:
                            strReturn = GetOSInfo();
                            break;

                        case SYSTIME:
                            strReturn = GetSystemTime();
                            break;

                        case UPTIME:
                            strReturn = GetUptime();
                            break;

                        case UPTIMEMILLIS:
                            strReturn = GetUptimeMillis();
                            break;

                        case MEMORY:
                            strReturn = GetMemoryInfo();
                            break;

                        case POWER:
                            strReturn += GetPowerInfo();
                            break;

                        case SUTUSERINFO:
                            strReturn += GetSutUserInfo();
                            break;

                        case TEMPERATURE:
                            strReturn += GetTemperatureInfo();
                            break;

                        default:
                            break;
                        }
                    }
                break;

            case STAT:
                if (Argc == 2)
                    strReturn = StatProcess(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for ping command!";
                break;

            case PING:
                if (Argc == 2)
                    strReturn = SendPing(Argv[1], cmdOut);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for ping command!";
                break;

            case HASH:
                if (Argc == 2)
                    strReturn = HashFile(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for hash command!";
                break;

            case PRUNE:
                if (Argc == 2)
                    strReturn = PruneDir(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for prune command!";
                break;

            case FTPG:
                if (Argc == 4)
                    strReturn = FTPGetFile(Argv[1], Argv[2], Argv[3], cmdOut);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for ftpg command!";
                break;

            case CAT:
                if (Argc == 2)
                    strReturn = Cat(Argv[1], cmdOut);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for cat command!";
                break;

            case DIRWRITABLE:
                if (Argc == 2)
                    strReturn = IsDirWritable(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for dirwritable command!";
                break;

            case TIME:
                if (Argc == 2)
                    strReturn = PrintFileTimestamp(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for time command!";
                break;

            case MKDR:
                if (Argc == 2)
                    strReturn = MakeDir(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for mkdr command!";
                break;

            case RM:
                if (Argc == 2)
                    strReturn = RemoveFile(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for rm command!";
                break;

            case MV:
                if (Argc == 3)
                    strReturn = Move(Argv[1], Argv[2]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for mv command!";
                break;

            case CP:
                if (Argc == 3)
                    strReturn = CopyFile(Argv[1], Argv[2]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for cp command!";
                break;

            case QUIT:
            case EXIT:
                strReturn = Argv[0];
                break;

            case DBG:
                Debug.waitForDebugger();
                strReturn = "waitForDebugger on";
                break;

            case ADB:
                if (Argc == 2) {
                    if (Argv[1].contains("ip") || Argv[1].contains("usb")) {
                        strReturn = SetADB(Argv[1]);
                    } else {
                        strReturn = sErrorPrefix + "Unrecognized argument for adb command!";
                    }
                } else {
                    strReturn = sErrorPrefix + "Wrong number of arguments for adb command!";
                }
                break;

            case TEST:
                long lFreeMemory = Runtime.getRuntime().freeMemory();
                long lTotMemory = Runtime.getRuntime().totalMemory();
                long lMaxMemory = Runtime.getRuntime().maxMemory();


                if (lFreeMemory > 0) {
                    strReturn = "Max memory: " + lMaxMemory + "\nTotal Memory: " + lTotMemory + "\nFree memory: " + lFreeMemory;
                    break;
                }

                ContentResolver cr = contextWrapper.getContentResolver();
                Uri ffxFiles = null;

                if (Argv[1].contains("fennec")) {
                    ffxFiles = Uri.parse("content://" + fenProvider + "/dir");
                } else if (Argv[1].contains("firefox")) {
                    ffxFiles = Uri.parse("content://" + ffxProvider + "/dir");
                }

//                Uri ffxFiles = Uri.parse("content://org.mozilla.fencp/file");
                String[] columns = new String[] {
                        "_id",
                        "isdir",
                        "filename",
                        "length"
                    };
//                String[] columns = new String[] {
//                        "_id",
//                        "chunk"
//                     };
                Cursor myCursor = cr.query(    ffxFiles,
                                            columns,                         // Which columns to return
                                            (Argc > 1 ? Argv[1] : null),    // Which rows to return (all rows)
                                            null,                           // Selection arguments (none)
                                            null);                            // Put the results in ascending order by name
/*
                if (myCursor != null) {
                    int nRows = myCursor.getCount();
                    String [] colNames = myCursor.getColumnNames();
                    int    nID = 0;
                    int nBytesRecvd = 0;

                    for (int lcv = 0; lcv < nRows; lcv++) {
                        if  (myCursor.moveToPosition(lcv)) {
                            nID = myCursor.getInt(0);
                            byte [] buf = myCursor.getBlob(1);
                            if (buf != null) {
                                nBytesRecvd += buf.length;
                                strReturn += new String(buf);
                                buf = null;
                            }
                        }
                    }
                    strReturn += "[eof - " + nBytesRecvd + "]";
                    myCursor.close();
                }

*/
                if (myCursor != null)
                    {
                    int nRows = myCursor.getCount();
                    int    nID = 0;
                    String sFileName = "";
                    long lFileSize = 0;
                    boolean bIsDir = false;

                    for (int lcv = 0; lcv < nRows; lcv++)
                        {
                        if  (myCursor.moveToPosition(lcv))
                            {
                            nID = myCursor.getInt(0);
                            bIsDir = (myCursor.getInt(1) == 1 ? true : false);
                            sFileName = myCursor.getString(2);
                            lFileSize = myCursor.getLong(3);
                            strReturn += "" + nID + "\t" + (bIsDir ? "<dir> " : "      ") + sFileName + "\t" + lFileSize + "\n";
                            }
                        }
                    myCursor.close();
                    }
                break;

            case EXEC:
            case ENVRUN:
                if (Argc >= 2)
                    {
                    String [] theArgs = new String [Argc - 1];

                    for (int lcv = 1; lcv < Argc; lcv++)
                        {
                        theArgs[lcv - 1] = Argv[lcv];
                        }

                    strReturn = StartPrg2(theArgs, cmdOut, null, false);
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
                    }
                break;

            case EXECSU:
                if (Argc >= 2)
                    {
                    String [] theArgs = new String [Argc - 1];

                    for (int lcv = 1; lcv < Argc; lcv++)
                        {
                        theArgs[lcv - 1] = Argv[lcv];
                        }

                    strReturn = StartPrg2(theArgs, cmdOut, null, true);
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
                    }
                break;

            case EXECCWD:
                if (Argc >= 3)
                    {
                    String [] theArgs = new String [Argc - 2];

                    for (int lcv = 2; lcv < Argc; lcv++)
                        {
                        theArgs[lcv - 2] = Argv[lcv];
                        }

                    strReturn = StartPrg2(theArgs, cmdOut, Argv[1], false);
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
                    }
                break;

            case EXECCWDSU:
                if (Argc >= 3)
                    {
                    String [] theArgs = new String [Argc - 2];

                    for (int lcv = 2; lcv < Argc; lcv++)
                        {
                        theArgs[lcv - 2] = Argv[lcv];
                        }

                    strReturn = StartPrg2(theArgs, cmdOut, Argv[1], true);
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
                    }
                break;

            case RUN:
                if (Argc >= 2)
                    {
                    String [] theArgs = new String [Argc - 1];

                    for (int lcv = 1; lcv < Argc; lcv++)
                        {
                        theArgs[lcv - 1] = Argv[lcv];
                        }

                    if (Argv[1].contains("/") || Argv[1].contains("\\") || !Argv[1].contains("."))
                        strReturn = StartPrg(theArgs, cmdOut, false);
                    else
                        strReturn = StartJavaPrg(theArgs, null);
                    }
                else
                    {
                    strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
                    }
                break;

            case KILL:
                if (Argc == 2)
                    strReturn = KillProcess(Argv[1], cmdOut);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for kill command!";
                break;

            case DISK:
                strReturn = GetDiskInfo((Argc == 2 ? Argv[1] : "/"));
                break;

            case UNZP:
                strReturn = Unzip(Argv[1], (Argc == 3 ? Argv[2] : ""));
                break;

            case ZIP:
                strReturn = Zip(Argv[1], (Argc == 3 ? Argv[2] : ""));
                break;

            case CHMOD:
                if (Argc == 2)
                    strReturn = ChmodDir(Argv[1]);
                else
                    strReturn = sErrorPrefix + "Wrong number of arguments for chmod command!";
                break;

            case HELP:
                strReturn = PrintUsage();
                break;

            default:
                strReturn = sErrorPrefix + "[" + Argv[0] + "] command";
                if (Argc > 1)
                    {
                    strReturn += " with arg(s) =";
                    for (int lcv = 1; lcv < Argc; lcv++)
                        {
                        strReturn += " [" + Argv[lcv] + "]";
                        }
                    }
                strReturn += " is currently not implemented.";
                break;
            }

        return(strReturn);
        }

    private void SendNotification(String tickerText, String expandedText) {
        NotificationManager notificationManager = (NotificationManager)contextWrapper.getSystemService(Context.NOTIFICATION_SERVICE);
        int icon = R.drawable.ateamlogo;
        long when = System.currentTimeMillis();

        Notification notification = new Notification(icon, tickerText, when);

        notification.flags |= (Notification.FLAG_INSISTENT | Notification.FLAG_AUTO_CANCEL);
        notification.defaults |= Notification.DEFAULT_SOUND;
        notification.defaults |= Notification.DEFAULT_VIBRATE;
        notification.defaults |= Notification.DEFAULT_LIGHTS;

        Context context = contextWrapper.getApplicationContext();

        // Intent to launch an activity when the extended text is clicked
        Intent intent2 = new Intent(contextWrapper, SUTAgentAndroid.class);
        PendingIntent launchIntent = PendingIntent.getActivity(context, 0, intent2, 0);

        notification.setLatestEventInfo(context, tickerText, expandedText, launchIntent);

        notificationManager.notify(1959, notification);
    }

private void CancelNotification()
    {
    NotificationManager notificationManager = (NotificationManager)contextWrapper.getSystemService(Context.NOTIFICATION_SERVICE);
    notificationManager.cancel(1959);
    }

    public void StartAlert(String sTitle, String sMsg)
        {
        // start the alert message
        SendNotification(sTitle, sMsg);
        }

    public void StopAlert()
        {
        CancelNotification();
        }

    public String [] parseCmdLine2(String theCmdLine)
        {
        String    cmdString;
        String    workingString;
        String    workingString2;
        String    workingString3;
        List<String> lst = new ArrayList<String>();
        int nLength = 0;
        int nFirstSpace = -1;

        // Null cmd line
        if (theCmdLine == null)
            {
            String [] theArgs = new String [1];
            theArgs[0] = new String("");
            return(theArgs);
            }
        else
            {
            nLength = theCmdLine.length();
            nFirstSpace = theCmdLine.indexOf(' ');
            }

        if (nFirstSpace == -1)
            {
            String [] theArgs = new String [1];
            theArgs[0] = new String(theCmdLine);
            return(theArgs);
            }

        // Get the command
        cmdString = new String(theCmdLine.substring(0, nFirstSpace));
        lst.add(cmdString);

        // Jump past the command and trim
        workingString = (theCmdLine.substring(nFirstSpace + 1, nLength)).trim();

        while ((nLength = workingString.length()) > 0)
            {
            int nEnd = 0;
            int    nStart = 0;

            // if we have a quote
            if (workingString.startsWith("\"") || workingString.startsWith("'"))
                {
                char quoteChar = '"';
                if (workingString.startsWith("\'"))
                    quoteChar = '\'';

                // point to the first non quote char
                nStart = 1;
                // find the matching quote
                nEnd = workingString.indexOf(quoteChar, nStart);

                char prevChar;

                while(nEnd != -1)
                    {
                    // check to see if the quotation mark has been escaped
                    prevChar = workingString.charAt(nEnd - 1);
                    if (prevChar == '\\')
                        {
                        // if escaped, point past this quotation mark and find the next
                        nEnd++;
                        if (nEnd < nLength)
                            nEnd = workingString.indexOf(quoteChar, nEnd);
                        else
                            nEnd = -1;
                        }
                    else
                        break;
                    }

                // there isn't one
                if (nEnd == -1)
                    {
                    // point at the quote
                    nStart = 0;
                    // so find the next space
                    nEnd = workingString.indexOf(' ', nStart);
                    // there isn't one of those either
                    if (nEnd == -1)
                        nEnd = nLength;    // Just grab the rest of the cmdline
                    }
                }
            else // no quote so find the next space
                {
                nEnd = workingString.indexOf(' ', nStart);
                // there isn't one of those
                if (nEnd == -1)
                    nEnd = nLength;    // Just grab the rest of the cmdline
                }

            // get the substring
            workingString2 = workingString.substring(nStart, nEnd);

            // if we have escaped quotes, convert them into standard ones
            while (workingString2.contains("\\\"") || workingString2.contains("\\'"))
                {
                    workingString2 = workingString2.replace("\\\"", "\"");
                    workingString2 = workingString2.replace("\\'", "'");
                }

            // add it to the list
            lst.add(new String(workingString2));

            // if we are dealing with a quote
            if (nStart > 0)
                nEnd++; //  point past the end one

            // jump past the substring and trim it
            workingString = (workingString.substring(nEnd)).trim();
            }

        // ok we're done package up the results
        int nItems = lst.size();

        String [] theArgs = new String [nItems];

        for (int lcv = 0; lcv < nItems; lcv++)
            {
            theArgs[lcv] = lst.get(lcv);
            }

        return(theArgs);
        }

    public String [] parseCmdLine(String theCmdLine) {
        String    cmdString;
        String    workingString;
        String    workingString2;
        List<String> lst = new ArrayList<String>();
        int nLength = 0;
        int nFirstSpace = -1;

        // Null cmd line
        if (theCmdLine == null)
            {
            String [] theArgs = new String [1];
            theArgs[0] = new String("");
            return(theArgs);
            }
        else
            {
            nLength = theCmdLine.length();
            nFirstSpace = theCmdLine.indexOf(' ');
            }

        if (nFirstSpace == -1)
            {
            String [] theArgs = new String [1];
            theArgs[0] = new String(theCmdLine);
            return(theArgs);
            }

        // Get the command
        cmdString = new String(theCmdLine.substring(0, nFirstSpace));
        lst.add(cmdString);

        // Jump past the command and trim
        workingString = (theCmdLine.substring(nFirstSpace + 1, nLength)).trim();

        while ((nLength = workingString.length()) > 0)
            {
            int nEnd = 0;
            int    nStart = 0;

            // if we have a quote
            if (workingString.startsWith("\""))
                {
                // point to the first non quote char
                nStart = 1;
                // find the matching quote
                nEnd = workingString.indexOf('"', nStart);
                // there isn't one
                if (nEnd == -1)
                    {
                    // point at the quote
                    nStart = 0;
                    // so find the next space
                    nEnd = workingString.indexOf(' ', nStart);
                    // there isn't one of those either
                    if (nEnd == -1)
                        nEnd = nLength;    // Just grab the rest of the cmdline
                    }
                else
                    {
                    nStart = 0;
                    nEnd++;
                    }
                }
            else // no quote so find the next space
                {
                nEnd = workingString.indexOf(' ', nStart);

                // there isn't one of those
                if (nEnd == -1)
                    nEnd = nLength;    // Just grab the rest of the cmdline
                }

            // get the substring
            workingString2 = workingString.substring(nStart, nEnd);

            // add it to the list
            lst.add(new String(workingString2));

            // jump past the substring and trim it
            workingString = (workingString.substring(nEnd)).trim();
            }

        int nItems = lst.size();

        String [] theArgs = new String [nItems];

        for (int lcv = 0; lcv < nItems; lcv++)
            {
            theArgs[lcv] = lst.get(lcv);
            }

        return(theArgs);
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

    public String AddFilesToZip(ZipOutputStream out, String baseDir, String relDir)
    {
        final int             BUFFER     = 2048;
        String                sRet    = "";
        String                 curDir     = "";
        String                relFN    = "";
        BufferedInputStream origin = null;
        byte                 data[] = new byte[BUFFER];

        if (relDir.length() > 0)
            curDir = baseDir + "/" + relDir;
        else
            curDir = baseDir;

        File f = new File(curDir);

        if (f.isFile())
            {
            try {
                relFN = ((relDir.length() > 0) ? relDir + "/" + f.getName() : f.getName());
                System.out.println("Adding: "+relFN);
                sRet += "Adding: "+    relFN + lineSep;
                FileInputStream fi = new FileInputStream(curDir);
                origin = new BufferedInputStream(fi, BUFFER);
                ZipEntry entry = new ZipEntry(relFN);
                out.putNextEntry(entry);
                int count;
                while((count = origin.read(data, 0, BUFFER)) != -1)
                    {
                    out.write(data, 0, count);
                    }
                origin.close();
                }
            catch(Exception e)
                {
                e.printStackTrace();
                }

            return(sRet);
            }

        String    files[] = f.list();

        if (files != null)
            {
            try {
                for(int i = 0; i < files.length; i++)
                    {
                    f = new File(curDir + "/" + files[i]);
                    if (f.isDirectory())
                        {
                        if (relDir.length() > 0)
                            sRet += AddFilesToZip(out, baseDir, relDir + "/" + files[i]);
                        else
                            sRet += AddFilesToZip(out, baseDir, files[i]);
                        }
                    else
                        {
                        relFN = ((relDir.length() > 0) ? relDir + "/" + files[i] : files[i]);
                        System.out.println("Adding: "+relFN);
                        sRet += "Adding: "+    relFN + lineSep;
                        FileInputStream fi = new FileInputStream(curDir + "/" + files[i]);
                        origin = new BufferedInputStream(fi, BUFFER);
                        ZipEntry entry = new ZipEntry(relFN);
                        out.putNextEntry(entry);
                        int count;
                        while((count = origin.read(data, 0, BUFFER)) != -1)
                            {
                            out.write(data, 0, count);
                            }
                        origin.close();
                        }
                    }
                }
            catch(Exception e)
                {
                e.printStackTrace();
                }
            }

        return(sRet);
    }

    public String Zip(String zipFileName, String srcName)
        {
        String    fixedZipFileName = fixFileName(zipFileName);
        String    fixedSrcName = fixFileName(srcName);
        String sRet = "";

        try {
            FileOutputStream dest = new FileOutputStream(fixedZipFileName);
            CheckedOutputStream checksum = new CheckedOutputStream(dest, new Adler32());
            ZipOutputStream out = new ZipOutputStream(new BufferedOutputStream(checksum));
            out.setMethod(ZipOutputStream.DEFLATED);

            sRet += AddFilesToZip(out, fixedSrcName, "");

            out.close();
            System.out.println("checksum:                   "+checksum.getChecksum().getValue());
            sRet += "checksum:                   "+checksum.getChecksum().getValue();
            }
        catch(Exception e)
            {
            e.printStackTrace();
            }

        return(sRet);
    }

    public String Unzip(String zipFileName, String dstDirectory)
        {
        String     sRet = "";
        String    fixedZipFileName = fixFileName(zipFileName);
        String    fixedDstDirectory = fixFileName(dstDirectory);
        String    dstFileName = "";
        int        nNumExtracted = 0;
        boolean bRet = false;

        try {
            final int BUFFER = 2048;
            BufferedOutputStream dest = null;
            ZipFile zipFile = new ZipFile(fixedZipFileName);
            int nNumEntries = zipFile.size();
            zipFile.close();

            FileInputStream fis = new FileInputStream(fixedZipFileName);
            CheckedInputStream checksum = new CheckedInputStream(fis, new Adler32());
            ZipInputStream zis = new ZipInputStream(new BufferedInputStream(checksum));
            ZipEntry entry;

            byte [] data = new byte[BUFFER];

            while((entry = zis.getNextEntry()) != null)
                {
                System.out.println("Extracting: " + entry);
                int count;
                if (fixedDstDirectory.length() > 0)
                    dstFileName = fixedDstDirectory + entry.getName();
                else
                    dstFileName = entry.getName();

                String tmpDir = dstFileName.substring(0, dstFileName.lastIndexOf('/'));
                File tmpFile = new File(tmpDir);
                if (!tmpFile.exists())
                    {
                    bRet = tmpFile.mkdirs();
                    }
                else
                    bRet = true;

                if (bRet)
                    {
                    // if we aren't just creating a directory
                    if (dstFileName.lastIndexOf('/') != (dstFileName.length() - 1))
                        {
                        // write out the file
                        FileOutputStream fos = new FileOutputStream(dstFileName);
                        dest = new BufferedOutputStream(fos, BUFFER);
                        while ((count = zis.read(data, 0, BUFFER)) != -1)
                            {
                            dest.write(data, 0, count);
                            }
                        dest.flush();
                        dest.close();
                        dest = null;
                        fos.close();
                        fos = null;
                        }
                    nNumExtracted++;
                    }
                else
                    sRet += " - failed" + lineSep;
                }

            data = null;
            zis.close();
            System.out.println("Checksum:          "+checksum.getChecksum().getValue());
            sRet += "Checksum:          "+checksum.getChecksum().getValue();
            sRet += lineSep + nNumExtracted + " of " + nNumEntries + " successfully extracted";
            }
        catch(Exception e)
            {
            e.printStackTrace();
            }

        return(sRet);
        }

    public String StatProcess(String string)
        {
        String sRet = "";
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        int    [] nPids = new int [1];

        nPids[0] = Integer.parseInt(string);

        android.os.Debug.MemoryInfo[] mi = aMgr.getProcessMemoryInfo(nPids);

        sRet  = "Dalvik Private Dirty pages         " + mi[0].dalvikPrivateDirty     + " kb\n";
        sRet += "Dalvik Proportional Set Size       " + mi[0].dalvikPss              + " kb\n";
        sRet += "Dalvik Shared Dirty pages          " + mi[0].dalvikSharedDirty      + " kb\n\n";
        sRet += "Native Private Dirty pages heap    " + mi[0].nativePrivateDirty     + " kb\n";
        sRet += "Native Proportional Set Size heap  " + mi[0].nativePss              + " kb\n";
        sRet += "Native Shared Dirty pages heap     " + mi[0].nativeSharedDirty      + " kb\n\n";
        sRet += "Other Private Dirty pages          " + mi[0].otherPrivateDirty      + " kb\n";
        sRet += "Other Proportional Set Size        " + mi[0].otherPss               + " kb\n";
        sRet += "Other Shared Dirty pages           " + mi[0].otherSharedDirty       + " kb\n\n";
        sRet += "Total Private Dirty Memory         " + mi[0].getTotalPrivateDirty() + " kb\n";
        sRet += "Total Proportional Set Size Memory " + mi[0].getTotalPss()          + " kb\n";
        sRet += "Total Shared Dirty Memory          " + mi[0].getTotalSharedDirty()  + " kb";


        return(sRet);
        }

    public String GetTestRoot()
        {

        // According to all the docs this should work, but I keep getting an
        // exception when I attempt to create the file because I don't have
        // permission, although /data/local/tmp is supposed to be world
        // writeable/readable
        File tmpFile = new java.io.File("/data/local/tmp/tests");
        try{
            tmpFile.createNewFile();
        } catch (IOException e){
            Log.i("SUTAgentAndroid", "Caught exception creating file in /data/local/tmp: " + e.getMessage());
        }
   
        String state = Environment.getExternalStorageState();
        // Ensure sdcard is mounted and NOT read only
        if (state.equalsIgnoreCase(Environment.MEDIA_MOUNTED) &&
            (Environment.MEDIA_MOUNTED_READ_ONLY.compareTo(state) != 0))
            {
            return(Environment.getExternalStorageDirectory().getAbsolutePath());
            }
        if (tmpFile.exists()) 
            {
            return("/data/local");
            }
        Log.e("SUTAgentAndroid", "ERROR: Cannot access world writeable test root");

        return(null);
        }

    public String GetAppRoot(String AppName)
        {
        String sRet = sErrorPrefix + " internal error [no context]";
        Context ctx = contextWrapper.getApplicationContext();

        if (ctx != null)
            {
            try {
                Context appCtx = ctx.createPackageContext(AppName, 0);
                ContextWrapper appCtxW = new ContextWrapper(appCtx);
                sRet = appCtxW.getApplicationInfo().dataDir;
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

    public String isDirectory(String sDir)
        {
        String    sRet = sErrorPrefix + sDir + " does not exist";
        String    tmpDir    = fixFileName(sDir);
        int    nFiles = 0;

        if (tmpDir.contains("org.mozilla.fennec") || tmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (tmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");

            String[] columns = new String[] {
                    "_id",
                    "isdir",
                    "filename",
                    "length"
                };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,     // Which columns to return
                                        tmpDir,     // Which rows to return (all rows)
                                        null,       // Selection arguments (none)
                                        null);        // Order clause (none)
            if (myCursor != null) {
                nFiles = myCursor.getCount();

                // If no entries the dir is empty
                if (nFiles > 0) {
                    if  (myCursor.moveToPosition(0)) {
                        sRet = ((myCursor.getLong(myCursor.getColumnIndex("isdir")) == 1) ? "TRUE" : "FALSE");
                    }
                }
                myCursor.close();
            }
        } else {
            File tmpFile = new java.io.File(tmpDir);

            if (tmpFile.exists()) {
                sRet = (tmpFile.isDirectory() ? "TRUE" : "FALSE");
            }
            else {
                try {
                    pProc = Runtime.getRuntime().exec(this.getSuArgs("ls -l " + sDir));
                    RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
                    outThrd.start();
                    outThrd.joinAndStopRedirect(5000);
                    sRet = outThrd.strOutput;
                    if (!sRet.contains("No such file or directory") && sRet.startsWith("l"))
                        sRet = "FALSE";
                }
                catch (IOException e) {
                    sRet = e.getMessage();
                    e.printStackTrace();
                }
                catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }

        return(sRet);
        }


    public String changeDir(String newDir)
        {
        String    tmpDir    = fixFileName(newDir);
        String    sRet = sErrorPrefix + "Couldn't change directory to " + tmpDir;
        int    nFiles = 0;

        if (tmpDir.contains("org.mozilla.fennec") || tmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (tmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");

            String[] columns = new String[] {
                    "_id",
                    "isdir",
                    "filename"
                };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,     // Which columns to return
                                        tmpDir,     // Which rows to return (all rows)
                                        null,       // Selection arguments (none)
                                        null);        // Order clause (none)
            if (myCursor != null) {
                nFiles = myCursor.getCount();

                if (nFiles > 0) {
                    if  (myCursor.moveToPosition(0)) {
                        if (myCursor.getLong(myCursor.getColumnIndex("isdir")) == 1) {
                            currentDir = myCursor.getString(myCursor.getColumnIndex("filename"));
                            sRet = "";
                        }
                    }
                } else {
                    sRet = sErrorPrefix + tmpDir + " is not a valid directory";
                }
                myCursor.close();
            }
        } else {
            File tmpFile = new java.io.File(tmpDir);

            if (tmpFile.exists()) {
                try {
                    if (tmpFile.isDirectory()) {
                        currentDir = tmpFile.getCanonicalPath();
                        sRet = "";
                    }
                else
                    sRet = sErrorPrefix + tmpDir + " is not a valid directory";
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        return(sRet);
        }

    static final String HEXES = "0123456789abcdef";

    public static String getHex( byte [] raw )
        {
        if ( raw == null )
            {
            return null;
            }

        final StringBuilder hex = new StringBuilder( 2 * raw.length );
        for ( final byte b : raw )
            {
            hex.append(HEXES.charAt((b & 0xF0) >> 4)).append(HEXES.charAt((b & 0x0F)));
            }
        return hex.toString();
        }

    public String HashFile(String fileName)
        {
        String            sTmpFileName = fixFileName(fileName);
        String            sRet         = sErrorPrefix + "Couldn't calculate hash for file " + sTmpFileName;
        byte[]             buffer         = new byte [4096];
        int                nRead         = 0;
        long             lTotalRead     = 0;
        MessageDigest    digest         = null;

        try {
            digest = java.security.MessageDigest.getInstance("MD5");
            }
        catch (NoSuchAlgorithmException e)
            {
            e.printStackTrace();
            }

        if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = null;

            ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");

            String[] columns = new String[] {
                    "_id",
                    "chunk"
                    };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,         // Which columns to return
                                        sTmpFileName,   // Which rows to return (all rows)
                                        null,           // Selection arguments (none)
                                        null);            // Order clause (none)
            if (myCursor != null) {
                int nRows = myCursor.getCount();
                int nBytesRecvd = 0;

                for (int lcv = 0; lcv < nRows; lcv++) {
                    if  (myCursor.moveToPosition(lcv)) {
                        byte [] buf = myCursor.getBlob(1);
                        if (buf != null) {
                            nBytesRecvd += buf.length;
                            digest.update(buf, 0, buf.length);
                            lTotalRead += nRead;
                            buf = null;
                        }
                    }
                }
                myCursor.close();
                byte [] hash = digest.digest();

                sRet = getHex(hash);
            }
        } else {
            try {
                FileInputStream srcFile  = new FileInputStream(sTmpFileName);
                while((nRead = srcFile.read(buffer)) != -1) {
                    digest.update(buffer, 0, nRead);
                    lTotalRead += nRead;
                }
                srcFile.close();
                byte [] hash = digest.digest();

                sRet = getHex(hash);
            }
            catch (FileNotFoundException e) {
                sRet += " file not found";
            }
            catch (IOException e) {
                sRet += " io exception";
                e.printStackTrace();
            }
        }
        return(sRet);
    }

    public String RemoveFile(String fileName)
        {
        String    sTmpFileName = fixFileName(fileName);
        String    sRet = sErrorPrefix + "Couldn't delete file " + sTmpFileName;

        if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");
            if (cr.delete(ffxFiles, sTmpFileName, null) == 1) {
                sRet = "deleted " + sTmpFileName;
            }
        } else {
            File f = new File(sTmpFileName);

            if (f.delete())
                sRet = "deleted " + sTmpFileName;
        }

        return(sRet);
        }

    public String PruneDir(String sDir)
        {
        String    sRet = "";
        int nFiles = 0;
        String sSubDir = null;
        String    sTmpDir = fixFileName(sDir);

        if (sTmpDir.contains("org.mozilla.fennec") || sTmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (sTmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");
            if (cr.delete(ffxFiles, sTmpDir, null) > 0) {
                sRet = "deleted " + sTmpDir;
            }
        } else {
            File dir = new File(sTmpDir);

            if (dir.isDirectory()) {
                sRet = "Deleting file(s) from " + sTmpDir;

                File [] files = dir.listFiles();
                if (files != null) {
                    if ((nFiles = files.length) > 0) {
                        for (int lcv = 0; lcv < nFiles; lcv++) {
                            if (files[lcv].isDirectory()) {
                                sSubDir = files[lcv].getAbsolutePath();
                                sRet += "\n" + PruneDir(sSubDir);
                            }
                            else {
                                if (files[lcv].delete()) {
                                sRet += "\n\tDeleted " + files[lcv].getName();
                                }
                                else {
                                    sRet += "\n\tUnable to delete " + files[lcv].getName();
                                }
                            }
                        }
                    }
                    else
                        sRet += "\n\t<empty>";
                }

                if (dir.delete()) {
                    sRet += "\nDeleting directory " + sTmpDir;
                }
                else {
                    sRet += "\nUnable to delete directory " + sTmpDir;
                }
            }
            else {
                sRet += sErrorPrefix + sTmpDir + " is not a directory";
            }
        }

        return(sRet);
        }

    public String PrintDir(String sDir)
        {
        String    sRet = "";
        int nFiles = 0;
        String    sTmpDir = fixFileName(sDir);

        if (sTmpDir.contains("org.mozilla.fennec") || sTmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = null;

            ffxFiles = Uri.parse("content://" + (sTmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");

            String[] columns = new String[] {
                    "_id",
                    "isdir",
                    "filename",
                    "length"
                };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,     // Which columns to return
                                        sTmpDir,    // Which rows to return (all rows)
                                        null,       // Selection arguments (none)
                                        null);        // Order clause (none)
            if (myCursor != null) {
                nFiles = myCursor.getCount();

                // If only one entry and the index is -1 this is not a directory
                int nNdx = myCursor.getColumnIndex("_id");
                // If no entries the dir is empty
                if (nFiles == 1) {
                    sRet = "<empty>";
                } else {
                    // Show the entries
                    for (int lcv = 1; lcv < nFiles; lcv++) {
                        if  (myCursor.moveToPosition(lcv)) {
                            if ((lcv == 0) && (myCursor.getLong(nNdx) == -1)) {
                                sRet = sErrorPrefix + sTmpDir + " is not a directory";
                            } else {
                                sRet += myCursor.getString(2);
                                if (lcv < (nFiles - 1))
                                    sRet += "\n";
                            }
                        }
                    }
                }
                myCursor.close();
            }
        } else {
            File dir = new File(sTmpDir);

            if (dir.isDirectory()) {
                File [] files = dir.listFiles();

                if (files != null) {
                    if ((nFiles = files.length) > 0) {
                        for (int lcv = 0; lcv < nFiles; lcv++) {
                            sRet += files[lcv].getName();
                            if (lcv < (nFiles - 1)) {
                                sRet += "\n";
                            }
                        }
                    }
                    else {
                        sRet = "<empty>";
                    }
                }
            }
            else {
                sRet = sErrorPrefix + sTmpDir + " is not a directory";
            }
        }
        return(sRet);
    }

    public String Move(String sTmpSrcFileName, String sTmpDstFileName) {
        String sRet = sErrorPrefix + "Could not move " + sTmpSrcFileName + " to " + sTmpDstFileName;
        String sTmp = CopyFile(sTmpSrcFileName, sTmpDstFileName);
        if (sTmp.contains(" copied to ")) {
            sTmp = RemoveFile(sTmpSrcFileName);
            if (sTmp.startsWith("deleted ")) {
                sRet = sTmpSrcFileName + " moved to " + sTmpDstFileName;
            }
        }

        return(sRet);
    }

    public String CopyFile(String sTmpSrcFileName, String sTmpDstFileName) {
        String sRet = sErrorPrefix + "Could not copy " + sTmpSrcFileName + " to " + sTmpDstFileName;
        ContentValues cv = null;
        File destFile = null;
        Uri ffxSrcFiles = null;
        Uri ffxDstFiles = null;
        FileInputStream srcFile  = null;
        FileOutputStream dstFile  = null;
        byte[] buffer = new byte [4096];
        int    nRead = 0;
        long lTotalRead = 0;
        long lTotalWritten = 0;
        ContentResolver crIn = null;
        ContentResolver crOut = null;

        if (sTmpSrcFileName.contains("org.mozilla.fennec") || sTmpSrcFileName.contains("org.mozilla.firefox")) {
            ffxSrcFiles = Uri.parse("content://" + (sTmpSrcFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");
            crIn = contextWrapper.getContentResolver();
        } else {
            try {
                srcFile  = new FileInputStream(sTmpSrcFileName);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }

        if (sTmpDstFileName.contains("org.mozilla.fennec") || sTmpDstFileName.contains("org.mozilla.firefox")) {
            ffxDstFiles = Uri.parse("content://" + (sTmpDstFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");
            crOut = contextWrapper.getContentResolver();
            cv = new ContentValues();
        } else {
            try {
                dstFile  = new FileOutputStream(sTmpDstFileName);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }

        if (srcFile != null) {
            try {
                while((nRead = srcFile.read(buffer)) != -1)    {
                    lTotalRead += nRead;
                    if (dstFile != null) {
                        dstFile.write(buffer, 0, nRead);
                        dstFile.flush();
                    } else {
                        cv.put("length", nRead);
                        cv.put("chunk", buffer);
                        if (crOut.update(ffxDstFiles, cv, sTmpDstFileName, null) == 0)
                            break;
                        lTotalWritten += nRead;
                    }
                }

                srcFile.close();

                if (dstFile != null) {
                    dstFile.flush();
                    dstFile.close();

                    destFile = new File(sTmpDstFileName);
                    lTotalWritten = destFile.length();
                }

                if (lTotalWritten == lTotalRead) {
                    sRet = sTmpSrcFileName + " copied to " + sTmpDstFileName;
                }
                else {
                    sRet = sErrorPrefix + "Failed to copy " + sTmpSrcFileName + " [length = " + lTotalWritten + "] to " + sTmpDstFileName + " [length = " + lTotalRead + "]";
                }
            } catch (IOException e) {
                e.printStackTrace();
            }

        } else {
            String[] columns = new String[] {
                    "_id",
                    "chunk",
                    "length"
                    };

            Cursor myCursor = crIn.query(ffxSrcFiles,
                                        columns,             // Which columns to return
                                        sTmpSrcFileName,       // Which rows to return (all rows)
                                        null,               // Selection arguments (none)
                                        null);                // Order clause (none)
            if (myCursor != null) {
                int nRows = myCursor.getCount();

                byte [] buf = null;

                for (int lcv = 0; lcv < nRows; lcv++) {
                    if  (myCursor.moveToPosition(lcv)) {
                        buf = myCursor.getBlob(myCursor.getColumnIndex("chunk"));
                        if (buf != null) {
                            nRead = buf.length;
                            try {
                                lTotalRead += nRead;
                                if (dstFile != null) {
                                    dstFile.write(buffer, 0, nRead);
                                    dstFile.flush();
                                } else {
                                    cv.put("length", nRead);
                                    cv.put("chunk", buffer);
                                    if (crOut.update(ffxDstFiles, cv, sTmpDstFileName, null) == 0)
                                        break;
                                    lTotalWritten += nRead;
                                }
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                            buf = null;
                        }
                    }
                }

                if (nRows == -1) {
                    sRet = sErrorPrefix + sTmpSrcFileName + ",-1\nNo such file or directory";
                }
                else {
                    myCursor.close();

                    if (dstFile != null) {
                        try {
                            dstFile.flush();
                            dstFile.close();

                            destFile = new File(sTmpDstFileName);
                            lTotalWritten = destFile.length();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }

                    if (lTotalWritten == lTotalRead) {
                        sRet = sTmpSrcFileName + " copied to " + sTmpDstFileName;
                    }
                    else {
                        sRet = sErrorPrefix + "Failed to copy " + sTmpSrcFileName + " [length = " + lTotalWritten + "] to " + sTmpDstFileName + " [length = " + lTotalRead + "]";
                    }
                }
            }
            else {
                sRet = sErrorPrefix + sTmpSrcFileName + ",-1\nUnable to access file (internal error)";
            }
        }

        return (sRet);
    }

    public String IsDirWritable(String sDir)
        {
        String    sTmpDir = fixFileName(sDir);
        String sRet = sErrorPrefix + "[" + sTmpDir + "] is not a directory";

        if (sTmpDir.contains("org.mozilla.fennec") || sTmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = null;

            ffxFiles = Uri.parse("content://" + (sTmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");

            String[] columns = new String[] {
                    "_id",
                    "isdir",
                    "filename",
                    "length",
                    "writable"
                };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,     // Which columns to return
                                        sTmpDir,    // Which rows to return (all rows)
                                        null,       // Selection arguments (none)
                                        null);        // Order clause (none)
            if (myCursor != null) {
                if (myCursor.getCount() > 0) {
                    if (myCursor.moveToPosition(0)) {
                        if (myCursor.getLong(myCursor.getColumnIndex("isdir")) == 1) {
                            sRet = "[" + sTmpDir + "] " + ((myCursor.getLong(myCursor.getColumnIndex("writable")) == 1) ? "is" : "is not") + " writable";
                        }
                    }
                }
            }
        } else {
            File dir = new File(sTmpDir);

            if (dir.isDirectory()) {
                sRet = "[" + sTmpDir + "] " + (dir.canWrite() ? "is" : "is not") + " writable";
            } else {
                sRet = sErrorPrefix + "[" + sTmpDir + "] is not a directory";
            }
        }
        return(sRet);
    }

    public String Push(String fileName, BufferedInputStream bufIn, long lSize)
    {
        byte []                buffer             = new byte [8192];
        int                    nRead            = 0;
        long                lRead            = 0;
        String                sTmpFileName     = fixFileName(fileName);
        FileOutputStream     dstFile            = null;
        ContentResolver     cr                 = null;
        ContentValues        cv                = null;
        Uri                 ffxFiles         = null;
        String                sRet            = sErrorPrefix + "Push failed!";

        try {
            if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
                cr = contextWrapper.getContentResolver();
                ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");
                cv = new ContentValues();
            }
            else {
                dstFile = new FileOutputStream(sTmpFileName, false);
            }

            while((nRead != -1) && (lRead < lSize))
                {
                nRead = bufIn.read(buffer);
                if (nRead != -1) {
                    if (dstFile != null) {
                        dstFile.write(buffer, 0, nRead);
                        dstFile.flush();
                    }
                    else {
                        cv.put("offset", lRead);
                        cv.put("length", nRead);
                        cv.put("chunk", buffer);
                        cr.update(ffxFiles, cv, sTmpFileName, null);
                    }
                    lRead += nRead;
                }
            }

            if (dstFile != null) {
                dstFile.flush();
                dstFile.close();
            }

            if (lRead == lSize)    {
                sRet = HashFile(sTmpFileName);
            }
        }
        catch (IOException e) {
            e.printStackTrace();
        }

        buffer = null;

        return(sRet);
    }

    public String FTPGetFile(String sServer, String sSrcFileName, String sDstFileName, OutputStream out)
        {
        byte[] buffer = new byte [4096];
        int    nRead = 0;
        long lTotalRead = 0;
        String sRet = sErrorPrefix + "FTP Get failed for " + sSrcFileName;
        String strRet = "";
        int    reply = 0;
        FileOutputStream outStream = null;
        String    sTmpDstFileName = fixFileName(sDstFileName);

        FTPClient ftp = new FTPClient();
        try
            {
            ftp.connect(sServer);
            reply = ftp.getReplyCode();
            if(FTPReply.isPositiveCompletion(reply))
                {
                ftp.login("anonymous", "b@t.com");
                reply = ftp.getReplyCode();
                if(FTPReply.isPositiveCompletion(reply))
                    {
                    ftp.enterLocalPassiveMode();
                    if (ftp.setFileType(FTP.BINARY_FILE_TYPE))
                        {
                        File dstFile = new File(sTmpDstFileName);
                        outStream = new FileOutputStream(dstFile);
                        FTPFile [] ftpFiles = ftp.listFiles(sSrcFileName);
                        if (ftpFiles.length > 0)
                            {
                            long lFtpSize = ftpFiles[0].getSize();
                            if (lFtpSize <= 0)
                                lFtpSize = 1;

                            InputStream ftpIn = ftp.retrieveFileStream(sSrcFileName);
                            while ((nRead = ftpIn.read(buffer)) != -1)
                                {
                                lTotalRead += nRead;
                                outStream.write(buffer, 0, nRead);
                                strRet = "\r" + lTotalRead + " of " + lFtpSize + " bytes received " + ((lTotalRead * 100) / lFtpSize) + "% completed";
                                out.write(strRet.getBytes());
                                out.flush();
                                }
                            ftpIn.close();
                            @SuppressWarnings("unused")
                            boolean bRet = ftp.completePendingCommand();
                            outStream.flush();
                            outStream.close();
                            strRet = ftp.getReplyString();
                            reply = ftp.getReplyCode();
                            }
                        else
                            {
                            strRet = sRet;
                            }
                        }
                    ftp.logout();
                    ftp.disconnect();
                    sRet = "\n" + strRet;
                    }
                else
                    {
                    ftp.disconnect();
                    System.err.println("FTP server refused login.");
                    }
                }
            else
                {
                ftp.disconnect();
                System.err.println("FTP server refused connection.");
                }
            }
        catch (SocketException e)
            {
            sRet = e.getMessage();
            strRet = ftp.getReplyString();
            reply = ftp.getReplyCode();
            sRet += "\n" + strRet;
            e.printStackTrace();
            }
        catch (IOException e)
            {
            sRet = e.getMessage();
            strRet = ftp.getReplyString();
            reply = ftp.getReplyCode();
            sRet += "\n" + strRet;
            e.printStackTrace();
            }
        return (sRet);
    }

    public String Pull(String fileName, long lOffset, long lLength, OutputStream out)
        {
        String    sTmpFileName = fixFileName(fileName);
        String    sRet = sErrorPrefix + "Could not read the file " + sTmpFileName;
        byte[]    buffer = new byte [4096];
        int        nRead = 0;
        long    lSent = 0;

        if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = null;

            ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");

            String[] columns = new String[] {
                    "_id",
                    "chunk",
                    "length"
                    };

            String [] args = new String [2];
            args[0] = Long.toString(lOffset);
            args[1] = Long.toString(lLength);

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,         // Which columns to return
                                        sTmpFileName,   // Which rows to return (all rows)
                                        args,           // Selection arguments (none)
                                        null);            // Order clause (none)
            if (myCursor != null) {
                int nRows = myCursor.getCount();
                long lFileLength = 0;

                for (int lcv = 0; lcv < nRows; lcv++) {
                    if  (myCursor.moveToPosition(lcv)) {
                        if (lcv == 0) {
                            lFileLength = myCursor.getLong(2);
                            String sTmp = sTmpFileName + "," + lFileLength + "\n";
                            try {
                                out.write(sTmp.getBytes());
                            } catch (IOException e) {
                                e.printStackTrace();
                                break;
                            }
                        }

                        if (lLength != 0) {
                            byte [] buf = myCursor.getBlob(1);
                            if (buf != null) {
                                nRead = buf.length;
                                try {
                                    if ((lSent + nRead) <= lFileLength)    {
                                        out.write(buf,0,nRead);
                                        lSent += nRead;
                                    }
                                    else {
                                        nRead = (int) (lFileLength - lSent);
                                        out.write(buf,0,nRead);
                                        Log.d("pull warning", "more bytes read than expected");
                                        break;
                                    }
                                } catch (IOException e) {
                                    e.printStackTrace();
                                    sRet = sErrorPrefix + "Could not write to out " + sTmpFileName;
                                }
                                buf = null;
                            }
                        }
                    }
                }
                if (nRows == 0) {
                    String sTmp = sTmpFileName + "," + lFileLength + "\n";
                    try {
                        out.write(sTmp.getBytes());
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
                if (nRows == -1) {
                    sRet = sErrorPrefix + sTmpFileName + ",-1\nNo such file or directory";
                }
                else {
                    myCursor.close();
                    sRet = "";
                }
            }
            else {
                sRet = sErrorPrefix + sTmpFileName + ",-1\nUnable to access file (internal error)";
            }
        }
        else {
            try {
                File f = new File(sTmpFileName);
                long lFileLength = f.length();
                FileInputStream fin = new FileInputStream(f);
                if (lFileLength == 0) {
                    while ((nRead = fin.read(buffer)) != -1) {
                        lFileLength += nRead;
                    }
                    fin.close();
                    fin = new FileInputStream(f);
                }

                // lLength == -1 return everything between lOffset and eof
                // lLength == 0 return file length
                // lLength > 0 return lLength bytes
                if (lLength == -1) {
                    lFileLength = lFileLength - lOffset;
                } else if (lLength == 0) {
                    // just return the file length
                } else {
                    lFileLength = ((lLength <= (lFileLength - lOffset)) ? lLength : (lFileLength - lOffset));
                }

                String sTmp = sTmpFileName + "," + lFileLength + "\n";
                out.write(sTmp.getBytes());
                if (lLength != 0) {
                    if  (lOffset > 0) {
                        fin.skip(lOffset);
                    }
                    while ((nRead = fin.read(buffer)) != -1) {
                        if ((lSent + nRead) <= lFileLength)    {
                            out.write(buffer,0,nRead);
                            lSent += nRead;
                        }
                        else {
                            nRead = (int) (lFileLength - lSent);
                            out.write(buffer,0,nRead);
                            if (lLength != -1)
                                Log.d("pull warning", "more bytes read than sent");
                            break;
                        }
                    }
                }
                fin.close();
                out.flush();
                sRet = "";
                }
            catch (FileNotFoundException e)    {
                sRet = sErrorPrefix + sTmpFileName + ",-1\nNo such file or directory";
            }
            catch (IOException e) {
                sRet = e.toString();
            }
        }
        return (sRet);
    }

    public String Cat(String fileName, OutputStream out)
        {
        String    sTmpFileName = fixFileName(fileName);
        String    sRet = sErrorPrefix + "Could not read the file " + sTmpFileName;
        byte[]    buffer = new byte [4096];
        int        nRead = 0;

        if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = null;

            ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/file");

            String[] columns = new String[] {
                    "_id",
                    "chunk"
                    };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,         // Which columns to return
                                        sTmpFileName,   // Which rows to return (all rows)
                                        null,           // Selection arguments (none)
                                        null);            // Order clause (none)
            if (myCursor != null) {
                int nRows = myCursor.getCount();
                int nBytesRecvd = 0;

                for (int lcv = 0; lcv < nRows; lcv++) {
                    if  (myCursor.moveToPosition(lcv)) {
                        byte [] buf = myCursor.getBlob(1);
                        if (buf != null) {
                            nBytesRecvd += buf.length;
                            try {
                                out.write(buf);
                                sRet = "";
                            } catch (IOException e) {
                                e.printStackTrace();
                                sRet = sErrorPrefix + "Could not write to out " + sTmpFileName;
                            }
                            buf = null;
                        }
                    }
                }
                if (nRows == 0) {
                    sRet = "";
                }

                myCursor.close();
            }
        } else {
            try {
                FileInputStream fin = new FileInputStream(sTmpFileName);
                while ((nRead = fin.read(buffer)) != -1) {
                    out.write(buffer,0,nRead);
                }
                fin.close();
                out.flush();
                sRet = "";
                }
            catch (FileNotFoundException e) {
                sRet = sErrorPrefix + sTmpFileName + " No such file or directory";
            }
            catch (IOException e) {
                sRet = e.toString();
            }
        }
        return (sRet);
    }

    public String MakeDir(String sDir)
        {
        String    sTmpDir = fixFileName(sDir);
        String sRet = sErrorPrefix + "Could not create the directory " + sTmpDir;
        if (sTmpDir.contains("org.mozilla.fennec") || sTmpDir.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (sTmpDir.contains("fennec") ? fenProvider : ffxProvider) + "/dir");
            ContentValues cv = new ContentValues();

            if (cr.update(ffxFiles, cv, sTmpDir, null) == 1) {
                sRet = sDir + " successfully created";
            }
        }
        else {
            File dir = new File(sTmpDir);

            if (dir.mkdirs()) {
                sRet = sDir + " successfully created";
            }
        }

        return (sRet);
        }
    // move this to SUTAgentAndroid.java
    public String GetScreenInfo()
        {
        String sRet = "";
        DisplayMetrics metrics = new DisplayMetrics();
        WindowManager wMgr = (WindowManager) contextWrapper.getSystemService(Context.WINDOW_SERVICE);
        wMgr.getDefaultDisplay().getMetrics(metrics);
        sRet = "X:" + metrics.widthPixels + " Y:" + metrics.heightPixels;
        return (sRet);
        }
    // move this to SUTAgentAndroid.java
    public int [] GetScreenXY()
        {
            int [] nRetXY = new int [2];
            DisplayMetrics metrics = new DisplayMetrics();
            WindowManager wMgr = (WindowManager) contextWrapper.getSystemService(Context.WINDOW_SERVICE);
            wMgr.getDefaultDisplay().getMetrics(metrics);
            nRetXY[0] = metrics.widthPixels;
            nRetXY[1] = metrics.heightPixels;
            return(nRetXY);
        }

    public String GetRotationInfo()
        {
            WindowManager wMgr = (WindowManager) contextWrapper.getSystemService(Context.WINDOW_SERVICE);
            int nRotationDegrees = 0; // default
            switch(wMgr.getDefaultDisplay().getRotation())
                {
                case Surface.ROTATION_90:
                    nRotationDegrees = 90;
                    break;
                case Surface.ROTATION_180:
                    nRotationDegrees = 180;
                    break;
                case Surface.ROTATION_270:
                    nRotationDegrees = 270;
                    break;
                }
            return "ROTATION:" + nRotationDegrees;
        }

    public String SetADB(String sWhat) {
        String sRet = "";
        String sTmp = "";
        String sCmd;

        if (sWhat.contains("ip")) {
            sCmd = "setprop service.adb.tcp.port 5555";
        } else {
            sCmd = "setprop service.adb.tcp.port -1";
        }

        try {
            pProc = Runtime.getRuntime().exec(this.getSuArgs(sCmd));
            RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
            outThrd.start();
            outThrd.joinAndStopRedirect(5000);
            sTmp = outThrd.strOutput;
            Log.e("ADB", sTmp);
            if (outThrd.nExitCode == 0) {
                pProc = Runtime.getRuntime().exec(this.getSuArgs("stop adbd"));
                outThrd = new RedirOutputThread(pProc, null);
                outThrd.start();
                outThrd.joinAndStopRedirect(5000);
                sTmp = outThrd.strOutput;
                Log.e("ADB", sTmp);
                if (outThrd.nExitCode == 0) {
                    pProc = Runtime.getRuntime().exec(this.getSuArgs("start adbd"));
                    outThrd = new RedirOutputThread(pProc, null);
                    outThrd.start();
                    outThrd.joinAndStopRedirect(5000);
                    sTmp = outThrd.strOutput;
                    Log.e("ADB", sTmp);
                    if (outThrd.nExitCode == 0) {
                        sRet = "Successfully set adb to " + sWhat + "\n";
                    } else {
                        sRet = sErrorPrefix + "Failed to start adbd\n";
                    }
                } else {
                    sRet = sErrorPrefix + "Failed to stop adbd\n";
                }
            } else {
                sRet = sErrorPrefix + "Failed to setprop service.adb.tcp.port 5555\n";
            }

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
        return(sRet);
    }

    public String KillProcess(String sProcName, OutputStream out)
        {
        String sRet = sErrorPrefix + "Unable to kill " + sProcName + "\n";
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
        int lcv = 0;
        String sFoundProcName = "";
        int nProcs = 0;
        boolean bFoundAppProcess = false;

        if (lProcesses != null)
            nProcs = lProcesses.size();

        for (lcv = 0; lcv < nProcs; lcv++)
            {
            if (lProcesses.get(lcv).processName.contains(sProcName))
                {
                sFoundProcName = lProcesses.get(lcv).processName;
                bFoundAppProcess = true;

                try
                    {
                    pProc = Runtime.getRuntime().exec(this.getSuArgs("kill " + lProcesses.get(lcv).pid));
                    RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
                    outThrd.start();
                    outThrd.joinAndStopRedirect(15000);
                    String sTmp = outThrd.strOutput;
                    Log.e("KILLPROCESS", sTmp);
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
                }
            }

        if (bFoundAppProcess)
            {
            // Give the messages a chance to be processed
            try {
                Thread.sleep(2000);
                }
            catch (InterruptedException e)
                {
                e.printStackTrace();
                }

            sRet = "Successfully killed " + sProcName + "\n";
            lProcesses = aMgr.getRunningAppProcesses();
            nProcs = 0;
            if (lProcesses != null)
                nProcs = lProcesses.size();
            for (lcv = 0; lcv < nProcs; lcv++)
                {
                if (lProcesses.get(lcv).processName.contains(sProcName))
                    {
                    sRet = sErrorPrefix + "Unable to kill " + sProcName + " (couldn't kill " + sFoundProcName +")\n";
                    break;
                    }
                }
            }
        else
            {
            // To kill processes other than Java applications - processes
            // like xpcshell - a different strategy is necessary: use ps
            // to find the process' PID.
            try
                {
                pProc = Runtime.getRuntime().exec("ps");
                RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
                outThrd.start();
                outThrd.joinAndStopRedirect(10000);
                String sTmp = outThrd.strOutput;
                StringTokenizer stokLines = new StringTokenizer(sTmp, "\n");
                while(stokLines.hasMoreTokens())
                    {
                    String sLine = stokLines.nextToken();
                    StringTokenizer stokColumns = new StringTokenizer(sLine, " \t\n");
                    stokColumns.nextToken();
                    String sPid = stokColumns.nextToken();
                    stokColumns.nextToken();
                    stokColumns.nextToken();
                    stokColumns.nextToken();
                    stokColumns.nextToken();
                    stokColumns.nextToken();
                    stokColumns.nextToken();
                    String sName = null;
                    if (stokColumns.hasMoreTokens())
                        {
                        sName = stokColumns.nextToken();
                        if (sName.contains(sProcName))
                            {
                            NewKillProc(sPid, out);
                            sRet = "Successfully killed " + sName + "\n";
                            }
                        }
                    }
                }
            catch (Exception e)
                {
                e.printStackTrace();
                }
            }

        return (sRet);
        }

    public boolean IsProcessDead(String sProcName)
        {
        boolean bRet = false;
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.ProcessErrorStateInfo> lProcesses = aMgr.getProcessesInErrorState();
        int lcv = 0;

        if (lProcesses != null)
            {
            for (lcv = 0; lcv < lProcesses.size(); lcv++)
                {
                if (lProcesses.get(lcv).processName.contentEquals(sProcName) &&
                    lProcesses.get(lcv).condition != ActivityManager.ProcessErrorStateInfo.NO_ERROR)
                    {
                    bRet = true;
                    break;
                    }
                }
            }

        return (bRet);
        }

    public String GetProcessInfo()
        {
        String sRet = "";
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
        int    nProcs = 0;
        int lcv = 0;
        String strProcName = "";
        int    nPID = 0;
        int nUser = 0;

        if (lProcesses != null) 
            nProcs = lProcesses.size();

        for (lcv = 0; lcv < nProcs; lcv++)
            {
            strProcName = lProcesses.get(lcv).processName;
            nPID = lProcesses.get(lcv).pid;
            nUser = lProcesses.get(lcv).uid;
            sRet += nUser + "\t" + nPID + "\t" + strProcName;
            if (lcv < (nProcs - 1))
                sRet += "\n";
            }

        return (sRet);
        }

    public String GetOSInfo()
        {
        String sRet = "";

        sRet = Build.DISPLAY;

        return (sRet);
        }

    public String GetPowerInfo()
        {
        String sRet = "";

        sRet = "Power status:\n  AC power " + SUTAgentAndroid.sACStatus + "\n";
        sRet += "  Battery charge " + SUTAgentAndroid.sPowerStatus + "\n";
        sRet += "  Remaining charge:      " + SUTAgentAndroid.nChargeLevel + "%\n";
        sRet += "  Battery Temperature:   " + (((float)(SUTAgentAndroid.nBatteryTemp))/10) + " (c)\n";
        return (sRet);
        }

    public String GetSutUserInfo()
        {
        String sRet = "";
        try {
            // based on patch in https://bugzilla.mozilla.org/show_bug.cgi?id=811763
            Context context = contextWrapper.getApplicationContext();
            Object userManager = context.getSystemService("user");
            if (userManager != null) {
                // if userManager is non-null that means we're running on 4.2+ and so the rest of this
                // should just work
                Object userHandle = android.os.Process.class.getMethod("myUserHandle", (Class[])null).invoke(null);
                Object userSerial = userManager.getClass().getMethod("getSerialNumberForUser", userHandle.getClass()).invoke(userManager, userHandle);
                sRet += "User Serial:" + userSerial.toString();
            }
        } catch (Exception e) {
            // Guard against any unexpected failures
            e.printStackTrace();
        }

        return sRet;
        }

    public String GetTemperatureInfo()
        {
        String sTempVal = "unknown";
        String sDeviceFile = "/sys/bus/platform/devices/temp_sensor_hwmon.0/temp1_input";
        try {
            pProc = Runtime.getRuntime().exec(this.getSuArgs("cat " + sDeviceFile));
            RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
            outThrd.start();
            outThrd.joinAndStopRedirect(5000);
            String output = outThrd.strOutput;
            // this only works on pandas (with the temperature sensors turned
            // on), other platforms we just get a file not found error... we'll
            // just return "unknown" for that case
            try {
                sTempVal = String.valueOf(Integer.parseInt(output.trim()) / 1000.0);
            } catch (NumberFormatException e) {
                // not parsed! probably not a panda
            }
        } catch (IOException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        return "Temperature: " + sTempVal;
        }

    // todo
    public String GetDiskInfo(String sPath)
        {
        String sRet = "";
        StatFs statFS = new StatFs(sPath);

        int nBlockCount = statFS.getBlockCount();
        int nBlockSize = statFS.getBlockSize();
        int nBlocksAvail = statFS.getAvailableBlocks();
        int nBlocksFree = statFS.getFreeBlocks();

        sRet = "total:     " + (nBlockCount * nBlockSize) + "\nfree:      " + (nBlocksFree * nBlockSize) + "\navailable: " + (nBlocksAvail * nBlockSize);

        return (sRet);
        }

    public String GetMemoryInfo()
        {
        String sRet = "PA:" + GetMemoryConfig() + ", FREE: " + GetMemoryUsage();
        return (sRet);
        }

    public long GetMemoryConfig()
        {
        ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo outInfo = new ActivityManager.MemoryInfo();
        aMgr.getMemoryInfo(outInfo);
        long lMem = outInfo.availMem;

        return (lMem);
        }

    public long GetMemoryUsage()
        {

        String load = "";
        try {
            RandomAccessFile reader = new RandomAccessFile("/proc/meminfo", "r");
            load = reader.readLine(); // Read in the MemTotal
            load = reader.readLine(); // Read in the MemFree
        } catch (IOException ex) {
            return (0);
        }

        String[] toks = load.split(" ");
        int i = 1;
        for (i=1; i < toks.length; i++) {
            String val = toks[i].trim();
            if (!val.equals("")) {
                break;
            }
        }
        if (i <= toks.length) {
            long lMem = Long.parseLong(toks[i].trim());
            return (lMem * 1024);
        }
        return (0);
        }

    public String UpdateCallBack(String sFileName)
        {
        String sRet = sErrorPrefix + "No file specified";
        String sIP = "";
        String sPort = "";
        int nEnd = 0;
        int nStart = 0;

        if ((sFileName == null) || (sFileName.length() == 0))
            return(sRet);

        Context ctx = contextWrapper.getApplicationContext();
        try {
            FileInputStream fis = ctx.openFileInput(sFileName);
            int nBytes = fis.available();
            if (nBytes > 0)
                {
                byte [] buffer = new byte [nBytes + 1];
                int nRead = fis.read(buffer, 0, nBytes);
                fis.close();
                ctx.deleteFile(sFileName);
                if (nRead > 0)
                    {
                    String sBuffer = new String(buffer);
                    nEnd = sBuffer.indexOf(',');
                    if (nEnd > 0)
                        {
                        sIP = (sBuffer.substring(0, nEnd)).trim();
                        nStart = nEnd + 1;
                        nEnd = sBuffer.indexOf('\r', nStart);
                        if (nEnd > 0)
                            {
                            sPort = (sBuffer.substring(nStart, nEnd)).trim();
                            Thread.sleep(5000);
                            sRet = RegisterTheDevice(sIP, sPort, sBuffer.substring(nEnd + 1));
                            }
                        }
                    }
                }
            }
        catch (FileNotFoundException e)
            {
            sRet = sErrorPrefix + "Nothing to do";
            }
        catch (IOException e)
            {
            sRet = sErrorPrefix + "Couldn't send info to " + sIP + ":" + sPort;
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            }
        return(sRet);
        }

    public String RegisterTheDevice(String sSrvr, String sPort, String sData)
        {
        String sRet = "";
        String line = "";

//        Debug.waitForDebugger();

        if (sSrvr != null && sPort != null && sData != null)
            {
            try
                {
                int nPort = Integer.parseInt(sPort);
                Socket socket = new Socket(sSrvr, nPort);
                PrintWriter out = new PrintWriter(socket.getOutputStream(), false);
                BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                out.println(sData);
                if ( out.checkError() == false )
                    {
                    socket.setSoTimeout(30000);
                    while (socket.isInputShutdown() == false)
                        {
                        line = in.readLine();

                        if (line != null)
                            {
                            line = line.toLowerCase();
                            sRet += line;
                            // ok means we're done
                            if (line.contains("ok"))
                                break;
                            }
                        else
                            {
                            // end of stream reached
                            break;
                            }
                        }
                    }
                out.close();
                in.close();
                socket.close();
                }
            catch(NumberFormatException e)
                {
                sRet += "reg NumberFormatException thrown [" + e.getLocalizedMessage() + "]";
                e.printStackTrace();
                }
            catch (UnknownHostException e)
                {
                sRet += "reg UnknownHostException thrown [" + e.getLocalizedMessage() + "]";
                e.printStackTrace();
                }
            catch (IOException e)
                {
                sRet += "reg IOException thrown [" + e.getLocalizedMessage() + "]";
                e.printStackTrace();
                }
            }
        return(sRet);
        }

    public String GetInternetData(String sHost, String sPort, String sURL)
        {
        String sRet = "";
        String sNewURL = "";
        HttpClient httpClient = new DefaultHttpClient();
        try
            {
            sNewURL = "http://" + sHost + ((sPort.length() > 0) ? (":" + sPort) : "") + sURL;

            HttpGet request = new HttpGet(sNewURL);
            HttpResponse response = httpClient.execute(request);
            int status = response.getStatusLine().getStatusCode();
            // we assume that the response body contains the error message
            if (status != HttpStatus.SC_OK)
                {
                ByteArrayOutputStream ostream = new ByteArrayOutputStream();
                response.getEntity().writeTo(ostream);
                Log.e("HTTP CLIENT", ostream.toString());
                }
            else
                {
                InputStream content = response.getEntity().getContent();
                byte [] data = new byte [2048];
                int nRead = content.read(data);
                sRet = new String(data, 0, nRead);
                content.close(); // this will also close the connection
                }
            }
        catch (IllegalArgumentException e)
            {
            sRet = e.getLocalizedMessage();
            e.printStackTrace();
            }
        catch (ClientProtocolException e)
            {
            sRet = e.getLocalizedMessage();
            e.printStackTrace();
            }
        catch (IOException e)
            {
            sRet = e.getLocalizedMessage();
            e.printStackTrace();
            }

        return(sRet);
        }

    public String GetTimeZone()
        {
        String    sRet = "";
        TimeZone tz;

        tz = TimeZone.getDefault();
        Date now = new Date();
        sRet = tz.getDisplayName(tz.inDaylightTime(now), TimeZone.LONG);

        return(sRet);
        }

    public String SetTimeZone(String sTimeZone)
        {
        String            sRet = "Unable to set timezone to " + sTimeZone;
        TimeZone         tz = null;
        AlarmManager     amgr = null;

        if ((sTimeZone.length() > 0) && (sTimeZone.startsWith("GMT")))
            {
            amgr = (AlarmManager) contextWrapper.getSystemService(Context.ALARM_SERVICE);
            if (amgr != null)
                amgr.setTimeZone(sTimeZone);
            }
        else
            {
            String [] zoneNames = TimeZone.getAvailableIDs();
            int nNumMatches = zoneNames.length;
            int    lcv = 0;

            for (lcv = 0; lcv < nNumMatches; lcv++)
                {
                if (zoneNames[lcv].equalsIgnoreCase(sTimeZone))
                    break;
                }

            if (lcv < nNumMatches)
                {
                amgr = (AlarmManager) contextWrapper.getSystemService(Context.ALARM_SERVICE);
                if (amgr != null)
                    amgr.setTimeZone(zoneNames[lcv]);
                }
            }

        if (amgr != null)
            {
            tz = TimeZone.getDefault();
            Date now = new Date();
            sRet = tz.getDisplayName(tz.inDaylightTime(now), TimeZone.LONG);
            }

        return(sRet);
        }

    public String GetSystemTime()
        {
        String sRet = "";
        Calendar cal = Calendar.getInstance();
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd hh:mm:ss:SSS");
        sRet = sdf.format(cal.getTime());

        return (sRet);
        }

    public String SetSystemTime(String sDate, String sTime, OutputStream out) {
        String sRet = "";
        String sM = "";
        String sMillis = "";

        if (((sDate != null) && (sTime != null)) &&
            (sDate.contains("/") || sDate.contains(".")) &&
            (sTime.contains(":"))) {
            int year = Integer.parseInt(sDate.substring(0,4));
            int month = Integer.parseInt(sDate.substring(5,7));
            int day = Integer.parseInt(sDate.substring(8,10));

            int hour = Integer.parseInt(sTime.substring(0,2));
            int mins = Integer.parseInt(sTime.substring(3,5));
            int secs = Integer.parseInt(sTime.substring(6,8));

            Calendar cal = new GregorianCalendar(TimeZone.getDefault());
            cal.set(year, month - 1, day, hour, mins, secs);
            long lMillisecs = cal.getTime().getTime();

            sM = Long.toString(lMillisecs);
            sMillis = sM.substring(0, sM.length() - 3) + "." + sM.substring(sM.length() - 3);

        } else {
            sRet += "Invalid argument(s)";
        }

        // if we have an argument
        if (sMillis.length() > 0) {
            try {
                pProc = Runtime.getRuntime().exec(this.getSuArgs("date -u " + sMillis));
                RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
                outThrd.start();
                outThrd.joinAndStopRedirect(10000);
                sRet += GetSystemTime();
            } catch (IOException e) {
                sRet = e.getMessage();
                e.printStackTrace();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        return (sRet);
    }

    public String GetClok()
        {
        long lMillisecs = System.currentTimeMillis();
        String sRet = "";

        if (lMillisecs > 0)
            sRet = Long.toString(lMillisecs);

        return(sRet);
        }

    public String GetUptime()
        {
        String sRet = "";
        long lHold = 0;
        long lUptime = SystemClock.elapsedRealtime();
        int    nDays = 0;
        int    nHours = 0;
        int nMinutes = 0;
        int nSecs = 0;
        int nMilliseconds = 0;

        if (lUptime > 0)
            {
            nDays = (int)(lUptime / (24L * 60L * 60L * 1000L));
            lHold = lUptime % (24L * 60L * 60L * 1000L);
            nHours = (int)(lHold / (60L * 60L * 1000L));
            lHold %= 60L * 60L * 1000L;
            nMinutes = (int)(lHold / (60L * 1000L));
            lHold %= 60L * 1000L;
            nSecs = (int)(lHold / 1000L);
            nMilliseconds = (int)(lHold % 1000);
            sRet = "" + nDays + " days " + nHours + " hours " + nMinutes + " minutes " + nSecs + " seconds " + nMilliseconds + " ms";
            }

        return (sRet);
        }

    public String GetUptimeMillis()
        {
        return Long.toString(SystemClock.uptimeMillis());
        }
 
    public String NewKillProc(String sProcId, OutputStream out)
        {
        String sRet = "";

        try
            {
            pProc = Runtime.getRuntime().exec("kill "+sProcId);
            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.joinAndStopRedirect(5000);
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

        return(sRet);
        }

    public String SendPing(String sIPAddr, OutputStream out)
        {
        String sRet = "";
        String [] theArgs = new String [4];

        theArgs[0] = "ping";
        theArgs[1] = "-c";
        theArgs[2] = "3";
        theArgs[3] = sIPAddr;

        try
            {
            pProc = Runtime.getRuntime().exec(theArgs);
            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.joinAndStopRedirect(5000);
            if (out == null)
                sRet = outThrd.strOutput;
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

    public String GetTmpDir()
    {
        String     sRet = "";
        Context ctx = contextWrapper.getApplicationContext();
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

    public String PrintFileTimestamp(String sFile)
        {
        String     sRet = "";
        String    sTmpFileName = fixFileName(sFile);
        long    lModified = -1;

        if (sTmpFileName.contains("org.mozilla.fennec") || sTmpFileName.contains("org.mozilla.firefox")) {
            ContentResolver cr = contextWrapper.getContentResolver();
            Uri ffxFiles = Uri.parse("content://" + (sTmpFileName.contains("fennec") ? fenProvider : ffxProvider) + "/dir");

            String[] columns = new String[] {
                    "_id",
                    "isdir",
                    "filename",
                    "length",
                    "ts"
                };

            Cursor myCursor = cr.query(    ffxFiles,
                                        columns,         // Which columns to return
                                        sTmpFileName,   // Which rows to return (all rows)
                                        null,           // Selection arguments (none)
                                        null);            // Order clause (none)
            if (myCursor != null) {
                if (myCursor.getCount() > 0) {
                    if (myCursor.moveToPosition(0)) {
                        lModified = myCursor.getLong(myCursor.getColumnIndex("ts"));
                    }
                }
                myCursor.close();
            }
        }
        else {
            File theFile = new File(sTmpFileName);

            if (theFile.exists()) {
                lModified = theFile.lastModified();
            }
        }

        if (lModified != -1) {
            Date dtModified = new Date(lModified);
            SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd hh:mm:ss:SSS");
            sRet = "Last modified: " + sdf.format(dtModified);
        }
        else {
            sRet = sErrorPrefix + "[" + sTmpFileName + "] doesn't exist";
        }

        return(sRet);
        }

    public String GetIniData(String sSection, String sKey, String sFile)
        {
        String sRet = "";
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

    public String RunReboot(OutputStream out, String sCallBackIP, String sCallBackPort)
        {
        String sRet = "";
        Context ctx = contextWrapper.getApplicationContext();

        try {
            if ((sCallBackIP != null) && (sCallBackPort != null) &&
                (sCallBackIP.length() > 0) && (sCallBackPort.length() > 0))    {
                FileOutputStream fos = ctx.openFileOutput("update.info", Context.MODE_WORLD_READABLE | Context.MODE_WORLD_WRITEABLE);
                String sBuffer = sCallBackIP + "," + sCallBackPort + "\rSystem rebooted\r";
                fos.write(sBuffer.getBytes());
                fos.flush();
                fos.close();
                fos = null;
            }
        } catch (FileNotFoundException e) {
            sRet = sErrorPrefix + "Callback file creation error [rebt] call failed " + e.getMessage();
            e.printStackTrace();
        } catch (IOException e) {
            sRet = sErrorPrefix + "Callback file error [rebt] call failed " + e.getMessage();
            e.printStackTrace();
        }

        try {
            // Tell all of the data channels we are rebooting
            ((ASMozStub)this.contextWrapper).SendToDataChannel("Rebooting ...");

            pProc = Runtime.getRuntime().exec(this.getSuArgs("reboot"));
            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            outThrd.joinAndStopRedirect(10000);
        } catch (IOException e) {
            sRet = e.getMessage();
            e.printStackTrace();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        return (sRet);
        }

    private String [] getSuArgs(String cmdString)
        {
        String [] theArgs = new String [3];
        theArgs[0] = "su";
        theArgs[1] = "-c";
        // as a security measure, ICS and later resets LD_LIBRARY_PATH. reset
        // it here when executing the command
        theArgs[2] = "LD_LIBRARY_PATH=/vendor/lib:/system/lib " + cmdString;
        return theArgs;
        }

    public String UnInstallApp(String sApp, OutputStream out, boolean reboot)
        {
        String sRet = "";

        try
            {
            if (reboot == true) {
                pProc = Runtime.getRuntime().exec(this.getSuArgs("pm uninstall " + sApp + ";reboot;exit"));
            } else {
                pProc = Runtime.getRuntime().exec(this.getSuArgs("pm uninstall " + sApp + ";exit"));
            }

            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            try {
                outThrd.joinAndStopRedirect(60000);
                int nRet = pProc.exitValue();
                sRet = "\nuninst complete [" + nRet + "]";
                }
            catch (IllegalThreadStateException itse) {
                itse.printStackTrace();
                sRet = "\nuninst command timed out";
                }
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
        File    srcFile = new File(sApp);

        try
            {
            pProc = Runtime.getRuntime().exec(this.getSuArgs("pm install -r " + sApp + " Cleanup;exit"));
            RedirOutputThread outThrd3 = new RedirOutputThread(pProc, out);
            outThrd3.start();
            try {
                outThrd3.joinAndStopRedirect(60000);
                int nRet3 = pProc.exitValue();
                sRet = "\ninstallation complete [" + nRet3 + "]";
                }
            catch (IllegalThreadStateException itse) {
                itse.printStackTrace();
                sRet = "\npm install command timed out";
            }
            try {
                out.write(sRet.getBytes());
                out.flush();
                }
            catch (IOException e1)
                {
                e1.printStackTrace();
                }
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

    public String StrtUpdtOMatic(String sPkgName, String sPkgFileName, String sCallBackIP, String sCallBackPort)
        {
        String sRet = "";

        Context ctx = contextWrapper.getApplicationContext();
        PackageManager pm = ctx.getPackageManager();

        Intent prgIntent = new Intent();
        prgIntent.setPackage("com.mozilla.watcher");

        try {
            PackageInfo pi = pm.getPackageInfo("com.mozilla.watcher", PackageManager.GET_SERVICES | PackageManager.GET_INTENT_FILTERS);
            ServiceInfo [] si = pi.services;
            for (int i = 0; i < si.length; i++)
                {
                ServiceInfo s = si[i];
                if (s.name.length() > 0)
                    {
                    prgIntent.setClassName(s.packageName, s.name);
                    break;
                    }
                }
            }
        catch (NameNotFoundException e)
            {
            e.printStackTrace();
            sRet = sErrorPrefix + "watcher is not properly installed";
            return(sRet);
            }

        prgIntent.putExtra("command", "updt");
        prgIntent.putExtra("pkgName", sPkgName);
        prgIntent.putExtra("pkgFile", sPkgFileName);
        prgIntent.putExtra("reboot", true);

        try
            {
            if ((sCallBackIP != null) && (sCallBackPort != null) &&
                (sCallBackIP.length() > 0) && (sCallBackPort.length() > 0))
                {
                FileOutputStream fos = ctx.openFileOutput("update.info", Context.MODE_WORLD_READABLE | Context.MODE_WORLD_WRITEABLE);
                String sBuffer = sCallBackIP + "," + sCallBackPort + "\rupdate started " + sPkgName + " " + sPkgFileName + "\r";
                fos.write(sBuffer.getBytes());
                fos.flush();
                fos.close();
                fos = null;
                prgIntent.putExtra("outFile", ctx.getFilesDir() + "/update.info");
                }
            else {
                if (prgIntent.hasExtra("outFile")) {
                    System.out.println("outFile extra unset from intent");
                    prgIntent.removeExtra("outFile");
                }
            }

            ComponentName cn = contextWrapper.startService(prgIntent);
            if (cn != null)
                sRet = "exit";
            else
                sRet = sErrorPrefix + "Unable to use watcher service";
            }
        catch(ActivityNotFoundException anf)
            {
            sRet = sErrorPrefix + "Activity Not Found Exception [updt] call failed";
            anf.printStackTrace();
            }
        catch (FileNotFoundException e)
            {
            sRet = sErrorPrefix + "File creation error [updt] call failed";
            e.printStackTrace();
            }
        catch (IOException e)
            {
            sRet = sErrorPrefix + "File error [updt] call failed";
            e.printStackTrace();
            }

        ctx = null;

        return (sRet);
        }

    public String StartJavaPrg(String [] sArgs, Intent preIntent)
        {
        String sRet = "";
        String sArgList = "";
        String sUrl = "";
//        String sRedirFileName = "";
        Intent prgIntent = null;

        Context ctx = contextWrapper.getApplicationContext();
        PackageManager pm = ctx.getPackageManager();

        if (preIntent == null)
            prgIntent = new Intent();
        else
            prgIntent = preIntent;

        prgIntent.setPackage(sArgs[0]);
        prgIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try {
            PackageInfo pi = pm.getPackageInfo(sArgs[0], PackageManager.GET_ACTIVITIES | PackageManager.GET_INTENT_FILTERS);
            ActivityInfo [] ai = pi.activities;
            for (int i = 0; i < ai.length; i++)
                {
                ActivityInfo a = ai[i];
                if (a.name.length() > 0)
                    {
                    prgIntent.setClassName(a.packageName, a.name);
                    break;
                    }
                }
            }
        catch (NameNotFoundException e)
            {
            e.printStackTrace();
            }

        if (sArgs.length > 1)
            {
            if (sArgs[0].contains("android.browser"))
                prgIntent.setAction(Intent.ACTION_VIEW);
            else
                prgIntent.setAction(Intent.ACTION_MAIN);

            if (sArgs[0].contains("fennec") || sArgs[0].contains("firefox"))
                {
                sArgList = "";
                sUrl = "";

                for (int lcv = 1; lcv < sArgs.length; lcv++)
                    {
                    if (sArgs[lcv].contains("://"))
                        {
                        prgIntent.setAction(Intent.ACTION_VIEW);
                        sUrl = sArgs[lcv];
                        }
                    else
                        {
                        if (sArgs[lcv].equals(">"))
                            {
                            lcv++;
                            if (lcv < sArgs.length)
                                lcv++;
//                                sRedirFileName = sArgs[lcv++];
                            }
                        else
                            sArgList += " " + sArgs[lcv];
                        }
                    }

                if (sArgList.length() > 0)
                    prgIntent.putExtra("args", sArgList.trim());

                if (sUrl.length() > 0)
                    prgIntent.setData(Uri.parse(sUrl.trim()));
                }
            else
                {
                for (int lcv = 1; lcv < sArgs.length; lcv++)
                    sArgList += " " + sArgs[lcv];

                prgIntent.setData(Uri.parse(sArgList.trim()));
                }
            }
        else
            {
            prgIntent.setAction(Intent.ACTION_MAIN);
            }

        try
            {
            contextWrapper.startActivity(prgIntent);
            FindProcThread findProcThrd = new FindProcThread(contextWrapper, sArgs[0]);
            findProcThrd.start();
            findProcThrd.join(7000);
            if (!findProcThrd.bFoundIt && !findProcThrd.bStillRunning) {
                sRet = "Unable to start " + sArgs[0] + "";
                }
            }
        catch(ActivityNotFoundException anf)
            {
            anf.printStackTrace();
            }
        catch (InterruptedException e) {
            e.printStackTrace();
        }

        ctx = null;
        return (sRet);
        }

    public String StartPrg(String [] progArray, OutputStream out, boolean startAsRoot)
        {
        String sRet = "";
        int    lcv = 0;

        try {
            if (startAsRoot)
                {
                    // we need to requote the program string here, in case
                    // there's spaces or other characters which need quoting
                    // before being passed to su
                    List<String> quotedProgList = new ArrayList<String>();
                    for (String arg : progArray)
                        {
                            String quotedArg = arg;
                            quotedArg = quotedArg.replace("\"", "\\\"");
                            quotedArg = quotedArg.replace("\'", "\\\'");
                            if (quotedArg.contains(" "))
                                {
                                    quotedArg = "\"" + quotedArg + "\"";
                                }
                            quotedProgList.add(quotedArg);
                        }
                    pProc = Runtime.getRuntime().exec(this.getSuArgs(TextUtils.join(" ", quotedProgList)));
                }
            else
                {
                    pProc = Runtime.getRuntime().exec(progArray);
                }
            RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
            outThrd.start();
            while (lcv < 30) {
                try {
                    outThrd.join(10000);
                    int nRetCode = pProc.exitValue();
                    sRet = "return code [" + nRetCode + "]";
                    break;
                    }
                catch (IllegalThreadStateException itse) {
                    lcv++;
                    }
                }
            outThrd.stopRedirect();
            }
        catch (IOException e)
            {
            e.printStackTrace();
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            sRet = "Timed out!";
            }

        return (sRet);
        }

    public String StartPrg2(String [] progArray, OutputStream out, String cwd, boolean startAsRoot)
        {
        String sRet = "";

        int    nArraySize = 0;
        int    nArgs = progArray.length - 1; // 1st arg is the environment string
        int    lcv    = 0;
        int    temp = 0;

        String sEnvString = progArray[0];

        if (!sEnvString.contains("=") && (sEnvString.length() > 0))
            {
            if (sEnvString.contains("/") || sEnvString.contains("\\") || !sEnvString.contains("."))
                sRet = StartPrg(progArray, out, startAsRoot);
            else
                sRet = StartJavaPrg(progArray, null);
            return(sRet);
            }

        // Set up command line args stripping off the environment string
        String [] theArgs = new String [nArgs];
        for (lcv = 0; lcv < nArgs; lcv++)
            {
            theArgs[lcv] = progArray[lcv + 1];
            }

        try
            {
            String [] envStrings = sEnvString.split(",");
            Map<String, String> newEnv = new HashMap<String, String>();

            for (lcv = 0; lcv < envStrings.length; lcv++)
                {
                temp = envStrings[lcv].indexOf("=");
                if (temp > 0)
                    {
                    newEnv.put(    envStrings[lcv].substring(0, temp),
                                envStrings[lcv].substring(temp + 1, envStrings[lcv].length()));
                    }
                }

            Map<String, String> sysEnv = System.getenv();

            nArraySize = sysEnv.size();

            for (Map.Entry<String, String> entry : newEnv.entrySet())
                {
                if (!sysEnv.containsKey(entry.getKey()))
                    {
                    nArraySize++;
                    }
                }

            String[] envArray = new String[nArraySize];

            int        i = 0;
            int        offset;
            String    sKey = "";
            String     sValue = "";

            for (Map.Entry<String, String> entry : sysEnv.entrySet())
                {
                sKey = entry.getKey();
                if (newEnv.containsKey(sKey))
                    {
                    sValue = newEnv.get(sKey);
                    if ((offset = sValue.indexOf("$" + sKey)) != -1)
                        {
                        envArray[i++] = sKey +
                                        "=" +
                                        sValue.substring(0, offset) +
                                        entry.getValue() +
                                        sValue.substring(offset + sKey.length() + 1);
                        }
                    else
                        envArray[i++] = sKey + "=" + sValue;
                    newEnv.remove(sKey);
                    }
                else
                    envArray[i++] = entry.getKey() + "=" + entry.getValue();
                }

            for (Map.Entry<String, String> entry : newEnv.entrySet())
                {
                envArray[i++] = entry.getKey() + "=" + entry.getValue();
                }

            if (theArgs[0].contains("/") || theArgs[0].contains("\\") || !theArgs[0].contains("."))
                {
                if (cwd != null)
                    {
                    File f = new File(cwd);
                    pProc = Runtime.getRuntime().exec(theArgs, envArray, f);
                    }
                else
                    {
                    pProc = Runtime.getRuntime().exec(theArgs, envArray);
                    }

                RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
                outThrd.start();

                lcv = 0;

                while (lcv < 30) {
                    try {
                        outThrd.join(10000);
                        int nRetCode = pProc.exitValue();
                        sRet = "return code [" + nRetCode + "]";
                        lcv = 30;
                        }
                    catch (IllegalThreadStateException itse) {
                        lcv++;
                        }
                    }
                outThrd.stopRedirect();
                }
            else
                {
                Intent preIntent = new Intent();
                for (lcv = 0; lcv < envArray.length; lcv++)
                    {
                    preIntent.putExtra("env" + lcv, envArray[lcv]);
                    }
                sRet = StartJavaPrg(theArgs, preIntent);
                }
            }
        catch(UnsupportedOperationException e)
            {
            if (e != null)
                e.printStackTrace();
            }
        catch(ClassCastException e)
            {
            if (e != null)
                e.printStackTrace();
            }
        catch(IllegalArgumentException e)
            {
            if (e != null)
                e.printStackTrace();
            }
        catch(NullPointerException e)
            {
            if (e != null)
                e.printStackTrace();
            }
        catch (IOException e)
            {
            e.printStackTrace();
            }
        catch (InterruptedException e)
            {
            e.printStackTrace();
            sRet = "Timed out!";
            }

        return (sRet);
        }

    public String ChmodDir(String sDir)
        {
        String sRet = "";
        int nFiles = 0;
        String sSubDir = null;
        String sTmpDir = fixFileName(sDir);

        File dir = new File(sTmpDir);

        if (dir.isDirectory()) {
            sRet = "Changing permissions for " + sTmpDir;

            File [] files = dir.listFiles();
            if (files != null) {
                if ((nFiles = files.length) > 0) {
                    for (int lcv = 0; lcv < nFiles; lcv++) {
                        if (files[lcv].isDirectory()) {
                            sSubDir = files[lcv].getAbsolutePath();
                            sRet += "\n" + ChmodDir(sSubDir);
                        }
                        else {
                            // set the new file's permissions to rwxrwxrwx, if possible
                            try {
                                Process pProc = Runtime.getRuntime().exec("chmod 777 "+files[lcv]);
                                RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
                                outThrd.start();
                                outThrd.joinAndStopRedirect(5000);
                                sRet += "\n\tchmod " + files[lcv].getName() + " ok";
                            } catch (InterruptedException e) {
                                sRet += "\n\ttimeout waiting for chmod " + files[lcv].getName();
                            } catch (IOException e) {
                                sRet += "\n\tunable to chmod " + files[lcv].getName();
                            }
                        }
                    }
                }
                else
                    sRet += "\n\t<empty>";
                }
            }

        // set the new directory's (or file's) permissions to rwxrwxrwx, if possible
        try {
            Process pProc = Runtime.getRuntime().exec("chmod 777 "+sTmpDir);
            RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
            outThrd.start();
            outThrd.joinAndStopRedirect(5000);
            sRet += "\n\tchmod " + sTmpDir + " ok";
        } catch (InterruptedException e) {
            sRet += "\n\ttimeout waiting for chmod " + sTmpDir;
        } catch (IOException e) {
            sRet += "\n\tunable to chmod " + sTmpDir;
        }

        return(sRet);
        }

    private String PrintUsage()
        {
        String sRet =
            "run [cmdline]                   - start program no wait\n" +
            "exec [env pairs] [cmdline]      - start program no wait optionally pass env\n" +
            "                                  key=value pairs (comma separated)\n" +
            "execcwd [env pairs] [cmdline]   - start program from specified directory\n" +
            "execsu [env pairs] [cmdline]    - start program as privileged user\n" +
            "execcwdsu [env pairs] [cmdline] - start program from specified directory as privileged user\n" +
            "kill [program name]             - kill program no path\n" +
            "killall                         - kill all processes started\n" +
            "ps                              - list of running processes\n" +
            "info                            - list of device info\n" +
            "        [os]                    - os version for device\n" +
            "        [id]                    - unique identifier for device\n" +
            "        [uptime]                - uptime for device\n" +
            "        [uptimemillis]          - uptime for device in milliseconds\n" +
            "        [systime]               - current system time\n" +
            "        [screen]                - width, height and bits per pixel for device\n" +
            "        [memory]                - physical, free, available, storage memory\n" +
            "                                  for device\n" +
            "        [processes]             - list of running processes see 'ps'\n" +
            "deadman timeout                 - set the duration for the deadman timer\n" +
            "alrt [on/off]                   - start or stop sysalert behavior\n" +
            "disk [arg]                      - prints disk space info\n" +
            "cp file1 file2                  - copy file1 to file2\n" +
            "time file                       - timestamp for file\n" +
            "hash file                       - generate hash for file\n" +
            "cd directory                    - change cwd\n" +
            "cat file                        - cat file\n" +
            "cwd                             - display cwd\n" +
            "mv file1 file2                  - move file1 to file2\n" +
            "push filename                   - push file to device\n" +
            "rm file                         - delete file\n" +
            "rmdr directory                  - delete directory even if not empty\n" +
            "mkdr directory                  - create directory\n" +
            "dirw directory                  - tests whether the directory is writable\n" +
            "isdir directory                 - test whether the directory exists\n" +
            "chmod directory|file            - change permissions of directory and contents (or file) to 777\n" +
            "stat processid                  - stat process\n" +
            "dead processid                  - print whether the process is alive or hung\n" +
            "mems                            - dump memory stats\n" +
            "ls                              - print directory\n" +
            "tmpd                            - print temp directory\n" +
            "ping [hostname/ipaddr]          - ping a network device\n" +
            "unzp zipfile destdir            - unzip the zipfile into the destination dir\n" +
            "zip zipfile src                 - zip the source file/dir into zipfile\n" +
            "rebt                            - reboot device\n" +
            "inst /path/filename.apk         - install the referenced apk file\n" +
            "uninst packagename              - uninstall the referenced package and reboot\n" +
            "uninstall packagename           - uninstall the referenced package without a reboot\n" +
            "updt pkgname pkgfile            - unpdate the referenced package\n" +
            "clok                            - the current device time expressed as the" +
            "                                  number of millisecs since epoch\n" +
            "settime date time               - sets the device date and time\n" +
            "                                  (YYYY/MM/DD HH:MM:SS)\n" +
            "tzset timezone                  - sets the device timezone format is\n" +
            "                                  GMTxhh:mm x = +/- or a recognized Olsen string\n" +
            "tzget                           - returns the current timezone set on the device\n" +
            "rebt                            - reboot device\n" +
            "adb ip|usb                      - set adb to use tcp/ip on port 5555 or usb\n" +
            "quit                            - disconnect SUTAgent\n" +
            "exit                            - close SUTAgent\n" +
            "ver                             - SUTAgent version\n" +
            "help                            - you're reading it";
        return (sRet);
        }
}
