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
 * The Initial Developer of the Original Code is Giao Nguyen
 * <grail@cafebabe.org>.  Portions created by Giao Nguyen are Copyright 
 * (C) 1999 Giao Nguyen.  All Rights Reserved.
 */

package grendel.widgets;

import javax.swing.JMenuBar;
import javax.swing.JMenu;
import javax.swing.JMenuItem;

import java.util.Hashtable;
import java.util.Enumeration;

public class MenuCtrl extends JMenu {
  protected Hashtable controls = new Hashtable();

  /**
   * Get the control by its name.
   * @param name the control's name
   * @return the menu item
   */
  public JMenuItem getItemByName(String name) {
    JMenuItem item = (JMenuItem)controls.get(name);
    
    // if it's not a toplevel item, it must be in one of the submenus
    if (item != null) {
      Enumeration e = controls.elements();
      
      while (e.hasMoreElements()) {
        Object o = e.nextElement();
        
        if (o instanceof MenuCtrl) {
          MenuCtrl c = (MenuCtrl)o;
          item = (JMenuItem)c.getItemByName(name);
          if (item != null) {
            break;
          }
        }
      }

    }
    
    return item;
  }
  
  /**
   * Add a menu item to the menu.
   * @param name the name of the menu
   * @param component the menu item
   */
  public void addItemByName(String name, JMenuItem component) {
    controls.put(name, component);
    add(component);
  }
}

