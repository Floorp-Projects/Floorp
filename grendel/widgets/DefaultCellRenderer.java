/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Created: Will Scullin <scullin@netscape.com>,  2 Oct 1997.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.widgets;

import calypso.util.Assert;

import grendel.ui.Util;

import java.awt.Color;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Insets;

import javax.swing.Icon;
import javax.swing.JComponent;
import javax.swing.UIManager;
import javax.swing.plaf.basic.BasicGraphicsUtils;
import javax.swing.border.BevelBorder;
import javax.swing.border.Border;
import javax.swing.BorderFactory;

/**
 * The default TreeTable cell renderer. Handles text and not much
 * else.
 * @see TreeTable
 * @see CellRenderer
 */

public class DefaultCellRenderer extends JComponent implements CellRenderer {
  protected Object        fObject;
  protected Object        fData;
  protected boolean       fSelected;

  public DefaultCellRenderer() {
//    setFont(BasicGraphicsUtils.controlFont);
    setFont(Font.decode("SansSerif-12"));
  }

  // HeaderRenderer Interface

  public void setValue(Object aObject, Object aData, boolean aSelected) {
    fObject = aObject;
    Assert.Assertion(aData != null);
    fData = aData;
    fSelected = aSelected;

    setSize(getPreferredSize());
  }

  public Component getComponent() {
    return this;
  }

  // Component overloads

  public Dimension getPreferredSize() {
    Font font = getFont();
    String text = fData.toString();

    Insets insets = getInsets();
    Dimension res = new Dimension(insets.left + insets.right,
                                  insets.top + insets.bottom);

    if (font != null && text != null) {
      FontMetrics fm = getToolkit().getFontMetrics(font);
      res.height += 2 + fm.getHeight();
      res.width += 2 + fm.stringWidth(text);
    }
    return res;
  }

  public Dimension getMinimumSize() {
    return getPreferredSize();
  }

  public Dimension getMaximumSize() {
    return getPreferredSize();
  }

  public void paint(Graphics g) {
    Font font = getFont();
    String text = fData.toString();
    Insets insets = getInsets();

    if (font != null && text != null) {
      g.setFont(font);
      FontMetrics fm = getToolkit().getFontMetrics(font);
      int x = insets.left + 2;
      int h = getHeight();
      int y = (h - fm.getHeight()) / 2 + fm.getAscent();

      g.setColor(fSelected ?
                 UIManager.getColor("textHighlightText") :
                 UIManager.getColor("textText"));
      Util.DrawTrimmedString(g, text, fm, Util.RIGHT, x, y, getWidth());
    }
  }
}
