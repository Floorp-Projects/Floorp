/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Giao Nguyen
 * <grail@cafebabe.org>.  Portions created by Giao Nguyen are
 * Copyright (C) 1999 Giao Nguyen. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
public class PageModel 
  implements ActionListener {

  /**
   * Hashtable of values the model will use to communicate with other
   * objects.
   */
  protected Hashtable values;

  protected void setStore(Hashtable values) {
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

  public void actionPerformed(ActionEvent event) {
  }
}


