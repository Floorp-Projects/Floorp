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
