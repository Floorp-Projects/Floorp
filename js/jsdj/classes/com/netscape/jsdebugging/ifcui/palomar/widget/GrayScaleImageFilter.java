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

public class GrayScaleImageFilter extends RGBImageFilter
{
   public GrayScaleImageFilter() {
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

                Color c = null;

                //  if there is any alpha just leave the color as it is

                if (alpha == 0) {
                   return rgb;
                } else {
                   // otherwise take the average of the rgb components and
                   // use that as the color
                   int a = (red + green + blue)/3;
                   rgb = ((a & 0xFF) << 16) | ((a & 0xFF) << 8) | ((a & 0xFF) << 0);

                   return 0xff000000 | rgb;
                }
   }

    private static final double FACTOR = 0.7;

}
