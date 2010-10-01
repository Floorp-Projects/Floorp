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

import java.net.ServerSocket;
import java.util.Timer;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
// import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.widget.Toast;

public class ASMozStub extends android.app.Service {
	
	private ServerSocket cmdChnl = null;
	private ServerSocket dataChnl = null;
	private Handler handler = new Handler();
	RunCmdThread runCmdThrd = null;
	RunDataThread runDataThrd = null;
	Thread monitor = null;
	Timer timer = null;
	
	@Override
	public IBinder onBind(Intent intent)
		{
		return null;
		}
	
	@Override
	public void onCreate() {
		super.onCreate();
		Toast.makeText(this, "Listener Service created...", Toast.LENGTH_LONG).show();
		}

	public void onStart(Intent intent, int startId) {
		super.onStart(intent, startId);
		
		try {
			cmdChnl = new ServerSocket(20701);
			runCmdThrd = new RunCmdThread(cmdChnl, this, handler);
			runCmdThrd.start();
			Toast.makeText(this, "Command channel port 20701 ...", Toast.LENGTH_LONG).show();
			
			dataChnl = new ServerSocket(20700);
			runDataThrd = new RunDataThread(dataChnl, this);
			runDataThrd.start();
			Toast.makeText(this, "Data channel port 20700 ...", Toast.LENGTH_LONG).show();
			} 
		catch (Exception e) {
//			Toast.makeText(getApplication().getApplicationContext(), e.toString(), Toast.LENGTH_LONG).show();
			}
		
		return;
		}
	
	public void onDestroy()
		{
		super.onDestroy();
		if (runCmdThrd.isAlive())
			{
			runCmdThrd.StopListening();
			}
		
		if (runDataThrd.isAlive())
			{
			runDataThrd.StopListening();
			}
		
		NotificationManager notificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
		notificationManager.cancel(1959);
		
		Toast.makeText(this, "Listener Service destroyed...", Toast.LENGTH_LONG).show();
		
		System.exit(0);
		}
	
	public void SendToDataChannel(String strToSend)
		{
		if (runDataThrd.isAlive())
			{
			runDataThrd.SendToDataChannel(strToSend);
			}
		}
}
