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

package com.netscape.jsdebugging.ifcui.palomar.widget.toolTip;

import netscape.application.*;
import netscape.util.*;

/**
 * When inserted as the RootView this class places tool tips above
 * and view that have been registered as having tool tips.
 * See the example ToolTipTest for a good example of how to use this
 * class.
 * Please take notice that when this class is used in an applet it requires
 * a modification the NetscapeApplet.java.
 */
public class ToolTipRootView extends RootView
{
   public ToolTipRootView()
   {
   }

   /**
    * Returns the main root view for the current application.
    * Use this method to get the ToolTipRootView whenever needed. But
    * make sure you have replaced the old root view with ToolTipRootView
    * or this method will return null.
    */
   public static ToolTipRootView mainToolTipRootView()
   {
       RootView main = Application.application().mainRootView();
       if (main instanceof ToolTipRootView)
          return (ToolTipRootView)main;
       else
          return null;
   }

   /**
    * Overloaded to intercept top level mouse events
    */
   public void processEvent(Event oEvent)
   {
      if (_tips.size() > 0)
      {
         if (oEvent instanceof MouseEvent)
         {
           MouseEvent oMouseEvent = (MouseEvent)oEvent;

           if (oMouseEvent.type() == MouseEvent.MOUSE_ENTERED)
             mouseEntered();
           else if (oMouseEvent.type() == MouseEvent.MOUSE_MOVED)
             mouseMoved(oMouseEvent.x, oMouseEvent.y);
           else if (oMouseEvent.type() == MouseEvent.MOUSE_DRAGGED)
             mouseDragged(oMouseEvent.x, oMouseEvent.y);
           else if (oMouseEvent.type() == MouseEvent.MOUSE_EXITED)
             mouseExited();
           else {
              if (_showingTip == true)
                 hideTip();
           }

         } else if (oEvent instanceof KeyEvent) {
            if (_showingTip == true)
               hideTip();
         }
      }

//      System.out.println(oEvent);
      super.processEvent(oEvent);
   }



   /**
    * Set the font for the tip
    */
   public void setTipFont(Font font)
   {
     _tipView.setFont(font);
   }

   public Font getTipFont(Font font)
   {
     return _tipView.font();
   }

   /**
    * How look it take before the tip appears. The default is 1000 ms
    */
   public void setTipDelay(int ms)
   {
      _delay = ms;
      if (_timer != null)
        _timer.setDelay(_delay);
   }

   public void setTipBackgroundColor(Color color)
   {
      _tipView.setBackgroundColor(color);
   }

   public Color getTipBackgroundColor()
   {
      return _tipView.backgroundColor();
   }

   public void setTipColor(Color color)
   {
      _tipView.setTextColor(color);
   }

   public Color getTipColor()
   {
      return _tipView.textColor();
   }


   public int getDelay()
   {
      return _delay;
   }

    /**
     * Adds a view that is to have a tip appear when the mouse
     * is moved over it.
     */
    public void addTip(View oView, String sTip)
    {
        _tips.put(oView, sTip);
    }

    /**
     * Removes a view that is to have a tip appear when the mouse
     * is moved over it.
     */
    public void removeTip(View oView)
    {
       _tips.remove(oView);
    }

    public void printToolTips()
    {
        System.out.println(_tips.size()+" tips:");
        Enumeration e = _tips.elements();
        while(e.hasMoreElements())
        {
            String s = (String)e.nextElement();
            System.out.println(s);
        }
    }

    private void createToolTip()
    {
       _tipWindow = new InternalWindow();
       _tipWindow.setLayer(InternalWindow.POPUP_LAYER);
       _tipWindow.setType(Window.BLANK_TYPE);
       _tipView.setBackgroundColor(new Color(255,255,231));
       _tipView.setBorder(LineBorder.blackLine());
       _tipWindow.contentView().addSubview(_tipView);
    }

    /**
     * Called when the manager is to be started. This is when the mouse first
     * moves into the tool tip managers window.
     */
    public void mouseEntered()
    {
       //System.out.println("Entered");
       startTimer();
    }

    /**
     * Called whenever the mouse moves
     */
    public void mouseMoved(int x, int y)
    {
       if (x != _x || y != _y)
       {
          //System.out.println("Moving: "+ new Point(x,y));
          // if the tip is showing hide it
          if (_showingTip == true)
              hideTip();

          _x = x;
          _y = y;

          startTimer();
       }
    }

    /**
     * Called whenever the mouse moves
     */
    public void mouseDragged(int x, int y)
    {
       if (_showingTip == true)
           hideTip();

       stopTimer();
    }


