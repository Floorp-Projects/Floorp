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

package com.mozilla.SUTAgentAndroid;

import java.io.File;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;
import java.util.List;
import java.util.Timer;
import com.mozilla.SUTAgentAndroid.service.ASMozStub;
import com.mozilla.SUTAgentAndroid.service.DoCommand;

// import dalvik.system.VMRuntime;
import android.app.Activity;
import android.app.KeyguardManager;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
// import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.WifiLock;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Debug;
import android.os.PowerManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class SUTAgentAndroid extends Activity 
	{
	public static final int START_PRG = 1959;
	MenuItem mExitMenuItem;
	Timer timer = null;
	
//	public static SUTAgentAndroid me = null;
    public static String sUniqueID = null;
    public static String sLocalIPAddr = null;
    public static String sACStatus = null;
    public static String sPowerStatus = null;
    public static int	nChargeLevel = 0;
    public static int	nBatteryTemp = 0;
    
	String lineSep = System.getProperty("line.separator");
    public PrintWriter dataOut = null;
    
    private static boolean bNetworkingStarted = false;
    private static String RegSvrIPAddr = "";
    private static String RegSvrIPPort = "";
    private static String HardwareID = "";
    private static String Pool = "";
    private static String sRegString = "";
    
    private WifiLock wl = null;
    private PowerManager.WakeLock pwl = null;
    
    private BroadcastReceiver battReceiver = null;
//    private ComponentName service = null;

	public boolean onCreateOptionsMenu(Menu menu)
		{
		mExitMenuItem = menu.add("Exit");
		mExitMenuItem.setIcon(android.R.drawable.ic_menu_close_clear_cancel);
		return super.onCreateOptionsMenu(menu);
		}

	public boolean onMenuItemSelected(int featureId, MenuItem item)
		{
		if (item == mExitMenuItem)
			{
			finish();
			}
		return super.onMenuItemSelected(featureId, item);
		}
	
	public static String getRegSvrIPAddr()
		{
		return(RegSvrIPAddr);
		}
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState)
    	{
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

//        Debug.waitForDebugger();

//        long lHeapSize = VMRuntime.getRuntime().getMinimumHeapSize();
//        lHeapSize = 16000000;
//        VMRuntime.getRuntime().setMinimumHeapSize(lHeapSize);
        
        // Keep phone from locking or remove lock on screen
        KeyguardManager km = (KeyguardManager)getSystemService(Context.KEYGUARD_SERVICE);
        if (km != null)
        	{
        	KeyguardManager.KeyguardLock kl = km.newKeyguardLock("SUTAgent");
        	if (kl != null)
        		kl.disableKeyguard();
        	}
        
        // No sleeping on the job
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null)
        	{
        	pwl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, "SUTAgent");
        	if (pwl != null)
        		pwl.acquire();
        	}
        
        DoCommand dc = new DoCommand(getApplication());
        
        // Get configuration settings from "ini" file
        File dir = getFilesDir();
        File iniFile = new File(dir, "SUTAgent.ini");
        String sIniFile = iniFile.getAbsolutePath();
        
        SUTAgentAndroid.RegSvrIPAddr = dc.GetIniData("Registration Server", "IPAddr", sIniFile);
        SUTAgentAndroid.RegSvrIPPort = dc.GetIniData("Registration Server", "PORT", sIniFile);
        SUTAgentAndroid.HardwareID = dc.GetIniData("Registration Server", "HARDWARE", sIniFile);
        SUTAgentAndroid.Pool = dc.GetIniData("Registration Server", "POOL", sIniFile);

        TextView  tv = (TextView) this.findViewById(R.id.Textview01);

        if (getLocalIpAddress() == null)
        	setUpNetwork(sIniFile);
        
