/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.npasw;

import netscape.plugin.Plugin;
import netscape.security.*;
import java.util.Hashtable;
import java.util.Enumeration;
import java.io.*;
import netscape.npasw.*;

public class SetupPlugin extends Plugin
{
    static private Hashtable       iniFileCache = new Hashtable();

    static final public void debug( String s )
    {
        System.out.println( s );
    }

    final public String[] newStringArray( int numElements )
    {
        return new String[ numElements ];
    }

    final public void SetKiosk( boolean flag )
    {
        if ( privilegeCheck() == true )
            SECURE_SetKiosk( flag );
    }

	final public void FlushCache()
	{
		if ( privilegeCheck() == false )
			return;
		
		for ( Enumeration iniFiles = iniFileCache.keys(); iniFiles.hasMoreElements(); )
		{
			File			iniFile = (File)iniFiles.nextElement();
			IniFileData		iniFileData = (IniFileData)iniFileCache.get( iniFile );
			
			try
			{
				//Trace.TRACE( "flushing ini cache object" );
				iniFileData.flush();
			}
			catch ( Throwable e )
			{
			}
		}
	}
	
    final public String GetNameValuePair( String filePath, String sectionName, String valueName )
    {
    /*  if ( privilegeCheck() == true )
            return SECURE_GetNameValuePair( filePath, sectionName, valueName);
        else
            return null;
    */
        if ( privilegeCheck() == false )
            return new String( "" );

        final String        section1 = sectionName;
        final String        value1 = valueName;

		try
		{
			File                file = new File( filePath );
			//Trace.TRACE( "GetNameValuePair" );
			//Trace.TRACE( "	file: " + file.getName() );
			//Trace.TRACE( "	hash: " + file.hashCode() );
			IniFileData         iniFile = (IniFileData)iniFileCache.get( file );

			if ( iniFile == null )
			{
	 			//Trace.TRACE( "file: " + file.getName() );
	           	//Trace.TRACE( "iniFile is null" );
	           	
				iniFile = new IniFileData( file );
				iniFileCache.put( file, iniFile );
			}
	      
	     	//Trace.TRACE( "getting file: " + filePath + " section: " + section1 + " value: " + value1 );
			String      value = iniFile.getValue( section1, value1 );
			if ( value == null )
				return new String( "" );
			else
			{
				//Trace.TRACE( "returning: " + value );
				return new String( value );
			}
		}
        catch ( Throwable e )
        {
            //Trace.TRACE( "caught an exception: " + e.getMessage() );
            return new String( "" );
        }
    }

    final public void SetNameValuePair( String filePath, String sectionName, String name, String value )
    {
    /*  if ( privilegeCheck() == true )
    		SECURE_SetNameValuePair( file, section, name, value );
	*/
    	if ( privilegeCheck() == false )
    		return;
    	
		try
		{
			File				file = new File( filePath );
			//Trace.TRACE( "SetNameValuePair" );
			//Trace.TRACE( "	file: " + file.getName() );
			//Trace.TRACE( "	hash: " + file.hashCode() );
			IniFileData			iniFile = (IniFileData)iniFileCache.get( file );
			
    		if ( iniFile == null )
    		{
				//Trace.TRACE( "file: " + file.getName() );
    			//Trace.TRACE( "iniFile is null" );
    			
    			iniFile = new IniFileData( file );
    			iniFileCache.put( file, iniFile );
			}
    		
			//Trace.TRACE( "setValue: " + sectionName + " " + name + " " + value );
			iniFile.setValue( sectionName, name, value );
		}
		catch ( Throwable e )
		{
		}    		
    }



    final public Object ReadFile( String file )
    {
        if ( privilegeCheck() == true )
            return SECURE_ReadFile( file );
        else
            return null;
    }

    final public void WriteFile( String file, Object data )
    {
        if ( privilegeCheck() == true )
            SECURE_WriteFile( file, data );
    }


