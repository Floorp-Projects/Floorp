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

// By Eric D Vaughan

package com.netscape.jsdebugging.ifcui.palomar.widget.layout;
import netscape.application.*;
import netscape.util.*;

public class BoxView extends ShapeableView
{
    public static final int VERT = 0;
    public static final int HORIZ = 1;

    /**
     * Creates a box given the direction of the box, either
     * VERT or HORIZ
     */
    public BoxView(int direction)
    {
        setLayoutManager(new BoxSpringLayout(direction));
        setAutoResizeSubviews(true);
    }

    public void setShowBoxesAndSprings(boolean show)
    {
        BoxSpringLayout layout = getBoxSpringLayoutManager();

        // add some margin so we can fit the box in
        if (show == true && _showBoxesAndSprings == false)
            layout.setMargin(layout.getMargin()+2);

        // remove the margin
        if (show == false && _showBoxesAndSprings == true)
            layout.setMargin(layout.getMargin()-2);

        resize();

        _showBoxesAndSprings = show;

        // ask search our children for other boxes
        // and set them as well
        setShowBoxesAndSprings(this,show);
    }

    public boolean getShowBoxesAndSprings()
    {
        return _showBoxesAndSprings;
    }

    private void setShowBoxesAndSprings(View view, boolean show)
    {
        Vector children = view.subviews();
        int size = children.size();
        for (int i=0;i < size; i++)
        {
            View child = (View)children.elementAt(i);
            if (child instanceof BoxView)
               ((BoxView)child).setShowBoxesAndSprings(show);
            else if (child instanceof SpringView)
               ((SpringView)child).setShowSpring(show);
            else
               setShowBoxesAndSprings(child,show);
        }
    }


    /**
     * Adds a view to the box and forces it to be fixed at its preferred size.
     * If the view implements the Shapeable interface then its preferred size and
     * min size will be retrieved from it. If its just a normal
     * view its preferred size will its current width and its max size will be infinite.
     * @param view The view to add to the box.
     */
    public Spring addFixedView(View view)
    {
        // create a spring that won't stetch
        Spring spring = new Spring();
        spring.setSpringConstant(STRUT_CONSTANT);
        addView(view, spring);
        return spring;
    }

    /**
     * Adds a view to the box will a given preferred size and forces it to be flexable.
     * If the view implements the Shapeable interface then its
     * max size will be retrieved from it and its preferred size will
     * be overridden by the given preferred size. If its just a normal
     * view its max size will be infinite.
     * @param view The view to add to the box.
     * @param preferredSize The initial preferred size of the view in pixels.
     */
    public Spring addFixedView(View view, int preferredSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setSpringConstant(STRUT_CONSTANT);
        addView(view, spring);
        return spring;
    }

    /**
     * Adds a view to the box and forces it to be flexable.
     * If the view implements the Shapeable interface then its preferred size,
     * and max size will be retrieved from it it. If its just a normal
     * view its preferred size will be its current width and max size
     * will be infinite.
     * @param view The view to add to the box.
     */
    public Spring addFlexibleView(View view)
    {
        // create a spring that won't stetch
        Spring spring = new Spring();
        spring.setSpringConstant((float)1.0);
        addView(view, spring);
        return spring;
    }

    /**
     * Adds a view to the box with a initial preferred size and forces it to be flexable.
     * If the view implements the Shapeable interface then its
     * max size will be retrieved from it and its preferred size will
     * be overridden by the given preferred size. If its just a normal
     * view its max size will be infinite.
     * @param view The view to add to the box.
     */
    public Spring addFlexibleView(View view, int preferredSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setSpringConstant((float)1.0);
        addView(view, spring);
        return spring;
    }


    /**
     * Adds a view to the box and forces it to be flexible.
     * Its preferred size, min size, and max size will
     * be overridden with the sizes given.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     * @param minSize The minimum size of the view in pixels.
     * @param maxSize The maximum size of the view in pixels.
     */
    public Spring addFlexibleView(View view, int preferredSize, int minSize, int maxSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setMinSize(minSize);
        spring.setMaxSize(maxSize);
        spring.setSpringConstant((float)1.0);
        addView(view, spring);
        return spring;
    }


