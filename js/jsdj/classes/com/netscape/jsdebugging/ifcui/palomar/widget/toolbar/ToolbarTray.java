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