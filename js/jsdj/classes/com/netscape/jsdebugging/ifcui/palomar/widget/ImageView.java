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

package com.netscape.jsdebugging.ifcui.palomar.widget;

import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import netscape.application.*;

public class ImageView extends View implements Shapeable
{
    public final static int ALIGN_LEFT = 0;
    public final static int ALIGN_RIGHT = 1;
    
    public final static int ALIGN_TOP = 0;
    public final static int ALIGN_BOTTOM = 1;
    
    public final static int ALIGN_CENTER = 2;
    public final static int ALIGN_SCALED = 3;
       
    public ImageView()
    {
        this(null);
    }

    public ImageView(Image image)
    {
        setImage(image);
    }

    public void setImage(Image image)
    {
        _image = image;
        calculateMinSize();
    }
    
    private void calculateMinSize()
    {
       int width = 0;
       int height = 0;
       
       if (_image != null)
       {
         if (halign != ALIGN_SCALED)
            width = _image.width();
            
         if (valign != ALIGN_SCALED)
            height = _image.height();
       }
       
       setMinSize(width, height);
    }

    public Image getImage()
    {
        return _image;
    }

    public void drawView(Graphics g)
    {
        if (_image != null) {
            
            int x = 0;
            int y = 0;
            int width = _image.width();            
            int height = _image.height();            
            switch (halign)
            {
                case ALIGN_CENTER:
                    x = width()/2-_image.width()/2;
                break;
                case ALIGN_LEFT:
                    x = 0;
                break;
                case ALIGN_RIGHT:
                    x = bounds.width - _image.width();
                break;
                case ALIGN_SCALED:
                    x = 0;
                    width = bounds.width;
                break;
                
            }
            
            switch (valign)
            {
                case ALIGN_CENTER:
                    y = height()/2-_image.height()/2;
                break;
                case ALIGN_TOP:
                    y = 0;
                break;
                case ALIGN_BOTTOM:
                    y = bounds.height - _image.height();
                break;
                case ALIGN_SCALED:
                    y = 0;
                    height = bounds.height;
                break;
            }
            
            _image.drawScaled(g,x,y,width,height);
        }
    }

    public Size preferredSize()
    {            
        if (_image != null)
            return new Size(_image.width(), _image.height());
        else
            return minSize();
    }
   
    public Size maxSize()
    {  
       return new Size(9999,9999);
    }
    
    public void setVerticalAlignment(int align)
    {
        valign = align;
        calculateMinSize();
    }
    
    public void setHorizontalAlignment(int align)
    {
        halign = align;
        calculateMinSize();
    }
    
    private Image _image;
    private int valign = ALIGN_CENTER; 
    private int halign = ALIGN_CENTER; 
}