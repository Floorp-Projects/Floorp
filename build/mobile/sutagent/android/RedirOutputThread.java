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
