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


