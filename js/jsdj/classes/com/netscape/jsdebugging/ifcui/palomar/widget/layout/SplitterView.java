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

// By Eric D Vaughan

package com.netscape.jsdebugging.ifcui.palomar.widget.layout;
import netscape.application.*;
import netscape.util.*;

public class SplitterView extends BoxView
{
    /**
     * Creates a splitter given the direction of the splitter, either
     * VERT or HORIZ
     */
    public SplitterView(int direction)
    {
        super(direction);

        // leave some spacing for the splitter controls
        getBoxSpringLayoutManager().setChildPadding(SPLITTER_SIZE);
    }

    public void drawSubviews(Graphics g)
    {
        super.drawSubviews(g);

        int direction = getDirection();

        Vector views = subviews();
        int size = views.size();
        if (size < 2)
          return;

        // draw the splitter controls
        for (int i=0;i < views.size()-1; i++)
        {
            View child = (View)views.elementAt(i);
            Rect bounds = child.bounds();

            if (direction == HORIZ) {
               g.setColor(Color.lightGray);
               g.drawLine(bounds.x+bounds.width,bounds.y,bounds.x+bounds.width,bounds.height);
               g.setColor(Color.white);
               g.drawLine(bounds.x+bounds.width+1,bounds.y,bounds.x+bounds.width+1,bounds.height);
               g.setColor(Color.lightGray);
               g.drawLine(bounds.x+bounds.width+2,bounds.y,bounds.x+bounds.width+2,bounds.height);
               g.setColor(Color.gray);
               g.drawLine(bounds.x+bounds.width+3,bounds.y,bounds.x+bounds.width+3,bounds.height);
               g.setColor(Color.darkGray);
               g.drawLine(bounds.x+bounds.width+4,bounds.y,bounds.x+bounds.width+4,bounds.height);

            } else {
               g.setColor(Color.lightGray);
               g.drawLine(bounds.x, bounds.y+bounds.height,bounds.width,bounds.y+bounds.height);
               g.setColor(Color.white);
               g.drawLine(bounds.x, bounds.y+bounds.height+1,bounds.width,bounds.y+bounds.height+1);
               g.setColor(Color.lightGray);
               g.drawLine(bounds.x, bounds.y+bounds.height+2,bounds.width,bounds.y+bounds.height+2);
               g.setColor(Color.gray);
               g.drawLine(bounds.x, bounds.y+bounds.height+3,bounds.width,bounds.y+bounds.height+3);
               g.setColor(Color.darkGray);
               g.drawLine(bounds.x, bounds.y+bounds.height+4,bounds.width,bounds.y+bounds.height+4);
            }
        }
    }

     public int cursorForPoint(int x,
                               int y)
     {
        // change the cursor so the user knows he/she can
        // change the size
        if (getDirection() == HORIZ)
           return E_RESIZE_CURSOR;
        else
           return N_RESIZE_CURSOR;
     }

     public void mouseDragged(MouseEvent event)
     {

        if (_dragSpringLeft == null)
           return;

        int delta = 0;

        // figure out how much to move the splitter
        if (getDirection() == HORIZ)
           delta = event.x - _dragAnchor.x;
        else
           delta = event.y - _dragAnchor.y;

        //System.out.println("delta="+delta);

        // change the splitters size if we can


        // get the sizes of views to resize
        int leftMinSize = getMinSizeFor(_dragViewLeft, _dragSpringLeft);
        int leftMaxSize = getMaxSizeFor(_dragViewLeft, _dragSpringLeft);
        int leftWidth = _dragWidthLeft+delta;

        int rightMinSize = getMinSizeFor(_dragViewRight, _dragSpringRight);
        int rightMaxSize = getMaxSizeFor(_dragViewRight, _dragSpringRight);
        int rightWidth = _dragWidthRight-delta;


        // make sure we don't shink things too small or make them too large
        if (leftWidth < leftMinSize) {
           rightWidth -= (leftMinSize-leftWidth);
           leftWidth = leftMinSize;
        } else if (leftWidth > leftMaxSize) {
           rightWidth -= (leftWidth-leftMaxSize);
           leftWidth = leftMaxSize;
        }

        if (rightWidth < rightMinSize) {
           leftWidth -= (rightMinSize-rightWidth);
           rightWidth = rightMinSize;
        } else if (rightWidth > rightMaxSize) {
           leftWidth -= (rightWidth-rightMaxSize);
           rightWidth = rightMaxSize;
        }

        //System.out.println("left minsize="+leftMinSize+"left width="+leftWidth);
        //System.out.println("Right minsize="+rightMinSize+"right width="+rightWidth);

        _dragSpringLeft.setPreferredSize(leftWidth);
        _dragSpringRight.setPreferredSize(rightWidth);


        // relayout and draw
        setDirty(true);
        layoutView(0,0);
     }

