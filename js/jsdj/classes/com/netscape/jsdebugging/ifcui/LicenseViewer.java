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

        _HtmlLoadedSuccesfully = true;

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
    public boolean  HtmlLoadedSuccesfully() {return _HtmlLoadedSuccesfully;}

    private boolean   _userPressedAccept     = false;
    private boolean   _HtmlLoadedSuccesfully = false;
    private TextView  _textView;

    private static final String CANCEL_CMD  = "CANCEL_CMD";
    private static final String ACCEPT_CMD  = "ACCEPT_CMD";
}

