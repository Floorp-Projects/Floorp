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

package com.netscape.jsdebugging.ifcui.palomar.widget;

import netscape.application.*;
import java.awt.image.*;

public class HighlightingImageFilter extends RGBImageFilter
{
   public HighlightingImageFilter() {
                // The filter's operation does not depend on the
                // pixel's location, so IndexColorModels can be
                // filtered directly.
                canFilterIndexColorModel = true;
   }

   public int filterRGB(int x, int y, int rgb)
   {
        // this routine should be faster if I didn't use colors
        // I could just do it with shifts but I don't want to figure it
        // out yet.

                int pixel = rgb;

                int alpha = (pixel >> 24) & 0xff;
                int red   = (pixel >> 16) & 0xff;
                int green = (pixel >> 8) & 0xff;
                int blue  = pixel & 0xff;

                //  if there is any alpha just leave the color as it is

                if (alpha == 0) {
                   return rgb;
                } else {
                   // lighten
                   if (red + green + blue > 96*3)
                   {
                    /*
                      red = Math.min((int)(red  *(1/(FACTOR))), 255);
                      green = Math.min((int)(green*(1/FACTOR)), 255);
			          blue = Math.min((int)(blue*(1/FACTOR)), 255);
			          */

			          red = Math.min(red + 32,255);
			          green = Math.min(green + 32,255);
			          blue = Math.min(blue + 32,255);

                   } else { // darken
                   /*
                      red = Math.min((int)(red*FACTOR), 255);
                      green = Math.min((int)(green*FACTOR), 255);
			          blue = Math.min((int)(blue*FACTOR), 255);
			          */

			          red = red/2;
			          green = green/2;
			          blue = blue/2;
                   }

                   rgb = ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | ((blue & 0xFF) << 0);
			       return 0xff000000 | rgb;
                }
   }

    private static final double FACTOR = 0.7;

}
