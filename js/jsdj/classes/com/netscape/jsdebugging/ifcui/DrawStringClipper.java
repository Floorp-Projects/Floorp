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
* helper class to trim a string and its rect to fit a clip rect width
*/

// when     who     what
// 09/23/98 jband   created file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

/* NOTE: results are in object vars -- caller must handle synchronization */

public class DrawStringClipper
{
    public DrawStringClipper( int charWidth ) {_charWidth = charWidth;}

    public void setCharWidth( int charWidth ) {_charWidth = charWidth;}

    public int doClip(String str, Rect rect, Rect clipRect)
    {
        _str = str;
        _rect = rect;
        _len = _str.length();

        int charsToTrim;
        int space;
        if( _len > 0 )
        {
            space = (_rect.x + _rect.width) - (clipRect.x + clipRect.width);
            if( space > _charWidth )
                {
                space = (space/_charWidth)*_charWidth;
                int trail = (_rect.width/_charWidth) - _len;
                charsToTrim = Math.min(_len, (space/_charWidth)- trail);
                if( charsToTrim > 0 )
                {
                    _rect.width -= space;
                    _str = _str.substring(0, _len-charsToTrim);
                    _len -= charsToTrim;
                }
            }
        }
        if( _len > 0 )
        {
            space = clipRect.x - _rect.x;
            if( space > _charWidth )
                {
                space = (space/_charWidth)*_charWidth;
                charsToTrim = Math.min(_len, space / _charWidth);
                if( charsToTrim > 0 )
                {
                    _rect.x += space;
                    _rect.width -= space;
                    _str = _str.substring(charsToTrim);
                    _len -= charsToTrim;
                }
            }
        }
        return _len;
    }

    public String getString()       {return _str;}
    public int    getStringLength() {return _len;}
    public Rect   getRect()         {return _rect;}
    
    private String  _str;
    private int     _len;
    private Rect    _rect;
    private int     _charWidth;
}    
