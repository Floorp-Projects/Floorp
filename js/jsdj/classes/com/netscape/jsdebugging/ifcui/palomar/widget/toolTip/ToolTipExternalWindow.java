/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// By Eric D Vaughan

package com.netscape.jsdebugging.ifcui.palomar.widget.toolTip;

import netscape.application.*;
import netscape.util.*;

/**
 * A convience class to create an external window with tool tips in it.
 */
public class ToolTipExternalWindow extends ExternalWindow
{
    /**
     * Overridden to create a special RootView that can intercept
     * mouse events before they are dispatched to there views.
     */
    protected FoundationPanel createPanel()
    {
       FoundationPanel panel = super.createPanel();

       RootView oRootView = new ToolTipRootView();

       panel.setRootView(oRootView);

       return panel;
    }
}

