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
