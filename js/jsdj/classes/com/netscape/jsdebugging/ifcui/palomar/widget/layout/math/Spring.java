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

/**
  * The length is the natural, unstressed length of the Spring.
  * 0 by default.
  *
  * The springConstant is in distance/force.  Thus a Spring with a
  * springConstant of 0.0 is a strut; it doesn't move.
  * The springConstant is 0.0 by default.
  *
  * currentLength is set by the SpringVector to the final value.
  *
  * Needless to say, if specified, minLength should be less than maxLength.
  * (I don't bother to check, the results could be fun.)
  * Interestingly enough, length doesn't necessarily have to be between
  * minLength and maxLength.
  **/
 public class Spring {
    public int _size = 0;
    public float _springConstant = (float) 0.0;
    public int _minSize;
    public int _maxSize;
    public int _currentSize;
    boolean _sizeValid = false;
    
    public Spring(int size, float springConstant, int minSize, int maxSize){
        _size = size;
        _springConstant = springConstant;
        _minSize = minSize;
        _maxSize = maxSize;
 }
}

