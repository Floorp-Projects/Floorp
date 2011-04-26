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

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.SocketTimeoutException;

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
        }
    catch (IOException e)
        {
        // TODO Auto-generated catch block
        e.printStackTrace();
        }
    }
}
