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
* Window used to give startup status to users (lest they think we're dead)
*/

// when     who     what
// 11/10/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

class StatusWindow
    extends InternalWindow
{
    private static final int _labelDX    = 300;
    private static final int _labelDY    = 24;
    private static final int _spacerDX  = 5;
    private static final int _spacerDY  = 5;

    public StatusWindow()
    {
        super();

        setBorder(LineBorder.blackLine());
        setCloseable( false );
        setResizable( false );

        int contentDX = _labelDX + _spacerDX * 2;
        int contentDY = _labelDY + _spacerDY * 2;
        int labelY    = _spacerDY;
        int labelX    = _spacerDX;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);

        _label = _newLabel(labelX, labelY, _labelDX, _labelDY, "");
        addSubview(_label);
        center();
    }


    public void setText(String text)
    {
        _label.setStringValue(text);
        draw();
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

    private TextField _label;
}


