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

package com.netscape.jsdebugging.ifcui.palomar.widget;

import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import netscape.application.*;

public class StringView extends View implements Shapeable
{

    public StringView()
    {
       this(null);
    }

    public StringView(String label)
    {
        _font = Font.defaultFont();
        _metrics = _font.fontMetrics();
        _color = Color.black;
        _label = label;
        calculateSize();
    }

    public void setColor(Color color)
    {
        _color = color;
    }

    public void setFont(Font font)
    {
        _font = font;
        _metrics = font.fontMetrics();
        calculateSize();
    }

    public void setLabel(String label)
    {
        _label = label;

        calculateSize();
    }

    public String getLabel()
    {
        return _label;
    }

    public Font getFont()
    {
        return _font;
    }

    public FontMetrics getFontMetrics()
    {
        return _metrics;
    }

    public Color getColor()
    {
        return _color;
    }

    protected void calculateSize()
    {
        if (_label != null)
        {
           Size size = _metrics.stringSize(_label);
           setMinSize(size.width,size.height);
        } else {
           setMinSize(0,0); //_metrics.charHeight());
        }
     }

    public void drawView(Graphics g)
    {
        if (_label != null)
        {
            g.setFont(_font);
            g.setColor(_color);
            g.drawStringInRect(_label, 0,0,width(),height(),Graphics.CENTERED);
        }
    }

    public Size preferredSize()
    {
        return minSize();
    }

    public Size maxSize()
    {
        return new Size(Integer.MAX_VALUE, Integer.MAX_VALUE);
    }

    private Font _font;
    private FontMetrics _metrics;
    private String _label;
    private Color _color;
}
