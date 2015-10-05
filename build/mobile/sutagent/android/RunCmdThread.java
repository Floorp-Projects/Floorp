/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;

import com.mozilla.SUTAgentAndroid.R;
import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;

public class RunCmdThread extends Thread
    {
    private ServerSocket SvrSocket = null;
    private Socket socket    = null;
    private Handler handler = null;
    boolean bListening    = true;
    boolean bNetError = false;
    List<CmdWorkerThread> theWorkers = new ArrayList<CmdWorkerThread>();
    android.app.Service    svc = null;

    public RunCmdThread(ServerSocket socket, android.app.Service service, Handler handler)
        {
        super("RunCmdThread");
        this.SvrSocket = socket;
        this.svc = service;
        this.handler = handler;
        }

    public void StopListening()
        {
        bListening = false;
        }

    public void run() {
        try {
            SvrSocket.setSoTimeout(5000);
            while (bListening)
                {
                try
                    {
                    socket = SvrSocket.accept();
                    CmdWorkerThread theWorker = new CmdWorkerThread(this, socket);
                    theWorker.start();
                    theWorkers.add(theWorker);
                    }
                catch (SocketTimeoutException toe)
                    {
                    continue;
                    }
                catch (IOException e)
                    {
                    e.printStackTrace();
                    continue;
                    }
                }

            int nNumWorkers = theWorkers.size();
            for (int lcv = 0; lcv < nNumWorkers; lcv++)
                {
                if (theWorkers.get(lcv).isAlive())
                    {
                    theWorkers.get(lcv).StopListening();
                    while(theWorkers.get(lcv).isAlive())
                        ;
                    }
                }

            theWorkers.clear();

            SvrSocket.close();

            svc.stopSelf();

//            SUTAgentAndroid.me.finish();
            }
        catch (IOException e)
            {
            e.printStackTrace();
            }
        return;
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
                        sleep(2000);
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

    private void SendNotification(String tickerText, String expandedText)
        {
        NotificationManager notificationManager = (NotificationManager)svc.getSystemService(Context.NOTIFICATION_SERVICE);

//        int icon = android.R.drawable.stat_notify_more;
//        int icon = R.drawable.ic_stat_first;
//        int icon = R.drawable.ic_stat_second;
//        int icon = R.drawable.ic_stat_neterror;
        int icon = R.drawable.ateamlogo;
        long when = System.currentTimeMillis();

        Context context = svc.getApplicationContext();

        // Intent to launch an activity when the extended text is clicked
        Intent intent2 = new Intent(svc, SUTAgentAndroid.class);
        PendingIntent launchIntent = PendingIntent.getActivity(context, 0, intent2, 0);


        Notification notification = new Notification.Builder(context)
            .setSmallIcon(icon)
            .setContentTitle(tickerText)
            .setContentText(expandedText)
            .setContentIntent(launchIntent)
            .setWhen(when)
            .build();

        notification.flags |= (Notification.FLAG_INSISTENT | Notification.FLAG_AUTO_CANCEL);
        notification.defaults |= Notification.DEFAULT_SOUND;
        notification.defaults |= Notification.DEFAULT_VIBRATE;
        notification.defaults |= Notification.DEFAULT_LIGHTS;

        notificationManager.notify(1959, notification);
        }

    private void CancelNotification()
        {
        NotificationManager notificationManager = (NotificationManager)svc.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(1959);
        }

    class doCancelNotification implements Runnable
        {
        public void run()
            {
            CancelNotification();
            }
        };

    class doSendNotification implements Runnable
        {
        private String sTitle = "";
        private String sBText = "";

        doSendNotification(String sTitle, String sBodyText)
            {
            this.sTitle = sTitle;
            this.sBText = sBodyText;
            }

        public void run()
            {
            SendNotification(sTitle, sBText);
            }
        };
}
