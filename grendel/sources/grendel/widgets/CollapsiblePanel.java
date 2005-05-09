/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This file has been contributed to Mozilla.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   R.J. Keller <rj.keller@beonex.com>
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
import java.awt.BorderLayout;

import javax.swing.Box;
import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JPanel;
import javax.swing.border.Border;
import javax.swing.plaf.metal.MetalIconFactory;
import javax.swing.ImageIcon;

import grendel.widgets.Collapsible;
import grendel.widgets.Spring;

/**
 * This is an implementation of the grendel.widgets.Collapsible interface,
 * which provides the standard Communicator-style collapsing toolbar panel.
 *
 * @author Jeff Galyan
 * @author R.J. Keller
 * @see Collapsible
 */
public class CollapsiblePanel extends JPanel implements Collapsible {


  private static final int VERTICAL = 0;
  private static final int HORIZONTAL = 1;


  private boolean collapsed = false;
  private Component curComponent;

  /**
   * Constructor
   */
  public CollapsiblePanel(boolean isDoubleBuffered) {
    super(isDoubleBuffered);

    setLayout(new BorderLayout());
    collapsed = false;
  }

  /**
   *Sets the component that is to be collapsed and uncollapsed. Only one
   *component can be shown in this panel at one time. For multiple components,
   *add them all to a JPanel. Since this is usually for toolbars only, it
   *shouldn't be an issue.
   */
  public void setComponent(Component comp) {
    curComponent = comp;

    removeAll();

    CollapseButton collapseButton = new CollapseButton(VERTICAL);
    add(collapseButton, BorderLayout.WEST);
    Dimension dim = collapseButton.getSize();
    collapseButton.reshape(0,0,dim.width,dim.height);

    //set the new components background to the panel background.
    setBackground(comp.getBackground());

    add(comp, BorderLayout.CENTER);

    revalidate();
  }

  public Component getComponent() {
    return curComponent;
  }

  /**
   * Collapses the panel.
   */
  public void collapse() {
    removeAll();
    add(new CollapseButton(HORIZONTAL), BorderLayout.WEST);
    Dimension dim2 = getSize();
    setSize(new Dimension(dim2.width, 5));

    add(new Spring(), BorderLayout.CENTER);

    revalidate();

    collapsed = true;
  }

  /**
   * Uncollapses the panel.
   */
  public void expand() {
    removeAll();

    add(new CollapseButton(VERTICAL), BorderLayout.WEST);
    add(curComponent, BorderLayout.CENTER);

    revalidate();

    collapsed = false;
  }

  /**
   * Tells you whether this component is collapsible.
   * @returns a boolean indicating this component is collapsible.
   */
  public boolean isCollapsible() {
    return collapsible;
  }

  /**
   * Tells you whether this component is currently collapsed.
   * Useful for checking the component's status.
   * @returns true if this component is collapsed, false if it is not.
   */
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