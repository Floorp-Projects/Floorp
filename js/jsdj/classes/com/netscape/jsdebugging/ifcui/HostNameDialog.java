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


