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
