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

package com.netscape.jsdebugging.ifcui.palomar.widget.toolbar;

import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.*;
import netscape.application.*;

public class ToolbarTray extends ShapeableView
{
    public ToolbarTray()
    {
        setLayoutManager(new MarginLayout());
        _toolBox = new BoxView(BoxView.VERT);
        addSubview(_toolBox);
    }

       /**
     * Sets the image to be used as a tile background for this toolbar.
     * First, checks to see if the specified file exists in the images directory.
     * If not, then does nothing.
     */
    public void setBackgroundImage(String name)
    {
        java.io.File bgImage = new java.io.File("images/" + name);
        if ( bgImage.exists() ) {
            _backgroundImage = Bitmap.bitmapNamed(name);
            setDirty(true);
        }
    }

    public void addToolbar(Toolbar toolbar)
    {
        // if the tray is empty add the initial separator
        if (_toolBox.count() == 0)
           _toolBox.addFixedView(new SeparatorView());

       // add the toolbar
        _toolBox.addFixedView(toolbar);

        // add the final separator
        _toolBox.addFixedView(new SeparatorView());
    }

    public void removeToolbar(Toolbar toolbar)
    {
        int index = _toolBox.indexOfView(toolbar);

        if (index != -1)
        {
           // remove toolbar
           _toolBox.removeElementAt(index);

           // remove separator
           _toolBox.removeElementAt(index);

           // if all toolbars are gone remove the first separator
           if (_toolBox.count() == 1)
              _toolBox.removeElementAt(0);
        }

    }

        public void drawView(Graphics g)
    {
        //System.out.println("_backgroundColor="+_backgroundColor);

        if (_backgroundColor != null)
        {
           g.setColor(_backgroundColor);
           g.fillRect(0,0,bounds.width,bounds.height);

        }

        if (_backgroundImage != null)
           _backgroundImage.drawTiled(g,0,0,bounds.width,bounds.height);

    }

       /**
     * Sets the image to be used as a tile background for this
     * toobar
     */
    public void setBackgroundColor(Color color)
    {
        _backgroundColor = color;
        setDirty(true);
    }

    public Color getBackgroundColor()
    {
        return _backgroundColor;
    }

    public Image getBackgroundImage()
    {
        return _backgroundImage;
    }



    private BoxView _toolBox;
    private Image   _backgroundImage = null;
    private Color   _backgroundColor = Color.lightGray;

}