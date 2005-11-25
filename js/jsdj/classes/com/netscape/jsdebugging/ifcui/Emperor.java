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
* Central 'Model' that creates and coordinates all models and views
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.io.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolTip.*;
import netscape.javascript.*;
import com.netscape.jsdebugging.api.*;

/**
* Emperor starts it up and is connect point to the Application
*/

public class Emperor
    implements WindowOwner
{
    public Emperor( EmperorOwner          owner,
                    ToolTipExternalWindow mainWindow,
                    Menu                  mainMenu,
                    MenuItem              fileMenu,
                    MenuView              menuView,
                    int                   hostMode,
                    String                host,
                    StatusWindow          statusWindow )

    {
        _owner      = owner;
        _mainWindow = mainWindow;
        _mainMenu   = mainMenu;
        _menuView   = menuView;

        _hostMode   = hostMode;
        _host       = host;

        _rootView   = mainWindow.rootView();
        _mainWindow.setOwner(this);
        _mainWindow.setHidesWhenPaused(false);

        _fixedFont = new Font( "Courier", Font.PLAIN, 13 );

        _debuggerIsActive = true;

        if(AS.DEBUG)
        {
            _uiThreadForAssertCheck = Thread.currentThread();
        }

        setWaitCursor(true);

        _gatherVersionInfo();

        statusWindow.setText(REMOTE_SERVER == hostMode ?
                      "Connecting to Server..." : "Connecting to Navigator...");
        _connectToTarget();

        statusWindow.setText("Initializing Models...");
        _createTyrants();
        statusWindow.setText("Initializing Views...");
        _createViews(fileMenu);

        statusWindow.setText("Reading Prefs...");
        _readPrefs();
        _readBreakpointListHackFile();
        _commandTyrant.refreshCmdStatesAndNotifyObservers();
        _showViews();

        setWaitCursor(false);
    }

    public static final int LOCAL           = 0;
    public static final int REMOTE_SERVER   = 1;
    public static final int RHINO           = 2;

    public static final String LOCAL_LOADER_NAME =
                            "com.netscape.jsdebugging.api.local.AdapterLoaderLocal";
    public static final String SERVER_LOADER_NAME =
                            "com.netscape.jsdebugging.api.corba.AdapterLoaderCorba";
    public static final String RHINO_LOADER_NAME =
                            "com.netscape.jsdebugging.api.rhino.AdapterLoaderRhino";

    public static boolean isDebuggingSupported(int which, String host)
    {
        String loaderName = null;
        switch(which)
        {
        case LOCAL:
            loaderName = LOCAL_LOADER_NAME;
            break;
        case REMOTE_SERVER:
            loaderName = SERVER_LOADER_NAME;
            break;
        case RHINO:
            loaderName = RHINO_LOADER_NAME;
            break;
        default:
            return false;
        }

        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            Class clazz = Class.forName(loaderName);
            AdapterLoader loader = (AdapterLoader) clazz.newInstance();
            loader.setHost(host);
            return loader.isDebuggingSupported();
        }
        catch( Throwable e )
        {
            System.out.println(e);
        }
        return false;
    }

    private boolean _connectToTarget()
    {
        String loaderName = null;
        switch(_hostMode)
        {
        case LOCAL:
            loaderName = LOCAL_LOADER_NAME;
            break;
        case REMOTE_SERVER:
            loaderName = SERVER_LOADER_NAME;
            break;
        case RHINO:
            loaderName = RHINO_LOADER_NAME;
            break;
        default:
            return false;
        }

        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            Class clazz = Class.forName(loaderName);
            AdapterLoader loader = (AdapterLoader) clazz.newInstance();

            loader.setHost(_host);

            _debugController = loader.getDebugController();
            _sourceTextProvider = loader.getSourceTextProvider();
            if( null == _sourceTextProvider || null == _debugController )
                return false;
            return true;
        }
        catch( Throwable e )
        {
            System.out.println(e);
        }
        return false;
    }

    private void _createTyrants()
    {
        _controlTyrant    = new ControlTyrant(this);
        _sourceTyrant     = new SourceTyrant(this);
        _stackTyrant      = new StackTyrant(this);
        _breakpointTyrant = new BreakpointTyrant(this);
        _consoleTyrant    = new ConsoleTyrant(this);
        _watchTyrant      = new WatchTyrant(this);
        _inspectorTyrant  = new InspectorTyrant(this);
        _commandTyrant    = new CommandTyrant(this);
        _sourceTyrant.refreshSourceTextItemVector();
    }

    private void _createViews( MenuItem fileMenu )
    {
        if( null != _menuView )
            _menuViewHeight = _menuView.itemHeight();

        _commandView = new CommandView( this, _mainMenu, fileMenu, _menuViewHeight );

        Rect rectCanvas = getClientRect();
        // hack for space for native menu
//        rectCanvas.height -= 20;

//        _lowerWindowDY = 200;
        _lowerWindowDY = rectCanvas.height / 3;
        int stackViewWidth = 150;
        int lowerWindowY = rectCanvas.y + rectCanvas.height - _lowerWindowDY;

        int popupDX = rectCanvas.width  * 5 / 6;
        int popupDY = rectCanvas.height * 1 / 2;

        Rect rectPageList =  new Rect(rectCanvas.x,rectCanvas.y,popupDX,popupDY);
        Rect rectWatchView =  new Rect(rectCanvas.x+20,rectCanvas.y+20,popupDX,popupDY);
        Rect rectBreakpointView =  new Rect(rectCanvas.x+40,rectCanvas.y+40,popupDX,popupDY);
        Rect rectInspectorView =  new Rect(rectCanvas.x+60,rectCanvas.y+60,popupDX,popupDY);
        Rect rectStackView = new Rect( 0, lowerWindowY,
                                       stackViewWidth,
                                       _lowerWindowDY );
        Rect rectConsole = new Rect( stackViewWidth, lowerWindowY,
                                     rectCanvas.width-stackViewWidth,
                                     _lowerWindowDY);

        // hack for "unsigned window" subwindow
        Rect rectSrc = new Rect(rectCanvas.x,rectCanvas.y,
                                rectCanvas.width,
                                rectCanvas.height-_lowerWindowDY);

        _pageListView      = new PageListView( this, rectPageList );
        _sourceViewManager = new SourceViewManager(this);
        _sourceViewManager.setPreferredNewViewRect( rectSrc );
        _stackView         = new StackView( this, rectStackView );
        _consoleView       = new ConsoleView(this, rectConsole );
        _watchView        = new WatchView(this, rectWatchView);
        _breakpointView   = new BreakpointView(this, rectBreakpointView);
        _inspectorView    = new InspectorView(this, rectInspectorView);
    }

    public void _showViews()
    {
        _toolBarWindow.show();
        _stackView.show();
        _consoleView.show();
    }


    private void _readPrefs()
    {
        PrivilegeManager.enablePrivilege("Debugger");
        PrivilegeManager.enablePrivilege("UniversalFileRead");

        String prefsDir = Env.getPrefsDir();
        if( null == prefsDir )
            return;

        String prefsFullFilename;
        if( LOCAL == _hostMode )
            prefsFullFilename = prefsDir + _prefsFilenameClient;
        else
            prefsFullFilename = prefsDir + _prefsFilenameServer;

        // we don't want to spit this info to the console if not an applet
        if( isApplet() )
            System.out.println("JSD is using prefs file: " + prefsFullFilename );

        _prefsFile = new java.io.File(prefsFullFilename);
        if( null == _prefsFile )
            return;

        // any existing prefs?
        if( ! _prefsFile.exists() )
            return;

        try
        {
            FileInputStream fis = new FileInputStream(_prefsFile);
            Archive archive = new Archive();
            archive.readASCII(fis);
            Unarchiver unarchiver = new Unarchiver(archive);

            // XXX: process each object that implements PrefsSupport here

            if( null != _breakpointTyrant )
                _breakpointTyrant.prefsRead( unarchiver );

            if( null != _sourceViewManager )
                _sourceViewManager.prefsRead( unarchiver );

            if( null != _watchTyrant )
                _watchTyrant.prefsRead( unarchiver );


            // add the rest...



            fis.close();
        }
        catch( Exception e )
        {
            // print and eat
            System.out.println(e);
        }


    }

    private void _writePrefs()
    {
        if( null == _prefsFile )
            return;
        PrivilegeManager.enablePrivilege("Debugger");
        PrivilegeManager.enablePrivilege("UniversalFileWrite");

        try
        {
            Archiver archiver = new Archiver(new Archive());

            // XXX: process each object that implements PrefsSupport here

            if( null != _breakpointTyrant )
                _breakpointTyrant.prefsWrite( archiver );

            if( null != _sourceViewManager )
                _sourceViewManager.prefsWrite( archiver );

            if( null != _watchTyrant )
                _watchTyrant.prefsWrite( archiver );

            // add the rest...



            FileOutputStream fos = new FileOutputStream(_prefsFile);
            archiver.archive().writeASCII(fos, true);
            fos.close();
        }
        catch( Exception e )
        {
            // print and eat
            System.out.println(e);
        }


    }

    private void _debuggerIsClosing()
    {
        if( ! _debuggerIsActive )
            return;

        _writePrefs();
        _debuggerIsActive = false;

        if( null != _controlTyrant )
            _controlTyrant.disableDebugger();
    }

    public void enableAppClose( boolean b ) {_appCanClose=b;}

    public void IFC_cleanupCalled()
    {
        _debuggerIsClosing();
    }


    // implement WindowOwner interface
    public boolean windowWillShow(Window aWindow)   {return true;}
    public void windowDidShow(Window aWindow)       {}
    public boolean windowWillHide(Window aWindow)
    {
        if( null == _mainWindow || _mainWindow != aWindow || _ignoreHide )
            return true;

        if( ! _appCanClose )
            return false;
        return true;
    }
    public void windowDidHide(Window aWindow)
    {
        if( null == _mainWindow || _mainWindow != aWindow || _ignoreHide )
            return;

        _debuggerIsClosing();

        // close the applet that owns us
        try
        {
            if( _isApplet )
            {
                JSObject jso = JSObject.getWindow( AWTCompatibility.awtApplet() );
                if( null != jso )
                    jso.eval( "window.close()" );
            }
        }
        catch(Throwable e){} // eat failure
    }
    public void windowDidBecomeMain(Window aWindow) {}
    public void windowDidResignMain(Window aWindow) {}
    public void windowWillSizeBy(Window aWindow, Size deltaSize)
    {
        if (_toolBarWindow != null)
        {
            Size minSize = _toolBarWindow.windowSizeForContentSize(_rootView.width(), 0);
            _toolBarWindow.sizeTo(minSize.width,_toolBarWindow.height());
        }
        if( null != _sourceViewManager )
            _sourceViewManager.setPreferredNewViewRect(getPreferedNewSourceViewRect());
    }


    private void _readBreakpointListHackFile()
    {
        if( null == _breakpointTyrant )
            return;
        PrivilegeManager.enablePrivilege("Debugger");
        PrivilegeManager.enablePrivilege("UniversalFileRead");

        String prefsDir = Env.getPrefsDir();
        if( null == prefsDir )
            return;

        String bpFullFilename;
        if( LOCAL == _hostMode )
            bpFullFilename = prefsDir + _bpFilenameClient;
        else
            bpFullFilename = prefsDir + _bpFilenameServer;
        java.io.File bpFile = new java.io.File(bpFullFilename);
        if( null == bpFile )
            return;

        // any existing breakpoints file?
        if( ! bpFile.exists() )
            return;

        // we don't want to spit this info to the console if not an applet
        if( isApplet() )
            System.out.println("JSD is reading breakpoint list file: " + bpFullFilename );

        try
        {
            DataInputStream s = new DataInputStream(new FileInputStream(bpFile));
            if( null != s )
            {
                String linetext;
                while( null != (linetext = s.readLine()) )
                {
                    String url;
                    String condition;
                    int sep1;
                    int sep2;

                    sep1 = linetext.indexOf(",");
                    if( -1 == sep1 )
                        continue;
                    sep2 = linetext.indexOf(",", sep1+1);
                    String lineString = linetext.substring(0,sep1).trim();

                    if( -1 != sep2 )
                    {
                        url = linetext.substring(sep1+1,sep2).trim();
                        condition = linetext.substring(sep2+1).trim();
                    }
                    else
                    {
                        url = linetext.substring(sep1+1).trim();
                        condition = null;
                    }

                    // System.out.println("lineString = " + lineString);
                    // System.out.println("url = " + url);
                    // System.out.println("condition = " + condition);

                    int line = Integer.parseInt(lineString);

                    if( 0 == line || null == url || 0 == url.length() )
                        continue;

                    // add the breakpoint

                    Location loc = new Location(url, line);
                    if( null == _breakpointTyrant.findBreakpoint(loc) )
                    {
                        Breakpoint bp = _breakpointTyrant.addBreakpoint(loc);
                        if( null != bp && null != condition )
                            bp.setBreakCondition(condition);
                    }
                }
                s.close();
            }
        }
        catch( Exception e )
        {
            // print and eat
            System.out.println(e);
        }
    }

    private void _gatherVersionInfo()
    {
        try
        {
            _isApplet = Application.application().isApplet();
            if( _isApplet )
            {
                // detect if this is a pre- 4.0b6 version of Navigator
                // check for "4.0bx " where x is < 6
                PrivilegeManager.enablePrivilege("Debugger");
                JSObject jso = JSObject.getWindow( AWTCompatibility.awtApplet() );
                if( null != jso )
                {
                    String ver = (String) jso.eval( "navigator.appVersion" );
                    if( null != ver )
                    {
                        // check for "4.0bx " where x is < 6
                        if( ver.length() >= 6 &&
                            ver.startsWith("4.0b") &&
                            ver.charAt(5) == ' ' &&
                            ver.charAt(4) >= '1' &&
                            ver.charAt(4) <= '5' )
                        {
                            System.out.println( "JSDebugger running in 4.0b5 or earlier, get new Communicator :)");
                            _isPre40b6 = true;
                        }
                    }
                }
            }
        }
        catch( Throwable e ) {} // eat any exception...
    }

    public Rect getClientRect()
    {
        Size s = _mainWindow.contentSize();
        return new Rect(0,_toolbarHeight+_menuViewHeight,
                        s.width,s.height-_toolbarHeight-_menuViewHeight);
    }

    private Rect getPreferredNewSourceViewRect()
    {
        Rect rect = getClientRect();
        rect.height -= _lowerWindowDY;
        return rect;
    }

    public View     getCanvasView() {return _rootView;}
    public RootView getRootView()   {return _rootView;}

    public ToolTipExternalWindow getMainWindow()    {return _mainWindow;}
    public Menu                  getMainMenu()      {return _mainMenu;}
    public InternalWindow        getToolBarWindow() {return _toolBarWindow;}

    public void setToolbarHeight(int i) {_toolbarHeight=i;}
    public void setToolBarWindow(InternalWindow w) {_toolBarWindow=w;}

    public ControlTyrant     getControlTyrant()      {return _controlTyrant   ;}
    public BreakpointTyrant  getBreakpointTyrant()   {return _breakpointTyrant;}
    public SourceTyrant      getSourceTyrant()       {return _sourceTyrant    ;}
    public StackTyrant       getStackTyrant()        {return _stackTyrant     ;}
    public WatchTyrant       getWatchTyrant()        {return _watchTyrant     ;}
    public ConsoleTyrant     getConsoleTyrant()      {return _consoleTyrant   ;}
    public CommandTyrant     getCommandTyrant()      {return _commandTyrant   ;}
    public InspectorTyrant   getInspectorTyrant()    {return _inspectorTyrant ;}

    public PageListView      getPageListView()       {return _pageListView    ;}
    public SourceViewManager getSourceViewManager()  {return  _sourceViewManager;}
    public CommandView       getCommandView()        {return _commandView     ;}
    public StackView         getStackView()          {return _stackView       ;}
    public ConsoleView       getConsoleView()        {return _consoleView     ;}
    public WatchView         getWatchView()          {return _watchView       ;}
    public BreakpointView    getBreakpointView()     {return _breakpointView  ;}
    public InspectorView     getInspectorView()      {return _inspectorView   ;}

    public SourceTextProvider getSourceTextProvider() {return _sourceTextProvider;}
    public DebugController    getDebugController()    {return _debugController;}

    public void     bringAppToFront()
    {
        try
        {
            if(AS.S)ER.T(Thread.currentThread()==_uiThreadForAssertCheck,"bringAppToFront() called on non-UI thread",this);
            RootView mrv = Application.application().mainRootView();
            if( null != mrv.mainWindow() )
                return;
            ExternalWindow win = mrv.externalWindow();
            win.show();
            win.moveToFront();
        }
        catch(Exception e){} // eat failure
    }

    public boolean isApplet() {return _isApplet;}

    public void setWaitCursor(boolean set)
    {
        _owner.setWaitCursor(set);
    }

    public Color    getBackgroundColor()         {return _backgroundColor;}
    public Color    getDisabledBackgroundColor() {return _disabledBackgroundColor;}
    public Color    getSelectionColor()          {return _selectionColor;}
    public Font     getFixedFont()               {return _fixedFont;}
    public boolean  isPre40b6()                  {return _isPre40b6;}

    public boolean  isCorbaHostConnection()
    {
        // XXX this will need reworking as we use Corba in more ways...
        return _hostMode == REMOTE_SERVER;
    }
    public int      getHostMode()                {return _hostMode;}
    public String   getHost()                    {return _host;}

    public void     ignoreHide(boolean b)        {_ignoreHide = b;}


    // data...

    private EmperorOwner            _owner;
    private ToolTipExternalWindow   _mainWindow;
    private Menu                    _mainMenu;
    private MenuView                _menuView;
    private int                     _menuViewHeight;
    private InternalWindow          _toolBarWindow;
    private int                     _toolbarHeight;
    private RootView                _rootView;
    private boolean                 _appCanClose = true;

    private ControlTyrant    _controlTyrant   ;
    private BreakpointTyrant _breakpointTyrant;
    private SourceTyrant     _sourceTyrant    ;
    private StackTyrant      _stackTyrant     ;
    private WatchTyrant      _watchTyrant     ;
    private ConsoleTyrant    _consoleTyrant   ;
    private InspectorTyrant  _inspectorTyrant ;
    private CommandTyrant    _commandTyrant   ;

    private PageListView     _pageListView    ;
    private SourceViewManager _sourceViewManager;
    private CommandView      _commandView     ;
    private StackView        _stackView       ;
    private ConsoleView      _consoleView     ;
    private WatchView        _watchView       ;
    private BreakpointView   _breakpointView  ;
    private InspectorView    _inspectorView   ;

    private SourceTextProvider _sourceTextProvider;
    private DebugController    _debugController;

    private int              _lowerWindowDY;

    private static final Color  _backgroundColor         = Color.white;
    private static final Color  _disabledBackgroundColor = Color.lightGray;
    private static final Color  _selectionColor          = Color.lightGray;
//    private static final Color  _backgroundColor = Color.lightGray;
//    private static final Color  _selectionColor  = Color.white;

    private static final String _prefsFilenameClient = "jsdcpref.txt";
    private static final String _prefsFilenameServer = "jsdspref.txt";
    private static final String _bpFilenameClient    = "jsdcbp.txt";
    private static final String _bpFilenameServer    = "jsdsbp.txt";

    private Font                _fixedFont = null;
    private boolean             _isPre40b6 = false;
    private boolean             _debuggerIsActive = false;
    private java.io.File        _prefsFile;

    private int                 _hostMode;
    private String              _host;
    private boolean             _ignoreHide;
    private Thread              _uiThreadForAssertCheck = null;

    private boolean             _isApplet;
}

