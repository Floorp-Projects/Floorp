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
* Popup (floating) menu support
*/

// By Jennifer Lateiner


/**
*
* jband 11/1/97
*
* Shamelessly stolen from com.netscape.jsdebugging.ifcui.palomar.widget.mdi
* We will use the 'real' version when it comes time to rely on
* that mdi code
*/

// package com.netscape.jsdebugging.ifcui.palomar.widget.mdi;
package com.netscape.jsdebugging.ifcui;

import netscape.application.*;

/**
 * MouseMenuView.java
 * <p>
 * Menu subclass for context menus,
 * Defaults to Vertical
 *
 * NOTE: Fixed problem where mouseUp in bottom 5 pixels of menu
 * were causing a null pointer exception by moving mouseEvent
 * on MouseUp up 5 pixels when the event was in that region.
 *
 * Also note: getTitle on a menu displayed by MouseMenuView
 * will have 4 leading spaces.
 *
 *  <pre>
 *  History:
 *  07/30/97:  Created by Jenni
 *  </pre>
 *  Comments to: <a href=mailTo:jenni@netscape.com>jenni@netscape.com</a><br>
 */
public class MouseMenuView extends MenuView {
    InternalWindow _window;
    BezelBorder _border;
    Font _menuFont;
    final static Color   winBlue = new Color(0, 0, 128);

    /** Constructs a MenuView with origin (0, 0) and zero width and height.
      * This MenuView will create its own top-level Menu, will be of type
      * HORIZONTAL, and will have no owner.
      */
    public MouseMenuView() {
        this(0, 0, 0, 0, null, null);
    }

    /** Constructs a MouseMenuView with origin (0, 0) and zero width and height.
      * This MouseMenuView will use <b>aMenu</b> to define its structure. This
      * MouseMenuView's owner will be null.
      */
    public MouseMenuView(Menu aMenu) {
        this(0, 0, 0, 0, aMenu, null);
    }

    /** Constructs a MouseMenuView with bounds (<b>x</b>, <b>y</b>, <b>width</b>,
      * </b>height</b>).
      */
    public MouseMenuView(int x, int y, int width, int height) {
        this(x, y, width, height, null, null);
    }

    /** Constructs a MouseMenuView with bounds (<b>x</b>, <b>y</b>, <b>width</b>,
      * </b>height</b>). This MouseMenuView will use <b>aMenu</b> to define
      * its structure, and will determine its type by whether or not
      * <b>aMenu</b> is top-level. This MouseMenuView's owner will be null.
      */
    public MouseMenuView(int x, int y, int width, int height, Menu aMenu) {
        this(x, y, width, height, aMenu, null);
    }


    /** Creates a MouseMenuView with anOwner.
      */
    public MouseMenuView(int x, int y, int width, int height, Menu aMenu,
                    MenuView anOwner) {
        super(x, y, width, height, aMenu, anOwner);

        // Get menufont from preferences
        // jband -- removed reliance on class I don't want...
//        _menuFont = com.netscape.jsdebugging.ifcui.palomar.widget.Preferences.textFont;
        _menuFont = Font.defaultFont();
        setType(VERTICAL);
    }

    /** Creates the InternalWindow that is used by this MouseMenuView for
      * displaying its Menu's MenuItems. We create the MenuWindow so that
      * we can track it.
      */
    protected InternalWindow createMenuWindow() {

        _window = new InternalWindow(0, 0, 0, 0);
        _window.setType(InternalWindow.BLANK_TYPE);
        _window.setLayer(InternalWindow.IGNORE_WINDOW_CLIPVIEW_LAYER+11);
        _window.setCanBecomeMain(false);
        _window.setScrollsToVisible(true);

        //window.setResizable(true);
        return _window;
    }