    /**
     * Adds a view to the box. And uses the given Spring object to determine
     * its properties.
     * @param view The view to add to the box.
     * @param spring The spring to use when laying out this view.
     */
    public Spring addView(View view, Spring spring)
    {
        // if the view is not a shapeable and we haven't set its
        // preferred size then set its preferred size to be whatever
        // its current size is.
        if (view instanceof Shapeable == false && spring.isPreferredSizeSet == false)
        {
            int min = 0;
            int current = 0;

            if (getDirection() == HORIZ) {
               min = view.minSize().width;
               current = view.width();
            } else {
               min = view.minSize().height;
               current = view.height();
            }
        }

        getBoxSpringLayoutManager().addSpring(spring);
        addSubview(view);
        resize();

        // if the view is a SpringView then tell it about the
        // spring so it can draw the spring correctly
        if (view instanceof SpringView)
           ((SpringView)view).setSpring(spring);

        return spring;
    }

    /**
     * Adds a view to the box. Its preferred size, min size, and max size
     * and springConstant will be overridden with the sizes given.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     * @param minSize The minimum size of the view in pixels.
     * @param maxSize The maximum size of the view in pixels.
     * @param springConstant a number between 1.0 and 0.0
     * springConstant is the constant used to layout the view where 1.0 is
     * the most flexable and 0.0 is fixed and won't stretch
     */
    public Spring addView(View view, int preferredSize, int minSize, int maxSize, float springConstant)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setMinSize(minSize);
        spring.setMaxSize(maxSize);
        spring.setSpringConstant(springConstant);
        addView(view, spring);
        return spring;
    }

    /**
     * Inserts a view into the box and forces it to be fixed at its preferred size.
     * If the view implements the Shapeable interface then its preferred size,
     * and max size will be retrieved from it. If its just a normal
     * view its preferred size will be its current size and its max size will be infinite.
     * @param index the index to insert the view at.
     * @param view The view to add to the box.
     */
    public Spring insertFixedView(int index, View view)
    {
        // create a spring that won't stetch
        Spring spring = new Spring();
        spring.setSpringConstant(STRUT_CONSTANT);
        insertView(index, view, spring);
        return spring;
    }

    /**
     * Inserts a view into the box at the specified location with the given preferred size
     * and forces it to be flexable.
     * @param index the index to insert the view at.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     */
    public Spring insertFixedView(int index, View view, int preferredSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setSpringConstant(STRUT_CONSTANT);
        insertView(index, view, spring);
        return spring;
    }

    /**
     * Inserts a view into the box and forces it to be flexable.
     * If the view implements the Shapeable interface then its preferred size
     * and max size will be retrieved from it. If its just a normal
     * view its preferred size will be its current size and its max size
     * will be infinite.
     * @param index the index to insert the view at.
     * @param view The view to add to the box.
     */
    public Spring insertFlexibleView(int index, View view)
    {
        // create a spring that won't stetch
        Spring spring = new Spring();
        spring.setSpringConstant((float)1.0);
        insertView(index, view, spring);
        return spring;
    }

    /**
     * Inserts a view into the box and forces it to be flexible. Its preferred will be
     * overridden by the given preferred size.
     * If the view implements the Shapeable interface then its
     * max size will be retrieved from it. If its just a normal
     * view its max size will be infinite.
     * @param index the index to insert the view at.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     */
    public Spring insertFlexibleView(int index, View view, int preferredSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setSpringConstant((float)1.0);
        insertView(index, view, spring);
        return spring;
    }


    /**
     * Inserts a view into the box. Its preferred size, min size, and max size will
     * be overridden with the sizes given.
     * @param index the index to insert the view at.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     * @param minSize The minimum size of the view in pixels.
     * @param maxSize The maximum size of the view in pixels.
     */
    public Spring insertFlexibleView(int index, View view, int preferredSize, int minSize, int maxSize)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setMinSize(minSize);
        spring.setMaxSize(maxSize);
        spring.setSpringConstant((float)1.0);
        insertView(index, view, spring);
        return spring;
    }

    /**
     * Inserts a view to the box. And uses the given Spring object to determine
     * its properties.
     * @param index the index to insert the view at
     * @param view The view to add to the box.
     * @param spring The spring to use when laying out this view.
     */
    public Spring insertView(int index, View view, Spring spring)
    {
        if (index < 0 || index > subviews().size())
            return null;

        Vector children = subviews();

        if (index == children.size())
           return addView(view,spring);

        // if the view is not a shapeable and we haven't set its
        // preferred size then set its preferred size to be whatever
        // its current size is.
        if (view instanceof Shapeable == false && spring.isPreferredSizeSet == false)
        {
            int min = 0;
            int current = 0;

            if (getDirection() == HORIZ) {
               min = view.minSize().width;
               current = view.width();
            } else {
               min = view.minSize().height;
               current = view.height();
            }
        }

        getBoxSpringLayoutManager().insertSpring(spring,index);

        // add the view into the view system then move it to
        // the correct location.
        addSubview(view);
        children.insertElementAt(view,index);
        children.removeElementAt(children.size()-1);

        // if the view is a SpringView then tell it about the
        // spring so it can draw the spring correctly
        if (view instanceof SpringView)
           ((SpringView)view).setSpring(spring);

        resize();

        return spring;
    }

    /**
     * Adds a view to the box. Its preferred size, min size, and max size
     * and springConstant will be overridden with the sizes given.
     * @param view The view to add to the box.
     * @param preferredSize The initial size of the view in pixels.
     * @param minSize The minimum size of the view in pixels.
     * @param maxSize The maximum size of the view in pixels.
     * @param springConstant a number between 1.0 and 0.0
     * springConstant is the constant used to layout the view where 1.0 is
     * the most flexable and 0.0 is fixed and won't stretch
     */
    public Spring insertView(int index, View view, int preferredSize, int minSize, int maxSize, float springConstant)
    {
        Spring spring = new Spring();
        spring.setPreferredSize(preferredSize);
        spring.setMinSize(minSize);
        spring.setMaxSize(maxSize);
        spring.setSpringConstant(springConstant);
        insertView(index, view, spring);
        return spring;
    }


   /**
     * Adds a spring to the view. This is essentually the same as
     * addView(new View()) except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * @param spring the spring to use
     */
    public Spring addSpring(Spring spring)
    {
        return addView(new SpringView(getDirection(),_showBoxesAndSprings), spring);
    }

    /**
     * Adds a spring to the view. This is essentually the same as
     * addFlexibleView(new View(),preferredSize) except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * If you have several views and want one to just eat up space add one
     * of these to it.
     * @param preferredSize the initial size of the spring
     */
    public Spring addSpring(int preferredSize)
    {
        return addFlexibleView(new SpringView(getDirection(),_showBoxesAndSprings), preferredSize, 0, Integer.MAX_VALUE);
    }

    /**
     * Adds a spring to the view. This is essentually the same as
     * addFlexibleView(new View(),preferredSize) except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * If you have several views and want one to just eat up space add one
     * of these to it.
     */
    public Spring addSpring()
    {
        return addFlexibleView(new SpringView(getDirection(),_showBoxesAndSprings), 0, 0, Integer.MAX_VALUE);
    }

    /**
     * Adds a spring to the view. This is essentually the same as
     * addFlexibleView(new View(),preferredSize) except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * If you have several views and want one to just eat up space add one
     * of these to it.
     * @param springConstant the spring constant for this spring between 0.0 and 1.0
     */
    public Spring addSpring(float springConstant)
    {
        return addView(new SpringView(getDirection(),_showBoxesAndSprings), 0, 0, Integer.MAX_VALUE, springConstant);
    }

    /**
     * Adds a spring to the view. This is essentually the same as
     * addFlexibleView(new View(),preferredSize) except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * If you have several views and want one to just eat up space add one
     * of these to it.
     * @param preferredSize the initial size of the spring
     * @param springConstant the spring constant for this spring between 0.0 and 1.0
     */
    public Spring addSpring(int preferredSize, float springConstant)
    {
        return addView(new SpringView(getDirection(),_showBoxesAndSprings), preferredSize, 0, Integer.MAX_VALUE, springConstant);
    }


    /**
     * Adds a spring to the view. This is essentually the same as
     * addFlexibleView(new View(),preferredSize,minSize,maxSize)
     * except the View added knows how
     * to draw a spring when showBoxesAndSprings is set to true.
     * If you have several views and want one to just eat up space add one
     * of these to it.
     * @param preferredSize The initial size of the spring in pixels.
     * @param minSize The minimum size of the spring in pixels.
     * @param maxSize The maximum size of the spring in pixels.
     */
    public Spring addSpring(int preferredSize, int minSize, int maxSize)
    {
        return addFlexibleView(new SpringView(getDirection(),_showBoxesAndSprings), preferredSize, minSize, maxSize);
    }

    /**
     * Adds a strut to the view. A Strut is really just a simple case
     * of a spring whose springConstant is 0 making it not stretchable.
     * Struts also know how to draw a "Strut" when showBoxesAndSprings is set to true.
     * Use these to place fixed padding between views.
     * @param preferredSize The initial size of the spring in pixels.
     */
    public Spring addStrut(int preferredSize)
    {
        return addFixedView(new SpringView(getDirection(),_showBoxesAndSprings),preferredSize);
    }

    /**
     * Removes the view, spring, or strut, that is at the specified index.
     * @param index the index of the view, spring, or strut in this box.
     */
    public Spring removeElementAt(int index)
    {
        View view = (View)subviews().elementAt(index);
        Spring spring = getBoxSpringLayoutManager().removeSpringAt(index);
        removeSubview(view);
        return spring;
    }

    /**
     * Removes the view, spring, or strut.
     * @param index the index of the view, spring, or strut in this box.
     */
    public Spring removeView(View view)
    {
        int index = indexOfView(view);
        if (index == -1)
            return null;

        Spring spring = getBoxSpringLayoutManager().removeSpringAt(index);
        removeSubview(view);
        resize();

        return spring;
    }

    public int indexOfView(View view)
    {
        return subviews().indexOf(view);
    }

    /**
     * Sets the view to be displayed at the given index. If the index is that of
     * a Spring or Strut the view will replace it but it will keep the spring's or
     * strut's properties.
     * @param index the index of the view, spring, or strut in this box.
     * @param view the view to replace with the old on with
     *
     * Returns the old view.
     *
     */
    public View setView(int index, View view)
    {
        Vector children = subviews();

        if (index < 0 || index >=children.size())
           return null;

        // get the old view

        View oldView = (View)children.elementAt(index);

        // if its the same as the new one then we are done
        if (oldView == view)
            return oldView;

        // remove the old one--this method recommended by Kloba, 3/30/97, and it fixes
	    // the non-redrawing TextView bug in Palomar Connection Builder - sgrennan
	    if (oldView != null)
            oldView.removeFromSuperview();

        // add the new one
        addSubview(view);

        // move the new one to where the old one was
        children.insertElementAt(view,index);
        children.removeElementAt(children.size()-1);

        if (view instanceof SpringView)
           ((SpringView)view).setSpring(getBoxSpringLayoutManager().getSpringAt(index));

        resize();

        return oldView;
    }

    public void removeAll()
    {
        Vector children = subviews();

        while(children.size() > 0)
           removeElementAt(0);

    }

    /**
     * Replaces the spring used to layout the view at index with
     * a new one.
     */
    public void setSpringAt(int index, Spring spring)
    {
        // set the spring one the layout manager
        getBoxSpringLayoutManager().setSpringAt(index, spring);

        // if the view is a SpringView lets tell the
        // spring view about it so it can display the
        // spring correctly
        View view = (View)subviews().elementAt(index);

        if (view instanceof SpringView)
           ((SpringView)view).setSpring(spring);

        resize();

    }

    /**
     * Returns the View that is displayed at the given index. If it is a
     * Spring or Strut it will return the emply view used to display it.
     * @param index the index of the view, spring, or strut in this box.
     */
    public View getViewAt(int index)
    {
        return (View)subviews().elementAt(index);
    }

    /**
     * Returns the element at the given index. This is the spring object that
     * controls the shape of the view at the given index.
     * @param index the index of the view, spring, or strut in this box.
     */
    public Spring getElementAt(int index)
    {
        return getBoxSpringLayoutManager().getSpringAt(index);
    }

    /**
     * Returns the direction this box displays itself.
     * VERT or HORIZ
     */
    public int getDirection()
    {
        return getBoxSpringLayoutManager().getDirection();
    }

    /**
     * Sets the direct this box lays out its views VERT or HORIZ
     */
    public void setDirection(int direction)
    {
       // set the layouts direction
       getBoxSpringLayoutManager().setDirection(direction);

       // if we have and SpringViews flip there direction
       // so they draw correctly.
       Vector children = subviews();
       int size = children.size();

       for (int i = 0; i < size; i++)
       {
          View view = (View)children.elementAt(i);
          if (view instanceof SpringView)
             ((SpringView)view).setDirection(direction);
       }

        resize();

    }

    /**
     * Returns all the views in he box
     */
    public Enumeration views()
    {
        return subviews().elements();
    }

    public Enumeration springs()
    {
        return getBoxSpringLayoutManager().springs();
    }

    /**
     * Returns the number of views in the box
     */
    public int count()
    {
        return subviews().size();
    }

    public void drawSubviews(Graphics g)
    {
        super.drawSubviews(g);

        int arrowHeadSize = 4;
        int halfArrowHeadSize = arrowHeadSize/2;

        if (_showBoxesAndSprings)
        {
           if (getDirection() == HORIZ)
              g.setColor(Color.blue);
           else
              g.setColor(Color.red);

           g.drawRect(1,1,width()-2,height()-2);
        }
    }

    public Size minSize()
    {
        if (_minSize == null)
            _minSize = super.minSize();

        return _minSize;
    }

    public Size preferredSize()
    {
        if (_preferredSize == null)
            _preferredSize = super.preferredSize();

        return _preferredSize;
    }

    public Size maxSize()
    {
        if (_maxSize == null)
            _maxSize = super.maxSize();

        return _maxSize;
    }

    /**
     * Tells the the box to reevaluate its size the next time its
     * asked for it or its is layed out
     */
    public void resize()
    {
        _minSize = null;
        _maxSize = null;
        _preferredSize = null;
    }

    /**
     * Returns the BoxSpringLayout used by us.
     * Is now public so I can set the childpadding, etc...
     */
    public BoxSpringLayout getBoxSpringLayoutManager()
    {
       return (BoxSpringLayout)layoutManager();
    }

    public static float STRUT_CONSTANT = (float)0.0;
    private boolean _showBoxesAndSprings = false;
    private Size _minSize        = null;
    private Size _maxSize        = null;
    private Size _preferredSize  = null;
}

