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

// By Don Tilman

package com.netscape.jsdebugging.ifcui.palomar.widget.layout.math;

import java.util.*;
import java.lang.Math;

/**
  * A Vector of zero or more springs.
  *
  * This class extends Vector so you can use all those methods directly
  * on it.
  *
  **/
public class SpringVector extends Vector {
    /**
      * Stretch the SpringVector to a new length.  The _currentSize of each
      * individual Spring will be set to the new value.
      *
      **/
    public void stretch (int size) {
        int nSprings = size();
        // Set sizeRemaining, springConstantsRemaining, validSize's.
        int sizeRemaining = size;
        float springConstantsRemaining = (float) 0.0;
        for (int i=0; i<nSprings; i++) {
            Spring thisSpring = (Spring) elementAt(i);
            thisSpring._sizeValid = false;
            if (thisSpring._springConstant == 0.0) 
            {
                thisSpring._springConstant = (float)0.001;
                thisSpring._maxSize = thisSpring._size;
                thisSpring._minSize = thisSpring._size;
            }
            springConstantsRemaining += thisSpring._springConstant;
            sizeRemaining -= thisSpring._size;
        }
        // Handle springs that limited by the min/max sizes.
        boolean limit = true;
        for (int pass=1; limit; pass++) {
            limit = false;
            for (int i=0; i<nSprings; i++) {
                Spring thisSpring=(Spring) elementAt(i);
                if (thisSpring._sizeValid==false) {
                    int newSize = thisSpring._size + Math.round(sizeRemaining*(thisSpring._springConstant/springConstantsRemaining));
                    if (newSize<=thisSpring._minSize) {
                        thisSpring._currentSize = thisSpring._minSize;
                        springConstantsRemaining -= thisSpring._springConstant;
                        sizeRemaining -= thisSpring._minSize-thisSpring._size;
                        thisSpring._sizeValid = true;
                        limit = true;
                    }
                    else if (newSize>=thisSpring._maxSize) {
                        //System.out.println("MaxSize hit");
                        //System.out.println(thisSpring._maxSize);
                        thisSpring._currentSize = thisSpring._maxSize;
                        springConstantsRemaining -= thisSpring._springConstant;
                        sizeRemaining -= thisSpring._minSize-thisSpring._size;
                        thisSpring._sizeValid = true;
                        limit = true;
                    }
                }
            }
        }
        // Okay, now the regular springs.
        float stretchFactor = sizeRemaining/springConstantsRemaining;
        for (int i=0; i<nSprings; i++) {
            Spring thisSpring=(Spring) elementAt(i);
            if (thisSpring._sizeValid==false) {
                thisSpring._currentSize = thisSpring._size + Math.round(thisSpring._springConstant*stretchFactor);
                thisSpring._sizeValid = true;
            }
        }
     }
  }
