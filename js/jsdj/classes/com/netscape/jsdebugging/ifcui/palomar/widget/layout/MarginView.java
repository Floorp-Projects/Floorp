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

/**
 * A convience class that places a margin around its subview.
 */
public class MarginView extends ShapeableView
{
    public MarginView()
    {
        this(0,0,0,0);
    }
    
    public MarginView(int left, int right, int top, int bottom)
    {
        setLayoutManager(new MarginLayout(left,right,top,bottom));           
        setAutoResizeSubviews(true);
    }
       
    public void setLeftMargin(int size)
    {
       ((MarginLayout)layoutManager()).setLeftMargin(size);
    }
    
    public void setRightMargin(int size)
    {
       ((MarginLayout)layoutManager()).setRightMargin(size);
    }
    
    public void setTopMargin(int size)
    {
       ((MarginLayout)layoutManager()).setTopMargin(size);
    }
    
    public void setBottomMargin(int size)
    {
       ((MarginLayout)layoutManager()).setBottomMargin(size);
    }
    
    public int getLeftMargin()
    {
        return ((MarginLayout)layoutManager()).getLeftMargin();
    }
    
    public int getRightMargin()
    {
        return ((MarginLayout)layoutManager()).getRightMargin();
    }
    
    public int getTopMargin()
    {
        return ((MarginLayout)layoutManager()).getTopMargin();
    }
    
    public int getBottomMargin()
    {
        return ((MarginLayout)layoutManager()).getBottomMargin();
    }
    
   
        /*
    protected MarginLayout getMarginLayoutManager()
    {
        return (MarginLayout)layoutManager();
    }
    */
}