//        me = this;
        
        WifiInfo wifi;
        WifiManager wifiMan = (WifiManager)getSystemService(Context.WIFI_SERVICE);
        String macAddress = "Unknown";
        if (wifiMan != null) 
        	{
			wifi = wifiMan.getConnectionInfo();
			if (wifi != null)
				{
				macAddress = wifi.getMacAddress();
				if (macAddress != null)
					sUniqueID = macAddress;
				}
        	}
        
        if (sUniqueID == null)
        	{
        	BluetoothAdapter ba = BluetoothAdapter.getDefaultAdapter();
        	if (ba.isEnabled() != true)
        		{
        		ba.enable();
        		while(ba.getState() != BluetoothAdapter.STATE_ON)
        			{
					try {
						Thread.sleep(1000);
						}
        			catch (InterruptedException e)
        				{
						e.printStackTrace();
        				}
        			}
        		
            	sUniqueID = ba.getAddress();
            	
            	ba.disable();
        		while(ba.getState() != BluetoothAdapter.STATE_OFF)
    				{
        			try {
        				Thread.sleep(1000);
						}
        			catch (InterruptedException e)
    					{
        				e.printStackTrace();
    					}
    				}
        		}
        	else
        		{
        		sUniqueID = ba.getAddress();
        		sUniqueID.toLowerCase();
        		}
        	}

        if (sUniqueID == null)
        	{
        	TelephonyManager mTelephonyMgr = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);
        	if (mTelephonyMgr != null)
        		{
        		sUniqueID = mTelephonyMgr.getDeviceId();
        		if (sUniqueID == null)
        			{
        			sUniqueID = "0011223344556677";
        			}
        		}
        	}
        
        sLocalIPAddr = getLocalIpAddress();
        Toast.makeText(getApplication().getApplicationContext(), "SUTAgent [" + sLocalIPAddr + "] ...", Toast.LENGTH_LONG).show();
        
        String sConfig = "Unique ID: " + sUniqueID + lineSep;
        sConfig += "OS Info" + lineSep;
        sConfig += "\t" + dc.GetOSInfo() + lineSep;
        sConfig += "Screen Info" + lineSep;
        int [] xy = dc.GetScreenXY();
        sConfig += "\t Width: " + xy[0] + lineSep;
        sConfig += "\t Height: " + xy[1] + lineSep;
        sConfig += "Memory Info" + lineSep;
        sConfig += "\t" + dc.GetMemoryInfo() + lineSep;
        sConfig += "Network Info" + lineSep;
        sConfig += "\tMac Address: " + macAddress + lineSep;
        sConfig += "\tIP Address: " + sLocalIPAddr + lineSep;
        
        sRegString = "NAME=" + sUniqueID;
        sRegString += "&IPADDR=" + sLocalIPAddr;
        sRegString += "&CMDPORT=" + 20701;
        sRegString += "&DATAPORT=" + 20700;
        sRegString += "&OS=Android-" + dc.GetOSInfo();
        sRegString += "&SCRNWIDTH=" + xy[0];
        sRegString += "&SCRNHEIGHT=" + xy[1];
        sRegString += "&BPP=8";
        sRegString += "&MEMORY=" + dc.GetMemoryConfig();
        sRegString += "&HARDWARE=" + HardwareID;
        sRegString += "&POOL=" + Pool;
        
        String sTemp = Uri.encode(sRegString,"=&");
        sRegString = "register " + sTemp;
        
        if (!bNetworkingStarted)
        	{
        	Thread thread = new Thread(null, doStartService, "StartServiceBkgnd");
        	thread.start();
//        	ToDoListening(1,300,dc);
        	bNetworkingStarted = true;
        	String sRegRet = "";
        	if (RegSvrIPAddr.length() > 0)
        		{
        		sRegRet = dc.RegisterTheDevice(RegSvrIPAddr, RegSvrIPPort, sRegString);
        		if (sRegRet.contains("ok"))
        			{
        			sConfig += "Registered with testserver" + lineSep;
        			sConfig += "\tIPAddress: " + RegSvrIPAddr + lineSep;
        			if (RegSvrIPPort.length() > 0)
        				sConfig += "\tPort: " + RegSvrIPPort + lineSep;
        			}
        		else
        			sConfig += "Not registered with testserver" + lineSep;
        		}
    		else
    			sConfig += "Not registered with testserver" + lineSep;
        	}
        
        tv.setText(sConfig);

        monitorBatteryState();
        
        final Button goButton = (Button) findViewById(R.id.Button01);
        goButton.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
    			finish();
                }
        	});
        }
    
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    	{
        if (requestCode == START_PRG)
        	{
           	Toast.makeText(getApplication().getApplicationContext(), "SUTAgent startprg finished ...", Toast.LENGTH_LONG).show();
        	}
    	}
    
    @Override
    public void onDestroy()
    	{
		super.onDestroy();
    	if (isFinishing())
    		{
    		Intent listenerSvc = new Intent(this, ASMozStub.class);
    		listenerSvc.setAction("com.mozilla.SUTAgentAndroid.service.LISTENER_SERVICE");
    		stopService(listenerSvc);
    		bNetworkingStarted = false;
    		
			unregisterReceiver(battReceiver);
	        KeyguardManager km = (KeyguardManager)getSystemService(Context.KEYGUARD_SERVICE);
	        if (km != null)
	        	{
	        	KeyguardManager.KeyguardLock kl = km.newKeyguardLock("SUTAgent");
	        	if (kl != null)
	        		kl.reenableKeyguard();
	        	}
	        
	        if (pwl != null)
	        	pwl.release();
	        
	    	if (wl != null)
	    		wl.release();
    		}
    	}
    
    private void monitorBatteryState() 
    	{
		battReceiver = new BroadcastReceiver()
			{
			public void onReceive(Context context, Intent intent)
				{
				StringBuilder sb = new StringBuilder();

				int rawlevel = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1); // charge level from 0 to scale inclusive
				int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1); // Max value for charge level
				int status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
				int health = intent.getIntExtra(BatteryManager.EXTRA_HEALTH, -1);
				boolean present = intent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false);
				int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1); //0 if the device is not plugged in; 1 if plugged into an AC power adapter; 2 if plugged in via USB.
