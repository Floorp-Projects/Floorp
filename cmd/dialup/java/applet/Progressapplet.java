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

import netscape.npasw.Trace;
import java.awt.*;
import java.applet.*;
//import AMDProgressBar;

public class ProgressApplet extends Applet implements Runnable
{
    Label               status;
    Label               progress;
    Button              cancel;

    final static Font   TEXTFONT = new Font( "Dialog", Font.BOLD, 12 );
    final static Font   BARFONT = new Font( "Helvetica", Font.PLAIN, 9 );
    final static Color  BOXCOLOR = Color.darkGray;
    final static Color  BARCOLOR = Color.blue;
    final static Color  BACKCOLOR = Color.gray;

    AMDProgressBar      progressBar;
    int                 index = 0;
    int                 loop1 = 1;
    Thread              thread = null;

	public void init()
	{
		super.init();
		Trace.TRACE( "applet got loaded" );
		//setBackground( Color.white );
		setLayout( null );
		//setResizable( false );
		//setModal( true );
		
		addNotify();
		resize( insets().left + insets().right + 360, insets().top + insets().bottom + 120 );
		setFont( TEXTFONT );
		
		status = new java.awt.Label( "" );
		status.reshape( insets().left + 12, insets().top + 8, 340, 24 );
		status.setFont( TEXTFONT );
		add( status );
		
		progress = new java.awt.Label( "" );
		progress.reshape( insets().left + 12, insets().top + 32, 340, 20 );
		progress.setFont( BARFONT );
		add( progress );
		
		//cancel = new java.awt.Button( "Cancel" );
		//cancel.reshape( insets().left + 240, insets().top + 80, 80, 20 );
		//add( cancel );
		
		// [canvas]
		progressBar = new AMDProgressBar();
		progressBar.setFont( TEXTFONT );
		progressBar.reshape( 12, 80, 300, 21 );
		progressBar.setBoxColors( BOXCOLOR, BOXCOLOR );
		progressBar.setBarColor( BARCOLOR );
		progressBar.setBackgroundColors( BACKCOLOR, BACKCOLOR );
		add( progressBar );
		
		show();
		enable();
		repaint();
		Trace.TRACE( "done initing applet" );
	}

	public boolean handleEvent( Event event )
	{
        if ( event.target == cancel && event.id == Event.ACTION_EVENT )
        {
            cancel_Clicked( event );
            return true;
        }
        return super.handleEvent( event );
    }

    void cancel_Clicked( Event event )
    {
        //pm.UserCancelled();
    }


    public void update( Graphics g )
    {
        paint( g );
    }

    public void paint( Graphics g )
    {
        super.paint( g );
    }

    public void run()
    {
    }

    public void start()
    {
    	Trace.TRACE( "starting applet" );
        thread = new Thread( this );
        thread.start();
    }

    public void stop()
    {
    	Trace.TRACE( "stopping applet" );
        thread.stop();
        thread = null;
    }
}
