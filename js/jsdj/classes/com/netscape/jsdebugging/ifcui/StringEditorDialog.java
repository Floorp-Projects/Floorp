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
