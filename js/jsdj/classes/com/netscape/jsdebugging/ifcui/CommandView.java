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
* Manages menus and toolbar
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolbar.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolTip.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.PopupButton;
import java.io.File;
// used internally...
class CmdViewItem
{
    public CmdViewItem(boolean enabled, boolean checked)
    {
        this.enabled  = enabled;
        this.checked  = checked;
    }

    public MenuItem    menuItem;
    public PopupButton button;
    public boolean     enabled;
    public boolean     checked;
    public String      checkedText;
    public String      uncheckedText;
}

public class CommandView
    implements Observer
{
    public CommandView( Emperor  emperor,
                        Menu     mainMenu,
                        MenuItem fileMenu,
                        int      originY )
    {
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _commandTyrant = emperor.getCommandTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);

        _commandTyrant.addObserver(this);

        // init storage

        int count = CommandTyrant.CMD_COUNT;
        _items = new CmdViewItem[count];
        for( int i = 0; i < count; i++ )
        {
            CmdState state = _commandTyrant.findCmdState(i);
            if(AS.S)ER.T(null!=state,"invalid cmd state initing _items",this);
            _items[i] = new CmdViewItem(state.enabled,state.checked);
        }

        // set special text cases

        _items[CommandTyrant.SHOW_LINE_NUMBERS].checkedText    = "Hide Line Numbers";
        _items[CommandTyrant.SHOW_LINE_NUMBERS].uncheckedText  = "Show Line Numbers";

        _items[CommandTyrant.PAGELIST_SHOW_HIDE].checkedText   = "Hide Open Dialog";
        _items[CommandTyrant.PAGELIST_SHOW_HIDE].uncheckedText = "Open...";

        _items[CommandTyrant.INTERRUPT].checkedText            = "Clear Interrupt";
        _items[CommandTyrant.INTERRUPT].uncheckedText          = "Set Interrupt";

        _items[CommandTyrant.TOGGLE_BREAKPOINT].checkedText    = "Clear Breakpoint";
        _items[CommandTyrant.TOGGLE_BREAKPOINT].uncheckedText  = "Set Breakpoint";

        _items[CommandTyrant.WATCHES_SHOW_HIDE].checkedText    = "Hide Watches Dialog";
        _items[CommandTyrant.WATCHES_SHOW_HIDE].uncheckedText  = "Watches...";

        _items[CommandTyrant.BREAKPOINTS_SHOW_HIDE].checkedText    = "Hide Breakpoints Dialog";
        _items[CommandTyrant.BREAKPOINTS_SHOW_HIDE].uncheckedText  = "Breakpoints...";

        _items[CommandTyrant.INSPECTOR_SHOW_HIDE].checkedText    = "Hide Inspector";
        _items[CommandTyrant.INSPECTOR_SHOW_HIDE].uncheckedText  = "Inspector...";

        // create toolbar
        _toolbarTray = new ToolbarTray();
        _mainToolbar = new Toolbar();

//        _mainToolbar.setBackgroundImage("Nautical.gif");
        _mainToolbar.setBackgroundColor(Color.lightGray);
        _mainToolbar.showLabels(true);

        addToolbarButtons();
        addMenuItems(mainMenu, fileMenu);
        refreshAllStates(true);

        // size toolbar

        _toolbarTray.addToolbar(_mainToolbar);
        _toolBarWindow = new InternalWindow(0,originY,0,0);
        _toolBarWindow.setBuffered(true);

        _toolBarWindow.setType(Window.BLANK_TYPE);
        MarginLayout layout = new MarginLayout();
        _toolBarWindow.contentView().setLayoutManager(layout);
        _toolBarWindow.contentView().addSubview(_toolbarTray);
        Size minSize = layout.minSize(_toolBarWindow.contentView());
        RootView rv = _emperor.getRootView();
        minSize = _toolBarWindow.windowSizeForContentSize(rv.width(), minSize.height);
        _toolBarWindow.sizeTo(minSize.width, minSize.height);

        _emperor.setToolbarHeight(minSize.height);
        _emperor.setToolBarWindow(_toolBarWindow);

        _toolBarWindow.setLayer(InternalWindow.PALETTE_LAYER);
//        _toolBarWindow.show();
    }

    private void addToolbarButtons()
    {
        addButton(CommandTyrant.PAGELIST_SHOW_HIDE, "Open",  "Hide/Show Open Dialog","PageListTool.gif",  true);
        addButton(CommandTyrant.REFRESH_ALL,        "Refrsh","Refresh All",          "RefreshTool.gif",   false);
        addButton(CommandTyrant.COPY,               "Copy",  "Copy",                 "CopyTool.gif",      false);
        addButton(CommandTyrant.PASTE,              "Paste", "Paste",                "PasteTool.gif",     false);
        addButton(CommandTyrant.EVAL_SEL_STRING,    "Eval",  "Evaluate",             "EvalTool.gif",      false);
        addButton(CommandTyrant.INTERRUPT,          "Intrpt","Interrupt",            "InterruptTool.gif", true);
        addButton(CommandTyrant.TOGGLE_BREAKPOINT,  "BrkPt", "Breakpoint",           "BreakpointTool.gif",true);
        addButton(CommandTyrant.RUN,                "Run",   "Run",                  "RunTool.gif",       false);
        addButton(CommandTyrant.STEP_OVER,          "Over",  "Step Over",            "StepOverTool.gif",  false);
        addButton(CommandTyrant.STEP_INTO,          "Into",  "Step Into",            "StepIntoTool.gif",  false);
        addButton(CommandTyrant.STEP_OUT,           "Out",   "Step Out",             "StepOutTool.gif",   false);
        addButton(CommandTyrant.ABORT,              "Abort", "Abort",                "AbortTool.gif",     false);
    }

    private void addMenuItems( Menu mainMenu, MenuItem fileMenu)
    {
          addMenuItem( CommandTyrant.PAGELIST_SHOW_HIDE,"Open...",      fileMenu );
          addMenuItem( CommandTyrant.REFRESH_ALL,       "Refresh All",  fileMenu );
          // NOTE: separator and "Exit" are later appended here by our creator

        MenuItem editMenu = mainMenu.addItemWithSubmenu("Edit");
          addMenuItem( CommandTyrant.CUT,               "Cut",          editMenu );
          addMenuItem( CommandTyrant.COPY,              "Copy",         editMenu );
          addMenuItem( CommandTyrant.PASTE,             "Paste",        editMenu );
          addMenuSeparator(editMenu);
          addMenuItem( CommandTyrant.EVAL_SEL_STRING,   "Evaluate",     editMenu );
          addMenuItem( CommandTyrant.INSPECT_SEL_STRING,"Inspect...",   editMenu );
          addMenuItem( CommandTyrant.COPY_TO_WATCH,     "Copy to Watch",editMenu );
          addMenuSeparator(editMenu);
          addMenuItem( CommandTyrant.CLEAR_CONSOLE,     "Clear Console",editMenu );
          addMenuSeparator(editMenu);
          MenuItem prefsMenu = editMenu.submenu().addItemWithSubmenu("Preferences");
            addMenuItem( CommandTyrant.SHOW_LINE_NUMBERS, "Show Line Numbers", prefsMenu );

        MenuItem controlMenu = mainMenu.addItemWithSubmenu("Control");
          addMenuItem( CommandTyrant.INTERRUPT,         "Interrupt",      controlMenu );
          addMenuItem( CommandTyrant.TOGGLE_BREAKPOINT, "Set Breakpoint", controlMenu );
          addMenuItem( CommandTyrant.EDIT_BREAKPOINT,   "Edit Breakpoint...",controlMenu );
          addMenuSeparator(controlMenu);
          addMenuItem( CommandTyrant.RUN,               "Run",           controlMenu );
          addMenuItem( CommandTyrant.STEP_OVER,         "Step Over",     controlMenu );
          addMenuItem( CommandTyrant.STEP_INTO,         "Step Into",     controlMenu );
          addMenuItem( CommandTyrant.STEP_OUT,          "Step Out",      controlMenu );
          addMenuItem( CommandTyrant.ABORT,             "Abort",         controlMenu );
          addMenuSeparator(controlMenu);
          addMenuItem( CommandTyrant.TEST,              "Test",          controlMenu );

        MenuItem windowsMenu = mainMenu.addItemWithSubmenu("Windows");
          addMenuItem( CommandTyrant.INSPECTOR_SHOW_HIDE,  "Inspector...",  windowsMenu );
          addMenuItem( CommandTyrant.WATCHES_SHOW_HIDE,    "Watches...",    windowsMenu );
          addMenuItem( CommandTyrant.BREAKPOINTS_SHOW_HIDE,"Breakpoints...",windowsMenu );
    }

    /*
    * Temporary method to add menu item separators until IFC gets a native one....
    */
    private static void addMenuSeparator(MenuItem item)
    {
        item.submenu().addSeparator();
//        AWTCompatibility.awtMenuForMenu(item.submenu()).addSeparator();
    }

    private void addMenuItem( int id, String text, MenuItem menuitem )
    {
        CmdState state = _commandTyrant.findCmdState(id);
        if(AS.S)ER.T(null!=state,"invalid cmd state while creating menu item",this);
        _items[id].menuItem = menuitem.submenu().addItem(text,state.name,_commandTyrant);
    }

    private void addButtonSpacer()
    {
        PopupButton button;
        button = _mainToolbar.addButton("",null,null,null,"NarrowSpaceTool.gif");
        button.setEnabled(false);
    }

    private void addButton( int id, String text, String tip, String image, boolean toggle )
    {
        CmdState state = _commandTyrant.findCmdState(id);
        if(AS.S)ER.T(null!=state,"invalid cmd state while creating buttons",this);
        CmdViewItem item = _items[id];

        Bitmap bmp = _loadBitmap(image, true);
        item.button = _mainToolbar.addButton(text,tip,_commandTyrant,state.name,bmp,null);

        if( toggle )
            item.button.setCanToggle(true);

    }

    private Bitmap _loadBitmap(String name, boolean fromJar)
    {
        java.awt.Image image = null;
        Bitmap bitmap = null;

        if(!fromJar)
            return Bitmap.bitmapNamed(name);

        try
        {
            java.io.InputStream in = null;
            String fullname = "images/"+name;
            ClassLoader loader = getClass().getClassLoader();
            if(null != loader)
            {
//                if(AS.DEBUG)System.out.println("using loader.getResourceAsStream()");
                in = loader.getResourceAsStream(fullname);
            }
            else
            {
//                if(AS.DEBUG)System.out.println("using ClassLoader.getSystemResourceAsStream()");
                in = ClassLoader.getSystemResourceAsStream(fullname);
            }

            if(null != in)
            {
//                System.err.println("good stream");
                byte[] buf = new byte[in.available()];
                in.read(buf);
                image = java.awt.Toolkit.getDefaultToolkit().createImage(buf);
            }
//            else
//                System.err.println("bad stream");
        }
        catch (java.io.IOException e)
        {
//            System.err.println("Unable to read image.");
//            e.printStackTrace();
        }
        if(null != image)
        {
            int width;
            int height;

            // XXX pretty iffy!
            while(-1 == (width = image.getWidth(null)) ||
                  -1 == (height = image.getHeight(null)))
                Thread.currentThread().yield();

//            System.err.println("width = "+width);
//            System.err.println("height = "+height);

            int[] pix = new int[width*height];

            java.awt.image.PixelGrabber grabber =
                    new java.awt.image.PixelGrabber(image, 0, 0, width, height,
                                                    pix, 0, width);

            try
            {
                if(grabber.grabPixels())
                    bitmap = new Bitmap(pix, width, height);
            }
            catch(InterruptedException ie)
            {
//                System.err.println("grabber interrupted");
//                ie.printStackTrace();
            }
        }
        if(null == bitmap)
        {
            if(AS.DEBUG)System.err.println("loading bitmap from jar failed, trying Bitmap.bitmapNamed()");
            bitmap = Bitmap.bitmapNamed(name);
        }

        return bitmap;
    }

    private void refreshSingleItemState(int i, boolean force)
    {
        CmdState state = _commandTyrant.findCmdState(i);
        if(AS.S)ER.T(null!=state,"invalid cmd state in refreshAllStates()",this);
        CmdViewItem item = _items[i];

        if( null != item.button )
        {
            if( force ||
                item.enabled != state.enabled ||
                item.checked != state.checked )
            {
                item.button.setEnabled(state.enabled);
                item.button.setChecked(state.checked);
                item.button.draw();
            }
        }
        if( null != item.menuItem )
        {
            if( force || item.enabled != state.enabled )
                item.menuItem.setEnabled(state.enabled);
            if( force || item.checked != state.checked )
            {
                if( state.checked && null != item.checkedText )
                    item.menuItem.setTitle( item.checkedText );
                else if( ! state.checked && null != item.uncheckedText )
                    item.menuItem.setTitle( item.uncheckedText );
            }
        }
        item.enabled = state.enabled;
        item.checked = state.checked;
    }

    private void refreshAllStates(boolean force)
    {
        int count = CommandTyrant.CMD_COUNT;
        for( int i = 0; i < count; i++ )
            refreshSingleItemState(i, force);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        refreshAllStates(false);
    }


    private Emperor         _emperor;
    private ControlTyrant   _controlTyrant;
    private CommandTyrant   _commandTyrant;

    private ToolbarTray     _toolbarTray;
    private Toolbar         _mainToolbar;
    private InternalWindow  _toolBarWindow;

    private CmdViewItem[] _items;

    // special cases for menu item text refreshes
    private boolean _oldLineNumbersShowing;
    private boolean _oldPageListShowing;
    private boolean _oldInterruptOn;
    private boolean _oldBkeakpointSet;
}

