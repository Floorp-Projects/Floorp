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