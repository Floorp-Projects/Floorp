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
* Dialog to edit one string
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

class StringEditorDialog
    extends InternalWindow
    implements Target
{
    private static final int _editDX     = 300;
    private static final int _editDY     = 24;
    private static final int _buttonDX   = 64;
    private static final int _buttonDY   = 24;
    private static final int _spacerDX  = 5;
    private static final int _spacerDY  = 5;

    public StringEditorDialog(String title, String s, Font font)
    {
        super();

        setTitle(title);
        setCloseable( false );
        setResizable( false );

        int contentDX = _editDX + _spacerDX * 2;
        int contentDY = _editDY + _spacerDY * 3 + _buttonDY;
        int buttonY   = _editDY + _spacerDY * 2;
        int buttonX2  = (_editDX + _spacerDX) - _buttonDX;
        int buttonX1  = buttonX2 - _spacerDX - _buttonDX;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);
        _textfield = new TextField(_spacerDX,_spacerDY,_editDX,_editDY);
        _textfield.setStringValue(new String(s));
        _textfield.setFont(font);
        addSubview(_textfield);

        Button button;

        button = new Button(buttonX1,buttonY,_buttonDX,_buttonDY);
        button.setTitle("OK");
        button.setTarget(this);
        button.setCommand(OK_CMD);
        addSubview(button);

        button = new Button(buttonX2,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Cancel");
        button.setTarget(this);
        button.setCommand(CANCEL_CMD);
        addSubview(button);

        setFocusedView( _textfield );
        center();
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(OK_CMD) )
        {
            _ok = true;
            hide();
        }
        else if( cmd.equals(CANCEL_CMD) )
        {
            hide();
        }
    }

    public boolean okPressed() {return _ok;}

    public String getString()
    {
        return null != _textfield ? _textfield.stringValue() : null;
    }

    private TextField _textfield;
    private boolean   _ok = false;
    private static final String OK_CMD     = "OK_CMD";
    private static final String CANCEL_CMD = "CANCEL_CMD";
}