    final public String[] GetFolderContents( String path, String suffix )
    {
    /*  if ( privilegeCheck() == true )
            return SECURE_GetFolderContents( path, suffix );
        else
            return null ;
    */
        if ( privilegeCheck() == false )
            return null;

        try
        {
            File            dir = new File( path );
            SuffixFilter    filter = new SuffixFilter( suffix );
            String[]        fileList = dir.list( filter );

            return fileList;
        }
        catch ( Exception e )
        {
            return null;
        }
    }



    final public String[] GetRegInfo( boolean flushDataFlag )
    {
        if ( privilegeCheck() == true )
            return SECURE_GetRegInfo( flushDataFlag );
        else
            return null;
    }

    final public void DesktopConfig( String accountName, String iconFilename, String acctsetFilename )
    {
        if ( privilegeCheck() == true )
            SECURE_DesktopConfig( accountName, iconFilename, acctsetFilename );
    }

    final public boolean SaveTextToFile( String suggestedFilename, String data, boolean promptFlag )
    {
        if ( privilegeCheck() == true )
            return SECURE_SaveTextToFile( suggestedFilename, data, promptFlag );
        else
            return false;
    }

    final public String EncryptString( String cleartext )
    {
        if ( privilegeCheck() == true )
            return SECURE_EncryptString( cleartext );
        else
            return null;
    }

    final public String EncryptPassword( String cleartext )
    {
        if ( privilegeCheck() == true )
            return SECURE_EncryptPassword( cleartext );
        else
            return null;
    }

    final public void OpenModemWizard()
    {
        if ( privilegeCheck() == true )
            SECURE_OpenModemWizard();
    }

    final public void CloseModemWizard()
    {
        if ( privilegeCheck() == true )
            SECURE_CloseModemWizard();
    }

    final public boolean IsModemWizardOpen()
    {
        if ( privilegeCheck() == true )
            return SECURE_IsModemWizardOpen();
        else
            return false;
    }

    final public String[] GetModemList()
    {
        if ( privilegeCheck() == true )
            return SECURE_GetModemList();
        else
            return null;
    }

    final public String GetCurrentModemName()
    {
        if ( privilegeCheck() == true )
            return SECURE_GetCurrentModemName();
        else
            return null;
    }

    final public String GetModemType( String modem )
    {
        if ( privilegeCheck() == true )
            return SECURE_GetModemType( modem );
        else
            return null;
    }

    final public boolean DialerConnect()
    {
        if ( privilegeCheck() == true )
            return SECURE_DialerConnect();
        else
            return false;
    }

    final public void DialerHangup()
    {
        if ( privilegeCheck() == true )
            SECURE_DialerHangup();
    }

    final public boolean IsDialerConnected()
    {
        if ( privilegeCheck() == true )
            return SECURE_IsDialerConnected();
        else
            return false;
    }

    final public void DialerConfig( String dialerData[], boolean regMode )
    {
        if ( privilegeCheck() == true )
            SECURE_DialerConfig( dialerData, regMode );
    }

    final public boolean GenerateComparePage( String localFolder, String sCGIUrl, String sRegRoot,
    	String metadataMode, String reggieData[] )
    {
		if ( privilegeCheck() == true )
			return CPGenerator.generateComparePage( localFolder, sCGIUrl, sRegRoot, metadataMode, reggieData );
		else
			return false;
    }

	final public String[] GetISPPopList( String isp )
	{
		if ( privilegeCheck() == true )
			return CPGenerator.getISPPopList( isp );
		else
			return null;
	}
	
	final public void CreateConfigIAS( String isp, int index )
	{
		if ( privilegeCheck() == true )
			CPGenerator.createConfigIAS( isp, index );
	}

	final public String GetISPDisplayName( String ispName )
	{
		if ( privilegeCheck() == true )
			return CPGenerator.getISPDisplayName( ispName );
		else
			return null;
	}
	
	final public String GetCurrentProfileDirectory()
	{
		if ( privilegeCheck() == true )
		{
			String		temp = SECURE_GetCurrentProfileDirectory();
			//Trace.TRACE( "getCurrentProfileDirectory: " + temp );
			return temp;
		}
		else
			return null;
	}

    final public String GetCurrentProfileName()
    {
        if ( privilegeCheck() == true )
            return SECURE_GetCurrentProfileName();
        else
            return null;
    }

