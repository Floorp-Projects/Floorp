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
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TimeZone;
import java.util.Timer;
import java.util.zip.Adler32;
import java.util.zip.CheckedInputStream;
import java.util.zip.CheckedOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.apache.commons.net.ftp.FTP;
import org.apache.commons.net.ftp.FTPClient;
import org.apache.commons.net.ftp.FTPFile;
import org.apache.commons.net.ftp.FTPReply;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;

import com.mozilla.SUTAgentAndroid.SUTAgentAndroid;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Debug;
import android.os.Environment;
import android.os.StatFs;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;

public class DoCommand {
	
	String lineSep = System.getProperty("line.separator");
	Process	pProc;
	OutputStream sutIn;
	InputStream	sutErr;
	InputStream	sutOut;
	AlertLooperThread alrt = null;
	ContextWrapper	contextWrapper = null;
	
	String	currentDir = "/";
	String	sErrorPrefix = "##AGENT-WARNING## ";
	
	private final String prgVersion = "SUTAgentAndroid Version 0.87";
	
	public enum Command
		{
		RUN ("run"),
		EXEC ("exec"),
		ENVRUN ("envrun"),
		KILL ("kill"),
		PS ("ps"),
		DEVINFO ("info"),
		OS ("os"),
		ID ("id"),
		UPTIME ("uptime"),
		SETTIME ("settime"),
		SYSTIME ("systime"),
		SCREEN ("screen"),
		MEMORY ("memory"),
		POWER ("power"),
		PROCESS ("process"),
		GETAPPROOT ("getapproot"),
		TESTROOT ("testroot"),
		ALRT ("alrt"),
		DISK ("disk"),
		CP ("cp"),
		TIME ("time"),
		HASH ("hash"),
		CD ("cd"),
		CAT ("cat"),
		CWD ("cwd"),
		MV ("mv"),
		PUSH ("push"),
		RM ("rm"),
		PRUNE ("rmdr"),
		MKDR ("mkdr"),
		DIRWRITABLE ("dirw"),
		DEAD ("dead"),
		MEMS ("mems"),
		LS ("ls"),
		TMPD ("tmpd"),
		PING ("ping"),
		REBT ("rebt"),
		UNZP ("unzp"),
		ZIP ("zip"),
		CLOK ("clok"),
		STAT ("stat"),
		QUIT ("quit"),
		EXIT ("exit"),
		HELP ("help"),
		FTPG ("ftpg"),
		FTPP ("ftpp"),
		INST ("inst"),
		UPDT ("updt"),
		UNINST ("uninst"),
		TEST ("test"),
		VER ("ver"),
		TZGET ("tzget"),
		TZSET ("tzset"),
		UNKNOWN ("unknown");
		
		private final String theCmd;
		
		Command(String theCmd) { this.theCmd = theCmd; }

		public String theCmd() {return theCmd;}
		
		public static Command getCmd(String sCmd)
			{
			Command retCmd = UNKNOWN;
			for (Command cmd : Command.values())
				{
				if (cmd.theCmd().equalsIgnoreCase(sCmd))
					{
					retCmd = cmd;
					break;
					}
				}
			return (retCmd);
			}
		}
	
	public DoCommand(ContextWrapper service)
		{
		this.contextWrapper = service;
		}
	
