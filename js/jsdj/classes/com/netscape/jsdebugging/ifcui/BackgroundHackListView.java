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
* Hacked ListView to workaround old Java bug on Unix
*/

// when     who     what
// 06/27/97 jband   added this class to implement Unix hack
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;

class BackgroundHackListView extends ListView
{
    public BackgroundHackListView()
    {
        super();
    }
    public BackgroundHackListView(Rect r)
    {
        super(r);
    }

    public BackgroundHackListView(int x, int y, int dx, int dy )
    {
        super(x,y,dx,dy);
    }

    public void drawViewBackground(Graphics g, int x, int y, int width,int height)
    {
        if(!isTransparent())
        {
            Color bc = backgroundColor();
            if( null != bc )
            {
                g.setColor(bc);

                // XXX: hackage to deal with Unix problem
                // (see bugsplat bug #78027)
                java.awt.Graphics awtg = AWTCompatibility.awtGraphicsForGraphics(g);
                awtg.setColor(java.awt.Color.white);
                awtg.setColor(AWTCompatibility.awtColorForColor(bc));

                g.fillRect(x, y, width, height);
            }
        }
    }
}