    final public void SetCurrentProfileName( String profileName )
    {
        if ( privilegeCheck() == true )
            SECURE_SetCurrentProfileName( profileName );
    }

    final public String GetExternalEditor()
    {
        if ( privilegeCheck() == true )
            return SECURE_GetExternalEditor();
        else
            return null;
    }

    final public void OpenFileWithEditor( String app, String file )
    {
        if ( privilegeCheck() == true )
            SECURE_OpenFileWithEditor( app, file );
    }

    final public boolean NeedReboot()
    {
        if ( privilegeCheck() == true )
            return SECURE_NeedReboot();
        else
            return false;
    }

    final public void Reboot( String accountSetupPathname )
    {
        if ( privilegeCheck() == true )
            SECURE_Reboot( accountSetupPathname );
    }

    final public void QuitNavigator()
    {
     	if ( privilegeCheck() == true )
    	{
    		FlushCache();
   			iniFileCache = null;
    		SECURE_QuitNavigator();
   		}
    }

    final public boolean CheckEnvironment()
    {
        if ( privilegeCheck() == true )
            return SECURE_CheckEnvironment();
        else
            return false;
    }

    final public boolean Milan( String name, String value, boolean pushPullFlag, boolean extendedLengthFlag )
    {
        if ( privilegeCheck() == true )
            return SECURE_Milan( name, value, pushPullFlag, extendedLengthFlag );
        else
            return false;
    }



/*
    Private methods:
*/

    private boolean privilegeCheck()
    {
        boolean     privilegeFlag = false;

        try
        {
            PrivilegeManager.checkPrivilegeEnabled( "AccountSetup" );       // All Hail The King !!!
            privilegeFlag = true;
        }
        catch ( Exception e )
        {
            System.out.println( "Account Setup Security Exception: " + e.toString() );
            e.printStackTrace();
            privilegeFlag = false;
        }

        //  un-comment the following line for testing:
        //privilegeFlag = true;

        return privilegeFlag;
    }



/*
    Private native methods:
*/

    private native void         SECURE_SetKiosk( boolean flag );

    //private native String     SECURE_GetNameValuePair( String file, String section, String name );
    private native void         SECURE_SetNameValuePair( String file, String section, String name, String value );

    private native Object       SECURE_ReadFile( String file );
    private native void         SECURE_WriteFile( String file,Object data );

    //private native String[]       SECURE_GetFolderContents( String path, String suffix );
    private native String[]     SECURE_GetRegInfo( boolean flushDataFlag );

    private native void         SECURE_DesktopConfig( String accountName, String iconFilename, String acctsetFilename );
    private native boolean      SECURE_SaveTextToFile( String suggestedFilename, String data, boolean promptFlag );
    private native String       SECURE_EncryptString( String cleartext );
    private native String       SECURE_EncryptPassword( String cleartext );

    private native void         SECURE_OpenModemWizard();
    private native void         SECURE_CloseModemWizard();
    private native boolean      SECURE_IsModemWizardOpen();

    private native String[]     SECURE_GetModemList();
    private native String       SECURE_GetModemType( String modem );
    private native String       SECURE_GetCurrentModemName();

    private native boolean      SECURE_DialerConnect();
    private native void         SECURE_DialerHangup();
    private native boolean      SECURE_IsDialerConnected();
    private native void         SECURE_DialerConfig( String dialerData[], boolean regMode );


    private native String       SECURE_GetCurrentProfileDirectory();
    private native String       SECURE_GetCurrentProfileName();
    private native void         SECURE_SetCurrentProfileName( String profileName );

    private native String       SECURE_GetExternalEditor();
    private native void         SECURE_OpenFileWithEditor( String app, String file );

    private native boolean      SECURE_NeedReboot();
    private native void         SECURE_Reboot( String accountSetupPathname );
    private native void         SECURE_QuitNavigator();

    private native boolean      SECURE_CheckEnvironment();

    private native boolean      SECURE_Milan( String name, String value, boolean pushPullFlag, boolean extendedLengthFlag );
}




/*
    Note: routines passing/returning string arrays use the format: "VARIABLE=DATA"
*/
