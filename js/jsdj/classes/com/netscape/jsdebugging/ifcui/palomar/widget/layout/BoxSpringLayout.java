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

public class BoxSpringLayout implements ShapeableLayoutManager
{
    public static final int VERTICAL   = 0;
    public static final int HORIZONTAL = 1;
    public static int INFINITE = Integer.MAX_VALUE;

    /**
     * Creates a box given the direction of the box, either
     * VERT or HORIZ
     */
    public BoxSpringLayout(int direction)
    {
        _direction = direction;
    }

    /**
     * sets the size in pixels of the margin around this layout
     */
    public void setMargin(int margin)
    {
        _margin = margin;
    }

    /**
     * sets space in pixels placed between each child view.
     * By default it is 0.
     */
    public void setChildPadding(int padding)
    {
        _childPadding = padding;
    }


    /**
     * returns the margin in pixels that appears around this layout.
     * default is 0.
     */
    public int getMargin()
    {
        return _margin;
    }

    /**
     * Adds a spring to the view.
     * @param size The initial size of the spring in pixels.
     * The min size of the spring whatever the views min size it.
     * The max size will be infinite
     * the spring contant will be 1.
     */
    public void addSpring(Spring spring)
    {
        _springs.addElement(spring);
    }

    /**
     * insert a spring to the view.
     * @param size The initial size of the spring in pixels.
     * The min size of the spring whatever the views min size it.
     * The max size will be infinite
     * the spring contant will be 1.
     */
    public void insertSpring(Spring spring, int index)
    {
        _springs.insertElementAt(spring,index);
    }

    /*
     * returns an enumeration of Spring objects
     */
    public Enumeration springs()
    {
        return _springs.elements();
    }

    /**
     * Removes the spring, or strut, that is at the specified index.
     * @param index the index of the view, spring, or strut in this box.
     */
    public Spring removeSpringAt(int index)
    {
        return (Spring)_springs.removeElementAt(index);
    }

    public Spring getSpringAt(int index)
    {
        return (Spring)_springs.elementAt(index);
    }

    public void setSpringAt(int index, Spring spring)
    {
        _springs.setElementAt(spring, index);
    }

    public int count()
    {
        return _springs.size();
    }

    /**
     * Returns the direction this box displays itself.
     * VERTICAL or HORIZONTAL
     */
    public int getDirection()
    {
        return _direction;
    }

    public void setDirection(int direction)
    {
        _direction = direction;
    }

    public void addSubview(View view)
    {
    }

    public void removeSubview(View view)
    {
    }

    /**
     * Lays out all of view's children according to the springs that have been added
     * to this layout manager. For example spring[0] should be attached to child[0] of view
     * spring[1] should be attached to child[1].
     */
    public void layoutView(View mainView,
                           int deltaWidth,
                           int deltaHeight)
    {
        Vector children = mainView.subviews();
        int views = children.size();
        int springs = _springs.size();

        // get the width and height of the view
        int width = mainView.width()-_margin*2;
        int height = mainView.height()-_margin*2;

        // if there is padding between cells subtract it
        if (_direction == HORIZONTAL)
           width -= _childPadding*(views-1);
        else
           height -= _childPadding*(views-1);

        // apply our springs to the views subviews and
        // populate the SpringView with the values so it
        // can do the calculations.
        for (int i=0; i < views && i < springs;i++)
        {
            // get the spring
            Spring spring = (Spring)_springs.elementAt(i);

            // get the view
            View view = (View)children.elementAt(i);

            // get our values for this child            
            Size preferredSize     = getPreferredSizeFor   (view,spring);
            Size minSize           = getMinSizeFor         (view,spring);
            Size maxSize           = getMaxSizeFor         (view,spring);
            float springConstant   = spring.springConstant();

            // add the spring to the spring vector

            com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.Spring vspring = null;

            if (_direction == HORIZONTAL)
            {
               vspring = new com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.Spring(preferredSize.width,
                                                        (float)springConstant,
                                                        minSize.width,
                                                        maxSize.width);
            } else {
               vspring = new com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.Spring(preferredSize.height,
                                                        (float)springConstant,
                                                        minSize.height,
                                                        maxSize.height);
            }

            _springVector.addElement(vspring);

        }

        // alright lets stretch the vector to the size of the view
        if (_direction == HORIZONTAL)
            _springVector.stretch(width);
        else
            _springVector.stretch(height);

        int offset = _margin;

        // layout all of the subview according to what the spring vector
        // tells us
        for (int i=0; i < views && i < springs;i++)
        {
             // get the view
            View view = (View)children.elementAt(i);

            com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.Spring vspring = (com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.Spring)_springVector.elementAt(i);

            if (_direction == HORIZONTAL)
            {
               view.setBounds(offset,_margin, vspring._currentSize,height);
               offset += vspring._currentSize+_childPadding;
            } else {
               view.setBounds(_margin,offset, width,vspring._currentSize);
               offset += vspring._currentSize + _childPadding;
            }
        }

        _springVector.removeAllElements();

    }

