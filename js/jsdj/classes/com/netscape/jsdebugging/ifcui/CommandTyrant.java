/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
* Manages validating and routing user commands
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.security.PrivilegeManager;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.PopupButton;
import com.netscape.jsdebugging.api.*;

public class CommandTyrant
    extends Observable
    implements Observer, Target
{
    // this list of ids must start with 0 and be contiguous and have count set
    // ............................................
    public static final int RUN                     = 0;
    public static final int ABORT                   = 1;
    public static final int STEP_OVER               = 2;
    public static final int STEP_INTO               = 3;
    public static final int STEP_OUT                = 4;
    public static final int INTERRUPT               = 5;
    public static final int PAGELIST_CLICK          = 6;
    public static final int PAGELIST_DBLCLICK       = 7;
    public static final int PAGELIST_SHOW_HIDE      = 8;
    public static final int REFRESH_ALL             = 9;
    public static final int STACKVIEW_CLICK         = 10;
    public static final int STACKVIEW_DBLCLICK      = 11;
    public static final int EVAL_STRING             = 12;
    public static final int EVAL_SEL_STRING         = 13;
    public static final int SHOW_LINE_NUMBERS       = 14;
    public static final int TOGGLE_BREAKPOINT       = 15;
    public static final int COPY                    = 16;
    public static final int PASTE                   = 17;
    public static final int WATCHES_SHOW_HIDE       = 18;
    public static final int COPY_TO_WATCH           = 19;
    public static final int EDIT_BREAKPOINT         = 20;
    public static final int BREAKPOINTS_SHOW_HIDE   = 21;
    public static final int INSPECTOR_SHOW_HIDE     = 22;
    public static final int INSPECT_SEL_STRING      = 23;
    public static final int WATCH_STRING            = 24;
    public static final int CLEAR_CONSOLE           = 25;
    public static final int COPY_STRING             = 26;
    public static final int CUT                     = 27;
    public static final int TEST                    = 28;
    // XXX add the rest...
    public static final int CMD_COUNT               = 29; // just a count of the above
    // ............................................

    private void _initCmdStates()
    {
        // initialize the cmdStatesArray
        int i;
        _cmdStatesArray = new CmdState[CMD_COUNT];
        for( i = 0; i < CMD_COUNT; i++ )
            _cmdStatesArray[i] = new CmdState(i,null,true,false);

        // add the names
        // these must match the full list above
        // ............................................
        _cmdStatesArray[RUN                     ].name = "RUN";
        _cmdStatesArray[ABORT                   ].name = "ABORT";
        _cmdStatesArray[STEP_OVER               ].name = "STEP_OVER";
        _cmdStatesArray[STEP_INTO               ].name = "STEP_INTO";
        _cmdStatesArray[STEP_OUT                ].name = "STEP_OUT";
        _cmdStatesArray[INTERRUPT               ].name = "INTERRUPT";
        _cmdStatesArray[PAGELIST_CLICK          ].name = "PAGELIST_CLICK";
        _cmdStatesArray[PAGELIST_DBLCLICK       ].name = "PAGELIST_DBLCLICK";
        _cmdStatesArray[PAGELIST_SHOW_HIDE      ].name = "PAGELIST_SHOW_HIDE";
        _cmdStatesArray[REFRESH_ALL             ].name = "REFRESH_ALL";
        _cmdStatesArray[STACKVIEW_CLICK         ].name = "STACKVIEW_CLICK";
        _cmdStatesArray[STACKVIEW_DBLCLICK      ].name = "STACKVIEW_DBLCLICK";
        _cmdStatesArray[EVAL_STRING             ].name = "EVAL_STRING";
        _cmdStatesArray[EVAL_SEL_STRING         ].name = "EVAL_SEL_STRING";
        _cmdStatesArray[SHOW_LINE_NUMBERS       ].name = "SHOW_LINE_NUMBERS";
        _cmdStatesArray[TOGGLE_BREAKPOINT       ].name = "TOGGLE_BREAKPOINT";
        _cmdStatesArray[COPY                    ].name = "COPY";
        _cmdStatesArray[PASTE                   ].name = "PASTE";
        _cmdStatesArray[WATCHES_SHOW_HIDE       ].name = "WATCHES_SHOW_HIDE";
        _cmdStatesArray[COPY_TO_WATCH           ].name = "COPY_TO_WATCH";
        _cmdStatesArray[EDIT_BREAKPOINT         ].name = "EDIT_BREAKPOINT";
        _cmdStatesArray[BREAKPOINTS_SHOW_HIDE   ].name = "BREAKPOINTS_SHOW_HIDE";
        _cmdStatesArray[INSPECTOR_SHOW_HIDE     ].name = "INSPECTOR_SHOW_HIDE";
        _cmdStatesArray[INSPECT_SEL_STRING      ].name = "INSPECT_SEL_STRING";
        _cmdStatesArray[WATCH_STRING            ].name = "WATCH_STRING";
        _cmdStatesArray[CLEAR_CONSOLE           ].name = "CLEAR_CONSOLE";
        _cmdStatesArray[COPY_STRING             ].name = "COPY_STRING";
        _cmdStatesArray[CUT                     ].name = "CUT";
        _cmdStatesArray[TEST                    ].name = "TEST";

//        _cmdStatesArray[].name = "";
        // XXX add the rest...
        // ............................................

        // check in debugging build only...
        if(AS.DEBUG)
        {
            for( i = 0; i < CMD_COUNT; i++ )
                if(AS.S)ER.T(null!=_cmdStatesArray[i].name, "name not set for cmdstate number " + i, this);
        }

        // build the hashtable
        _cmdStatesHashtable = new Hashtable(CMD_COUNT);
        for( i = 0; i < CMD_COUNT; i++ )
            _cmdStatesHashtable.put(_cmdStatesArray[i].name,_cmdStatesArray[i]);

    }

    public void refreshCmdStatesAndNotifyObservers()
    {
        refreshCmdStates();
        _notify();
    }

    public void refreshCmdStates()
    {
        _getViewsFromEmperor();

        boolean running = _controlTyrant.getState() == ControlTyrant.RUNNING;

        if( running )
        {
            _cmdStatesArray[RUN                 ].enabled = false;
            _cmdStatesArray[ABORT               ].enabled = false;
            _cmdStatesArray[STEP_OVER           ].enabled = false;
            _cmdStatesArray[STEP_INTO           ].enabled = false;
            _cmdStatesArray[STEP_OUT            ].enabled = false;
            _cmdStatesArray[EVAL_STRING         ].enabled = false;
            _cmdStatesArray[EVAL_SEL_STRING     ].enabled = false;
            _cmdStatesArray[INSPECT_SEL_STRING  ].enabled = false;
            _cmdStatesArray[WATCH_STRING        ].enabled = false;
            _cmdStatesArray[TEST                ].enabled = false;
        }
        else
        {
            _cmdStatesArray[RUN                 ].enabled = true;
            _cmdStatesArray[ABORT               ].enabled = true;
            _cmdStatesArray[STEP_OVER           ].enabled = true;
            _cmdStatesArray[STEP_INTO           ].enabled = true;
            _cmdStatesArray[STEP_OUT            ].enabled = true;
            _cmdStatesArray[EVAL_STRING         ].enabled = true;
            _cmdStatesArray[WATCH_STRING        ].enabled = true;
            _cmdStatesArray[TEST                ].enabled = true;
            if( null != _sourceTyrant && null != _sourceTyrant.getSelectedText())
            {
                _cmdStatesArray[EVAL_SEL_STRING ].enabled = true;
                _cmdStatesArray[INSPECT_SEL_STRING ].enabled = true;
            }
            else
            {
                _cmdStatesArray[EVAL_SEL_STRING ].enabled = false;
                _cmdStatesArray[INSPECT_SEL_STRING ].enabled = false;
            }
        }
        if( _controlTyrant.getInterrupt() )
            _cmdStatesArray[INTERRUPT           ].checked = true;
        else
            _cmdStatesArray[INTERRUPT           ].checked = false;

        if( null == _pageListView || ! _pageListView.isVisible() )
            _cmdStatesArray[PAGELIST_SHOW_HIDE  ].checked = false;
        else
            _cmdStatesArray[PAGELIST_SHOW_HIDE  ].checked = true;

        if( null == _watchView || ! _watchView.isVisible() )
            _cmdStatesArray[WATCHES_SHOW_HIDE   ].checked = false;
        else
            _cmdStatesArray[WATCHES_SHOW_HIDE   ].checked = true;

        if( null == _breakpointView || ! _breakpointView.isVisible() )
            _cmdStatesArray[BREAKPOINTS_SHOW_HIDE   ].checked = false;
        else
            _cmdStatesArray[BREAKPOINTS_SHOW_HIDE   ].checked = true;

        if( null == _inspectorView || ! _inspectorView.isVisible() )
            _cmdStatesArray[INSPECTOR_SHOW_HIDE   ].checked = false;
        else
            _cmdStatesArray[INSPECTOR_SHOW_HIDE   ].checked = true;

        if( null == _sourceViewManager || ! _sourceViewManager.getShowLineNumbers() )
            _cmdStatesArray[SHOW_LINE_NUMBERS  ].checked = false;
        else
            _cmdStatesArray[SHOW_LINE_NUMBERS  ].checked = true;

        SourceTextItem item;
        String selURL;
        int selLine;
        if( null == _breakpointTyrant ||
            null == _sourceTyrant ||
            null == (selURL = _sourceTyrant.getUrlOfSelectedLine()) ||
            0    == (selLine = _sourceTyrant.getLineNumberOfSelectedLine()) ||
            null == (item = _sourceTyrant.findSourceItem(selURL)) )
        {
            _cmdStatesArray[TOGGLE_BREAKPOINT       ].enabled = false;
            _cmdStatesArray[TOGGLE_BREAKPOINT       ].checked = false;
            _cmdStatesArray[EDIT_BREAKPOINT         ].enabled = false;
        }
        else
        {
            _cmdStatesArray[TOGGLE_BREAKPOINT       ].enabled = true;

            int sysline = _sourceTyrant.userLine2SystemLine(item,selLine);
            if( null != _breakpointTyrant.findBreakpoint( new Location(selURL,sysline) ) )
            {
                _cmdStatesArray[TOGGLE_BREAKPOINT       ].checked = true;
                _cmdStatesArray[EDIT_BREAKPOINT         ].enabled = true;
            }
            else
            {
                _cmdStatesArray[TOGGLE_BREAKPOINT       ].checked = false;
                _cmdStatesArray[EDIT_BREAKPOINT         ].enabled = false;
            }
        }

        if( null == _consoleView    ||
            null == _inspectorView  ||
            null == _sourceTyrant )
        {
            _cmdStatesArray[COPY].enabled           = false;
            _cmdStatesArray[COPY_TO_WATCH].enabled  = false;
            _cmdStatesArray[PASTE].enabled          = false;
            _cmdStatesArray[CUT].enabled            = false;
        }
        else
        {
            if( null != _sourceTyrant.getSelectedText() ||
                _inspectorView.canCopy()                ||
                _consoleView.canCopy() )
            {
                _cmdStatesArray[COPY].enabled          = true;
                _cmdStatesArray[COPY_TO_WATCH].enabled = true;
            }
            else
            {
                _cmdStatesArray[COPY].enabled          = false;
                _cmdStatesArray[COPY_TO_WATCH].enabled = false;
            }

            if( _inspectorView.canCopy() || _consoleView.canCopy() )
                _cmdStatesArray[CUT].enabled          = true;
            else
                _cmdStatesArray[CUT].enabled          = false;

            if( _consoleView.canPaste() || _inspectorView.canPaste() )
                _cmdStatesArray[PASTE].enabled = true;
            else
                _cmdStatesArray[PASTE].enabled = false;
        }
    }

    public CommandTyrant(Emperor emperor)
    {
        _app = Application.application();
        _emperor = emperor;

        _controlTyrant    = emperor.getControlTyrant   ();
        _breakpointTyrant = emperor.getBreakpointTyrant();
        _sourceTyrant     = emperor.getSourceTyrant    ();
        _stackTyrant      = emperor.getStackTyrant     ();
        _watchTyrant      = emperor.getWatchTyrant     ();
        _consoleTyrant    = emperor.getConsoleTyrant   ();
        _inspectorTyrant  = emperor.getInspectorTyrant ();

        // figure out if native clipboard is supported...
        try
        {
            Class.forName("java.awt.datatransfer.Clipboard");
            Class.forName("netscape.application.jdk11compatibility.JDKClipboard");
//            if(AS.DEBUG)System.out.println( "using native clipboard");
        }
        catch(Exception e)
        {
            _usingLocalClipboard = true;
//            if(AS.DEBUG)System.out.println( "using local clipboard");
        }


        if(AS.S)ER.T(null!=_controlTyrant   ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_breakpointTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTyrant    ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant     ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_watchTyrant     ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_consoleTyrant   ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_inspectorTyrant ,"emperor init order problem", this);

        _controlTyrant.addObserver(this);
        _sourceTyrant.addObserver(this);
        _breakpointTyrant.addObserver(this);

        _initCmdStates();
        refreshCmdStates();
    }

    public CmdState findCmdState(int id) {return _cmdStatesArray[id];}
    public CmdState findCmdState(String name){return (CmdState)_cmdStatesHashtable.get(name);}

    public String   cmdString(int id)  {return _cmdStatesArray[id].name;}
    public int      cmdID(String name) {return ((CmdState)_cmdStatesHashtable.get(name)).id;}

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        refreshCmdStatesAndNotifyObservers();
    }

    private void _notify()
    {
        setChanged();
        notifyObservers(null);
    }


    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        SourceTextItem item;
        String selURL;
        int selLine;
        Location loc;
        Breakpoint bp;
        int sysline;

        _getViewsFromEmperor();

        CmdState cmdState = findCmdState(cmd);
        if( null == cmdState )
        {
            if(AS.S)ER.T( false, "failed to find cmdState named: " + cmd, this);
            return;
        }
        switch(cmdState.id)
        {
            case RUN:
                _controlTyrant.runit();
                break;
            case ABORT:
                _controlTyrant.abort();
                break;
            case STEP_OVER:
                _controlTyrant.stepOver();
                break;
            case STEP_INTO:
                _controlTyrant.stepInto();
                break;
            case STEP_OUT :
                _controlTyrant.stepOut();
                break;
            case INTERRUPT:
                if( null != _controlTyrant )
                    _controlTyrant.interrupt(! _controlTyrant.getInterrupt());
                refreshCmdStatesAndNotifyObservers();
                break;
            case PAGELIST_CLICK:
                _updateSelectedPageListItem( (ListView)data );
                break;
            case PAGELIST_DBLCLICK:
                _updateSelectedPageListItem( (ListView)data );
                SourceTextItem sti = _sourceTyrant.getSelectedSourceItem();
                if( null != sti && null != _sourceViewManager )
                {
                    SourceView view = _sourceViewManager.findView(sti);
                    if( null != view )
                        _sourceViewManager.activateView(view);
                    else
                        _sourceViewManager.createView(sti);
                }
                // fall through to hide the list...
                // break;
            case PAGELIST_SHOW_HIDE:
                if( null == _pageListView )
                    break;
                if( _pageListView.isVisible() )
                    _pageListView.hide();
                else
                {
                    _sourceTyrant.refreshSourceTextItemVector();
                    _pageListView.show();
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case REFRESH_ALL:
                // XXX do other things...
                _sourceTyrant.refreshSourceTextItemVector();
                break;
            case STACKVIEW_CLICK:
                _emperor.setWaitCursor(true);
                _stackTyrant.setCurrentFrame( ((ListView)data).selectedIndex() );
                _emperor.setWaitCursor(false);
                break;
            case STACKVIEW_DBLCLICK:
                // ignored
                break;
            case EVAL_STRING:
                _emperor.setWaitCursor(true);
                _consoleTyrant.eval((String)data);
                _emperor.setWaitCursor(false);
                break;
            case EVAL_SEL_STRING:
                if( null != _sourceTyrant && null != _sourceTyrant.getSelectedText())
                {
                    _emperor.setWaitCursor(true);
                    _consoleTyrant.eval(_sourceTyrant.getSelectedText());
                    _emperor.setWaitCursor(false);
                }
                break;
            case SHOW_LINE_NUMBERS:
                if( null != _sourceViewManager )
                {
                    _emperor.setWaitCursor(true);
                    _sourceViewManager.setShowLineNumbers( ! _sourceViewManager.getShowLineNumbers() );
                    _emperor.setWaitCursor(false);
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case TOGGLE_BREAKPOINT:
                if( null == _breakpointTyrant ||
                    null == _sourceTyrant ||
                    null == (selURL = _sourceTyrant.getUrlOfSelectedLine()) ||
                    0    == (selLine = _sourceTyrant.getLineNumberOfSelectedLine()) ||
                    null == (item = _sourceTyrant.findSourceItem(selURL)) )
                {
                    break;
                }
                sysline = _sourceTyrant.userLine2SystemLine(item,selLine);
                loc = new Location( selURL, sysline );
                bp = _breakpointTyrant.findBreakpoint(loc);
                if( null == bp )
                    _breakpointTyrant.addBreakpoint(loc);
                else
                    _breakpointTyrant.removeBreakpoint(bp);
                // redraw done on notification from BreakpointTyrant...
                break;
            case COPY:
                String s = _getCopyText();
                if( null != s )
                {
                    _setClipboardContents(s);
                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case PASTE:
                String contents = _getClipboardContents();
                if( null != contents )
                {
                    if( null != _inspectorView && _inspectorView.canPaste() )
                        _inspectorView.paste(contents);
                    else if( null != _consoleView && _consoleView.canPaste() )
                        _consoleView.paste(contents);
                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case WATCHES_SHOW_HIDE:
                if( null == _watchView )
                    break;
                if( _watchView.isVisible() )
                    _watchView.hide();
                else
                {
                    _watchView.refresh();
                    _watchView.show();
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case COPY_TO_WATCH:
                String str = _getCopyText();
                if( null != str )
                {
                    _watchTyrant.addWatchString(str);
                    _watchTyrant.evalList();

                    if( null != _watchView )
                        _watchView.refresh();

                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case EDIT_BREAKPOINT:
                if( null == _breakpointView ||
                    null == _breakpointTyrant ||
                    null == _sourceTyrant ||
                    null == (selURL = _sourceTyrant.getUrlOfSelectedLine()) ||
                    0    == (selLine = _sourceTyrant.getLineNumberOfSelectedLine()) ||
                    null == (item = _sourceTyrant.findSourceItem(selURL)) )
                {
                    break;
                }
                sysline = _sourceTyrant.userLine2SystemLine(item,selLine);
                loc = new Location( selURL, sysline );
                bp = _breakpointTyrant.findBreakpoint(loc);
//                if( null == bp )
//                    bp = _breakpointTyrant.addBreakpoint(loc);
                if( null != bp )
                    _breakpointView.editBreakpoint(bp);
                // redraw done on notification from BreakpointTyrant...
                break;
            case BREAKPOINTS_SHOW_HIDE:
                if( null == _breakpointView )
                    break;
                if( _breakpointView.isVisible() )
                    _breakpointView.hide();
                else
                {
                    _breakpointView.refresh();
                    _breakpointView.show();
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case INSPECTOR_SHOW_HIDE:
                if( null == _inspectorView )
                    break;
                if( _inspectorView.isVisible() )
                    _inspectorView.hide();
                else
                {
                    _inspectorView.refresh();
                    _inspectorView.show();
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case INSPECT_SEL_STRING:
                if( null != _sourceTyrant && null != _sourceTyrant.getSelectedText())
                {
                    _emperor.setWaitCursor(true);
                    _inspectorTyrant.setNewRootNode(_sourceTyrant.getSelectedText());
                    _emperor.setWaitCursor(false);
                }
                InspectorView iView = _emperor.getInspectorView();
                if( null == _inspectorView )
                    break;
                if( ! _inspectorView.isVisible() )
                {
                    _inspectorView.refresh();
                    _inspectorView.show();
                }
                refreshCmdStatesAndNotifyObservers();
                break;
            case WATCH_STRING:
                if( null != data )
                {
                    _watchTyrant.addWatchString((String)data);
                    _watchTyrant.evalList();

                    if( null != _watchView )
                        _watchView.refresh();

                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case CLEAR_CONSOLE:
                if( null != _consoleView )
                    _consoleView.clear();
                break;
            case COPY_STRING:
                if( null != data )
                {
                    _setClipboardContents((String)data);
                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case CUT:
                String s2 = _getCutText();
                if( null != s2 )
                {
                    _setClipboardContents(s2);
                    refreshCmdStatesAndNotifyObservers();
                }
                break;
            case TEST:
                Test.doTest(_emperor);
                break;
            default:
                if(AS.S)ER.T( false, "cmdState id not handled: " + cmdState.id, this);
                break;
        }
    }

    private void   _setClipboardContents(String s)
    {
        if( ! _usingLocalClipboard )
        {
            try
            {
                PrivilegeManager.enablePrivilege("UniversalSystemClipboardAccess");
                _app.setClipboardText(s);
                return;
            }
            catch(Exception e)
            {
                _usingLocalClipboard = true;
//                if(AS.DEBUG)System.out.println( "switching to use local clipboard");
            }
        }

        _localClipboard = s;
    }
    private String _getClipboardContents()
    {
        if( ! _usingLocalClipboard )
        {
            try
            {
                PrivilegeManager.enablePrivilege("UniversalSystemClipboardAccess");
                return _app.clipboardText();
            }
            catch(Exception e)
            {
                _usingLocalClipboard = true;
//                if(AS.DEBUG)System.out.println( "switching to use local clipboard");
            }
        }
        return _localClipboard;
    }

    private String _getCopyText()
    {
        String s = null;

        if( null != _sourceTyrant )
            s = _sourceTyrant.getSelectedText();
        if( null == s )
        {
            if( null != _inspectorView && _inspectorView.canCopy() )
                s = _inspectorView.copy();
            else if( null != _consoleView && _consoleView.canCopy() )
                s = _consoleView.copy();
        }
        return s;
    }

    private String _getCutText()
    {
        String s = null;

        if( null != _inspectorView && _inspectorView.canCopy() )
            s = _inspectorView.cut();
        else if( null != _consoleView && _consoleView.canCopy() )
            s = _consoleView.cut();
        return s;
    }

    private void _updateSelectedPageListItem( ListView listview )
    {
        ListItem li = listview.selectedItem();
        if( null != li )
        {
            SourceTextItem sti = PageListView.sourceTextItemForListItem(li);
            _sourceTyrant.setSelectedSourceItem(sti);
        }
        else
            _sourceTyrant.setSelectedSourceItem(null);
    }

    private void _getViewsFromEmperor()
    {
        if( _haveAllViews )
            return;

        if( null == _pageListView )
            _pageListView = _emperor.getPageListView();
        if( null == _sourceViewManager )
            _sourceViewManager = _emperor.getSourceViewManager();
        if( null == _commandView )
            _commandView = _emperor.getCommandView();
        if( null == _stackView )
            _stackView = _emperor.getStackView();
        if( null == _consoleView )
            _consoleView = _emperor.getConsoleView();
        if( null == _watchView )
            _watchView = _emperor.getWatchView();
        if( null == _breakpointView )
            _breakpointView = _emperor.getBreakpointView();
        if( null == _inspectorView )
            _inspectorView = _emperor.getInspectorView();

        _haveAllViews =
            null != _pageListView       &&
            null != _sourceViewManager  &&
            null != _commandView        &&
            null != _stackView          &&
            null != _consoleView        &&
            null != _watchView          &&
            null != _breakpointView     &&
            null != _inspectorView      ;
    }

    // data...

    private Application      _app;
    private Emperor          _emperor;
    private ControlTyrant    _controlTyrant   ;
    private BreakpointTyrant _breakpointTyrant;
    private SourceTyrant     _sourceTyrant    ;
    private StackTyrant      _stackTyrant     ;
    private WatchTyrant      _watchTyrant     ;
    private ConsoleTyrant    _consoleTyrant   ;
    private InspectorTyrant  _inspectorTyrant ;


    // if any views are added, then add them to _getViewsFromEmperor()
    private boolean          _haveAllViews = false;
    private PageListView     _pageListView    ;
    private SourceViewManager _sourceViewManager;
    private CommandView      _commandView     ;
    private StackView        _stackView       ;
    private ConsoleView      _consoleView     ;
    private WatchView        _watchView       ;
    private BreakpointView   _breakpointView  ;
    private InspectorView    _inspectorView   ;

    private String           _localClipboard;
    private boolean          _usingLocalClipboard;

    private CmdState[]       _cmdStatesArray;
    private Hashtable        _cmdStatesHashtable;
}



