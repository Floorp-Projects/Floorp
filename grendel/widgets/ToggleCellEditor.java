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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.widgets;

import java.awt.AWTEvent;
import java.awt.Component;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.util.EventObject;

import javax.swing.event.CellEditorListener;
import javax.swing.JCheckBox;
import javax.swing.event.ChangeEvent;
import javax.swing.UIManager;

public class ToggleCellEditor implements CellEditor, ActionListener {
  JCheckBox fToggle;
  CellEditorListener fListener;

  public ToggleCellEditor() {
    fToggle = new JCheckBox() {
      public void requestFocus() {
      }
    };
    fToggle.addActionListener(this);
  }

  public boolean isCellEditable(EventObject aEvent) {
    if (aEvent instanceof MouseEvent) {
      return (((MouseEvent) aEvent).getClickCount() == 1);
    }
    return false;
  }

  public boolean shouldSelectCell(EventObject aEvent) {
    return false;
  }

  public boolean startCellEditing(EventObject aEvent) {
    if (aEvent instanceof AWTEvent) {
      fToggle.dispatchEvent((AWTEvent) aEvent);
    }
    return true;
  }

  public boolean stopCellEditing() {
    if (fListener != null) {
      fListener.editingStopped(new ChangeEvent(this));
    }
    return true;
  }

  public void cancelCellEditing() {
    if (fListener != null) {
      fListener.editingCanceled(new ChangeEvent(this));
    }
  }

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

  public Component getCellEditorComponent() {
    return fToggle;
  }

  public Object getCellEditorValue() {
    return new Boolean(fToggle.isSelected());
  }

  public void addCellEditorListener(CellEditorListener aListener) {
    fListener = aListener;
  }

  public void removeCellEditorListener(CellEditorListener aListener) {
    if (fListener == aListener) {
      fListener = null;
    }
  }

  public void actionPerformed(ActionEvent e) {
    if (fListener != null) {
                fListener.editingStopped(new ChangeEvent(this));
    }
  }

  public JCheckBox getCheckBox() {
    return fToggle;
  }
}
