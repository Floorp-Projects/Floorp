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
 */

package grendel.widgets;

import java.awt.Component;

import javax.swing.JCheckBox;
import javax.swing.UIManager;

public class ToggleCellRenderer implements CellRenderer {
  JCheckBox fToggle = new JCheckBox();

  public void setValue(Object aObject, Object aData, boolean aSelected) {
    if (aSelected) {
      fToggle.setBackground(UIManager.getColor("textHighlight"));
      fToggle.setForeground(UIManager.getColor("textHighlightText"));
    } else {
      fToggle.setBackground(UIManager.getColor("window"));
      fToggle.setForeground(UIManager.getColor("textText"));
    }
    if (aData instanceof Boolean) {
      fToggle.setSelected(((Boolean) aData).booleanValue());
    } else {
      fToggle.setSelected(false);
    }
  }

  public Component getComponent() {
    return fToggle;
  }

  public JCheckBox getCheckBox() {
    return fToggle;
  }
}
