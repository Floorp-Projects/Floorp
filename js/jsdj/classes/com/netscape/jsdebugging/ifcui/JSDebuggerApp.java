/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
* Core Application class for debugger (used both when app or applet)
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.io.*;
import java.awt.Dimension;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.api.*;
import netscape.javascript.*;
import netscape.security.PrivilegeManager;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolbar.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolTip.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.PopupButton;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.AssertFailureHandler;


public class JSDebuggerApp
    extends Application
    implements Target, AssertFailureHandler, EmperorOwner
{
    private static final boolean WILL_EXPIRE = false;
    private static final boolean IS_INTERNAL_RELEASE = true;
    private static final boolean ALWAYS_SHOW_BUILD_DATE = true;

    // "15 Sep 1997" -> 874306800000L   // PR1 date
    // "31 Oct 1997" -> 878284800000L   // PR2 date
    // "31 Mar 1998" -> 891331200000L   // 1.1 PR1 Date (proposed)
    private static final long _deathTime = 891331200000L;

    public static final  int    MAJOR_VERSION   = 1;
    public static final  int    MINOR_VERSION   = 2;
    public static final  int    PREVIEW_VERSION = 1;

    private static final String _mainTitleLocal  = "Netscape JavaScript Debugger";
//    private static final String _mainTitleLocal  = "(CLIENT) Netscape JavaScript Debugger";
    private static final String _mainTitleServer = "(SERVER) Netscape JavaScript Debugger";
    private static final String _mainTitleRhino = "Netscape JavaScript for Java Debugger";

    // this is usually the string literal version of Emperor 'modes'
    // -- parsed below --
    public void setMode(String s)
    {
        if(AS.S)ER.T(null==_emperor, "attempt to setMode while running", this);
        _modeString = s;
    }
    public void setHost(String s)
    {
        if(AS.S)ER.T(null==_emperor, "attempt to setHost while running", this);
        _host = s;
    }

    // override Application.init()
    public void init()
    {
        super.init();
        initForReal();
    }

    private void initForReal()
    {
//        System.out.println("++++ initForReal() start" );

        // Let's ask for everything upfront to get this all out of the
        // way (from the user's point of view)
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            PrivilegeManager.enablePrivilege("UniversalFileRead");

            // It doesn't really matter that much if the user says no to
            // these (at this point)
            try
            {
                PrivilegeManager.enablePrivilege("UniversalFileWrite");
                PrivilegeManager.enablePrivilege("UniversalTopLevelWindow");
            }
            catch(Exception e)
            {
                // just eat these
            }

        }
        catch(Exception e)
        {
            performCommandLater( this, LOAD_FAILED_CMD, null );
            return;
        }

        if(AS.DEBUG)
        {
            Thread t = Thread.currentThread();
            _uiThreadForAssertCheck = t;
            ER.setFailureHandler(t,this);
        }

        // set some static globals...
        Env.Init();

        // we don't want to spit this info to the console if not an applet
        if( isApplet() )
        {
            int platform = Env.getCoursePlatformType();

            System.out.println("JSD platform is " +
                               (platform == Env.ENV_WIN ?
                                 "Windows" :
                                platform == Env.ENV_MAC ?
                                 "Mac" :
                                 "Unix" ));
            System.out.println("JSD install directory: " + Env.getCodebaseDir());
            System.out.println("JSD prefs directory: " + Env.getPrefsDir());
        }

        // set mode
        if(null == _modeString)
            _modeString = parameterNamed( "DebugHostMode" );

        if(null != _modeString)
        {
            if( _modeString.equals("LOCAL") )
                _mode = Emperor.LOCAL;
            else if( _modeString.equals("REMOTE_SERVER") )
                _mode = Emperor.REMOTE_SERVER;
            else if( _modeString.equals("RHINO") )
                _mode = Emperor.RHINO;
            else
                _mode = Emperor.LOCAL;
        }
        else
            _mode = Emperor.LOCAL;

        // set host (this may be further massaged below...

        if(null == _host)
            _host = parameterNamed( "DebugHost" );

        // init the system that will allow us access to the startup prefs file
        // this needs to be done after our mode is established
        _initStartupPrefs();

        if(AS.DEBUG)
        {
            if( Emperor.LOCAL == _mode )
                Log.setFilename("jsdclog.log", true);
            else
                Log.setFilename("jsdslog.log", true);
            Log.setEnabled(true);
            Log.setEnabledType(Log.ERROR, true);
            Log.setEnabledType(Log.WARN, true);
            Log.setEnabledType(Log.TRACE, false);
            Log.setShowDate(true);
            Log.setShowThreadName(true);
            Log.setLogToFile(true);
            Log.setLogToConsole(false);
            Log.log(null, "+++++++++++++++ startup begin +++++++++++++++" );
            // we don't want to spit this info to the console if not an applet
            if( isApplet() )
                System.out.println("JSD is using log file: " + Log.getFilename());
        }
        else
        {
            Log.setEnabled(false);
        }

        _mainWindow = new ToolTipExternalWindow();

        Dimension res = AWTCompatibility.awtToolkit().getScreenSize();
        _mainWindow.setBounds(0, 0, 525, res.height - 50);

        if( Emperor.REMOTE_SERVER == _mode )
            _mainWindow.setTitle(_mainTitleServer);
        else if( Emperor.RHINO == _mode )
            _mainWindow.setTitle(_mainTitleRhino);
        else
            _mainWindow.setTitle(_mainTitleLocal);

        setMainRootView(_mainWindow.rootView());

        _mainWindow.show();
        setWaitCursor(true);

        StatusWindow statusWindow = new StatusWindow();
        statusWindow.setText("Loading...");
        statusWindow.show();

        if( WILL_EXPIRE && daysTillDeath() <= 0 )
        {
            setWaitCursor(false);
            Alert.runAlertInternally( Alert.warningImage(),
                "Error",
                  "This Preview copy of Netscape JavaScript Debugger has expired\n"
                + "Please go to the Netscape home site to get a new version\n"
                + "\n"
                + "        The debugger will exit now...",
                "OK", null, null );
            performCommandLater( this, LOAD_FAILED_CMD, null );
            return;
        }

        if( staleTwisterClassesFound() )
        {
            setWaitCursor(false);
            Alert.runAlertInternally( Alert.warningImage(),
                "Error",
                  "Conflict with obsolete Java classes detected.\n"
                + "\n"
                + "Netscape Visual JavaScript Preview Release 1 must be uninstalled\n"
                + "before the Netscape JavaScript Debugger can be run.\n"
                + "\n"
                + "Please see the Debugger release notes for additional information\n"
                + "\n"
                + "        The debugger will exit now...",
                "OK", null, null );
            performCommandLater( this, LOAD_FAILED_CMD, null );
            return;
        }

//        statusWindow.setText("Checking License...");
//        if( ! userHasAgreedToLicense() )
//        {
//            performCommandLater( this, LOAD_FAILED_CMD, null );
//            return;
//        }
//        statusWindow.setText("Loading...");

        if( Emperor.REMOTE_SERVER == _mode && null == _host )
        {
            String hostname = _readHostName();
            HostNameDialog dlg = new
                    HostNameDialog("Choose Server",
                                   "Enter Hostname or ip address of Server",
                                   hostname);
            while(true)
            {
                statusWindow.hide();
                setWaitCursor(false);
                dlg.showModally();
                setWaitCursor(true);

                if( HostNameDialog.OK == dlg.getKeyPressed() )
                {
                    statusWindow.show();
                    statusWindow.setText("Looking for Server...");
                    hostname = dlg.getHostName();
                    _host = "http://" + hostname;
                    if( isDebuggingSupported() )
                        break;

                    setWaitCursor(false);
                    if( Alert.DEFAULT_OPTION ==
                        Alert.runAlertInternally( Alert.warningImage(),
                            "Error",
                            "JavaScript Debugger support was not found in "+ _host+"\n"
                            + "\n"
                            + "Try a different Server?",
                            "Yes", "No", null ) )
                    {
                        setWaitCursor(true);
                        continue;
                    }
                    setWaitCursor(true);
                    _host = null;
                    break;
                }
                else if( HostNameDialog.LOCALHOST == dlg.getKeyPressed() )
                {
                    statusWindow.show();
                    hostname = "127.0.0.1";
                    _host = "http://" + hostname;
                    break;
                }
                else // if( HostNameDialog.CANCEL == dlg.getKeyPressed() )
                {
                    statusWindow.show();
                    _host = null;
                    break;
                }
            }

            if( null == _host )
            {
                performCommandLater( this, LOAD_FAILED_CMD, null );
                return;
            }
            statusWindow.setText("Looking for Server...");
            _writeHostName(hostname);
        }

        if( ! isDebuggingSupported() )
        {
            String core_msg;
            String suggestion;

            if( Emperor.LOCAL == _mode )
            {
                core_msg = "JavaScript Debugger support was not found in this copy of Navigator";
                suggestion = "";
            }

            else
            {
                core_msg = "JavaScript Debugger support was not found in "+ _host;
                suggestion = "If the server has been restarted, then it may be necessary\n" +
                             "to restart Navigator in order to re-establish the connection\n\n";
            }

            setWaitCursor(false);
            Alert.runAlertInternally( Alert.warningImage(),
                "Error",
                  core_msg
                + "\n\n"
                + suggestion
                + "Please see the Debugger release notes for troubleshooting information\n"
                + "\n"
                + "        The debugger will exit now...",
                "OK", null, null );
            performCommandLater( this, LOAD_FAILED_CMD, null );
            return;
        }

        if( Emperor.REMOTE_SERVER == _mode )
            statusWindow.setText("Found Server...");

        Menu mainMenu = new Menu(true);
        MenuView menuView = new MenuView(mainMenu);
        MenuItem fileMenu = mainMenu.addItemWithSubmenu("File");
        MenuItem item;

        _emperor = new Emperor( this, _mainWindow, mainMenu, fileMenu,
                                menuView, _mode, _host, statusWindow );

        addSeparator(fileMenu);
        item = fileMenu.submenu().addItem("Exit", EXIT_APP_CMD, this);

        MenuItem helpMenu = mainMenu.addItemWithSubmenu("Help");
        item = helpMenu.submenu().addItem("About", SHOW_ABOUT_CMD, this);

        menuView.sizeToMinSize();
        _mainWindow.setMenuView(menuView);
        _mainWindow.menuView().setDirty(true);

        statusWindow.hide();
        setWaitCursor(false);

        if(AS.DEBUG)Log.log(null, "startup success" );
        _signalInitSuccessToHTMLPage();
//        System.out.println("++++ initForReal() end" );
    }


    /*
    * Temporary method to add menu item separators until IFC gets a native one....
    */
    private static void addSeparator(MenuItem item)
    {
        item.submenu().addSeparator();
//        AWTCompatibility.awtMenuForMenu(item.submenu()).addSeparator();
    }

    // implement Target interface
    public void performCommand( String cmd, Object theObject )
    {
        if( cmd.equals(EXIT_APP_CMD) )
        {
            if( null != _mainWindow )
                _mainWindow.hide();
        }
        else if( cmd.equals(LOAD_FAILED_CMD) )
        {
            if( null != _mainWindow )
                _mainWindow.hide();
            // close the applet that owns us
            try
            {
                JSObject jso = JSObject.getWindow( AWTCompatibility.awtApplet() );
                if( null != jso )
                    jso.eval( "window.close()" );
            }
            catch(Throwable t){} // eat failure
        }
        else if( cmd.equals(SHOW_ABOUT_CMD) )
        {
            showAboutDialog();
        }
    }

    // assumes that wait cursor is showing!!!
    private boolean userHasAgreedToLicense()
    {
        if(AS.S)ER.T(_waitCount == 1,"hasUserAgreedToLicense() called when _waitCount != 1");

        String baseDir = Env.getCodebaseDir();
        if( null == baseDir )
        {
            System.err.println( "failed to get CodebaseDir, can't show license");
            return false;
        }

        String acceptFilename = baseDir
                                + "accept_"+MAJOR_VERSION+""+MINOR_VERSION
                                + (PREVIEW_VERSION != 0 ? ("_pr"+PREVIEW_VERSION):"")
                                + ".txt";

        if( new java.io.File(acceptFilename).exists() )
            return true;

        String licenseHtmlFilename = baseDir+"license.html";

        LicenseViewer v =
            new LicenseViewer("Netscape JavaScript Debugger License", licenseHtmlFilename);

        if( ! v.HtmlLoadedsuccessfully() )
        {
            setWaitCursor(false);
            int answer = Alert.runAlertInternally( Alert.warningImage(),
                "Error",
                "Unable to load license text from\n"
                + licenseHtmlFilename
                + "\n"
                + "\n"
                + "license.html should be in the same directory as the debugger start page.\n"
                + "\n"
                + "  Do you accept the terms of the license?",
                "I Accept", "Cancel", null );
            setWaitCursor(true);
            return Alert.DEFAULT_OPTION == answer;
        }

        setWaitCursor(false);
        v.showModally();
        setWaitCursor(true);

        if(v.userPressedAccept())
        {
            try
            {
                // write a zero length file
                new FileOutputStream(acceptFilename).close();
//                DataOutputStream dos =
//                    new DataOutputStream(new FileOutputStream(acceptFilename));
//                dos.writeBytes("accepted");
//                if( null != dos )
//                    dos.close();
            }
            catch(Exception e)
            {
                // eat it...
            }
            return true;
        }
        return false;
    }

    private void showAboutDialog()
    {
        String title = "About";
        String msg;

        msg  = "Netscape JavaScript Debugger\n";
        msg += "Version "+MAJOR_VERSION+"."+MINOR_VERSION;
        if( PREVIEW_VERSION != 0 )
            msg += " Preview Release "+PREVIEW_VERSION;
        msg += "\n\n";
        if(AS.DEBUG || IS_INTERNAL_RELEASE || ALWAYS_SHOW_BUILD_DATE)
            msg += "Built: "+BuildDate.display()+"\n\n";
        if( WILL_EXPIRE )
            msg += "This software will expire in "+ daysTillDeath() +" days\n\n";
        msg += "This software is subject to the terms\n";
        msg += "of the accompanying license agreement.\n";

        Alert.runAlertInternally( title, msg, "OK", null, null );

    }

    public boolean isDebuggingSupported()
    {
        return Emperor.isDebuggingSupported(_mode, _host);
    }

    private void _initStartupPrefs()
    {
        String prefsDir = Env.getPrefsDir();
        if( null == prefsDir )
            return;

        String startupPrefsFullFilename;
        if( Emperor.LOCAL == _mode )
            startupPrefsFullFilename = prefsDir + _startupPrefsFilenameClient;
        else
            startupPrefsFullFilename = prefsDir + _startupPrefsFilenameServer;

        _startupPrefsFile = new java.io.File(startupPrefsFullFilename);
    }


    // NOTE: this is destructive write of just this one key=value pair
    // This system will have to be improved if further pairs are needed.
    private void _writeHostName(String hostname)
    {
        if( null == _startupPrefsFile )
            return;

        try
        {
            DataOutputStream dos =
                new DataOutputStream(new FileOutputStream(_startupPrefsFile));
            dos.writeBytes(_LastServerHostNameKey+"="+hostname);
            if( null != dos )
                dos.close();
        }
        catch(Exception e)
        {
            // eat it...
        }
    }

    private String _readHostName()
    {
        String retval = "127.0.0.1";

        if( null == _startupPrefsFile )
            return retval;

        // any existing prefs?
        if( ! _startupPrefsFile.exists() )
            return retval;

        try
        {
            DataInputStream dis =
                new DataInputStream(new FileInputStream(_startupPrefsFile));

            String line;
            String key = _LastServerHostNameKey+"=";
            while( null != (line = dis.readLine()) )
            {
                if( line.startsWith(key) )
                {
                    retval = line.substring(key.length()).trim();
                    break;
                }
            }
            if( null != dis )
                dis.close();
        }
        catch(Exception e)
        {
            // eat it...
        }
        return retval;
    }

    /*
    * The scheme is that if the applet tag has a PARAM called "AppletInitProp",
    * then we will use it to get the name of the window property to be set.
    * We then set that window property to "1".
    */

    private void    _signalInitSuccessToHTMLPage()
    {
        String initProp = parameterNamed( "AppletInitProp" );
        if( null == initProp )
            return;

        String expression = "window."+initProp+"=1";
        try
        {
            JSObject jso = JSObject.getWindow( AWTCompatibility.awtApplet() );
            if( null != jso )
            {
//                System.out.println( "writing: " + expression );
                jso.eval( expression );
            }
        }
        catch(Throwable t){} // eat failure
    }

    private int daysTillDeath()
    {
        long millisecondsTillDeath = _deathTime - new java.util.Date().getTime();
        return 1 + (int) (millisecondsTillDeath / (1000*60*60*24));
    }

    // this tries to run a method which was not present in Twister PR1 but
    // is present in our jar. If this fails, then Twister PR1 is in the
    // classpath (which is very bad for us).
    private boolean staleTwisterClassesFound()
    {
        try
        {
            new com.netscape.jsdebugging.ifcui.palomar.widget.PopupButton(null,null,null,null,null).showLabel(false);
        }
        catch(Throwable t)
        {
            return true;
        }
        return false;
    }

    // override Application.cleanup
    public void cleanup()
    {
        if( null != _emperor )
            _emperor.IFC_cleanupCalled();
        super.cleanup();
    }

    // implement AssertFailureHandler
    public int assertFailed( String msgRaw, String msgCooked, Object ob )
    {
        if(AS.DEBUG)
        {
            // All these single use vars are here because the code
            // used to allow for internal or external dlgs (and may again)
            Image  img = Alert.questionImage();
            String title = "Assertion failed";
            String msg = msgCooked + "\n\n" + "(set debugger to catch \"com.netscape.jsdebugging.ifcui.palomar.util.DebuggerCaughtException\")";
            String opt1 = "Continue";
            String opt2 = "Abort";
            String opt3 = "Debug";

            Log.error("Assertion failed", msgCooked );

            int choice = Alert.runAlertExternally(img,title,msg,opt1,opt2,opt3);

            switch(choice)
            {
                case Alert.DEFAULT_OPTION: return AssertFailureHandler.CONTINUE;
                case Alert.SECOND_OPTION:  return AssertFailureHandler.ABORT;
                case Alert.THIRD_OPTION:   return AssertFailureHandler.DEBUG;
            }
        }
        return AssertFailureHandler.DEBUG;
    }

    // implement EmperorOwner
    public void setWaitCursor(boolean set)
    {
        _waitCount += set ? 1 : -1;

        if(AS.S)ER.T(_waitCount >= 0,"_waitCount went negative", this );
        if(AS.S)ER.T(Thread.currentThread()==_uiThreadForAssertCheck,"setWaitCursor() called on non-UI thread",this);

        if( (1 == _waitCount && set) || (0 == _waitCount && !set) )
        {
            Vector windows = externalWindows();
            int size = windows.size();
            for (int i= 0; i < size; i++)
            {
                ExternalWindow window = (ExternalWindow)windows.elementAt(i);
                FoundationPanel panel = window.panel();

                if (set)
                    panel.setCursor(View.WAIT_CURSOR);
                else
                    panel.setCursor(View.ARROW_CURSOR);
            }
        }
    }


    /****************************************************/

    // data...

    private ToolTipExternalWindow   _mainWindow;
    private InternalWindow          _toolBarWindow;
    private Emperor                 _emperor;
    private String                  _modeString;
    private int                     _mode;
    private String                  _host;
    private Thread                  _uiThreadForAssertCheck = null;
    private int                     _waitCount = 0;

    private static final String _startupPrefsFilenameServer = "jsdsstrt.txt";
    private static final String _startupPrefsFilenameClient = "jsdcstrt.txt";
    private java.io.File        _startupPrefsFile;

    private static final String _LastServerHostNameKey = "LastHost0";

    private static final String EXIT_APP_CMD = "EXIT_APP_CMD";
    private static final String LOAD_FAILED_CMD = "LOAD_FAILED_CMD";
    private static final String SHOW_ABOUT_CMD = "SHOW_ABOUT_CMD";
}
