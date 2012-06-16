/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.mozilla.SUTAgentAndroid.service;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;

public class RedirOutputThread extends Thread
    {
    OutputStream out;
    InputStream    sutErr;
    InputStream    sutOut;
    Process pProc;
    String    strOutput;
    int    nExitCode = -1;

    public RedirOutputThread(Process pProc, OutputStream out)
        {
        super("RedirOutputThread");
        if (pProc != null)
            {
            this.pProc = pProc;
            sutErr = pProc.getErrorStream(); // Stderr
            sutOut = pProc.getInputStream(); // Stdout
            }
        if (out != null)
            this.out = out;

        strOutput = "";
        }

    public void run()
        {
        boolean bStillRunning = true;
        int    nBytesOut = 0;
        int nBytesErr = 0;
        int nBytesRead = 0;
        PrintWriter pOut = null;
        byte[] buffer = new byte[1024];

        if (out != null)
            pOut = new PrintWriter(out);
        else
            bStillRunning = true;

        while (bStillRunning)
            {
            try
                {
                // If there's no output to collect, sleep for a while
                // rather than checking again immediately, to avoid 
                // using up cpu capacity in a tight loop.
                if (sutOut.available() == 0 && sutErr.available() == 0)
                    {
                    Thread.sleep(50);
                    }
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
                        if (pOut != null)
                            {
                            pOut.print(sRep);
                            pOut.flush();
                            }
                        else
                            strOutput += sRep;
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
                        if (pOut != null)
                            {
                            pOut.print(sRep);
                            pOut.flush();
                            }
                        else
                            strOutput += sRep;
                        }
                    }

                bStillRunning = (IsProcRunning(pProc) || (sutOut.available() > 0) || (sutErr.available() > 0));
                }
            catch (IOException e)
                {
                e.printStackTrace();
                }
            catch (java.lang.IllegalArgumentException e)
                {
                // Bug 743766: InputStream.available() unexpectedly throws this sometimes
                e.printStackTrace();
                }
            catch (InterruptedException e) 
                {
                e.printStackTrace();
                }
            }

        pProc.destroy();
        buffer = null;
        System.gc();
        }

    private boolean IsProcRunning(Process pProc)
        {
        boolean bRet = false;

        try
            {
            nExitCode = pProc.exitValue();
            }
        catch (IllegalThreadStateException z)
            {
            nExitCode = -1;
            bRet = true;
            }

        return(bRet);
        }
    }