    private Size getMinSizeFor(View view, Spring spring)
    {
            Size size = view.minSize();

            // what is its minimum size?
            // ask the view, then apply the springs value
            if (_direction == HORIZONTAL)
            {
               if (spring.isMinSizeSet)
                  size.width = spring.minSize();
            } else {
               if (spring.isMinSizeSet)
                  size.height = spring.minSize();
            }

            return size;
    }

    private Size getPreferredSizeFor(View view, Spring spring)
    {
            Size size = null;

            // if the view is shapeable its easy just ask the
            // the shapeable for its prefered size. Otherwise
            // its prefered size is it min size.
            if (view instanceof Shapeable)
               size = ((Shapeable)view).preferredSize();
            else
               size = view.minSize();


            // apply the spring to the preferred size
            if (_direction == HORIZONTAL)
            {
               if (spring.isPreferredSizeSet)
                  size.width = spring.preferredSize();
            } else {
               if (spring.isPreferredSizeSet)
                  size.height = spring.preferredSize();
            }

            return size;
    }

    private Size getMaxSizeFor(View view, Spring spring)
    {
            Size size = null;

            // if the view is shapeable its easy just ask the
            // the shapeable for its max size.
            if (view instanceof Shapeable)
               size = ((Shapeable)view).maxSize();
            else
               size = new Size(INFINITE, INFINITE);

            // apply the spring to the max size
            if (_direction == HORIZONTAL)
            {
               if (spring.isMaxSizeSet)
                  size.width = spring.maxSize();
             } else {
               if (spring.isMaxSizeSet)
                  size.height = spring.maxSize();
            }

            return size;
    }

    /**
     * Returns the desired size of the given view according to this
     * layout manager.
     */
    public Size preferredSize(View mainView)
    {
        // should return the desired size of the view by looking at its children,
        // and putting them on springs.

        // if the direction is HORIZONTAL then dimention1 is the sum of all of our
        // children's widths and dimention2 is the largest height.

        // if the direction is VERTICAL then dimention2 is the sum of all of our
        // children's heights and dimention2 is the largest width.

        int dimention1 = 0;
        int dimention2 = 0;

        Vector children = mainView.subviews();
        int views = children.size();
        int springs = _springs.size();

        for (int i=0; i < views && i < springs;i++)
        {
            // get the spring
            Spring spring = (Spring)_springs.elementAt(i);

            // get the view
            View view = (View)children.elementAt(i);

            // get our child's size
            Size preferredSize  = getPreferredSizeFor(view,spring);

            // calculate dimention1 and dimention2
            int dimention = 0;
            if (_direction == HORIZONTAL)
            {
               dimention1 += preferredSize.width;
               dimention = preferredSize.height;
            } else {
               dimention1 += preferredSize.height;
               dimention = preferredSize.width;
            }

             // dimention2 should equal the biggest child
            if (dimention > dimention2)
                dimention2 = dimention;
        }

        // add in the cell padding
        dimention1 += _childPadding*(views-1);

        // add in the margin
        dimention1 += _margin;
        dimention2 += _margin;

        // depending on the direction set the dimentions right
        if (_direction == HORIZONTAL)
           return new Size(dimention1,dimention2);
        else
           return new Size(dimention2,dimention1);
    }

