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

public class Spring
{
    /**
     * A constant that can be used when setting max sizes of springs that
     * represents infinite
     */
    public static final int INFINITE = Integer.MAX_VALUE;

    public void setPreferredSize(int preferredSize)
    {
       _preferredSize = preferredSize;
       isPreferredSizeSet = true;
    }

    public int preferredSize()
    {
       return _preferredSize;
    }

    public void setMinSize(int minSize)
    {
       _minSize = minSize;
       isMinSizeSet = true;
    }

    public int minSize()
    {
       return _minSize;
    }

    public void setMaxSize(int minSize)
    {
       _maxSize = minSize;
       isMaxSizeSet = true;
    }

    public int maxSize()
    {
       return _maxSize;
    }

    public void setSpringConstant(float springConstant)
    {
       _springConstant = springConstant;
    }

    public float springConstant()
    {
       return _springConstant;
    }

    protected int    _preferredSize  = 0;
    protected int    _maxSize        = 0;
    protected int    _minSize        = 0;
    protected float  _springConstant = 0;

    public boolean isPreferredSizeSet = false;
    public boolean isMinSizeSet       = false;
    public boolean isMaxSizeSet       = false;
}