/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.ifcui.palomar.widget;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

public class SeparatorView extends ShapeableView
{
    public void drawView(Graphics g)
    {
        g.setColor(Color.gray);
        g.drawLine(0,0,bounds.width,0);
        g.setColor(Color.white);
        g.drawLine(0,1,bounds.width,1);
    }

    public Size preferredSize()
    {
        return _maxSize;
    }

    public Size maxSize()
    {
        return _maxSize;
    }

    public Size minSize()
    {
        return _minSize;
    }

    private Size _maxSize = new Size(9999,2);
    private Size _minSize = new Size(2,2);
}
