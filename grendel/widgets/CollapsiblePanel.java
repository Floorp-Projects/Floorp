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
 * Created: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.widgets;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.Insets;

import javax.swing.Box;
import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JPanel;
import javax.swing.border.Border;
import javax.swing.plaf.metal.MetalIconFactory;
import javax.swing.ImageIcon;

import grendel.ui.ToolBarLayout;
import grendel.widgets.Animation;
import grendel.widgets.Collapsible;
import grendel.widgets.Spring;

public class CollapsiblePanel extends JPanel implements Collapsible {
    
    private boolean collapsed = false;
    private boolean collapsible = true;
    private Component myComponents[];
    private int componentCount;
    private final int VERTICAL = 0;
    private final int HORIZONTAL = 1;
    private GridBagConstraints constraints;
    private Box aBox;
    int width = 10, height = 50;
    

    private ToolBarLayout layout;
    
    public CollapsiblePanel(boolean isDoubleBuffered) {
	super(isDoubleBuffered);
        
        constraints = new GridBagConstraints();
        constraints.ipadx = 0;
        constraints.ipady = 0;
       
	layout = new ToolBarLayout();
        layout.setInsets(new Insets(0,0,0,0));
        layout.setIPadX(0);
        layout.setIPadY(0);
	setLayout(layout);
        constraints.insets = new Insets(0,0,0,0);
        constraints.anchor = GridBagConstraints.NORTHWEST;
        constraints.fill = GridBagConstraints.NONE;
        
        CollapseButton collapseButton = new CollapseButton(VERTICAL);
	add(collapseButton, constraints);
        Dimension dim = collapseButton.getSize();
        collapseButton.reshape(0,0,dim.width,dim.height);

        //    constraints.insets = new Insets(5,5,5,5);
        constraints.anchor = GridBagConstraints.WEST;
	collapsed = false;
        revalidate();
    }


    public void collapse() {

	GridBagConstraints constraints = new GridBagConstraints();

	Component myComponents2[] = getComponents();
	componentCount = getComponentCount();
	myComponents = new Component[componentCount];

	for (int i = 0; i < componentCount; i++) {
	    myComponents[i] = myComponents2[i];
	    remove(myComponents2[i]);
	}
	constraints.anchor = GridBagConstraints.NORTHWEST;
	constraints.fill = GridBagConstraints.NONE;

	add(new CollapseButton(HORIZONTAL), constraints);
	Dimension dim2 = getSize();
	setSize(new Dimension(dim2.width, 5));

	constraints.fill = GridBagConstraints.HORIZONTAL;
	constraints.weightx = 10.0;
	constraints.gridwidth = GridBagConstraints.REMAINDER;

	add(new Spring(), constraints);

	revalidate();
	
	collapsed = true;
    }
    
    public void expand() {
	Dimension dim = new Dimension(height, width);
	
	//	layout.defaultConstraints.anchor = GridBagConstraints.WEST;
	removeAll();
	Dimension dim2 = getSize();
	setSize(dim2.width, 40);
	myComponents[0] = new CollapseButton(VERTICAL);

	for (int i = 0; i < componentCount; i++) {
	    add(myComponents[i]);
	}
        
	revalidate();
	
	collapsed = false;
    }

    public boolean isCollapsible() {
	return collapsible;
    }

    public boolean isCollapsed() {
	return collapsed;
    }

    class CollapseListener implements ActionListener {
	
	public void actionPerformed(ActionEvent evt) {
	    if (isCollapsed() == true) {
		expand();
	    } else {
		collapse();
	    }
	}
    }

    class CollapseButton extends JButton {
	private ImageIcon collapseButtonIconVertical = new ImageIcon("widgets/images/collapseButton-vertical.gif", "regular collapseButton icon");
	private ImageIcon collapseButtonVerticalRollover = new ImageIcon("widgets/images/collapseButton-vertical-rollover.gif", "vertical rollover icon");
	private ImageIcon collapseButtonVerticalPressed = new ImageIcon("widgets/images/collapseButton-vertical-pressed.gif", "vertical pressed icon");
	private ImageIcon collapseButtonIconHorizontal = new ImageIcon("widgets/images/collapseButton-horizontal.gif", "horizontal normal icon");
	private ImageIcon collapseButtonHorizontalRollover = new ImageIcon("widgets/images/collapseButton-horizontal-rollover.gif", "horizontal rollover icon");
	private ImageIcon collapseButtonHorizontalPressed = new ImageIcon("widgets/images/collapseButton-horizontal-pressed.gif", "horizontal pressed icon");

	final int VERTICAL = 0;
	final int HORIZONTAL = 1;

	public CollapseButton(int orientation) {
	    super();
	    setRolloverEnabled(true);
	    setFocusPainted(false);
            setDefaultCapable(false);
            setBorder(null);
            setBorderPainted(false);
	    setMargin(new Insets(0,0,0,0));
            setToolTipText("Collapses/Expands the ToolBar");

	    if (orientation == VERTICAL) {
		setIcon(collapseButtonIconVertical);
		setRolloverIcon(collapseButtonVerticalRollover);
		setPressedIcon(collapseButtonVerticalPressed);
	    } else {
		setIcon(collapseButtonIconHorizontal);
		setRolloverIcon(collapseButtonHorizontalRollover);
		setPressedIcon(collapseButtonHorizontalPressed);
	    }
	    
	    addActionListener(new CollapseListener());
	}

    }
	
}
	
	
