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

// By Eric D Vaughan

package com.netscape.jsdebugging.ifcui.palomar.widget;

import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import netscape.application.*;
import netscape.util.*;

/**
 * A class that draw a border arounds    }nntent view
 */
public class BorderView extends MarginView
{
    public BorderView()
    {
    }
    
    public BorderView(Border border)
    {
        setBorder(border);

        _border = border;
    }

    public void setBorder(Border border)
    {
        _border = border;

        if (border == null)
        {
            setLeftMargin(0);
            setRightMargin(0);
            setTopMargin(0);
            setBottomMargin(0);
        } else {
           setLeftMargin(border.leftMargin());
           setRightMargin(border.rightMargin());
           setTopMargin(border.topMargin());
           setBottomMargin(border.bottomMargin());
        }
    }

    public Border getBorder()
    {
        return _border;
    }

    public void drawSubviews(Graphics g)
    {
        super.drawSubviews(g);

        if (_border != null && _hideBorder == false)
           _border.drawInRect(g,0,0,width(),height());
    }

    public void setHideBorder(boolean hide)
    {
        _hideBorder = hide;
    }

    public boolean doesHideBorder()
    {
        return _hideBorder;
    }

    private Border _border;
    private boolean _hideBorder = false;
}