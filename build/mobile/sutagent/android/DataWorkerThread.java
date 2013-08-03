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
import java.text.SimpleDateFormat;
import java.util.Calendar;

// import com.mozilla.SUTAgentAndroid.DoCommand;
import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

public class DataWorkerThread extends Thread
{
    private RunDataThread theParent = null;
    private Socket socket    = null;
    boolean bListening    = true;
    PrintWriter out = null;
    SimpleDateFormat sdf = null;

    public DataWorkerThread(RunDataThread theParent, Socket workerSocket)
        {
        super("DataWorkerThread");
        this.theParent = theParent;
        this.socket = workerSocket;
        this.sdf = new SimpleDateFormat("yyyyMMdd-HH:mm:ss");
        }

    public void StopListening()
        {
        bListening = false;
        }

    public void SendString(String strToSend)
        {
        if (this.out != null)
            {
            Calendar cal = Calendar.getInstance();
            String strOut = sdf.format(cal.getTime());
            strOut += " " + strToSend + "\r\n";

            out.write(strOut);
            out.flush();
            }
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

            if (in.available() > 0)
                {
                in.mark(1024);
                nByte = in.read();

                while (nByte != -1)
                    {
                    cChar = ((char)(nByte & 0xFF));
                    if ((cChar == '\r') || (cChar == '\n'))
                        {
                        if (in.available() > 0)
                            {
                            in.mark(1024);
                            nByte = in.read();
                            }
                        else
                            nByte = -1;
                        }
                    else
                        {
                        in.reset();
                        break;
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
        String    sRet = "";
        long lEndTime = System.currentTimeMillis() + 60000;

        try {
            while(bListening)
                {
                OutputStream cmdOut = socket.getOutputStream();
                InputStream cmdIn = socket.getInputStream();
                this.out = new PrintWriter(cmdOut, true);
                BufferedInputStream in = new BufferedInputStream(cmdIn);
                String inputLine, outputLine;
                DoCommand dc = new DoCommand(theParent.svc);

                Calendar cal = Calendar.getInstance();
                sRet = sdf.format(cal.getTime());
                sRet += " trace output";

                out.println(sRet);
                out.flush();
                int nAvail = cmdIn.available();
                cmdIn.skip(nAvail);

                while (bListening)
                    {
                    if (System.currentTimeMillis() > lEndTime)
                        {
                        cal = Calendar.getInstance();
                        sRet = sdf.format(cal.getTime());
                        sRet += " Thump thump - " + SUTAgentAndroid.sUniqueID + "\r\n";

                        out.write(sRet);
                        out.flush();

                        lEndTime = System.currentTimeMillis() + 60000;
                        }

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
                                inputLine = (char)nRead + "";
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
                        outputLine = dc.processCommand(inputLine, out, in, cmdOut);
                        if (outputLine == null)
                            {
                            outputLine = "";
                            }
                        out.print(outputLine + "\n");
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
                }
            }
        catch (IOException e)
            {
            // TODO Auto-generated catch block
            e.printStackTrace();
            }
        }
}
