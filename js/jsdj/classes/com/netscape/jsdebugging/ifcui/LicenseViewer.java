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
* Simple html viewer to show licence and get acceptance
*/

// when     who     what
// 11/10/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import java.io.FileInputStream;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

class LicenseViewer
    extends InternalWindow
    implements Target
{
    private static final int _editDX     = 400;
    private static final int _editDY     = 400;
    private static final int _buttonDX   = 80;
    private static final int _buttonDY   = 24;
    private static final int _spacerDX   = 5;
    private static final int _spacerDY   = 5;

    public LicenseViewer(String title, String licenseFilename)
    {
        setTitle(title);
        setCloseable(true);
        setResizable(false);

        int contentDX = _editDX + _spacerDX * 2;
        int contentDY = _editDY + _buttonDY + _spacerDY * 3;
        int labelY    = _spacerDY;
        int editY     = _spacerDY * 1;
        int buttonY   = _editDY + _spacerDY * 2;
        int editX     = _spacerDX;

        int buttonX2  = contentDX - _spacerDX - _buttonDX;
        int buttonX1  = buttonX2 - _spacerDX - _buttonDX;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);

        _textView = new TextView();
        _textView.setEditable(false);
        _textView.setSelectable(false);
        try
        {
            FileInputStream fis = new FileInputStream(licenseFilename);
            _textView.importHTMLInRange(fis, new Range(0,1),null);
        }
        catch(Exception e)
        {
            return;
        }

        ScrollGroup sg = new ScrollGroup(new Rect(editX,editY,_editDX,_editDY));
        sg.setVertScrollBarDisplay(ScrollGroup.ALWAYS_DISPLAY);
        sg.setHorizScrollBarDisplay(ScrollGroup.NEVER_DISPLAY);
        sg.setContentView(_textView);
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        addSubview(sg);

        _textView.setBounds(0,0,
                            _editDX
                                - sg.border().leftMargin()
                                - sg.border().rightMargin()
                                - sg.vertScrollBar().width(),
                            0);

        _HtmlLoadedSuccessfully = true;

        Button button;

        button = new Button(buttonX1,buttonY,_buttonDX,_buttonDY);
        button.setTitle("I Accept");
        button.setTarget(this);
        button.setCommand(ACCEPT_CMD);
        addSubview(button);

        button = new Button(buttonX2,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Cancel");
        button.setTarget(this);
        button.setCommand(CANCEL_CMD);
        addSubview(button);

        center();
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(ACCEPT_CMD) )
        {
            _userPressedAccept = true;
            hide();
        }
        else if( cmd.equals(CANCEL_CMD) )
        {
            hide();
        }
    }

    public void showModally()
    {
        _userPressedAccept = false;
        super.showModally();
    }

    public boolean  userPressedAccept()     {return _userPressedAccept;}
    public boolean  HtmlLoadedSuccessfully() {return _HtmlLoadedSuccessfully;}

    private boolean   _userPressedAccept     = false;
    private boolean   _HtmlLoadedSuccessfully = false;
    private TextView  _textView;

    private static final String CANCEL_CMD  = "CANCEL_CMD";
    private static final String ACCEPT_CMD  = "ACCEPT_CMD";
}

