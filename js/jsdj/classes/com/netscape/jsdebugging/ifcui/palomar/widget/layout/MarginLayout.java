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

/**
 * This layout manager is designed to layout a single subview, placing
 * left, right, top, and bottom margins around it. It also understands
 * Shapeable Views. These are views that implement the Shapeable interface
 * allowing them to be querried for preferredSize and maxSize in additon
 * to minSize.
 */
public class MarginLayout implements ShapeableLayoutManager
{

   public MarginLayout()
   {
      _left   = 0;
      _right  = 0;
      _top    = 0;
      _bottom = 0;
   }

   public MarginLayout(int left, int right, int top, int bottom)
   {
      _left   = left;
      _right  = right;
      _top    = top;
      _bottom = bottom;
   }

   public void setLeftMargin(int left)
   {
    _left = left;
   }

   public void setRightMargin(int right)
   {
    _right = right;
   }

   public void setTopMargin(int top)
   {
    _top = top;
   }

   public void setBottomMargin(int bottom)
   {
    _bottom = bottom;
   }

   public int getLeftMargin()
   {
    return _left;
   }

   public int getRightMargin()
   {
    return _right;
   }

   public int getTopMargin()
   {
    return _top;
   }

   public int getBottomMargin()
   {
    return _bottom;
   }



   /**
    * return the minimum size required to layout the view
    */
   private Size sizeFor(View aView, int type)
   {
      Size size = new Size();
    
      Vector views = aView.subviews();
      if (views.size() > 0)
      {
         View view = (View)views.firstElement();
       
         Size vsize = null;
         
         switch (type)
         {
            case MIN_SIZE:
               vsize = view.minSize();
               break;

            case PREFERRED_SIZE:
               if (view instanceof Shapeable)
                  vsize = ((Shapeable)view).preferredSize();
               else
                  vsize = view.minSize();
               break;

            case MAX_SIZE:
               if (view instanceof Shapeable)
                  vsize = ((Shapeable)view).maxSize();
               else
                  return new Size(Integer.MAX_VALUE, Integer.MAX_VALUE);
               break;
         }
         
         if (vsize != null)
         {
            size.width = vsize.width;
            size.height = vsize.height;
         }
      }
            
      if (size.width != Integer.MAX_VALUE)
         size.width += _left+_right;

      if (size.height != Integer.MAX_VALUE)
         size.height += _top+_bottom;

      return size;
   }

   public Size minSize(View aView)
   {
      return sizeFor(aView, MIN_SIZE);
   }

   public Size maxSize(View aView)
   {
      return sizeFor(aView, MAX_SIZE);
   }

   public Size preferredSize(View aView)
   {
      return sizeFor(aView, PREFERRED_SIZE);
   }

   public void addSubview(View aView)
   {
   }

   public void removeSubview(View aView)
   {
   }

   public void layoutView(View aView,
                          int deltaWidth,
                          int deltaHeight)
   {
      Vector views = aView.subviews();
      if (views.size() > 0)
      {
         View view = (View)views.firstElement();

         int width = aView.width();
         int height = aView.height();
          view.setBounds(_left,_top,width - _left - _right, height - _top - _bottom);
      }
   }

   protected int _left      = 0;
   protected int _right     = 0;
   protected int _top       = 0;
   protected int _bottom    = 0;
   private static final int MIN_SIZE      = 0;
   private static final int PREFERRED_SIZE = 1;
   private static final int MAX_SIZE      = 2;
}