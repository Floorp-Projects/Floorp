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
* ListItems that can handle their own events
*/

// when     who     what
// 10/31/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

public class SmartListItem extends ListItem
{
    public SmartListItem()
    {
        super();
    }

    // these defaults can be overridden
    public boolean mouseDown(MouseEvent me) {return superMouseDown(me);}
    public void mouseDragged(MouseEvent me) {superMouseDragged(me);}
    public void mouseUp(MouseEvent me)      {superMouseUp(me);}

    // these provide an easy way to ask for default behavior
    public boolean superMouseDown(MouseEvent me)
    {
        return ((SmartItemListView)listView()).superMouseDown(me);
    }
    public void superMouseDragged(MouseEvent me)
    {
        ((SmartItemListView)listView()).superMouseDragged(me);
    }
    public void superMouseUp(MouseEvent me)
    {
        ((SmartItemListView)listView()).superMouseUp(me);
    }

    protected final Rect myRect()
    {
        ListView lv = listView();
        if(AS.S)ER.T(lv!=null,"ListItem with no parent called myRect()",this);
        return lv.rectForItem(this);
    }

}