/**
 * Helper class to draw springs and struts when showBoxesAndSprings is true
 */
class SpringView extends View
{
    public static final int VERT   = 0;
    public static final int HORIZ = 1;

    public SpringView(int direction, boolean showSpring)
    {
         _showSpring = showSpring;
         _direction = direction;
    }

    public void setShowSpring(boolean show)
    {
        _showSpring = show;
        setDirty(true);
    }

    public void setDirection(int direction)
    {
        _direction = direction;
    }

    public int getDirection()
    {
        return _direction;
    }

    public void setSpring(Spring spring)
    {
        _spring = spring;
    }

    public Spring getSpring()
    {
        return _spring;
    }

    public void drawView(Graphics g)
    {
        // see if we need to show the springs
        if (_showSpring == false)
           return;

         if (_spring.springConstant() <= BoxView.STRUT_CONSTANT)
            drawStrut(g);
         else
            drawSpring(g);
    }

    public void drawStrut(Graphics g)
    {
        if (_direction == HORIZ)
        {
               g.setColor(Color.black);
               g.drawLine(0, height()/2+1, width(), height()/2+1);
               g.setColor(Color.white);
               g.drawLine(0, height()/2, width(), height()/2);
        } else {
               g.setColor(Color.black);
               g.drawLine(width()/2+1, 0, width()/2+1, height() );
               g.setColor(Color.white);
               g.drawLine(width()/2, 0, width()/2, height());
        }
     }

    public void drawSpring(Graphics g)
    {
        // if we do draw the coils
        int distance = 0;
        int center = 0;

        if (_direction == HORIZ)
        {
           distance = width();
           center = height()/2;
        } else {
           distance = height();
           center = width()/2;
        }

        int coilSize = 8;
        int coils = distance/coilSize;

        int halfCoilSize = coilSize/2;
        int offset = 0;

        for (int i=0; i < coils+1; i++)
        {
            if (_direction == HORIZ)
            {
               g.setColor(Color.black);
               g.drawLine(offset, center+2, offset+halfCoilSize, center-2);
               g.setColor(Color.white);
               g.drawLine(offset+halfCoilSize, center-2, offset+coilSize, center+2);
            } else {
               g.setColor(Color.black);
               g.drawLine(center+2, offset, center-2, offset+halfCoilSize);
               g.setColor(Color.white);
               g.drawLine(center-2, offset+halfCoilSize, center+2, offset+coilSize);
            }

            offset += coilSize;
        }
    }

    private int _direction = HORIZ;
    private Spring _spring;
    private boolean _showSpring = false;
}