    /**
     * Returns the minimum size of the given view according to this
     * layout manager.
     */
    public Size minSize(View mainView)
    {
        // should return the desired size of the view by looking at its children,
        // and putting them on springs.

        // if the direction is HORIZONTAL then dimention1 is the sum of all of our
        // children's widths and dimention2 is the largest height.

        // if the direction is VERTICAL then dimention2 is the sum of all of our
        // children's heights and dimention2 is the largest width.

        int dimention1 = 0;
        int dimention2 = 0;

        Vector children = mainView.subviews();
        int views = children.size();
        int springs = _springs.size();

        for (int i=0; i < views && i < springs;i++)
        {
            // get the spring
            Spring spring = (Spring)_springs.elementAt(i);

            // get the view
            View view = (View)children.elementAt(i);

            Size minSize = getMinSizeFor(view,spring);

            // calculate dimention1 and dimention2
            int dimention = 0;
            if (_direction == HORIZONTAL)
            {
               if (spring.springConstant() == 0.0) {
                   // if the spring is 0.0 meaning is can't be stretched
                   // then the min and max sizes become the same as the preferred
                   // size. We only handle min size here so let do it.

                   Size preferredSize = getPreferredSizeFor(view,spring);
                   dimention1 += preferredSize.width;
               } else {
                   dimention1 += minSize.width;
               }
               
               dimention = minSize.height;
            } else {
               if (spring.springConstant() == 0.0) {
                   // if the spring is 0.0 meaning is can't be stretched
                   // then the min and max sizes become the same as the preferred
                   // size. We only handle min size here so let do it.

                   Size preferredSize = getPreferredSizeFor(view,spring);
                   dimention1 += preferredSize.height;
               } else {
                    dimention1 += minSize.height;
               }

               dimention = minSize.width;
            }

             // dimention2 should equal the biggest child
            if (dimention > dimention2)
                dimention2 = dimention;
        }

        // add in the cell padding
        dimention1 += _childPadding*(views-1);

        // add in the margin
        dimention1 += _margin;
        dimention2 += _margin;

        // depending on the direction set the dimentions right
        if (_direction == HORIZONTAL)
           return new Size(dimention1,dimention2);
        else
           return new Size(dimention2,dimention1);
    }

    /**
     * Returns the maximum size of the given view according to this
     * layout manager.
     */
    public Size maxSize(View mainView)
    {
        // should return the desired size of the view by looking at its children,
        // and putting them on springs.

        // if the direction is HORIZONTAL then dimention1 is the sum of all of our
        // children's widths and dimention2 is the largest height.

        // if the direction is VERTICAL then dimention2 is the sum of all of our
        // children's heights and dimention2 is the largest width.

        int dimention1 = 0;
        int dimention2 = 0;

        Vector children = mainView.subviews();
        int views = children.size();
        int springs = _springs.size();

        for (int i=0; i < views && i < springs;i++)
        {
            // get the spring
            Spring spring = (Spring)_springs.elementAt(i);

            // get the view
            View view = (View)children.elementAt(i);

            // get our child's size
            Size maxSize  = getMaxSizeFor(view,spring);

            // calculate dimention1 and dimention2
            int dimention = 0;
            if (_direction == HORIZONTAL)
            {
               if (dimention1 != Integer.MAX_VALUE)
               {
                  int size = maxSize.width;
                  if (spring.springConstant() == 0.0)
                  {
                      // if the spring is 0.0 meaning is can't be stretched
                      // then the min and max sizes become the same as the preferred
                      // size. We only handle min size here so let do it.

                      Size preferredSize = getPreferredSizeFor(view,spring);
                      size = preferredSize.width;
                  }

                  if (size != Integer.MAX_VALUE)
                        dimention1 += size;
                     else
                        dimention1 = Integer.MAX_VALUE;
               }

               dimention = maxSize.height;
            } else {
              if (dimention1 != Integer.MAX_VALUE)
              {
                  int size = maxSize.height;
                  if (spring.springConstant() == 0.0)
                  {
                      // if the spring is 0.0 meaning is can't be stretched
                      // then the min and max sizes become the same as the preferred
                      // size. We only handle min size here so let do it.

                      Size preferredSize = getPreferredSizeFor(view,spring);
                      size = preferredSize.height;
                  }

                  if (size != Integer.MAX_VALUE)
                        dimention1 += size;
                     else
                        dimention1 = Integer.MAX_VALUE;
               }

               dimention = maxSize.width;
            }

             // dimention2 should equal the biggest child
            if (dimention > dimention2)
                dimention2 = dimention;
        }

        // add in the cell padding
        if (dimention1 < Integer.MAX_VALUE)
        {
           dimention1 += _childPadding*(views-1);
           dimention2 += _margin;
        }

        // add in the margin
        if (dimention2 < Integer.MAX_VALUE)
           dimention2 += _margin;

        // depending on the direction set the dimentions right
        if (_direction == HORIZONTAL)
           return new Size(dimention1,dimention2);
        else
           return new Size(dimention2,dimention1);

    }

    private Vector _springs   = new Vector();
    private int    _direction = HORIZONTAL;
    private com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.SpringVector _springVector = new com.netscape.jsdebugging.ifcui.palomar.widget.layout.math.SpringVector();
    private int _margin = 0;
    private int _childPadding = 0;
}