//				int voltage = intent.getIntExtra(BatteryManager.EXTRA_VOLTAGE, -1); // voltage in millivolts
				nBatteryTemp = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1); // current battery temperature in tenths of a degree Centigrade
//				String technology = intent.getStringExtra(BatteryManager.EXTRA_TECHNOLOGY);

				nChargeLevel = -1;  // percentage, or -1 for unknown
				if (rawlevel >= 0 && scale > 0)
					{
					nChargeLevel = (rawlevel * 100) / scale;
					}
				
				if (plugged > 0)
					sACStatus = "ONLINE";
				else
					sACStatus = "OFFLINE";
				
				if (present == false)
					sb.append("NO BATTERY");
				else
					{
					if (nChargeLevel < 10)
						sb.append("Critical");
					else if (nChargeLevel < 33)
						sb.append("LOW");
					else if (nChargeLevel > 80)
						sb.append("HIGH");
					}
				
				if (BatteryManager.BATTERY_HEALTH_OVERHEAT == health)
					{
					sb.append("Overheated ");
					sb.append((((float)(nBatteryTemp))/10));
					sb.append("(°C)");
					}
				else
					{
					switch(status)
						{
						case BatteryManager.BATTERY_STATUS_UNKNOWN:
							// old emulator; maybe also when plugged in with no battery
							if (present == true)
								sb.append(" UNKNOWN");
							break;
						case BatteryManager.BATTERY_STATUS_CHARGING:
							sb.append(" CHARGING");
							break;
						case BatteryManager.BATTERY_STATUS_DISCHARGING:
							sb.append(" DISCHARGING");
							break;
						case BatteryManager.BATTERY_STATUS_NOT_CHARGING:
							sb.append(" NOTCHARGING");
							break;
						case BatteryManager.BATTERY_STATUS_FULL:
							sb.append(" FULL");
							break;
						default:
							if (present == true)
								sb.append("Unknown");
							break;
						}
					}
				
				sPowerStatus = sb.toString();
				}
			};
			
		IntentFilter battFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
		registerReceiver(battReceiver, battFilter);
    	}
 
    public boolean setUpNetwork(String sIniFile)
    	{
    	boolean	bRet = false;
    	int	lcv	= 0;
    	int	lcv2 = 0;
    	WifiManager wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
    	WifiConfiguration wc = new WifiConfiguration();
    	DoCommand tmpdc = new DoCommand(getApplication());
    	
    	String ssid = tmpdc.GetIniData("Network Settings", "SSID", sIniFile);
    	String auth = tmpdc.GetIniData("Network Settings", "AUTH", sIniFile);
    	String encr = tmpdc.GetIniData("Network Settings", "ENCR", sIniFile);
    	String key = tmpdc.GetIniData("Network Settings", "KEY", sIniFile);
    	String eap = tmpdc.GetIniData("Network Settings", "EAP", sIniFile);
    	String adhoc = tmpdc.GetIniData("Network Settings", "ADHOC", sIniFile);
    	
		Toast.makeText(getApplication().getApplicationContext(), "Starting and configuring network", Toast.LENGTH_LONG).show();
/*		
		ContentResolver cr = getContentResolver();
		int nRet;
		try {
			nRet = Settings.System.getInt(cr, Settings.System.WIFI_USE_STATIC_IP);
			String foo2 = "" + nRet;
		} catch (SettingNotFoundException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}
*/
/*    	
    	wc.SSID = "\"Mozilla-Build\"";
    	wc.preSharedKey  = "\"MozillaBuildQA500\"";
    	wc.hiddenSSID = true;
    	wc.status = WifiConfiguration.Status.ENABLED;    
    	wc.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
    	wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
    	wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
    	wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
    	wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
    	wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
    	wc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
*/    	
		wc.SSID = "\"" + ssid + "\"";
//    	wc.SSID = "\"Mozilla-G\"";
//    	wc.SSID = "\"Mozilla\"";
		
		if (auth.contentEquals("wpa2"))
			{
	    	wc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
			wc.preSharedKey  = null;
			}
		
		if (encr.contentEquals("aes"))
			{
	    	wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
	    	wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
			}
		
		if (eap.contentEquals("peap"))
			{
	    	wc.eap.setValue("PEAP");
	    	wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_EAP);
	    	wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.IEEE8021X);
			}
		
    	wc.hiddenSSID = false;
    	wc.status = WifiConfiguration.Status.ENABLED;
    	
    	wc.password.setValue("\"password\"");
    	wc.identity.setValue("\"bmoss@mozilla.com\"");
    	
    	if (!wifi.isWifiEnabled())
    		wifi.setWifiEnabled(true);
    	
    	while(wifi.getWifiState() != WifiManager.WIFI_STATE_ENABLED)
    		{
    		Thread.yield();
    		if (++lcv > 10000)
    			return(bRet);
    		}
    	
    	wl = wifi.createWifiLock(WifiManager.WIFI_MODE_FULL, "SUTAgent");
    	if (wl != null)
    		wl.acquire();
    	
    	WifiConfiguration	foo = null;
    	int					nNetworkID = -1;
    	
    	List<WifiConfiguration> connsLst =  wifi.getConfiguredNetworks();
    	int nConns = connsLst.size();
    	for (int i = 0; i < nConns; i++)
    		{
    		
    		foo = connsLst.get(i);
    		if (foo.SSID.equalsIgnoreCase(wc.SSID))
    			{
    			nNetworkID = foo.networkId;
    			wc.networkId = foo.networkId;
    			break;
    			}
    		}
    	
    	int res;
    	
    	if (nNetworkID != -1)
    		{
    		res = wifi.updateNetwork(wc);
    		}
    	else
    		{
        	res = wifi.addNetwork(wc);
    		}
    		
    	Log.d("WifiPreference", "add Network returned " + res );
    	
    	boolean b = wifi.enableNetwork(res, true);        
    	Log.d("WifiPreference", "enableNetwork returned " + b );
    	
    	wifi.saveConfiguration();
    	
    	WifiInfo wi = wifi.getConnectionInfo();
    	SupplicantState ss = wi.getSupplicantState();
    	
    	lcv = 0;
    	lcv2 = 0;
    	
    	while (ss.compareTo(SupplicantState.COMPLETED) != 0)
    		{
    		try {
				Thread.sleep(1000);
				}
    		catch (InterruptedException e)
    			{
				e.printStackTrace();
    			}
    		
    		if (wi != null)
    			wi = null;
    		if (ss != null)
    			ss = null;
    		wi = wifi.getConnectionInfo();
    		ss = wi.getSupplicantState();
    		if (++lcv > 60)
    			{
    			if (++lcv2 > 5)
    				{
    				Toast.makeText(getApplication().getApplicationContext(), "Unable to start and configure network", Toast.LENGTH_LONG).show();
    				return(bRet);
    				}
    			else
    				{
    				Toast.makeText(getApplication().getApplicationContext(), "Resetting wifi interface", Toast.LENGTH_LONG).show();
    		    	if (wl != null)
    		    		wl.release();
    	    		wifi.setWifiEnabled(false);
    	        	while(wifi.getWifiState() != WifiManager.WIFI_STATE_DISABLED)
    	    			{
    	        		Thread.yield();
    	    			}
    	        	
    	    		wifi.setWifiEnabled(true);
    	        	while(wifi.getWifiState() != WifiManager.WIFI_STATE_ENABLED)
	    				{
    	        		Thread.yield();
	    				}
    	        	b = wifi.enableNetwork(res, true);        
    	        	Log.d("WifiPreference", "enableNetwork returned " + b );
    	        	if (wl != null)
    	        		wl.acquire();
    	    		lcv = 0;
    				}
    			}
    		}
    	
    	lcv = 0;
    	while(getLocalIpAddress() == null)
    		{
    		if (++lcv > 10000)
    			return(bRet);
    		}
    	
		Toast.makeText(getApplication().getApplicationContext(), "Network started and configured", Toast.LENGTH_LONG).show();
    	bRet = true;
    	
    	return(bRet);
    	}
    
    private Runnable doStartService = new Runnable()
    	{
    	public void run()
    		{
			Intent listenerService = new Intent();
			listenerService.setAction("com.mozilla.SUTAgentAndroid.service.LISTENER_SERVICE");
			startService(listenerService);
//			service = startService(listenerService);
    		}
    	};