	public String processCommand(String theCmdLine, PrintWriter out, BufferedInputStream in, OutputStream cmdOut)
		{
		String 	strReturn = "";
		Command	cCmd = null;
		Command cSubCmd = null;
		
		String [] Argv = parseCmdLine2(theCmdLine);
		
		int Argc = Argv.length;
		
		cCmd = Command.getCmd(Argv[0]);
		
		switch(cCmd)
			{
			case VER:
				strReturn = prgVersion;
				break;
				
			case CLOK:
				strReturn = GetClok();
				break;
				
			case TZGET:
				strReturn = GetTimeZone();
				break;
				
			case TZSET:
				if (Argc == 2)
					strReturn = SetTimeZone(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for settz command!";
				break;
				
			case UPDT:
				strReturn = StartUpdateOMatic(Argv[1], Argv[2]);
				break;
			
			case SETTIME:
				strReturn = SetSystemTime(Argv[1], Argv[2], cmdOut);
				break;
			
			case CWD:
				try {
					strReturn = new java.io.File(currentDir).getCanonicalPath();
					} 
				catch (IOException e)
					{
					e.printStackTrace();
					}
				break;
				
			case CD:
				if (Argc == 2)
					strReturn = changeDir(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for cd command!";
				break;
			
			case LS:
				strReturn = PrintDir(((Argc > 1) ? Argv[1] : currentDir));
				break;
				
			case GETAPPROOT:
				if (Argc == 2)
					strReturn = GetAppRoot(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for getapproot command!";
				break;
				
			case TESTROOT:
				strReturn = GetTestRoot();
				break;
				
			case DEAD:
				if (Argc == 2)
					strReturn = (IsProcessDead(Argv[1]) ? (Argv[1] + " is hung or unresponsive") : (Argv[1] + " is ok"));
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for dead command!";
				break;
				
			case PS:
				strReturn = GetProcessInfo();
				break;
				
			case PUSH:
				if (Argc == 3)
					{
					long lArg = 0;
				    try
				    	{
				        lArg = Long.parseLong(Argv[2].trim());
				        System.out.println("long l = " + lArg);
				    	}
				    catch (NumberFormatException nfe)
				    	{
				        System.out.println("NumberFormatException: " + nfe.getMessage());
				    	}
					
					strReturn = Push(Argv[1], in, lArg);
					}
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for push command!";
				break;
				
			case INST:
				if (Argc >= 2)
					strReturn = InstallApp(Argv[1], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for inst command!";
				break;
				
			case UNINST:
				if (Argc >= 2)
					strReturn = UnInstallApp(Argv[1], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for inst command!";
				break;
				
			case ALRT:
				if (Argc == 2)
					{
					if (Argv[1].contentEquals("on"))
						{
						StartAlert();
						}
					else
						{
						StopAlert();
						}
					}
				else
					{
					strReturn = sErrorPrefix + "Wrong number of arguments for alrt command!";
					}
				break;
				
			case REBT:
				RunReboot(cmdOut);
				break;
				
			case TMPD:
				strReturn = GetTmpDir();
				break;
				
			case DEVINFO:
				if (Argc == 1)
					{
					strReturn += SUTAgentAndroid.sUniqueID;
					strReturn += "\n";
					strReturn += GetOSInfo();
					strReturn += "\n";
					strReturn += GetSystemTime();
					strReturn += "\n";
					strReturn += GetUptime();
					strReturn += "\n";
					strReturn += GetScreenInfo();
					strReturn += "\n";
					strReturn += GetMemoryInfo();
					strReturn += "\n";
					strReturn += GetPowerInfo();
					strReturn += "\n";
					strReturn += GetProcessInfo();
					}
				else
					{
					cSubCmd = Command.getCmd(Argv[1]);
					switch(cSubCmd)
						{
						case ID:
							strReturn = SUTAgentAndroid.sUniqueID;
							break;
							
						case SCREEN:
							strReturn = GetScreenInfo();
							break;
							
						case PROCESS:
							strReturn = GetProcessInfo();
							break;
							
						case OS:
							strReturn = GetOSInfo();
							break;
							
						case SYSTIME:
							strReturn = GetSystemTime();
							break;
							
						case UPTIME:
							strReturn = GetUptime();
							break;
							
						case MEMORY:
							strReturn = GetMemoryInfo();
							break;
							
						case POWER:
							strReturn += GetPowerInfo();
							break;
							
						default:
							break;
						}
					}
				break;
				
			case STAT:
				if (Argc == 2)
					strReturn = StatProcess(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for ping command!";
				break;
				
			case PING:
				if (Argc == 2)
					strReturn = SendPing(Argv[1], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for ping command!";
				break;
				
			case HASH:
				if (Argc == 2)
					strReturn = HashFile(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for hash command!";
				break;
				
			case PRUNE:
				if (Argc == 2)
					strReturn = PruneDir(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for prune command!";
				break;
				
			case FTPG:
				if (Argc == 4)
					strReturn = FTPGetFile(Argv[1], Argv[2], Argv[3], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for ftpg command!";
				break;
				
			case CAT:
				if (Argc == 2)
					strReturn = Cat(Argv[1], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for cat command!";
				break;
				
			case DIRWRITABLE:
				if (Argc == 2)
					strReturn = IsDirWritable(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for dirwritable command!";
				break;
				
			case TIME:
				if (Argc == 2)
					strReturn = PrintFileTimestamp(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for time command!";
				break;
				
			case MKDR:
				if (Argc == 2)
					strReturn = MakeDir(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for mkdr command!";
				break;
				
			case RM:
				if (Argc == 2)
					strReturn = RemoveFile(Argv[1]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for mkdr command!";
				break;
				
			case MV:
				if (Argc == 3)
					strReturn = Move(Argv[1], Argv[2]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for mv command!";
				break;
				
			case CP:
				if (Argc == 3)
					strReturn = CopyFile(Argv[1], Argv[2]);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for cp command!";
				break;
				
			case QUIT:
			case EXIT:
				strReturn = Argv[0];
				break;
				
			case TEST:
//				boolean bRet = false;
/*				
				Configuration userConfig = new Configuration();
				Settings.System.getConfiguration( contextWrapper.getContentResolver(), userConfig );
				Calendar cal = Calendar.getInstance( userConfig.locale);
				TimeZone ctz = cal.getTimeZone();
				String sctzLongName = ctz.getDisplayName();
				String pstzName = TimeZone.getDefault().getDisplayName();
*/
				String sTimeZoneName = GetTimeZone();
				
				TimeZone tz = TimeZone.getTimeZone("America/Los_Angeles");
				TimeZone tz2 = TimeZone.getTimeZone("GMT-08:00");
				int	nOffset = (-8 * 3600000);
				String [] zoneNames = TimeZone.getAvailableIDs(nOffset);
				int nNumMatches = zoneNames.length;
				TimeZone.setDefault(tz);
				
				String sOldTZ = System.setProperty("persist.sys.timezone", "America/Los_Angeles");
				
/*				
				byte[] buffer = new byte [4096];
				int	nRead = 0;
				long lTotalRead = 0;

				Context ctx = SUTAgentAndroid.me.getApplicationContext();

				FTPClient ftp = new FTPClient();
				try 
					{
					String strRet = "";
					int	reply = 0;
					FileOutputStream outStream = null;
					
					ftp.connect("ftp.mozilla.org");
					strRet = ftp.getReplyString();
					reply = ftp.getReplyCode();
					
				    if(!FTPReply.isPositiveCompletion(reply))
				    	{
				        ftp.disconnect();
				        System.err.println("FTP server refused connection.");
				        System.exit(1);
				        }
				    // transfer files
				    
				    ftp.login("anonymous", "b@t.com");
					strRet = ftp.getReplyString();
					reply = ftp.getReplyCode();
					
				    if(!FTPReply.isPositiveCompletion(reply))
				    	{
				        ftp.disconnect();
				        System.err.println("FTP server refused connection.");
				        System.exit(1);
				        }
				    
				    ftp.enterLocalPassiveMode();
				    
				    if (ftp.setFileType(FTP.BINARY_FILE_TYPE))
				    	{
				    	File root = Environment.getExternalStorageDirectory();
				    	if (root.canWrite())
				    		{
				    		File outFile = new File(root, "firefox-3.6b4.cab");
				    		outStream = new FileOutputStream(outFile);
				    		}
				    	else
				    		outStream = ctx.openFileOutput("firefox-3.6b4.cab", Context.MODE_WORLD_READABLE | Context.MODE_WORLD_WRITEABLE);
//				    	outStream = new FileOutputStream("/sdcard/firefox-3.6b4.cab");
				    	InputStream ftpIn = ftp.retrieveFileStream("pub/mozilla.org/firefox/releases/3.6b4/wince-arm/en-US/firefox-3.6b4.cab");
						while ((nRead = ftpIn.read(buffer)) != -1)
							{
							lTotalRead += nRead;
							outStream.write(buffer, 0, nRead);
							strRet = "\r" + lTotalRead + " bytes received";
							cmdOut.write(strRet.getBytes());
							cmdOut.flush();
							}
						
						ftpIn.close();
						boolean bRet = ftp.completePendingCommand();
						outStream.flush();

				    	/*				    	
				    	if (ftp.retrieveFile("pub/mozilla.org/firefox/releases/3.6b4/wince-arm/en-US/firefox-3.6b4.cab", outStream))
				    		{
				    		outStream.flush();
				    		}
				    	 * /				    		
			    		outStream.close();
						strRet = ftp.getReplyString();
						reply = ftp.getReplyCode();
				    	}
					strRet = ftp.getReplyString();
					reply = ftp.getReplyCode();
				    ftp.logout();
				    
				    strReturn = "\r\n" + strRet; 
					}
				catch (SocketException e)
					{
					// TODO Auto-generated catch block
					strReturn = e.getMessage();
					e.printStackTrace();
					}
				catch (IOException e)
					{
					// TODO Auto-generated catch block
					strReturn = e.getMessage();
					e.printStackTrace();
					}
*/				
//				strReturn = InstallApplication();
//				strReturn = InstallApp(Argv[1], cmdOut);
				
//				strReturn = UninstallApplication();
//				String sPingCheck = SendPing("www.mozilla.org",null);
//				if (sPingCheck.contains("3 received"))
//					strReturn = sPingCheck;
//				RunReboot(cmdOut);
/*
				try 
					{
					FileOutputStream outFile = ctx.openFileOutput("test.txt", Context.MODE_WORLD_READABLE | Context.MODE_WORLD_WRITEABLE);
					OutputStreamWriter outS = new OutputStreamWriter(outFile);
					outS.write("Hello world 1" + lineSep);
					outS.write("Hello world 2" + lineSep);
					outS.write("Hello world 3" + lineSep);
					outS.write("Hello world 4" + lineSep);
					outS.flush();
					outS.close();
					
					String [] files = ctx.fileList();
					File aFile   = ctx.getFilesDir();
					String aPath = aFile.getCanonicalPath();
					String hold = aFile.getName();
					
					strReturn = PrintDir(aPath);
					strReturn += "\r\n";
					
					String src = aPath + "/test.txt";
					String dst = aPath + "/test2.txt";
					strReturn += CopyFile(src, dst);
					strReturn += "\r\n";
					
					strReturn += PrintDir(aPath);
					strReturn += "\r\n";
					
					dst = aPath + "/test3.txt";
					strReturn += Move(src, dst);
					strReturn += "\r\n";
					
					strReturn += PrintDir(aPath);
					strReturn += "\r\n";

					src = aPath + "/test2.txt";
					strReturn += RemoveFile(src);
					strReturn += "\r\n";
					strReturn += RemoveFile(dst);
					strReturn += "\r\n";
					strReturn += PrintDir(aPath);
					}
				catch (FileNotFoundException e)
					{
					// TODO Auto-generated catch block
					e.printStackTrace();
					} 
				catch (IOException e) 
					{
					// TODO Auto-generated catch block
					e.printStackTrace();
					}
*/
				break;
				
			case ENVRUN:
				if (Argc >= 2)
					{
					String [] theArgs = new String [Argc - 1];
			
					for (int lcv = 1; lcv < Argc; lcv++)
						{
						theArgs[lcv - 1] = Argv[lcv];
						}
			
					strReturn = StartPrg2(theArgs, cmdOut);
					}
				else
					{
					strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
					}
				break;
				
			case EXEC:
			case RUN:
				if (Argc >= 2)
					{
					String [] theArgs = new String [Argc - 1];
				
					for (int lcv = 1; lcv < Argc; lcv++)
						{
						theArgs[lcv - 1] = Argv[lcv];
						}
				
					if (Argv[1].contains("/") || Argv[1].contains("\\") || !Argv[1].contains("."))
						strReturn = StartPrg(theArgs, cmdOut);
					else
						strReturn = StartJavaPrg(theArgs);
					}
				else
					{
					strReturn = sErrorPrefix + "Wrong number of arguments for " + Argv[0] + " command!";
					}
				break;

			case KILL:
				if (Argc == 2)
//					strReturn = NewKillProc(Argv[1], Argv[2], cmdOut);
//					strReturn = NewKillProc(Argv[1], cmdOut);
					strReturn = KillProcess(Argv[1], cmdOut);
				else
					strReturn = sErrorPrefix + "Wrong number of arguments for kill command!";
				break;
				
			case DISK:
				strReturn = GetDiskInfo((Argc == 2 ? Argv[1] : "/"));
				break;
				
			case UNZP:
				strReturn = Unzip(Argv[1], (Argc == 3 ? Argv[2] : ""));
				break;
				
			case ZIP:
				strReturn = Zip(Argv[1], (Argc == 3 ? Argv[2] : ""));
				break;
				
			case HELP:
				strReturn = PrintUsage();
				break;
				
			default:
				strReturn = sErrorPrefix + "[" + Argv[0] + "] command";
				if (Argc > 1)
					{
					strReturn += " with arg(s) =";
					for (int lcv = 1; lcv < Argc; lcv++)
						{
						strReturn += " [" + Argv[lcv] + "]";
						}
					}
				strReturn += " is currently not implemented.";
				break;
			}
		
		return(strReturn);
		}
/*
	class AlertLooperThread extends Thread
		{
	    public Handler mHandler;
	    private Looper looper = null;
	    private DoAlert da	= null;
	    
	    public void term()
	    	{
	    	if (da != null)
	    		da.term();
	    	}
	    
	    public void quit()
	    	{
	    	if (looper != null)
	    		looper.quit();
	    	}
	    
	    public void run()
	    	{
	        Looper.prepare();
	        
	        looper = Looper.myLooper();
	          
	        mHandler = new Handler()
	        	{
	            public void handleMessage(Message msg)
	            	{
	                  // process incoming messages here
	            	}
	        	};
	          
			alertTimer = new Timer();
			da = new DoAlert();
    		alertTimer.scheduleAtFixedRate(da, 0, 5000);
    		Looper.loop();
	    	}
		}
	
	class DoAlert extends TimerTask
		{
		int	lcv = 0;
		Toast toast = null;
		Ringtone rt = null;
		
		DoAlert()
			{
			Context	ctx = SUTAgentAndroid.me.getApplication().getApplicationContext();
			this.toast = Toast.makeText(ctx, "Help me!", Toast.LENGTH_LONG);
			rt = RingtoneManager.getRingtone(ctx, RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM));
			}
		
		public void term()
			{
			if (rt != null)
				{
				if (rt.isPlaying())
					rt.stop();
				}
			
			if (toast != null)
				toast.cancel();
			}
	
		public void run ()
			{
			String sText =(((lcv++ % 2) == 0)  ? "Help me!" : "I've fallen down!" );
			toast.setText(sText);
			toast.show();
			if (rt != null)
				rt.play();
			}
		}
*/
	public void StartAlert()
		{
		// start the alert message
		alrt = new AlertLooperThread(this.contextWrapper);
		alrt.start();
		}

	public void StopAlert()
		{
		if (alrt == null)
			return;
		
		Timer alertTimer = alrt.getAlertTimer();
		// stop the alert message
		if (alertTimer != null)
			{
			// Kill the timer
			alertTimer.cancel();
			alertTimer.purge();
			alertTimer = null;
			// Clear any messages either queued or on screen
			alrt.term();
			// Give the messages a chance to be processed
			try {
				Thread.sleep(1000);
				}
			catch (InterruptedException e)
				{
				e.printStackTrace();
				}
			// Kill the thread
			alrt.quit();
			alrt = null;
			System.gc();
			}
		}

	public String [] parseCmdLine2(String theCmdLine)
		{
		String	cmdString;
		String	workingString;
		String	workingString2;
		String	workingString3;
		List<String> lst = new ArrayList<String>();
		int nLength = 0;
		int nFirstSpace = -1;
		
		// Null cmd line
		if (theCmdLine == null)
			{
			String [] theArgs = new String [1];
			theArgs[0] = new String("");
			return(theArgs);
			}
		else
			{
			nLength = theCmdLine.length();
			nFirstSpace = theCmdLine.indexOf(' ');
			}
		
		if (nFirstSpace == -1)
			{
			String [] theArgs = new String [1];
			theArgs[0] = new String(theCmdLine);
			return(theArgs);
			}
		
		// Get the command
		cmdString = new String(theCmdLine.substring(0, nFirstSpace)); 
		lst.add(cmdString);
		
		// Jump past the command and trim
		workingString = (theCmdLine.substring(nFirstSpace + 1, nLength)).trim();
		
		while ((nLength = workingString.length()) > 0)
			{
			int nEnd = 0;
			int	nStart = 0;
			
			// if we have a quote
			if (workingString.startsWith("\""))
				{
				// point to the first non quote char
				nStart = 1;
				// find the matching quote
				nEnd = workingString.indexOf('"', nStart);
				
				char prevChar;
				
				while(nEnd != -1)
					{
					// check to see if the quotation mark has been escaped
					prevChar = workingString.charAt(nEnd - 1);
					if (prevChar == '\\')
						{
						// if escaped, point past this quotation mark and find the next
						nEnd++;
						if (nEnd < nLength)
							nEnd = workingString.indexOf('"', nEnd);
						else
							nEnd = -1;
						}
					else
						break;
					}
				
				// there isn't one
				if (nEnd == -1)
					{
					// point at the quote
					nStart = 0;
					// so find the next space
					nEnd = workingString.indexOf(' ', nStart);
					// there isn't one of those either
					if (nEnd == -1)
						nEnd = nLength;	// Just grab the rest of the cmdline
					}
				}
			else // no quote so find the next space
				{
				nEnd = workingString.indexOf(' ', nStart);
				// there isn't one of those
				if (nEnd == -1)
					nEnd = nLength;	// Just grab the rest of the cmdline
				}
			
			// get the substring
			workingString2 = workingString.substring(nStart, nEnd);

			// if we have escaped quotes
			if (workingString2.contains("\\\""))
				{
				do
					{
					// replace escaped quote with embedded quote
					workingString3 = workingString2.replace("\\\"", "\"");
					workingString2 = workingString3;
					}
				while(workingString2.contains("\\\""));
				}

			// add it to the list
			lst.add(new String(workingString2));
			
			// if we are dealing with a quote
			if (nStart > 0)
				nEnd++; //  point past the end one
			
			// jump past the substring and trim it
			workingString = (workingString.substring(nEnd)).trim();
			}
		
		// ok we're done package up the results
		int nItems = lst.size();
		
		String [] theArgs = new String [nItems];
		
		for (int lcv = 0; lcv < nItems; lcv++)
			{
			theArgs[lcv] = lst.get(lcv);
			}
	
		return(theArgs);
		}
	
	public String [] parseCmdLine(String theCmdLine) {
		String	cmdString;
		String	workingString;
		String	workingString2;
		List<String> lst = new ArrayList<String>();
		int nLength = 0;
		int nFirstSpace = -1;
		
		// Null cmd line
		if (theCmdLine == null)
			{
			String [] theArgs = new String [1];
			theArgs[0] = new String("");
			return(theArgs);
			}
		else
			{
			nLength = theCmdLine.length();
			nFirstSpace = theCmdLine.indexOf(' ');
			}
		
		if (nFirstSpace == -1)
			{
			String [] theArgs = new String [1];
			theArgs[0] = new String(theCmdLine);
			return(theArgs);
			}
		
		// Get the command
		cmdString = new String(theCmdLine.substring(0, nFirstSpace)); 
		lst.add(cmdString);
		
		// Jump past the command and trim
		workingString = (theCmdLine.substring(nFirstSpace + 1, nLength)).trim();
		
		while ((nLength = workingString.length()) > 0)
			{
			int nEnd = 0;
			int	nStart = 0;
			
			// if we have a quote
			if (workingString.startsWith("\""))
				{
				// point to the first non quote char
				nStart = 1;
				// find the matching quote
				nEnd = workingString.indexOf('"', nStart);
				// there isn't one
				if (nEnd == -1)
					{
					// point at the quote
					nStart = 0;
					// so find the next space
					nEnd = workingString.indexOf(' ', nStart);
					// there isn't one of those either
					if (nEnd == -1)
						nEnd = nLength;	// Just grab the rest of the cmdline
					}
				else
					{
					nStart = 0;
					nEnd++;
					}
				}
			else // no quote so find the next space
				{
				nEnd = workingString.indexOf(' ', nStart);
				// there isn't one of those
				if (nEnd == -1)
					nEnd = nLength;	// Just grab the rest of the cmdline
				}
			
			// get the substring
			workingString2 = workingString.substring(nStart, nEnd);
			
			// add it to the list
			lst.add(new String(workingString2));
			
			// if we are dealing with a quote
//			if (nStart > 0)
//				nEnd++; //  point past the end one
			
			// jump past the substring and trim it
			workingString = (workingString.substring(nEnd)).trim();
			}
		
		int nItems = lst.size();
		
		String [] theArgs = new String [nItems];
		
		for (int lcv = 0; lcv < nItems; lcv++)
			{
			theArgs[lcv] = lst.get(lcv);
			}
	
		return(theArgs);
		}
	
	public String fixFileName(String fileName)
		{
		String	sRet = "";
		String	sTmpFileName = "";
		
		sRet = fileName.replace('\\', '/');
		
		if (sRet.startsWith("/"))
			sTmpFileName = sRet;
		else
			sTmpFileName = currentDir + "/" + sRet;
		
		sRet = sTmpFileName.replace('\\', '/');
		sTmpFileName = sRet;
		sRet = sTmpFileName.replace("//", "/");
		
		return(sRet);
		}
	
	public String AddFilesToZip(ZipOutputStream out, String baseDir, String relDir)
	{
		final int 			BUFFER 	= 2048;
		String				sRet	= "";
		String 				curDir 	= "";
		String				relFN	= "";
		BufferedInputStream origin = null;
	    byte 				data[] = new byte[BUFFER];
	    
	    if (relDir.length() > 0)
	    	curDir = baseDir + "/" + relDir;
	    else
	    	curDir = baseDir;
	    
		File f = new File(curDir);
		
		if (f.isFile())
			{
		    try {
		    	relFN = ((relDir.length() > 0) ? relDir + "/" + f.getName() : f.getName());
		    	System.out.println("Adding: "+relFN);
		    	sRet += "Adding: "+	relFN + lineSep;
		    	FileInputStream fi = new FileInputStream(curDir);
		    	origin = new BufferedInputStream(fi, BUFFER);
		    	ZipEntry entry = new ZipEntry(relFN);
		    	out.putNextEntry(entry);
		    	int count;
		    	while((count = origin.read(data, 0, BUFFER)) != -1)
        			{
		    		out.write(data, 0, count);
        			}
		    	origin.close();
				}
		    catch(Exception e)
		    	{
			    e.printStackTrace();
		    	}
		    
		    return(sRet);
			}
		
		String	files[] = f.list();
		
		if (files != null)
			{
			try {
				for(int i = 0; i < files.length; i++)
					{
					f = new File(curDir + "/" + files[i]);
					if (f.isDirectory())
						{
						if (relDir.length() > 0)
							sRet += AddFilesToZip(out, baseDir, relDir + "/" + files[i]);
						else
							sRet += AddFilesToZip(out, baseDir, files[i]);
						}
					else
						{
						relFN = ((relDir.length() > 0) ? relDir + "/" + files[i] : files[i]);
						System.out.println("Adding: "+relFN);
						sRet += "Adding: "+	relFN + lineSep;
						FileInputStream fi = new FileInputStream(curDir + "/" + files[i]);
						origin = new BufferedInputStream(fi, BUFFER);
						ZipEntry entry = new ZipEntry(relFN);
						out.putNextEntry(entry);
						int count;
						while((count = origin.read(data, 0, BUFFER)) != -1)
		        			{
							out.write(data, 0, count);
		        			}
						origin.close();
						}
					}
	    		}
			catch(Exception e)
	    		{
				e.printStackTrace();
	    		}
			}

		return(sRet);
	}
	
	public String Zip(String zipFileName, String srcName)
		{
		String	fixedZipFileName = fixFileName(zipFileName);
		String	fixedSrcName = fixFileName(srcName);
		String sRet = "";
		
		try {
		    FileOutputStream dest = new FileOutputStream(fixedZipFileName);
		    CheckedOutputStream checksum = new CheckedOutputStream(dest, new Adler32());
		    ZipOutputStream out = new ZipOutputStream(new BufferedOutputStream(checksum));
		    out.setMethod(ZipOutputStream.DEFLATED);
		    
		    sRet += AddFilesToZip(out, fixedSrcName, "");
		    
		    out.close();
		    System.out.println("checksum:		           "+checksum.getChecksum().getValue());
	        sRet += "checksum:		           "+checksum.getChecksum().getValue();
		    }
		catch(Exception e)
			{
		    e.printStackTrace();
		    }
		
		return(sRet);
	}

	public String Unzip(String zipFileName, String dstDirectory)
		{
		String 	sRet = "";
		String	fixedZipFileName = fixFileName(zipFileName);
		String	fixedDstDirectory = fixFileName(dstDirectory);
		String	dstFileName = "";
		int		nNumExtracted = 0;
		boolean bRet = false;
		
		try {
			final int BUFFER = 2048;
			BufferedOutputStream dest = null;
			ZipFile zipFile = new ZipFile(fixedZipFileName);
			int nNumEntries = zipFile.size();
			zipFile.close();
			
			FileInputStream fis = new FileInputStream(fixedZipFileName);
			CheckedInputStream checksum = new CheckedInputStream(fis, new Adler32());
			ZipInputStream zis = new ZipInputStream(new BufferedInputStream(checksum));
			ZipEntry entry;

			byte [] data = new byte[BUFFER];
			
			while((entry = zis.getNextEntry()) != null)
        		{
				System.out.println("Extracting: " + entry);
				int count;
				if (fixedDstDirectory.length() > 0)
					dstFileName = fixedDstDirectory + entry.getName();
				else
					dstFileName = entry.getName();
				
				String tmpDir = dstFileName.substring(0, dstFileName.lastIndexOf('/'));
				File tmpFile = new File(tmpDir);
				if (!tmpFile.exists())
					{
					bRet = tmpFile.mkdirs();
					}
				else
					bRet = true;
				
				if (bRet)
					{
					// if we aren't just creating a directory
					if (dstFileName.lastIndexOf('/') != (dstFileName.length() - 1))
						{
						// write out the file
						FileOutputStream fos = new FileOutputStream(dstFileName);
						dest = new BufferedOutputStream(fos, BUFFER);
						while ((count = zis.read(data, 0, BUFFER)) != -1)
        					{
							dest.write(data, 0, count);
        					}
						dest.flush();
						dest.close();
						dest = null;
						fos.close();
						fos = null;
						}
					nNumExtracted++;
					}
				else
					sRet += " - failed" + lineSep;
        		}
			
			data = null;
			zis.close();
			System.out.println("Checksum:          "+checksum.getChecksum().getValue());
			sRet += "Checksum:          "+checksum.getChecksum().getValue();
			sRet += lineSep + nNumExtracted + " of " + nNumEntries + " sucessfully extracted";
			}
		catch(Exception e)
			{
			e.printStackTrace();
			}
		
		return(sRet);
		}
	
	public String StatProcess(String string)
		{
		String sRet = "";
		ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
		int	[] nPids = new int [1];
		
		nPids[0] = Integer.parseInt(string);
		
		android.os.Debug.MemoryInfo[] mi = aMgr.getProcessMemoryInfo(nPids);
		
		sRet  = "Dalvik Private Dirty pages         " + mi[0].dalvikPrivateDirty     + " kb\n";
		sRet += "Dalvik Proportional Set Size       " + mi[0].dalvikPss              + " kb\n";
		sRet += "Dalvik Shared Dirty pages          " + mi[0].dalvikSharedDirty      + " kb\n\n";
		sRet += "Native Private Dirty pages heap    " + mi[0].nativePrivateDirty     + " kb\n";
		sRet += "Native Proportional Set Size heap  " + mi[0].nativePss              + " kb\n";
		sRet += "Native Shared Dirty pages heap     " + mi[0].nativeSharedDirty      + " kb\n\n";
		sRet += "Other Private Dirty pages          " + mi[0].otherPrivateDirty      + " kb\n";
		sRet += "Other Proportional Set Size        " + mi[0].otherPss               + " kb\n";
		sRet += "Other Shared Dirty pages           " + mi[0].otherSharedDirty       + " kb\n\n";
		sRet += "Total Private Dirty Memory         " + mi[0].getTotalPrivateDirty() + " kb\n";
		sRet += "Total Proportional Set Size Memory " + mi[0].getTotalPss()          + " kb\n";
		sRet += "Total Shared Dirty Memory          " + mi[0].getTotalSharedDirty()  + " kb";
		
		
		return(sRet);
		}
	
	public String GetTestRoot()
		{
		String	sRet = null;
		
		if (Environment.getExternalStorageState().equalsIgnoreCase(Environment.MEDIA_MOUNTED))
			{
			sRet = Environment.getExternalStorageDirectory().getAbsolutePath();
			}
		else
			{
			sRet = GetTmpDir();			
			}
		
		return(sRet);
		}
	
	public String GetAppRoot(String AppName)
		{
		String sRet = "";
		Context ctx = contextWrapper.getApplicationContext();
		
		if (ctx != null)
			{
			try {
				Context appCtx = ctx.createPackageContext(AppName, 0);
				ContextWrapper appCtxW = new ContextWrapper(appCtx);
				sRet = appCtxW.getPackageResourcePath();
				appCtxW = null;
				appCtx = null;
				ctx = null;
				System.gc();
				}
			catch (NameNotFoundException e)
				{
				e.printStackTrace();
				}
			}
		return(sRet);
		}

	public String changeDir(String newDir)
		{
		String	tmpDir	= fixFileName(newDir);
		String	sRet = sErrorPrefix + "Couldn't change directory to " + tmpDir;
		
		File tmpFile = new java.io.File(tmpDir);
		
		if (tmpFile.exists())
			{
			try {
				currentDir = tmpFile.getCanonicalPath();
				sRet = "";
				}
			catch (IOException e)
				{
				// TODO Auto-generated catch block
				e.printStackTrace();
				}
			}
		
		return(sRet);
		}
	
	static final String HEXES = "0123456789abcdef";
	
	public static String getHex( byte [] raw )
		{
	    if ( raw == null )
	    	{
	    	return null;
	    	}
	    
	    final StringBuilder hex = new StringBuilder( 2 * raw.length );
	    for ( final byte b : raw )
	    	{
	    	hex.append(HEXES.charAt((b & 0xF0) >> 4)).append(HEXES.charAt((b & 0x0F)));
	    	}
	    return hex.toString();
		}

	public String HashFile(String fileName)
		{
		String			sTmpFileName = fixFileName(fileName);
		String			sRet 		= sErrorPrefix + "Couldn't calculate hash for file " + sTmpFileName;
		byte[] 			buffer 		= new byte [4096];
		int				nRead 		= 0;
		long 			lTotalRead 	= 0;
		MessageDigest	digest 		= null;
		
		try {
			digest = java.security.MessageDigest.getInstance("MD5");
			}
		catch (NoSuchAlgorithmException e)
			{
			e.printStackTrace();
			}
		
		try {
			FileInputStream srcFile  = new FileInputStream(sTmpFileName);
			while((nRead = srcFile.read(buffer)) != -1)
				{
				digest.update(buffer, 0, nRead);
				lTotalRead += nRead;
				}
			srcFile.close();
			byte [] hash = digest.digest();
			
			sRet = getHex(hash);
			}
		catch (FileNotFoundException e)
			{
			sRet += " file not found";
			e.printStackTrace();
			}
		catch (IOException e)
			{
			sRet += " io exception";
			e.printStackTrace();
			} 
		return(sRet);
		}
	
	public String RemoveFile(String fileName)
		{
		String	sTmpFileName = fixFileName(fileName);
		String	sRet = sErrorPrefix + "Couldn't delete file " + sTmpFileName;
		
		File f = new File(sTmpFileName);
		
		if (f.delete())
			sRet = "deleted " + sTmpFileName;
		
		return(sRet);
		}
	
	public String PruneDir(String sDir)
		{
		String	sRet = "";
		int nFiles = 0;
		String sSubDir = null;
		String	sTmpDir = fixFileName(sDir);
		
		File dir = new File(sTmpDir);
		
		if (dir.isDirectory())
			{
			sRet = "Deleting file(s) from " + sTmpDir;
			
			File [] files = dir.listFiles();
			if (files != null)
				{
				if ((nFiles = files.length) > 0)
					{
					for (int lcv = 0; lcv < nFiles; lcv++)
						{
						if (files[lcv].isDirectory())
							{
							sSubDir = files[lcv].getAbsolutePath();
							sRet += "\n" + PruneDir(sSubDir);
							}
						else
							{
							if (files[lcv].delete())
								{
								sRet += "\n\tDeleted " + files[lcv].getName();
								}
							else
								{
								sRet += "\n\tUnable to delete " + files[lcv].getName();
								}
							}
						}
					}
				else
					sRet += "\n\t<empty>";
				}
			
			if (dir.delete())
				{
				sRet += "\nDeleting directory " + sTmpDir;
				}
			else
				{
				sRet += "\nUnable to delete directory " + sTmpDir;
				}
			}
		else
			{
			sRet += sErrorPrefix + sTmpDir + " is not a directory";
			}
		
		return(sRet);
		}
	
	public String PrintDir(String sDir)
		{
		String	sRet = "";
		int nFiles = 0;
		String	sTmpDir = fixFileName(sDir);
		
		File dir = new File(sTmpDir);
		
		if (dir.isDirectory())
			{
			File [] files = dir.listFiles();
		
			if (files != null)
				{
				if ((nFiles = files.length) > 0)
					{
					for (int lcv = 0; lcv < nFiles; lcv++)
						{
						sRet += files[lcv].getName();
						if (lcv < (nFiles - 1))
							sRet += "\n";
						}
					}
				else
					sRet = "<empty>";
				}
			}
		else
			{
			sRet = sErrorPrefix + sTmpDir + " is not a directory";
			}
		
		return(sRet);
		}
	
	public String Move(String srcFileName, String dstFileName)
		{
		String	sTmpSrcFileName = fixFileName(srcFileName);
		String	sTmpDstFileName = fixFileName(dstFileName);
		String sRet = sErrorPrefix + "Could not move " + sTmpSrcFileName + " to " + sTmpDstFileName;
		
		File srcFile = new File(sTmpSrcFileName);
		File dstFile = new File(sTmpDstFileName);
		
		if (srcFile.renameTo(dstFile))
			sRet = sTmpSrcFileName + " moved to " + sTmpDstFileName;
		
		return (sRet);
		}
	
	public String CopyFile(String srcFileName, String dstFileName)
		{
		String	sTmpSrcFileName = fixFileName(srcFileName);
		String	sTmpDstFileName = fixFileName(dstFileName);
		String sRet = sErrorPrefix + "Could not copy " + sTmpSrcFileName + " to " + sTmpDstFileName;
		File destFile = null;
		byte[] buffer = new byte [4096];
		int	nRead = 0;
		long lTotalRead = 0;
		long lTotalWritten = 0;
		
		try 
			{
			FileInputStream srcFile  = new FileInputStream(sTmpSrcFileName);
			FileOutputStream dstFile = new FileOutputStream(sTmpDstFileName);
			
			while((nRead = srcFile.read(buffer)) != -1)
				{
				lTotalRead += nRead;
				dstFile.write(buffer, 0, nRead);
				}
			dstFile.flush();
			dstFile.close();
			
			destFile = new File(sTmpDstFileName);
			lTotalWritten = destFile.length();

			if (lTotalWritten == lTotalRead)
				sRet = sTmpSrcFileName + " copied to " + sTmpDstFileName;
			else
				sRet = sErrorPrefix + "Failed to copy " + sTmpSrcFileName + " [length = " + lTotalWritten + "] to " + sTmpDstFileName + " [length = " + lTotalRead + "]";
			}
		catch (FileNotFoundException e)
			{
			e.printStackTrace();
			} 
		catch (IOException e)
			{
			e.printStackTrace();
			}

		return (sRet);
		}
	
	public String IsDirWritable(String sDir)
		{
		String sRet = "";
		String	sTmpDir = fixFileName(sDir);
		File dir = new File(sTmpDir);
		
		if (dir.isDirectory())
			{
			sRet = "[" + sDir + "] " + (dir.canWrite() ? "is" : "is not") + " writable";
			}
		else
			{
			sRet = sErrorPrefix + "[" + sDir + "] is not a directory";
			}
		
		return(sRet);
		}
	
	public String Push(String fileName, BufferedInputStream bufIn, long lSize)
	{
		byte []				buffer 			= new byte [8192];
		int					nRead			= 0;
		long				lRead			= 0;
		String				sTmpFileName 	= fixFileName(fileName);
		String				sRet			= sErrorPrefix + "Push failed!";
		
		try {
			FileOutputStream dstFile = new FileOutputStream(sTmpFileName, false);
			while((nRead != -1) && (lRead < lSize))
				{
				nRead = bufIn.read(buffer);
				if (nRead != -1)
					{
					dstFile.write(buffer, 0, nRead);
					dstFile.flush();
					lRead += nRead;
					}
				}
			
			dstFile.flush();
			dstFile.close();
			
			if (lRead == lSize)
				{
				sRet = HashFile(sTmpFileName);
				}
			}
		catch (IOException e)
			{
			e.printStackTrace();
			}
		
		buffer = null;
		
		return(sRet);
	}
	
	public String FTPGetFile(String sServer, String sSrcFileName, String sDstFileName, OutputStream out)
		{
		byte[] buffer = new byte [4096];
		int	nRead = 0;
		long lTotalRead = 0;
		String sRet = sErrorPrefix + "FTP Get failed for " + sSrcFileName;
		String strRet = "";
		int	reply = 0;
		FileOutputStream outStream = null;
		String	sTmpDstFileName = fixFileName(sDstFileName);
		
		FTPClient ftp = new FTPClient();
		try 
			{
			ftp.connect(sServer);
			reply = ftp.getReplyCode();
		    if(FTPReply.isPositiveCompletion(reply))
		    	{
			    ftp.login("anonymous", "b@t.com");
				reply = ftp.getReplyCode();
			    if(FTPReply.isPositiveCompletion(reply))
			    	{
				    ftp.enterLocalPassiveMode();
				    if (ftp.setFileType(FTP.BINARY_FILE_TYPE))
				    	{
				    	File dstFile = new File(sTmpDstFileName);
				    	outStream = new FileOutputStream(dstFile);
				    	FTPFile [] ftpFiles = ftp.listFiles(sSrcFileName);
				    	long lFtpSize = ftpFiles[0].getSize();
				    	if (lFtpSize <= 0)
				    		lFtpSize = 1;
				    	
				    	InputStream ftpIn = ftp.retrieveFileStream(sSrcFileName);
						while ((nRead = ftpIn.read(buffer)) != -1)
							{
							lTotalRead += nRead;
							outStream.write(buffer, 0, nRead);
							strRet = "\r" + lTotalRead + " of " + lFtpSize + " bytes received " + ((lTotalRead * 100) / lFtpSize) + "% completed";
							out.write(strRet.getBytes());
							out.flush();
							}
						ftpIn.close();
						@SuppressWarnings("unused")
						boolean bRet = ftp.completePendingCommand();
						outStream.flush();
			    		outStream.close();
						strRet = ftp.getReplyString();
						reply = ftp.getReplyCode();
				    	}
					strRet = ftp.getReplyString();
					reply = ftp.getReplyCode();
				    ftp.logout();
				    ftp.disconnect();
				    sRet = "\n" + strRet; 
			    	}
			    else
			    	{
			        ftp.disconnect();
			        System.err.println("FTP server refused login.");
			        }
		    	}
		    else
		    	{
		        ftp.disconnect();
		        System.err.println("FTP server refused connection.");
		        }
			}
		catch (SocketException e)
			{
			sRet = e.getMessage();
			strRet = ftp.getReplyString();
			reply = ftp.getReplyCode();
			sRet += "\n" + strRet;
			e.printStackTrace();
			}
		catch (IOException e)
			{
			sRet = e.getMessage();
			strRet = ftp.getReplyString();
			reply = ftp.getReplyCode();
			sRet += "\n" + strRet;
			e.printStackTrace();
			}
		return (sRet);
	}
	
	public String Cat(String fileName, OutputStream out)
		{
		String	sTmpFileName = fixFileName(fileName);
		String	sRet = sErrorPrefix + "Could not read the file " + sTmpFileName;
		byte[]	buffer = new byte [4096];
		int		nRead = 0;
		
		try {
			FileInputStream fin = new FileInputStream(sTmpFileName);
			while ((nRead = fin.read(buffer)) != -1)
				{
				out.write(buffer,0,nRead);
				}
			out.flush();
			sRet = "";
			}
		catch (FileNotFoundException e)
			{
			sRet = e.toString();
			} 
		catch (IOException e) 
			{
			sRet = e.toString();
			}
		return (sRet);
		}
	
	public String MakeDir(String sDir)
		{
		String	sTmpDir = fixFileName(sDir);
		String sRet = sErrorPrefix + "Could not create the directory " + sTmpDir;
		File dir = new File(sTmpDir);
		
		if (dir.mkdirs())
			sRet = sDir + " successfully created";
		
		return (sRet);
		}
	// move this to SUTAgentAndroid.java
	public String GetScreenInfo()
		{
		String sRet = "";
		DisplayMetrics metrics = new DisplayMetrics();
		WindowManager wMgr = (WindowManager) contextWrapper.getSystemService(Context.WINDOW_SERVICE);
		wMgr.getDefaultDisplay().getMetrics(metrics);
		sRet = "X:" + metrics.widthPixels + " Y:" + metrics.heightPixels;
		return (sRet);
		}
	// move this to SUTAgentAndroid.java
	public int [] GetScreenXY()
		{
			int [] nRetXY = new int [2];
			DisplayMetrics metrics = new DisplayMetrics();
			WindowManager wMgr = (WindowManager) contextWrapper.getSystemService(Context.WINDOW_SERVICE);
			wMgr.getDefaultDisplay().getMetrics(metrics);
			nRetXY[0] = metrics.widthPixels;
			nRetXY[1] = metrics.heightPixels;
			return(nRetXY);
		}
	
	public String KillProcess(String sProcName, OutputStream out)
		{
		String [] theArgs = new String [3];
		
		theArgs[0] = "su";
		theArgs[1] = "-c";
		theArgs[2] = "kill";

		String sRet = sErrorPrefix + "Unable to kill " + sProcName + "\n";
		ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
		List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
		int lcv = 0;
		String strProcName = "";
		int	nPID = 0;
		
		for (lcv = 0; lcv < lProcesses.size(); lcv++)
			{
			if (lProcesses.get(lcv).processName.contains(sProcName))
				{
				strProcName = lProcesses.get(lcv).processName;
				nPID = lProcesses.get(lcv).pid;
				sRet = sErrorPrefix + "Failed to kill " + nPID + " " + strProcName + "\n";

				theArgs[2] += " " + nPID;
				
				try 
					{
					pProc = Runtime.getRuntime().exec(theArgs);
					RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
					outThrd.start();
					outThrd.join(5000);
					}
				catch (IOException e) 
					{
					sRet = e.getMessage();
					e.printStackTrace();
					} 
				catch (InterruptedException e)
					{
					e.printStackTrace();
					}

				// Give the messages a chance to be processed
				try {
					Thread.sleep(2000);
					}
				catch (InterruptedException e)
					{
					e.printStackTrace();
					}
				break;
				}
			}
		
		if (nPID > 0)
			{
			sRet = "Successfully killed " + nPID + " " + strProcName + "\n";
			lProcesses = aMgr.getRunningAppProcesses();
			for (lcv = 0; lcv < lProcesses.size(); lcv++)
				{
				if (lProcesses.get(lcv).processName.contains(sProcName))
					{
					sRet = sErrorPrefix + "Unable to kill " + nPID + " " + strProcName + "\n";
					break;
					}
				}
			}
		
		return (sRet);
		}

	public boolean IsProcessDead(String sProcName)
		{
		boolean bRet = false;
		ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
		List <ActivityManager.ProcessErrorStateInfo> lProcesses = aMgr.getProcessesInErrorState();
		int lcv = 0;
		
		if (lProcesses != null)
			{
			for (lcv = 0; lcv < lProcesses.size(); lcv++)
				{
				if (lProcesses.get(lcv).processName.contentEquals(sProcName) && 
					lProcesses.get(lcv).condition != ActivityManager.ProcessErrorStateInfo.NO_ERROR)
					{
					bRet = true;
					break;
					}
				}
			}
	
		return (bRet);
		}

	public String GetProcessInfo()
		{
		String sRet = "";
		ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
		List <ActivityManager.RunningAppProcessInfo> lProcesses = aMgr.getRunningAppProcesses();
		int	nProcs = lProcesses.size();
		int lcv = 0;
		String strProcName = "";
		int	nPID = 0;
		int nUser = 0;
		
		for (lcv = 0; lcv < nProcs; lcv++)
			{
			strProcName = lProcesses.get(lcv).processName;
			nPID = lProcesses.get(lcv).pid;
			nUser = lProcesses.get(lcv).uid;
			sRet += nUser + "\t" + nPID + "\t" + strProcName;
			if (lcv < (nProcs - 1))
				sRet += "\n";
			}
			
		return (sRet);
		}
	
	public String GetOSInfo()
		{
		String sRet = "";
		
		sRet = Build.DISPLAY;
		
		return (sRet);
		}
	
	public String GetPowerInfo()
		{
		String sRet = "";
	
		sRet = "Power status:\n  AC power " + SUTAgentAndroid.sACStatus + "\n";
		sRet += "  Battery charge " + SUTAgentAndroid.sPowerStatus + "\n";
		sRet += "  Remaining charge:      " + SUTAgentAndroid.nChargeLevel + "%\n";
		sRet += "  Battery Temperature:   " + (((float)(SUTAgentAndroid.nBatteryTemp))/10) + " (c)\n";
		return (sRet);
		}

	public String GetDiskInfo(String sPath)
		{
		String sRet = "";
		StatFs statFS = new StatFs(sPath);
		
		int nBlockCount = statFS.getBlockCount();
		int nBlockSize = statFS.getBlockSize();
		int nBlocksAvail = statFS.getAvailableBlocks();
		int nBlocksFree = statFS.getFreeBlocks();
	
		sRet = "total:     " + (nBlockCount * nBlockSize) + "\nfree:      " + (nBlocksFree * nBlockSize) + "\navailable: " + (nBlocksAvail * nBlockSize);
	
		return (sRet);
		}

	public String GetMemoryInfo()
		{
		String sRet = "PA:" + GetMemoryConfig();
		return (sRet);
		}

	public long GetMemoryConfig()
		{
		ActivityManager aMgr = (ActivityManager) contextWrapper.getSystemService(Activity.ACTIVITY_SERVICE);
		ActivityManager.MemoryInfo outInfo = new ActivityManager.MemoryInfo();
		aMgr.getMemoryInfo(outInfo);
		long lMem = outInfo.availMem;

		return (lMem);
		}
	
	public String RegisterTheDevice(String sSrvr, String sPort, String sData)
		{
		String sRet = "";
		String line = "";
		
		if (sSrvr != null && sPort != null && sData != null)
			{
			try
				{
				int nPort = Integer.parseInt(sPort);
				Socket socket = new Socket(sSrvr, nPort);
				PrintWriter out = new PrintWriter(socket.getOutputStream(), false);
				BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
				out.println(sData);
				if ( out.checkError() == false )
					{
					socket.setSoTimeout(30000);
					while (socket.isInputShutdown() == false)
						{
						line = in.readLine();
						line = line.toLowerCase();
						if ((line == null) || (line.contains("ok")))
							{
							sRet += line;
							break;
							}
						sRet += line;
						}
					}
				out.close();
				in.close();
				socket.close();
				}
			catch(NumberFormatException e)
				{
				e.printStackTrace();
				} 
			catch (UnknownHostException e)
				{
				e.printStackTrace();
				}
			catch (IOException e)
				{
				sRet += "reg exception thrown";
				e.printStackTrace();
				}
			}
		return(sRet);
		}
	
	public String GetInternetData(String sHost, String sPort, String sURL)
		{
		String sRet = "";
		String sNewURL = "";
		HttpClient httpClient = new DefaultHttpClient();
		try 
			{
			sNewURL = "http://" + sHost + ((sPort.length() > 0) ? (":" + sPort) : "") + sURL;

			HttpGet request = new HttpGet(sNewURL);
			HttpResponse response = httpClient.execute(request);
			int status = response.getStatusLine().getStatusCode();
			// we assume that the response body contains the error message
			if (status != HttpStatus.SC_OK)
				{
			    ByteArrayOutputStream ostream = new ByteArrayOutputStream();
			    response.getEntity().writeTo(ostream);
			    Log.e("HTTP CLIENT", ostream.toString());
				}
			else
				{
			    InputStream content = response.getEntity().getContent();
//			    int nAvailable = content.available();
//			    byte [] data = new byte [nAvailable];
			    byte [] data = new byte [2048];
			    int nRead = content.read(data);
			    sRet = new String(data, 0, nRead);
			    content.close(); // this will also close the connection
				}
			}
		catch (IllegalArgumentException e)
			{
			sRet = e.getLocalizedMessage();
			e.printStackTrace();
			}
		catch (ClientProtocolException e)
			{
			sRet = e.getLocalizedMessage();
			e.printStackTrace();
			}
		catch (IOException e)
			{
			sRet = e.getLocalizedMessage();
			e.printStackTrace();
			}
		
		return(sRet);
		}
	
	public String GetTimeZone()
		{
		String	sRet = "";
		TimeZone tz;
		
		tz = TimeZone.getDefault();
		Date now = new Date();
		sRet = tz.getDisplayName(tz.inDaylightTime(now), TimeZone.LONG);
		
		return(sRet);
		}
	
	public String SetTimeZone(String sTimeZone)
		{
		String			sRet = "Unable to set timezone to " + sTimeZone;
		TimeZone 		tz = null;
		AlarmManager 	amgr = null;
		
		if ((sTimeZone.length() > 0) && (sTimeZone.startsWith("GMT")))
			{
			amgr = (AlarmManager) contextWrapper.getSystemService(Context.ALARM_SERVICE);
			if (amgr != null)
				amgr.setTimeZone(sTimeZone);
			}
		else
			{
			String [] zoneNames = TimeZone.getAvailableIDs();
			int nNumMatches = zoneNames.length;
			int	lcv = 0;
			
			for (lcv = 0; lcv < nNumMatches; lcv++)
				{
				if (zoneNames[lcv].equalsIgnoreCase(sTimeZone))
					break;
				}

			if (lcv < nNumMatches)
				{
				amgr = (AlarmManager) contextWrapper.getSystemService(Context.ALARM_SERVICE);
				if (amgr != null)
					amgr.setTimeZone(zoneNames[lcv]);
				}
			}
		
		if (amgr != null)
			{
			tz = TimeZone.getDefault();
			Date now = new Date();
			sRet = tz.getDisplayName(tz.inDaylightTime(now), TimeZone.LONG);
			}
		
		return(sRet);
		}

	public String GetSystemTime()
		{
		String sRet = "";
		Calendar cal = Calendar.getInstance();
		SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd hh:mm:ss:SSS");
		sRet = sdf.format(cal.getTime());
	
		return (sRet);
		}
	
	public String SetSystemTime(String sDate, String sTime, OutputStream out)
		{
		String sRet = "";
		
		if (((sDate != null) && (sTime != null)) && 
			(sDate.contains("/") || sDate.contains(".")) &&
			(sTime.contains(":")))
			{
			int year = Integer.parseInt(sDate.substring(0,4));
			int month = Integer.parseInt(sDate.substring(5,7));
			int day = Integer.parseInt(sDate.substring(8,10));
			
			int hour = Integer.parseInt(sTime.substring(0,2));
			int mins = Integer.parseInt(sTime.substring(3,5));
			int secs = Integer.parseInt(sTime.substring(6,8));

			Calendar cal = new GregorianCalendar(TimeZone.getDefault());
			cal.set(year, month - 1, day, hour, mins, secs);
			long lMillisecs = cal.getTime().getTime();
			
			String sM = Long.toString(lMillisecs);
			String sMillis = sM.substring(0, sM.length() - 3) + "." + sM.substring(sM.length() - 3);
			String [] theArgs = new String [3];
		
			theArgs[0] = "su";
			theArgs[1] = "-c";
			theArgs[2] = "date -u " + sMillis;
		
			try 
				{
				pProc = Runtime.getRuntime().exec(theArgs);
				RedirOutputThread outThrd = new RedirOutputThread(pProc, null);
				outThrd.start();
				outThrd.join(10000);
				sRet = GetSystemTime();
				}
			catch (IOException e) 
				{
				sRet = e.getMessage();
				e.printStackTrace();
				} 
			catch (InterruptedException e)
				{
				e.printStackTrace();
				}
			}
		else
			{
			sRet = "Invalid argument(s)";
			}

		return (sRet);
		}

	public String GetClok()
		{
		long lMillisecs = System.currentTimeMillis();
		String sRet = "";
		
		if (lMillisecs > 0)
			sRet = Long.toString(lMillisecs);
		
		return(sRet);
		}
	
	public String GetUptime()
		{
		String sRet = "";
		long lHold = 0;
		long lUptime = SystemClock.elapsedRealtime();
		int	nDays = 0;
		int	nHours = 0;
		int nMinutes = 0;
		int nSecs = 0;
		int nMilliseconds = 0;
		
		if (lUptime > 0)
			{
			nDays = (int)(lUptime / (24L * 60L * 60L * 1000L));
			lHold = lUptime % (24L * 60L * 60L * 1000L);
			nHours = (int)(lHold / (60L * 60L * 1000L));
			lHold %= 60L * 60L * 1000L;
			nMinutes = (int)(lHold / (60L * 1000L));
			lHold %= 60L * 1000L;
			nSecs = (int)(lHold / 1000L);
			nMilliseconds = (int)(lHold % 1000);
			sRet = "" + nDays + " days " + nHours + " hours " + nMinutes + " minutes " + nSecs + " seconds " + nMilliseconds + " ms";
			}

		return (sRet);
		}

	public String NewKillProc(String sProcId, OutputStream out)
		{
		String sRet = "";
		String [] theArgs = new String [3];
		
		theArgs[0] = "su";
		theArgs[1] = "-c";
		theArgs[2] = "kill " + sProcId;

		try 
			{
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(5000);
			}
		catch (IOException e) 
			{
			sRet = e.getMessage();
			e.printStackTrace();
			} 
		catch (InterruptedException e)
			{
			// TODO Auto-generated catch block
			e.printStackTrace();
			}

		return(sRet);
		}
	
	public String SendPing(String sIPAddr, OutputStream out)
		{
		String sRet = "";
		String [] theArgs = new String [4];
		
		theArgs[0] = "ping";
		theArgs[1] = "-c";
		theArgs[2] = "3";
		theArgs[3] = sIPAddr;
		
		try 
			{
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(5000);
			if (out == null)
				sRet = outThrd.strOutput;
			}
		catch (IOException e) 
			{
			sRet = e.getMessage();
			e.printStackTrace();
			} 
		catch (InterruptedException e)
			{
			e.printStackTrace();
			}
	
		return (sRet);
		}
	
	public String GetTmpDir()
	{
		String 	sRet = "";
		Context ctx = contextWrapper.getApplicationContext();
        File dir = ctx.getFilesDir();
        ctx = null;
        try {
			sRet = dir.getCanonicalPath();
			} 
        catch (IOException e)
        	{
			e.printStackTrace();
        	}
        return(sRet);
	}
	
	public String PrintFileTimestamp(String sFile)
		{
		String 	sRet = "";
		String	sTmpFileName = fixFileName(sFile);
		File 	theFile = new File(sTmpFileName);
		
		if (theFile.exists())
			{
			long lModified = theFile.lastModified();
			Date dtModified = new Date(lModified);
			SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd hh:mm:ss:SSS");
			sRet = "Last modified: " + sdf.format(dtModified);
			}
		else
			{
			sRet = sErrorPrefix + "[" + sTmpFileName + "] doesn't exist";
			}

		return(sRet);
		}
	
	public String GetIniData(String sSection, String sKey, String sFile)
		{
		String sRet = "";
		String sComp = "";
		String sLine = "";
		boolean bFound = false;
		BufferedReader in = null;
		String	sTmpFileName = fixFileName(sFile);
		
		try {
			in = new BufferedReader(new FileReader(sTmpFileName));
			sComp = "[" + sSection + "]";
			while ((sLine = in.readLine()) != null)
				{
				if (sLine.equalsIgnoreCase(sComp))
					{
					bFound = true;
					break;
					}
				}
			
			if (bFound)
				{
				sComp = (sKey + " =").toLowerCase();
				while ((sLine = in.readLine()) != null)
					{
					if (sLine.toLowerCase().contains(sComp))
						{
						String [] temp = null;
						temp = sLine.split("=");
						if (temp != null)
							{
							sRet = temp[1].trim();
							}
						break;
						}
					}
				}
			in.close();
			}
		catch (FileNotFoundException e)
			{
			sComp = e.toString();
			} 
		catch (IOException e) 
			{
			sComp = e.toString();
			}
		return (sRet);
		}
	
	public String RunReboot(OutputStream out)
		{
		String sRet = "";
		String [] theArgs = new String [3];
	
		theArgs[0] = "su";
		theArgs[1] = "-c";
		theArgs[2] = "reboot";
	
		try 
			{
			// Tell all of the data channels we are rebooting
			((ASMozStub)this.contextWrapper).SendToDataChannel("Rebooting ...");
			
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(10000);
			}
		catch (IOException e) 
			{
			sRet = e.getMessage();
			e.printStackTrace();
			} 
		catch (InterruptedException e)
			{
			e.printStackTrace();
			}

		return (sRet);
		}
	
	public String UnInstallApp(String sApp, OutputStream out)
		{
		String sRet = "";
		String [] theArgs = new String [3];

		theArgs[0] = "su";
		theArgs[1] = "-c";
		theArgs[2] = "pm uninstall " + sApp + ";reboot;exit";
		
		try 
			{
			pProc = Runtime.getRuntime().exec(theArgs);
		
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(60000);
			int nRet = pProc.exitValue();
			sRet = "\nuninst complete [" + nRet + "]";
			}
		catch (IOException e) 
			{
			sRet = e.getMessage();
			e.printStackTrace();
			} 
		catch (InterruptedException e)
			{
			// TODO Auto-generated catch block
			e.printStackTrace();
			}
		
		return (sRet);
	}
	
	public String InstallApp(String sApp, OutputStream out)
		{
		String sRet = "";
		String [] theArgs = new String [3];
		File	srcFile = new File(sApp);
//		boolean bDone = false;
//		int		nExitCode;

		theArgs[0] = "su";
		theArgs[1] = "-c";
		theArgs[2] = "mv " + GetTmpDir() + "/" + srcFile.getName() + " /data/local/tmp/" + srcFile.getName() + ";exit";
//		theArgs[2] += ";chmod 666 /data/local/tmp/" + srcFile.getName();
//		theArgs[2] += ";pm install /data/local/tmp/" + srcFile.getName() + " Cleanup";
//		theArgs[2] += ";done;exit";
		
		sRet = CopyFile(sApp, GetTmpDir() + "/" + srcFile.getName());
		try {
			out.write(sRet.getBytes());
			out.flush();
		} catch (IOException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}
//		CopyFile(sApp, GetTmpDir() + "/" + srcFile.getName());

		try 
			{
			pProc = Runtime.getRuntime().exec(theArgs);
			
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(90000);
			int nRet = pProc.exitValue();
//			boolean bRet = outThrd.isAlive();
			sRet = "\nmove complete [" + nRet + "]";
			try 
				{
				out.write(sRet.getBytes());
				out.flush();
				}
			catch (IOException e1)
				{
				// TODO Auto-generated catch block
				e1.printStackTrace();
				}
			
			theArgs[2] = "chmod 666 /data/local/tmp/" + srcFile.getName() + ";exit";
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd2 = new RedirOutputThread(pProc, out);
			outThrd2.start();
			outThrd2.join(10000);
			int nRet2 = pProc.exitValue();
//			bRet = outThrd2.isAlive();
			sRet = "\npermission change complete [" + nRet2 + "]\n";
			try {
				out.write(sRet.getBytes());
				out.flush();
				}
			catch (IOException e1)
				{
				// TODO Auto-generated catch block
				e1.printStackTrace();
				}
			
			theArgs[2] = "pm install /data/local/tmp/" + srcFile.getName() + " Cleanup" + ";exit";
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd3 = new RedirOutputThread(pProc, out);
			outThrd3.start();
			outThrd3.join(60000);
			int nRet3 = pProc.exitValue();
			sRet = "\ninstallation complete [" + nRet3 + "]";
			try {
				out.write(sRet.getBytes());
				out.flush();
				}
			catch (IOException e1)
				{
				// TODO Auto-generated catch block
				e1.printStackTrace();
				}
			
			theArgs[2] = "rm /data/local/tmp/" + srcFile.getName() + ";exit";
			pProc = Runtime.getRuntime().exec(theArgs);
			RedirOutputThread outThrd4 = new RedirOutputThread(pProc, out);
			outThrd4.start();
			outThrd4.join(60000);
			int nRet4 = pProc.exitValue();
			sRet = "\ntmp file removed [" + nRet4 + "]";
			try {
				out.write(sRet.getBytes());
				out.flush();
				}
			catch (IOException e1)
				{
				// TODO Auto-generated catch block
				e1.printStackTrace();
				}
			sRet = "\nSuccess";
			}
		catch (IOException e) 
			{
			sRet = e.getMessage();
			e.printStackTrace();
			} 
		catch (InterruptedException e)
			{
			// TODO Auto-generated catch block
			e.printStackTrace();
			}

		return (sRet);
		}

	public String StartUpdateOMatic(String sPkgName, String sPkgFileName)
		{
		String sRet = "";
	
		Context ctx = contextWrapper.getApplicationContext();
		PackageManager pm = ctx.getPackageManager();

		Intent prgIntent = new Intent();
		prgIntent.setPackage("com.mozilla.UpdateOMatic");
		prgIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

		try {
			PackageInfo pi = pm.getPackageInfo("com.mozilla.UpdateOMatic", PackageManager.GET_ACTIVITIES | PackageManager.GET_INTENT_FILTERS);
			ActivityInfo [] ai = pi.activities;
			for (int i = 0; i < ai.length; i++)
				{
				ActivityInfo a = ai[i];
				if (a.name.length() > 0)
					{
					prgIntent.setClassName(a.packageName, a.name);
					break;
					}
				}
			}
		catch (NameNotFoundException e)
			{
			e.printStackTrace();
			}
		
		prgIntent.putExtra("pkgName", sPkgName);
		prgIntent.putExtra("pkgFileName", sPkgFileName);

		try 
			{
			contextWrapper.startActivity(prgIntent);
			sRet = "exit";
			}
		catch(ActivityNotFoundException anf)
			{
			anf.printStackTrace();
			} 
	
		ctx = null;
		return (sRet);
		}

	public String StartJavaPrg(String [] sArgs)
		{
		String sRet = "";
		String sArgList = "";
		String sUrl = "";
		String sRedirFileName = "";
		
		Context ctx = contextWrapper.getApplicationContext();
		PackageManager pm = ctx.getPackageManager();

		Intent prgIntent = new Intent();
		prgIntent.setPackage(sArgs[0]);
		prgIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

		try {
			PackageInfo pi = pm.getPackageInfo(sArgs[0], PackageManager.GET_ACTIVITIES | PackageManager.GET_INTENT_FILTERS);
			ActivityInfo [] ai = pi.activities;
			for (int i = 0; i < ai.length; i++)
				{
				ActivityInfo a = ai[i];
				if (a.name.length() > 0)
					{
					prgIntent.setClassName(a.packageName, a.name);
					break;
					}
				}
			}
		catch (NameNotFoundException e)
			{
			e.printStackTrace();
			}
		
		if (sArgs.length > 1)
			{
//			if (sArgs[0].contains("android.browser"))
				prgIntent.setAction(Intent.ACTION_VIEW);
			
			if (sArgs[0].contains("fennec"))
				{
				sArgList = "";
				sUrl = "";
				
				for (int lcv = 1; lcv < sArgs.length; lcv++)
					{
					if (sArgs[lcv].contains("://"))
						sUrl = sArgs[lcv];
					else
						{
						if (sArgs[lcv].equals(">"))
							{
							lcv++;
							if (lcv < sArgs.length)
								sRedirFileName = sArgs[lcv++];
							}
						else
							sArgList += " " + sArgs[lcv];
						}
					}
				
				if (sArgList.length() > 0)
					prgIntent.putExtra("args", sArgList.trim());
				
				if (sUrl.length() > 0)
					prgIntent.setData(Uri.parse(sUrl.trim()));
				}
			else
				{
				for (int lcv = 1; lcv < sArgs.length; lcv++)
					sArgList += " " + sArgs[lcv];
				
				prgIntent.setData(Uri.parse(sArgList.trim()));
				}
			}
		else
			prgIntent.setData(Uri.parse("about:blank"));

		try 
			{
			contextWrapper.startActivity(prgIntent);
			}
		catch(ActivityNotFoundException anf)
			{
			anf.printStackTrace();
			}
		
		ctx = null;
		return (sRet);
		}

	public String StartPrg(String [] progArray, OutputStream out)
		{
		String sRet = "";
		
		try 
			{
			pProc = Runtime.getRuntime().exec(progArray);
			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(10000);
			int nRetCode = pProc.exitValue();
			sRet = "return code [" + nRetCode + "]";
			}
		catch (IOException e) 
			{
			e.printStackTrace();
			}
		catch (InterruptedException e)
			{
			e.printStackTrace();
			sRet = "Timed out!";
			}

		return (sRet);
		}
/*	
	@SuppressWarnings("unchecked")
	public static void set(String key, String value) throws Exception
		{
	    Class[] classes = Collections.class.getDeclaredClasses();
	    Map env = System.getenv();
	    for(Class cl : classes)
	    	{
	        if("java.util.Collections$UnmodifiableMap".equals(cl.getName()))
	        	{
	            Field field = cl.getDeclaredField("m");
	            field.setAccessible(true);
	            Object obj = field.get(env);
	            Map<String, String> map = (Map<String, String>) obj;
	            map.put(key, value);
	        	}
	    	}
		}

*/	
	public String StartPrg2(String [] progArray, OutputStream out)
		{
		String sRet = "";
		
		int	nArraySize = 0;
		int	nArgs = progArray.length - 1; // 1st arg is the environment string
		int	lcv	= 0;
		int	temp = 0;

		String sEnvString = progArray[0];

		// Set up command line args stripping off the environment string
		String [] theArgs = new String [nArgs];
		for (lcv = 0; lcv < nArgs; lcv++)
			{
			theArgs[lcv] = progArray[lcv + 1];
			}
		
		try 
			{
			String [] envStrings = sEnvString.split(",");
			Map<String, String> newEnv = new HashMap<String, String>();
			
			for (lcv = 0; lcv < envStrings.length; lcv++)
				{
				temp = envStrings[lcv].indexOf("=");
				if (temp > 0)
					{
					newEnv.put(	envStrings[lcv].substring(0, temp), 
								envStrings[lcv].substring(temp + 1, envStrings[lcv].length()));
					}
				}
			
			Map<String, String> sysEnv = System.getenv();
			
			nArraySize = sysEnv.size();
			
			for (Map.Entry<String, String> entry : newEnv.entrySet())
				{
				if (!sysEnv.containsKey(entry.getKey()))
					{
					nArraySize++;
					}
				}
			
			String[] envArray = new String[nArraySize];
				
			int		i = 0;
			int		offset;
			String	sKey = "";
			String 	sValue = "";
			
	        for (Map.Entry<String, String> entry : sysEnv.entrySet())
	        	{
	        	sKey = entry.getKey();
	        	if (newEnv.containsKey(sKey))
	        		{
	        		sValue = newEnv.get(sKey);
	        		if ((offset = sValue.indexOf("$" + sKey)) != -1)
	        			{
	        			envArray[i++] = sKey + 
	        							"=" + 
	        							sValue.substring(0, offset) + 
	        							entry.getValue() + 
	        							sValue.substring(offset + sKey.length() + 1);
	        			}
	        		else
	        			envArray[i++] = sKey + "=" + sValue;
	        		newEnv.remove(sKey);
	        		}
	        	else
	        		envArray[i++] = entry.getKey() + "=" + entry.getValue();
	        	}
	        
			for (Map.Entry<String, String> entry : newEnv.entrySet())
				{
        		envArray[i++] = entry.getKey() + "=" + entry.getValue();
				}
	        
			pProc = Runtime.getRuntime().exec(theArgs, envArray);

			RedirOutputThread outThrd = new RedirOutputThread(pProc, out);
			outThrd.start();
			outThrd.join(10000);
			int nRetCode = pProc.exitValue();
			sRet = "return code [" + nRetCode + "]";
			}
		catch(UnsupportedOperationException e)
			{
			if (e != null)
				e.printStackTrace();
			}
		catch(ClassCastException e)
			{
			if (e != null)
				e.printStackTrace();
			}
		catch(IllegalArgumentException e)
			{
			if (e != null)
				e.printStackTrace();
			}
		catch(NullPointerException e)
			{
			if (e != null)
				e.printStackTrace();
			}
		catch (IOException e) 
			{
			e.printStackTrace();
			}
		catch (InterruptedException e)
			{
			e.printStackTrace();
			sRet = "Timed out!";
			}

		return (sRet);
		}
/*	
	public String InstallApplication()
		{
		String sRet = "";
		String sFileName = Environment.getExternalStorageDirectory() + "/org.mozilla.fennec.apk";
		
		Intent instIntent = new Intent();
		
		instIntent.setAction(android.content.Intent.ACTION_VIEW);
		instIntent.setDataAndType(Uri.fromFile(new File(sFileName)), "application/vnd.android.package-archive");
//		instIntent.setDataAndType(Uri.parse("file:///sdcard/org.mozilla.fennec.apk"), "application/vnd.android.package-archive");
		SUTAgentAndroid.me.startActivity(instIntent);
		
//		Instrumentation inst = new Instrumentation();
//		inst.sendKeyDownUpSync(KeyEvent.KEYCODE_SOFT_LEFT);
		
		return(sRet);
		}

	public String UninstallApplication()
		{
		String sRet = "";
		Uri		pkgURI = Uri.parse("package:" + "org.mozilla.fennec");
	
		Intent unInstIntent = new Intent(Intent.ACTION_DELETE, pkgURI);
	
		SUTAgentAndroid.me.startActivity(unInstIntent);

		return(sRet);
		}
*/
	private String PrintUsage()
		{
		String sRet = 
			"run [executable] [args]      - start program no wait\n" +
			"exec [executable] [args]     - start program wait\n" +
			"fire [executable] [args]     - start program no wait\n" +
			"envrun [env pairs] [cmdline] - start program no wait\n" +
			"kill [program name]          - kill program no path\n" +
			"killall                      - kill all processes started\n" +
			"ps                           - list of running processes\n" +
			"info                         - list of device info\n" +
			"        [os]                 - os version for device\n" +
			"        [id]                 - unique identifier for device\n" +
			"        [uptime]             - uptime for device\n" +
			"        [systime]            - current system time on device\n" +
			"        [screen]             - width, height and bits per pixel for device\n" +
			"        [memory]             - physical, free, available, storage memory for device\n" +
			"        [processes]          - list of running processes see 'ps'\n" +
			"deadman timeout              - set the duration for the deadman timer\n" +
			"alrt [on/off]                - start or stop sysalert behavior\n" +
			"disk [arg]                   - prints disk space info\n" +
			"cp file1 file2               - copy file1 to file2 on device\n" +
			"time file                    - timestamp for file on device\n" +
			"hash file                    - generate hash for file on device\n" +
			"cd directory                 - change cwd on device\n" +
			"cat file                     - cat file on device\n" +
			"cwd                          - display cwd on device\n" +
			"mv file1 file2               - move file1 to file2 on device\n" +
			"push filename                - push file to device\n" +
			"rm file                      - delete file on device\n" +
			"rmdr directory               - delete directory on device even if not empty\n" +
			"mkdr directory               - create directory on device\n" +
			"dirw directory               - tests whether the directory is writable on the device\n" +
			"stat processid               - stat process on device\n" +
			"dead processid               - print whether the process is alive or hung on device\n" +
			"mems                         - dump memory stats on device\n" +
			"ls                           - print directory on device\n" +
			"tmpd                         - print temp directory on device\n" +
			"ping [hostname/ipaddr]       - ping a network device\n" +
			"unzp zipfile destdir         - unzip the zipfile into the destination dir\n" +
			"zip zipfile src              - zip the source file/dir into zipfile\n" +
			"rebt                         - reboot device\n" +
			"inst /path/filename.apk      - install the referenced apk file\n" +
			"uninst packagename           - uninstall the referenced package\n" +
			"updt pkgname pkgfile         - unpdate the referenced package\n" +
			"clok                         - the current device time expressed as the number of millisecs since epoch\n" +
			"settime date time            - sets the device date and time (YYYY/MM/DD HH:MM:SS)\n" +
			"tzset timezone               - sets the device timezone format is GMTxhh:mm x = +/- or a recognized Olsen string\n" +
			"tzget                        - returns the current timezone set on the device\n" +
			"rebt                         - reboot device\n" +
			"quit                         - disconnect SUTAgent\n" +
			"exit                         - close SUTAgent\n" +
			"ver                          - SUTAgent version\n" +
			"help                         - you're reading it";
		return (sRet);
		}
}