     public boolean mouseDown(MouseEvent event)
     {
        _dragAnchor = new Point(event.x, event.y);
        //System.out.println("anchor set "+_dragAnchor);

        // figure out which splitter was moved
        int direction = getDirection();

        Vector views = subviews();
        int size = views.size();
        if (size < 2)
          return true;

        // set the preferred size of every item to what its
        // current bounds are.
        for (int i=0;i < views.size(); i++)
        {
            View child = (View)views.elementAt(i);
            Rect bounds = child.bounds();
            Spring spring = getElementAt(i);

            if (direction == HORIZ)
            {
               spring.setPreferredSize(bounds.width);
            } else {
               spring.setPreferredSize(bounds.height);
            }
        }

        // find the splitter that the mouse is over
        for (int i=0;i < views.size(); i++)
        {
            View child = (View)views.elementAt(i);
            Rect bounds = child.bounds();
            Spring spring = getElementAt(i);

            boolean found = false;

            if (direction == HORIZ)
            {
                found = (event.x >= bounds.x + bounds.width && event.x < bounds.x + bounds.width + SPLITTER_SIZE);
            } else {
                found = (event.y >= bounds.y + bounds.height && event.y < bounds.y + bounds.height + SPLITTER_SIZE);
            }

            if (found)
            {
                    // record all of the info we need to resize the
                    // splitter on the drag:
                    // 1) the views to the left and right of the splitter
                    //    to resize
                    // 2) the springs used to layout the views to the
                    //    left and right of the splitter
                    // 3) the initial widths of the views on the left and right of the
                    //    splitter

                    _indexDragged = i;
                    _dragSpringLeft = spring;
                    _dragWidthLeft = _dragSpringLeft.preferredSize();
                    _dragViewLeft = child;

                    _dragSpringRight = getElementAt(i+1);
                    _dragViewRight = (View)views.elementAt(i+1);

                    if (direction == HORIZ)
                       _dragWidthRight =  _dragViewRight.width();
                    else
                       _dragWidthRight =  _dragViewRight.height();


                    break;
            }
        }

        layoutView(0,0);
        setDirty(true);

        return true;
     }

     public void mouseUp(MouseEvent event)
     {
         if (_dragAnchor != null && _sizedTarget != null)
            _sizedTarget.performCommand(_sizedCommand,new Integer(_indexDragged));
         
         _dragAnchor = null;
     }

    private int getMinSizeFor(View view, Spring spring)
    {
            Size size = view.minSize();

            // what is its minimum size?
            // ask the view, then apply the springs value
            if (getDirection() == HORIZ)
            {
               if (spring.isMinSizeSet)
                  size.width = spring.minSize();

               return size.width;

            } else {
               if (spring.isMinSizeSet)
                  size.height = spring.minSize();

               return size.height;
            }
    }

    private int getMaxSizeFor(View view, Spring spring)
    {
            Size size = null;

            // if the view is shapeable its easy just ask the
            // the shapeable for its max size.
            if (view instanceof Shapeable)
               size = ((Shapeable)view).maxSize();
            else
               size = new Size(Integer.MAX_VALUE, Integer.MAX_VALUE);

            // apply the spring to the max size
            if (getDirection() == HORIZ)
            {
               if (spring.isMaxSizeSet)
                  size.width = spring.maxSize();

               return size.width;

            } else {
               if (spring.isMaxSizeSet)
                  size.height = spring.maxSize();

               return size.height;
            }
    }
    
    /**
     * Target to be called when a Splitter is moved
     * The object passed is a Integer containing the index
     * of the splitter that was resized
     */
    public void setSizedTarget(Target target)
    {
        _sizedTarget = target;
    }
    
    public void setSizedCommnad(String command)
    {
        _sizedCommand = command;
    }
    
    public Target getSizedTarget()
    {
        return _sizedTarget;
    }
    
    public String getSizedCommand()
    {
        return _sizedCommand;
    }

     private Point _dragAnchor = null;
     private final static int SPLITTER_SIZE = 5;
     private Spring _dragSpringLeft = null;
     private int _dragWidthLeft = 0;
     private Spring _dragSpringRight = null;
     private int _dragWidthRight = 0;
     private View _dragViewLeft = null;
     private View _dragViewRight = null;
     private Target _sizedTarget;
     private String _sizedCommand = "sized";
     private int _indexDragged = 0;

}

