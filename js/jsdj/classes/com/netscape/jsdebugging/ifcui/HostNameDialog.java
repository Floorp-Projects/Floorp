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
* Dialog used in remote debugging to choose server
*/

// when     who     what
// 11/10/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

class HostNameDialog
    extends InternalWindow
    implements Target, TextFilter
{
    private static final int _labelDX    = 300;
    private static final int _labelDY    = 24;
    private static final int _editDX     = 300;
    private static final int _editDY     = 24;
    private static final int _buttonDX   = 70;
    private static final int _buttonDY   = 24;
    private static final int _spacerDX  = 5;
    private static final int _spacerDY  = 5;

    public HostNameDialog(String title, String label, String hostname)
    {
        super();

        setTitle(title);
        setCloseable( false );
        setResizable( false );

        int contentDX = _editDX + _spacerDX * 2;
        int contentDY = _labelDY + _editDY + _buttonDY + _spacerDY * 4;
        int labelY    = _spacerDY;
        int editY     = _labelDY + _spacerDY * 2;
        int buttonY   = _labelDY + _editDY + _spacerDY * 3;

        int labelX    = _spacerDX;
        int editX     = _spacerDX;

        int buttonX3  = contentDX - _spacerDX - _buttonDX;
        int buttonX2  = buttonX3 - _spacerDX - _buttonDX;
        int buttonX1  = buttonX2 - _spacerDX - _buttonDX;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);

        addSubview( _newLabel(labelX, labelY, _labelDX, _labelDY, label) );

        _textField = new TextField(editX,editY,_editDX,_editDY);
        _textField.setStringValue(hostname);
        _textField.setInsertionPoint(hostname.length());
//        _textField.selectRange(new Range(0,hostname.length()));
        _textField.setFilter(this); // to catch return key
        addSubview(_textField);

        Button button;

        button = new Button(buttonX1,buttonY,_buttonDX,_buttonDY);
        button.setTitle("OK");
        button.setTarget(this);
        button.setCommand(OK_CMD);
        addSubview(button);

        button = new Button(buttonX2,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Localhost");
        button.setTarget(this);
        button.setCommand(LOCALHOST_CMD);
        addSubview(button);

        button = new Button(buttonX3,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Cancel");
        button.setTarget(this);
        button.setCommand(CANCEL_CMD);
        addSubview(button);

        center();
    }

    private TextField _newLabel( int x, int y, int dx, int dy, String text )
    {
        TextField label = new TextField(x,y,dx,dy);
        label.setStringValue(text);
        label.setEditable(false);
        label.setSelectable(false);
        label.setJustification(Graphics.LEFT_JUSTIFIED);
        label.setBackgroundColor(Color.lightGray);
        label.setBorder(null);
        return label;
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(OK_CMD) )
        {
            // get and validate data...

            _hostName = _textField.stringValue();
            if( null != _hostName )
                _hostName = _hostName.trim();
            if( null == _hostName || 0 == _hostName.length() )
            {
                _hostName = _hostName.trim();
                Alert.runAlertInternally(
                            Alert.notificationImage(),
                            "invalid entry",
                            "you must enter a valid hostname or ip address",
                            "OK", null,null );
                setFocusedView( _textField );
                return;
            }

            _keyPressed = OK;
            hide();
        }
        else if( cmd.equals(LOCALHOST_CMD) )
        {
            _keyPressed = LOCALHOST;
            hide();
        }
        else if( cmd.equals(CANCEL_CMD) )
        {
            _keyPressed = CANCEL;
            hide();
        }
    }

    // implement TextFilter interface
    public boolean acceptsEvent(Object o, KeyEvent ke , Vector vec)
    {
        if( ke.isReturnKey() )
        {
            Application.application().performCommandLater(this, OK_CMD, null);
            return false;
        }
        return true;
    }

    public void showModally()
    {
        _keyPressed = CANCEL;
        super.showModally();
    }

    public void show()
    {
    // for some reason this dialog does not want to really get the focus
    // until the user clicks on it. I'm not figuring out why. This sucks.
//        Application.application().mainRootView().setFocusedView(this);
        super.show();
        _textField.setFocusedView();
    }

    public static final int CANCEL    = 0;
    public static final int OK        = 1;
    public static final int LOCALHOST = 2;

    public String   getHostName()   {return _hostName;}
    public int      getKeyPressed() {return _keyPressed;}

    private TextField _textField;
    private int       _keyPressed;
    private String    _hostName;

    private static final String CANCEL_CMD    = "CANCEL_CMD";
    private static final String LOCALHOST_CMD = "LOCALHOST_CMD";
    private static final String OK_CMD        = "OK_CMD";
}


