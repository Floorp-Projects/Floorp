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

package netscape.asw;

import netscape.npasw.CPGenerator;
import netscape.npasw.ServerDownload;
import netscape.npasw.Trace;

import java.lang.*;
//import AMDProgressBar;

public class CPGeneratorProgress extends ProgressApplet
{
    final static String DOWNLOAD_STRING = "Downloading:";
    final static String UNJAR_STRING = "Uncompressing:";
    final static String DONE_STRING = "Done...";
    final static String CONTACTING_SERVER = "Contacting registration server...";
    final static String SENDING = "Sending registration information...";
    final static String WAITING = "Waiting for response...";
    final static String RECEIVING_RESPONSE = "Receiving server data...";
    final static String ABORT = "There were problems with the server connection...";
    final static String DIALING_STRING = "Calling registration server...";

    protected int getState()
    {
        if ( ServerDownload.getState() == ServerDownload.IDLE )
            return CPGenerator.getState();
        else
            return ServerDownload.getState();
    }

	public void init()
	{
		super.init();
		status.setText( DIALING_STRING );
	}
	
    public void run()
    {
        try
        {
			while ( true )
			{
				Trace.TRACE( "running applet" );
				
				int             lastState = CPGenerator.DONE;
				int             thisState = CPGenerator.DONE;
				String          lastString = "";
				String          thisString = "";
				
				while ( CPGenerator.done == false )
				{
					//Trace.TRACE( "CPGenerator not done" );
					thisState = getState();
					thisString = new String( CPGenerator.currentFile );
					
					if ( thisState != lastState )
					{
						String buffer = null;
						
						switch ( thisState )
						{
							case ServerDownload.DOWNLOADING:
								buffer = DOWNLOAD_STRING;
							break;
							
							case ServerDownload.UNJARRING:
								buffer = UNJAR_STRING;
							break;
							
							case CPGenerator.CONTACTING_SERVER:
								buffer = CONTACTING_SERVER;
							break;
							
							case CPGenerator.SENDING:
								buffer = SENDING;
							break;
							
							case CPGenerator.WAITING:
								buffer = WAITING;
							break;
							
							case CPGenerator.RECEIVING_RESPONSE:
								buffer = RECEIVING_RESPONSE;
							break;
							
							case CPGenerator.DONE:
								buffer = DONE_STRING;
							break;
							
							case CPGenerator.ABORT:
								buffer = ABORT;
							break;
	                    }
						
						status.setText( buffer );
						lastState = thisState;
					}
					
					if ( thisString.compareTo( lastString ) != 0 )
					{
						progress.setText( thisString );
						lastString = thisString;
					}
					
					if ( ServerDownload.getBytesDownloaded() == 0 || CPGenerator.totalBytes == 0 )
						progressBar.setPercent( 0.0 );
					else
						progressBar.setPercent( (double)ServerDownload.getBytesDownloaded() / (double)CPGenerator.totalBytes );
					
					repaint();
					Thread.sleep( 50 );
				}
				
				Trace.TRACE( "CPGenerator done" );
				progressBar.setPercent( 1.0 );
				progress.setText( "" );
				status.setText( DONE_STRING );
				
				repaint();
				Thread.sleep( 1000 );
			}
		}
		catch (Exception e)
		{
			;
		}
    }
}
