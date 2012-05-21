/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;

public class RunDataThread extends Thread
    {
    Timer heartBeatTimer;

    private ServerSocket SvrSocket = null;
    private Socket socket    = null;
    boolean bListening    = true;
    List<DataWorkerThread> theWorkers = new ArrayList<DataWorkerThread>();
    android.app.Service    svc = null;

    public RunDataThread(ServerSocket socket, android.app.Service service)
        {
        super("RunDataThread");
        this.SvrSocket = socket;
        this.svc = service;
        }

    public void StopListening()
        {
        bListening = false;
        }

    public void SendToDataChannel(String strToSend)
        {
        int nNumWorkers = theWorkers.size();
        for (int lcv = 0; lcv < nNumWorkers; lcv++)
            {
            if (theWorkers.get(lcv).isAlive())
                {
                theWorkers.get(lcv).SendString(strToSend);
                }
            }
        return;
        }

    public void run() {
        try {
            SvrSocket.setSoTimeout(5000);
            while (bListening)
                {
                try
                    {
                    socket = SvrSocket.accept();
                    DataWorkerThread theWorker = new DataWorkerThread(this, socket);
                    theWorker.start();
                    theWorkers.add(theWorker);
                    }
                catch (SocketTimeoutException toe)
                    {
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
            }
        catch (IOException e)
            {
//            Toast.makeText(SUTAgentAndroid.me.getApplicationContext(), e.getMessage(), Toast.LENGTH_LONG).show();
            e.printStackTrace();
            }
        return;
        }
    }
