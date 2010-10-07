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
	private Socket socket	= null;
	boolean bListening	= true;
	List<DataWorkerThread> theWorkers = new ArrayList<DataWorkerThread>();
	android.app.Service	svc = null;
	
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
//			Toast.makeText(SUTAgentAndroid.me.getApplicationContext(), e.getMessage(), Toast.LENGTH_LONG).show();
			e.printStackTrace();
			}
		return;
		}
	}
