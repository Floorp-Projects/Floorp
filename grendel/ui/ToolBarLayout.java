/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This file has been contributed to Mozilla.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Jeff Galyan <jeffrey.galyan@sun.com>, 2 Jan 1999
 */

package grendel.ui;

import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;

import grendel.widgets.Spring;


public class ToolBarLayout extends GridBagLayout {
    static GridBagConstraints gbc = new GridBagConstraints();
    public static final int HORIZONTAL = GridBagConstraints.HORIZONTAL;
    public static final int VERTICAL = GridBagConstraints.VERTICAL;
    static int orientation = GridBagConstraints.HORIZONTAL;
    Insets myInsets = new Insets(5,5,5,5);

    public ToolBarLayout() {
	super();
	defaultConstraints.insets = myInsets;
    }

    public ToolBarLayout(int anOrientation) {
	super();
	orientation = anOrientation;
	defaultConstraints.insets = myInsets;
    }

    public int getGridX() {
	return defaultConstraints.gridx;
    }

    public void setGridX(int x) {
	defaultConstraints.gridx = x;
    }

    public int getGridY() {
	return defaultConstraints.gridy;
    }

    public void setGridY(int y) {
	defaultConstraints.gridy = y;
    }

    public double getWeightX() {
	return defaultConstraints.weightx;
    }

    public void setWeightX(double x) {
	defaultConstraints.weightx = x;
    }

    public double getWeightY() {
	return defaultConstraints.weighty;
    }

    public void setWeightY(double y) {
	defaultConstraints.weighty = y;
    }

    public int getAnchor() {
	return defaultConstraints.anchor;
    }

    public void setAnchor(int anAnchor) {
	defaultConstraints.anchor = anAnchor;
    }

    public Insets getInsets() {
	return defaultConstraints.insets;
    }

    public void setInsets(Insets anInsets) {
       defaultConstraints.insets = anInsets;
    }
    
    public int getIPadX() {
	return defaultConstraints.ipadx;
    }

    public void setIPadX(int x) {
	defaultConstraints.ipadx = x;
    }

    public int getIPadY() {
	return defaultConstraints.ipady;
    }

    public void setIPadY(int y) {
	defaultConstraints.ipady = y;
    }

    public int getGridWidth() {
	return defaultConstraints.gridwidth;
    }
    
    public void setGridWidth(int aGridWidth) {
	defaultConstraints.gridwidth = aGridWidth;
    }
    
    public int getGridHeight() {
	return defaultConstraints.gridheight;
    }

    public void setGridHeight(int aGridHeight) {
	defaultConstraints.gridheight = aGridHeight;
    }
    
    public int getFill() {
	return defaultConstraints.fill;
    }

    public void setFill(int aFill) {
	defaultConstraints.fill = aFill;
    }

    public Spring createSpring() {
	setFill(orientation);
	setWeightX(10.0);
	Spring res = new Spring();

	return res;
    }
}

    