/*
    class ToDoListener extends TimerTask 
    	{
    	boolean 	bFirstRun = true;
    	DoCommand	dc = null;
    	
    	ToDoListener() {}
    	
    	ToDoListener(DoCommand dc)
    		{
    		this.dc = dc;
    		}
    	
		public void run ()
			{
			if (bFirstRun == true)
				{
				Intent listenerService = new Intent();
				listenerService.setAction("com.mozilla.SUTAgentAndroid.service.LISTENER_SERVICE");
				service = startService(listenerService);
				bFirstRun = false;
				}
			else
				{
				if (dc != null)
					{
					String sRet = this.dc.SendPing("www.mozilla.org", null);
					if (sRet.contains("3 received"))
						this.dc.StopAlert();
					else
						this.dc.StartAlert();
					sRet = null;
					System.gc();
					}
				}
			}
		}
	
	public void ToDoListening(int delay, int interval, DoCommand dc)
		{
		if (timer == null)
			timer = new Timer();
//		timer.scheduleAtFixedRate(new ToDoListener(dc), delay * 1000, interval * 1000);
//		timer.schedule(new ToDoListener(dc), delay * 1000);
		timer.schedule(new ToDoListener(), delay * 1000);
		}

	class DoHeartBeat extends TimerTask
		{
    	PrintWriter out;
	
    	DoHeartBeat(PrintWriter out)
			{
    		this.out = out;
			}
	
    	public void run ()
			{
    		String sRet = "";
    		
    		Calendar cal = Calendar.getInstance();
    		SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd-HH:mm:ss");
    		sRet = sdf.format(cal.getTime());
    		sRet += " Thump thump - " + sUniqueID + "\r\n";

    		out.write(sRet);
    		out.flush();
			}
		}

	public void StartHeartBeat(PrintWriter out)
		{
		// start the heartbeat
		this.dataOut = out;
		if (timer == null)
			timer = new Timer();
		timer.scheduleAtFixedRate(new DoHeartBeat(dataOut), 0, 60000);
		}
	
	public void StopHeartBeat()
		{
		// stop the heartbeat
		this.dataOut = null;
		if (timer != null)
			{
			timer.cancel();
			timer.purge();
			timer = null;
			System.gc();
			}
		}
*/	
	
    public String getLocalIpAddress()
		{
		try
    		{
			for (Enumeration<NetworkInterface> en = NetworkInterface.getNetworkInterfaces(); en.hasMoreElements();) 
        		{
				NetworkInterface intf = en.nextElement();
				for (Enumeration<InetAddress> enumIpAddr = intf.getInetAddresses(); enumIpAddr.hasMoreElements();) 
            		{
					InetAddress inetAddress = enumIpAddr.nextElement();
					if (!inetAddress.isLoopbackAddress()) 
                		{
						return inetAddress.getHostAddress().toString();
                		}
            		}
        		}
    		} 
		catch (SocketException ex)
    		{
			Toast.makeText(getApplication().getApplicationContext(), ex.toString(), Toast.LENGTH_LONG).show();
    		}
		return null;
		}
}