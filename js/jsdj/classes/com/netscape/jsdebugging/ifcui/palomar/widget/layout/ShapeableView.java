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

public class ShapeableView extends View implements Shapeable
{
    public Size preferredSize()
    {
        LayoutManager manager = layoutManager();
        if (manager != null)
        {
            if (manager instanceof ShapeableLayoutManager)
            {
                ShapeableLayoutManager smanager = (ShapeableLayoutManager)manager;
                return smanager.preferredSize(this);
            }
        }

        return minSize();
    }

    public Size minSize()
    {
        LayoutManager manager = layoutManager();
        if (manager != null)
        {
            if (manager instanceof ShapeableLayoutManager)
            {
                ShapeableLayoutManager smanager = (ShapeableLayoutManager)manager;
                return smanager.minSize(this);
            }
        }

        return super.minSize();
    }


    public Size maxSize()
    {
        LayoutManager manager = layoutManager();
        if (manager != null)
        {
            if (manager instanceof ShapeableLayoutManager)
            {
                ShapeableLayoutManager smanager = (ShapeableLayoutManager)manager;
                return smanager.maxSize(this);
            }
        }

        return new Size(Integer.MAX_VALUE, Integer.MAX_VALUE);
    }

    /**
     * If this view has a shapeable layout manager return it
     * otherwise return null.
     */
    public ShapeableLayoutManager getShapeableLayoutManager()
    {
        LayoutManager manager = layoutManager();
        if (manager != null)
        {
           if (manager instanceof ShapeableLayoutManager)
              return (ShapeableLayoutManager)manager;
        }

        return null;
    }
}
