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

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.toolTip.*;

/**  A View that implements a toolbar
  */
public class Toolbar extends MarginView
{
    /**
     * Create a tool bar. Make sure you call init before using.
     */

    public Toolbar()
    {
        // tool bar doesn't currently collapse to lets remove
        // the collapsers.
   //   ImageView iv = new ImageView(Bitmap.bitmapNamed("ToolbarCollapser.gif"));

      BoxView innerBox = new BoxView(BoxView.VERT);
  //    innerBox.addFlexibleView(iv,0);

      _outerBox = new BoxView(BoxView.HORIZ);
      _outerBox.addFixedView(innerBox);
      _toolBox = new BoxView(BoxView.HORIZ);
      _outerBox.addFlexibleView(_toolBox);
      addSubview(_outerBox);
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

    /**
     * Adds a tool to the tool bar.
     * @param name The name of the button
     * @param note the note that appears when the mouse moves over the button
     */
    public PopupButton addButton(String name, String note)
    {
        return addButton(name, note, null, null);
    }

    /**
     * Adds a tool to the tool bar will the given target and command
     * @param name The name of the tool
     * @param note the not that appears when the mouse moves over the tool
     * @param target the target to be called when this tool is clicked
     * @param command the string command to be called when this tool is clicked
     */
    public PopupButton addButton(String name, String note, Target target, String command)
    {
        return addButton(name, note, target, command, (Bitmap)null, (Bitmap)null);
    }


    /**
     * Adds a tool to the tool bar with the given target and the file names
     * of the 2 images.
     * @param name The name of the button
     * @param note the note that appears when the mouse moves over the tool
     * @param target the target to be called when this tool is clicked
     * @param command the string command to be called when this tool is clicked
     * @param imageName the image that appears when the mouse is not over the tool
     * @param altImageName the image that appers when the mouse is over the tool
     */
    public PopupButton addButton(String name, String note, Target target, String command, String imageName, String altImageName)
    {
        Bitmap image    = Bitmap.bitmapNamed(imageName);
        Bitmap altImage = Bitmap.bitmapNamed(altImageName);
        return addButton(name, note, target, command, image, altImage);
    }

    public PopupButton addButton(String name, String note, Target target, String command, String imageName)
    {
        Bitmap image    = Bitmap.bitmapNamed(imageName);
        return addButton(name, note, target, command, image, null);
    }

    /**
     * Determinrs whether the name of the button appears underneath it
     * or not.
     */
    public void showLabels(boolean show)
    {
        if (_showLabels != show)
        {
           _showLabels = show;
           Enumeration e = _toolBox.views();
           while(e.hasMoreElements())
           {
              View view = (View)e.nextElement();
              if (view instanceof PopupButton)
              {
                   PopupButton b = (PopupButton)view;
                   b.showLabel(_showLabels);
              }
           }
        }
    }

    /**
     * Adds a tool to the tool bar with the target and the 2 images.
     * @param name The name of the tool
     * @param note the note that appears when the mouse moves over the tool
     * @param target the target to be called when this tool is clicked
     * @param command the string command to be called when this tool is clicked
     * @param image the image that appears when the mouse is not over the tool
     * @param altImage the image that appers when the mouse is over the tool
     */
    public PopupButton addButton(String name, String note,Target target, String command,  Bitmap image, Bitmap altImage)
    {
        // add a the list of tools.
        PopupButton b = new PopupButton(name,target,command,image,altImage);
        b.showLabel(_showLabels);
        ToolTipRootView main = ToolTipRootView.mainToolTipRootView();
        if (main != null && note != null)
           main.addTip(b,note);

        addButton(b);
        return b;
    }


    /** Adds a tool to the tool bar.
     *
     * @param tool the tool to add
     */
    public void addButton(PopupButton button)
    {
        _toolBox.addFixedView(button);
        int maxSize = 0;

        Enumeration e = _toolBox.views();
        while(e.hasMoreElements())
        {
            View view = (View)e.nextElement();
            if (view instanceof PopupButton)
            {
                PopupButton b = (PopupButton)view;
                Size s = b.minSize();
                if (s.width > maxSize)
                   maxSize = s.width;
            }
        }

        int count = 0;
        e = _toolBox.springs();
        while(e.hasMoreElements())
        {
            Spring spring = (Spring)e.nextElement();
            spring.setPreferredSize(maxSize);
        }
    }

    public BoxView getToolBox()
    {
        return _toolBox;
    }

    public void relayout()
    {
        _outerBox.resize();
        _toolBox.resize();
        layoutView(0,0);
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

    protected void addTip(View view, String tip)
    {
        _records.addElement(new ToolbarRecord(view,tip));

        RootView root = rootView();
        if (root != null && root instanceof ToolTipRootView)
        {
            ToolTipRootView toolTipRoot = (ToolTipRootView)root;
            toolTipRoot.addTip(view, tip);
        }
    }

    protected void removeTip(View view)
    {
        // remove the record
        int size = _records.size();

        for (int i = 0; i < size; i++)
        {
            ToolbarRecord record = (ToolbarRecord)_records.elementAt(i);
            if (record.view == view) {
                 _records.removeElementAt(i);
                 break;
            }
        }

        // if the root is a tool tip manager remove from it
        RootView root = rootView();
        if (root != null && root instanceof ToolTipRootView)
        {
            ToolTipRootView toolTipRoot = (ToolTipRootView)root;
            toolTipRoot.removeTip(view);
        }
    }

    protected void ancestorWasAddedToViewHierarchy(View addedView)
    {
        super.ancestorWasAddedToViewHierarchy(addedView);
        RootView root = rootView();
        if (root != null && root instanceof ToolTipRootView)
        {
            ToolTipRootView toolTipRoot = (ToolTipRootView)root;
            int size = _records.size();

            for (int i = 0; i < size; i++)
            {
                ToolbarRecord record = (ToolbarRecord)_records.elementAt(i);
                toolTipRoot.addTip(record.view, record.tip);
            }
        }
    }

    protected void ancestorWillRemoveFromViewHierarchy(View removedView)
    {
        super.ancestorWillRemoveFromViewHierarchy(removedView);
        RootView root = rootView();
        if (root != null && root instanceof ToolTipRootView)
        {
            ToolTipRootView toolTipRoot = (ToolTipRootView)root;
            int size = _records.size();

            for (int i = 0; i < size; i++)
            {
                ToolbarRecord record = (ToolbarRecord)_records.elementAt(i);
                toolTipRoot.removeTip(record.view);
            }
        }
    }


    private Image   _backgroundImage = null;
    private Color   _backgroundColor = Color.lightGray;
    private BoxView _toolBox;
    private BoxView _outerBox;
    private Vector _records = new Vector();
    private boolean _showLabels = true;
}

class ToolbarRecord
{
    ToolbarRecord(View v, String t)
    {
        view = v;
        tip = t;
    }

    View view;
    String tip;
}