    /**
     * Called when the mouse leaves the ToolTip manager window
     */
    public void mouseExited()
    {
       //System.out.println("Exited");

       // if the tip is showing then hide it
       if (_showingTip == true)
           hideTip();

       // stop the timer if it is ready to go
       stopTimer();
    }

    private void startTimer()
    {
      //System.out.println("Timer started");
      if (_timer == null) {
         _timer = new Timer(new ToolTipTarget(this), "show tip", _delay);
         _timer.setRepeats(false);
      }
      _timer.start();
    }

    private void stopTimer()
    {
      if (_timer != null)
         _timer.stop();
    }

    public String getTipForMouse(int x, int y)
    {
        String tip = null;

        Vector windows = internalWindows();
        int size = windows.size();
        Point p = new Point();

        for (int i=size-1; i >= 0; i--)
        {
            InternalWindow nextWindow = (InternalWindow)windows.elementAt(i);
            //System.out.println(nextWindow);
            convertToView(nextWindow, x,y, p);
            if (nextWindow.containsPoint(p.x , p.y))
            {
               //System.out.println("inside "+nextWindow);
               return getTipFromSubviews(nextWindow, x,y);
            }
        }

        if (tip == null)
           tip = getTipFromSubviews(this,x,y);

        return tip;
    }

    /**
     * Given a view and a mouse location find the deepest view with a tip
     * and return it.
     */
    private String getTipFromSubviews(View oTopView, int x, int y)
    {
       //System.out.println("looking at: " + oTopView);
       // is the point inside the View?
       if (oTopView.bounds().contains(x,y) == false)
          return null;

       x -= oTopView.x();
       y -= oTopView.y();

       String sTip = null;

       // get the the views subviews. Iterate through them, recursively
       // calling ourselves until we find the deepest subview.
       Vector oViews = oTopView.subviews();

       if (oViews != null)
       {
          int length = oViews.size();

          for (int i = length - 1; i >= 0; i--)
          {
             View oChild = (View)oViews.elementAt(i);

             // we already looked through the internal windows
             // don't do them again
             if (oChild instanceof InternalWindow)
                 continue;

             //Point p = oTopView.convertToView(oChild,x,y);
             sTip = getTipFromSubviews(oChild, x,y);
             if (sTip != null)
                return sTip;
          }
       }

       // looks like none of our children have tips. So see if we have
       // a tip. If we do return it.
       sTip = (String)_tips.get(oTopView);
       if (sTip != null)
          return sTip;

       return null;
    }

    /**
     * Shows the tip as a bubble under the mouse
     */
    public void showTip()
    {
       //System.out.println("Showing tip");

       // stop the timer
       _timer.stop();

       // get the deepest tip
       String sTip = getTipForMouse(_x, _y);

       // if we get one then display it
       if (sTip != null) {
          // Only show the tip if it is not empty.
          // Empty tips can be used by a child view to
          // prevent its parent's tip from bieng shown when
          // the mouse is inside it.
          if (sTip.equals(""))
             return;

          if (_tipWindow == null)
             createToolTip();



          _tipView.setStringValue(sTip);

          Size s = _tipView.minSize();
          _tipView.sizeTo(s.width, s.height);
          s =_tipWindow.windowSizeForContentSize(s.width,
                                                  s.height);
          _tipWindow.sizeTo(s.width,s.height);


          // make sure it isn't off the right side
          if (_x+s.width > rootView().width())
             _x -= (_x+s.width) - rootView().width();

          // place the tip under the pointer. If there is no space
          // place it at the top.
          int ydiff = 0;
          if (_y+s.height > rootView().height())
             ydiff = -20;
          else
             ydiff = 20;

          _tipWindow.moveTo(_x, _y + ydiff);
          _tipWindow.show();
          _showingTip = true;
       }
    }

    public void hideTip()
    {
        _tipWindow.hide();
        _showingTip = false;
    }

    private Timer             _timer;
    private int               _delay = 1000;
    private Hashtable         _tips = new Hashtable();
    private int               _x = 0;
    private int               _y = 0;
    private TextField         _tipView = new TextField();
    private InternalWindow    _tipWindow;
    private boolean           _showingTip = false;
}

/**
 * Class that receives timer messages and tells the Manager
 * to pop up its tip.
 */
class ToolTipTarget implements Target
{
    public ToolTipTarget(ToolTipRootView oManager)
    {
        _manager = oManager;
    }

    /**
     * Called when it is time to display the tip
     */
    public void performCommand(String command, Object data)
    {
      //System.out.println("Timer happened!");

      if (command.equals("show tip"))
         _manager.showTip();
    }

    private ToolTipRootView _manager;
}
