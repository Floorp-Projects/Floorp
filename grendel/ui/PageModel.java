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
 * Created: Giao Nguyen <grail@cafebabe.org>, 10 Feb 1999.
 */

package grendel.ui;

import java.util.Hashtable;

import java.awt.event.TextEvent;
import java.awt.event.TextListener;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;

import javax.swing.JComponent;
import javax.swing.text.JTextComponent;
import javax.swing.JToggleButton;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;


/**
 * Model for a "page" or what is commonly termed as a panel.
 */
public class PageModel {

  /**
   * Hashtable of values the model will use to communicate with other
   * objects.
   */
  protected Hashtable values;

  void setStore(Hashtable values) {
    this.values = values;
  }

  /**
   * Get an attribute from the model.
   *
   * @param attribute the attribute key for the object
   */
  public Object getAttribute (String attribute) {
    return values.get(attribute);
  }

  /**
   * Puts an attribute into the model.
   */
  public void setAttribute(String attribute, Object value) {
    values.put(attribute, value);
  }

  public void add(JComponent comp, String key) {
    if (comp instanceof JTextComponent) {
      JTextComponent text = (JTextComponent)comp;
      text.getDocument().addDocumentListener(new TextHandler(text, key));
    } else if (comp instanceof JToggleButton) {
      JToggleButton toggle = (JToggleButton)comp;
      toggle.addItemListener(new ToggleHandler(toggle, key));
    }
  }

  class ToggleHandler
    implements ItemListener {
    JToggleButton comp;
    String key;
    ToggleHandler(JToggleButton button, String key) {
      comp = button;
      this.key = key;
    }

    public void itemStateChanged(ItemEvent event) {
      System.out.println(comp.isSelected());
    }
  }

  class TextHandler 
    implements DocumentListener {
    JTextComponent comp;
    String key;
    TextHandler(JTextComponent text, String key) {
      comp = text;
      this.key = key;
    }
    
    public void changedUpdate(DocumentEvent e) {
    }
    
    public void insertUpdate(DocumentEvent e) {
      updateModel(e);
    }
    
    public void removeUpdate(DocumentEvent e) {
      updateModel(e);
    }

    private void updateModel(DocumentEvent event) {
      int length = event.getDocument().getLength();
      String content;
      try {
        content = event.getDocument().getText(0, length);
        values.put(key, content);
      } catch (Exception e) {
        // ignore if we can't get the document.
      }    
    }
  }
}


