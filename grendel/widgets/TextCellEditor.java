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
 * Created: Will Scullin <scullin@netscape.com>, 30 Oct 1997.
 */

package grendel.widgets;

import java.awt.Component;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusAdapter;
import java.awt.event.FocusEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.util.EventObject;

import com.sun.java.swing.CellEditorListener;
import com.sun.java.swing.JTextField;
import com.sun.java.swing.KeyStroke;
import com.sun.java.swing.event.ChangeEvent;
import com.sun.java.swing.event.DocumentEvent;
import com.sun.java.swing.event.DocumentListener;

public class TextCellEditor extends JTextField implements CellEditor,
DocumentListener {
  CellEditorListener fListener;
  boolean fSelected, fStopping;
  Dimension fSize = new Dimension();

  public TextCellEditor() {
    addActionListener(new StopAction());
    addFocusListener(new MyFocusListener());

    registerKeyboardAction(new CancelAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0),
                           WHEN_FOCUSED);
    getDocument().addDocumentListener(this);
    setFont(Font.decode("SansSerif-12"));
  }

  public void cancelCellEditing() {
    if (fListener != null) {
      fListener.editingCanceled(new ChangeEvent(this));
    }
  }

  public void setValue(Object aObject, Object aData, boolean aSelected) {
    setText(aData.toString());
    fSelected = aSelected;
    fSize = getPreferredSize();
  }

  public Object getCellEditorValue() {
    return getText();
  }

  public boolean isCellEditable(EventObject aEvent) {
    if (aEvent instanceof MouseEvent) {
      return (((MouseEvent) aEvent).getID() == MouseEvent.MOUSE_RELEASED &&
              fSelected);
    }
    return false;
  }

  public boolean shouldSelectCell(EventObject aEvent) {
    return !fSelected;
  }

  public boolean stopCellEditing() {
    if (fListener != null && !fStopping) {
      fStopping = true;
      fListener.editingStopped(new ChangeEvent(this));
      fStopping = false;
    }
    return true;
  }

  public boolean startCellEditing(EventObject aEvent) {
    updateSize(getPreferredSize());
    requestFocus();
    return true;
  }

  public Component getCellEditorComponent() {
    return this;
  }

  public void addCellEditorListener(CellEditorListener aListener) {
    fListener = aListener;
  }

  public void removeCellEditorListener(CellEditorListener aListener) {
    if (fListener == aListener) {
      fListener = null;
    }
  }

  protected void updateSize(Dimension aSize) {
    int w = aSize.width > fSize.width ? aSize.width : fSize.width;
    int h = aSize.height > fSize.height ? aSize.height : fSize.height;

    setSize(w, h);
  }

  public void changedUpdate(DocumentEvent aEvent) {
    updateSize(getPreferredSize());
  }

  public void insertUpdate(DocumentEvent aEvent) {
    updateSize(getPreferredSize());
  }

  public void removeUpdate(DocumentEvent aEvent) {
    updateSize(getPreferredSize());
  }

  class CancelAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      cancelCellEditing();
    }
  }

  class StopAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      stopCellEditing();
    }
  }

  class MyFocusListener extends FocusAdapter {
    public void focusLost(FocusEvent aEvent) {
      stopCellEditing();
    }
  }
}
