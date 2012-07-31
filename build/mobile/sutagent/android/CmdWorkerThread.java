/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.SocketTimeoutException;

import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

import android.util.Log;

// import com.mozilla.SUTAgentAndroid.DoCommand;

public class CmdWorkerThread extends Thread
{
    private RunCmdThread theParent = null;
    private Socket socket    = null;
    private String prompt = null;
    boolean bListening    = true;

    public CmdWorkerThread(RunCmdThread theParent, Socket workerSocket)
        {
        super("CmdWorkerThread");
        this.theParent = theParent;
        this.socket = workerSocket;
        byte pr [] = new byte [3];
        pr[0] = '$';
        pr[1] = '>';
        pr[2] = 0;
        prompt = new String(pr,0,3);
        }

    public void StopListening()
        {
        bListening = false;
        }

    private String readLine(BufferedInputStream in)
        {
        String sRet = "";
        int nByte = 0;
        char cChar = 0;

        try
            {
            nByte = in.read();
            while (nByte != -1)
                {
                cChar = ((char)(nByte & 0xFF));
                if ((cChar != '\r') && (cChar != '\n'))
                    sRet += cChar;
                else
                    break;
                nByte = in.read();
                }

            if ((in.available() > 0) && (cChar != '\n'))
                {
                in.mark(1024);
                nByte = in.read();

                if (nByte != -1)
                    {
                    cChar = ((char)(nByte & 0xFF));
                    if (cChar != '\n')
                        {
                        in.reset();
                        }
                    }
                }
            }
        catch (IOException e)
            {
            // TODO Auto-generated catch block
            e.printStackTrace();
            }

        if (sRet.length() == 0)
            sRet = null;

        return(sRet);
        }

    public void run()
        {
        try {
            OutputStream cmdOut = socket.getOutputStream();
            InputStream cmdIn = socket.getInputStream();
            PrintWriter out = new PrintWriter(cmdOut, true);
            BufferedInputStream in = new BufferedInputStream(cmdIn);
            String inputLine, outputLine;
            DoCommand dc = new DoCommand(theParent.svc);

            SUTAgentAndroid.logToFile(dc.GetTestRoot(), dc.GetSystemTime(), "CmdWorkerThread starts: "+getId());

            int nAvail = cmdIn.available();
            cmdIn.skip(nAvail);

            out.print(prompt);
            out.flush();

            while (bListening)
                {
                if (!(in.available() > 0))
                    {
                    socket.setSoTimeout(500);
                    try {
                        int nRead = cmdIn.read();
                        if (nRead == -1)
                            {
                            bListening = false;
                            continue;
                            }
                        else
                            {
                            inputLine = ((char)nRead) + "";
                            socket.setSoTimeout(120000);
                            }
                        }
                    catch(SocketTimeoutException toe)
                        {
                        continue;
                        }
                    }
                else
                    inputLine = "";

                if ((inputLine += readLine(in)) != null)
                    {
                    String message = String.format("%s : %s",
                                     socket.getInetAddress().getHostAddress(), inputLine);
                    SUTAgentAndroid.logToFile(dc.GetTestRoot(), dc.GetSystemTime(), message);

                    outputLine = dc.processCommand(inputLine, out, in, cmdOut);
                    if (outputLine.length() > 0)
                        {
                        out.print(outputLine + "\n" + prompt);
                        }
                    else
                        out.print(prompt);
                    out.flush();
                    if (outputLine.equals("exit"))
                        {
                        theParent.StopListening();
                        bListening = false;
                        }
                    if (outputLine.equals("quit"))
                        {
                        bListening = false;
                        }
                    outputLine = null;
                    System.gc();
                    }
                else
                    break;
                }
            out.close();
            out = null;
            in.close();
            in = null;
            socket.close();
            SUTAgentAndroid.logToFile(dc.GetTestRoot(), dc.GetSystemTime(), "CmdWorkerThread ends: "+getId());
        }
    catch (IOException e)
        {
        // TODO Auto-generated catch block
        e.printStackTrace();
        }
    }
}