    /** Overridden to receive mouse up events; IFC was creating
      * null pointer exceptions if there was a mouse up between the
      * extreme bottom edge and the bottom of the last menu item.
      * So we trap and translate for this case.
      */
    public void mouseUp(MouseEvent event) {
     try{
        if (event.y > bounds.height - 5) {
            event.y -= 5;
        }
        /*
         * This assumes that there was no clicked item because the up
         * occured in a child.  We trap an IFC 1.1.1 bug in Menus here.
         */
        if(owner() == null && itemForPoint(event.x, event.y) == null) {
            MenuView menuView = child;
            MenuView tView;

            if(menuView != null)
                tView = menuView;
            else
                tView = this;

            while (menuView != null) {
                menuView = menuView.child;
                if(menuView != null)
                    tView = menuView;
            }

            if(tView != this) {
                try {
                    if(rootView() != null)
                        tView.mouseUp(convertEventToView(tView, event));
                } catch (netscape.util.InconsistencyException e) {
                    // See note below about the mouseUp and being removed
                    // before events are processed.
                    return;
                }
            }
            return;
        }

        super.mouseUp(event);
     } catch (java.lang.NullPointerException e) {
        // There are cases when during the mouse up (which also removes the
        // menuView) whereby the event isn't processed before the menu is
        // removed.  If this happens, the safest thing to do is to return
        // without attempting to process.
        // -Jenni

        //System.out.println("Menu already removed, returning without event");
        return;
     }
    }


    /** Other menu views want this, but we dont!
      * There is a quirk whereby autoscroll events can be sent
      * after we are removed from the view hierarchy.
      */
    public boolean wantsAutoscrollEvents() {
        return false;
    }


    /** Displays the menu in its own InternalWindow
      */
    public void show(RootView rootView, MouseEvent event) {
        boolean canRightFlip=true;
        int    count;
        Menu menu=menu();
        String tString;

        count = menu.itemCount();

        _window.setRootView(rootView);

        /* Somewhat of a hack to set colors (and fonts?)
         * quickly and easily.  It was faster to do it here than
         * elsewhere.
         *
         */

// jband 11/1/97  I don't want to have it do this...
//        for (int i = 0; i < count; i++) {
//            menu.itemAt(i).setSelectedColor(winBlue);
//            menu.itemAt(i).setSelectedTextColor(Color.white);
//            menu.itemAt(i).setFont(_menuFont);
//            if(!menu.itemAt(i).isSeparator()) {
//                if(!(tString = menu.itemAt(i).title()).startsWith("    ")) {
//                    menu.itemAt(i).setTitle("    "+tString);
//                }
//            }
//        }

        if(!(menu().border() instanceof BezelBorder)){
            menu().setBorder(new BezelBorder(BezelBorder.RAISED_BUTTON));

        }

        sizeToMinSize();

        super.show(rootView, event);

        /* Move the menu over and to the left a little from the
         * mouse event (which is the standard IFC
        if(owner() == null) {
            _window.setBounds(_window.bounds.x-5, _window.bounds.y-5,
                            _window.bounds.width,
                            _window.bounds.height);
        }
         */

        if(_window.bounds.x + _window.bounds.width >
                    rootView.bounds.width) {
            if(owner() == null) {
                _window.setBounds(rootView.bounds.width -
                        _window.bounds.width,
                        _window.bounds.y,
                        _window.bounds.width,
                        _window.bounds.height);
            } else if(owner() instanceof MouseMenuView) {
                InternalWindow tWin=((MouseMenuView)(owner())).getWindow();
                _window.setBounds(tWin.bounds.x -
                    _window.bounds.width +3,
                    _window.bounds.y,
                    _window.bounds.width,
                    _window.bounds.height);

                canRightFlip = false;

            }
        }

        if(_window.bounds.x < 0) {
            if(owner() == null) {
                _window.setBounds(0,
                        _window.bounds.y,
                        _window.bounds.width,
                        _window.bounds.height);
            } else if(canRightFlip && owner() instanceof MouseMenuView) {
                InternalWindow tWin=((MouseMenuView)(owner())).getWindow();
                _window.setBounds(tWin.bounds.x +
                    _window.bounds.width -3,
                    _window.bounds.y,
                    _window.bounds.width,
                    _window.bounds.height);
            }
        }

        if(_window.bounds.y + _window.bounds.height >
                    rootView.bounds.height) {
                _window.setBounds(_window.bounds.x,
                        rootView.bounds.height -
                        _window.bounds.height,
                        _window.bounds.width,
                        _window.bounds.height);
        }

        if(_window.bounds.y  < 0) {
                _window.setBounds(_window.bounds.x,
                        0,
                        _window.bounds.width,
                        _window.bounds.height);
        }

    }

    /** Returns the InternalWindow used to house the MouseMenuView
      */
    public InternalWindow getWindow() {
        return _window;
    }


    /** Overridden to provide a MouseMenuView.
      */
    protected MenuView createMenuView(Menu theMenu) {
        MenuView retval = ((MenuView) (new MouseMenuView(0, 0, 0, 0, theMenu, this)));
//        retval.window();
        return retval;
    }


}


