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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Will Scullin <scullin@netscape.com>, 26 Aug 1997.
 */

package grendel.widgets;

import java.awt.event.MouseEvent;
import java.util.Enumeration;

/**
 * Interface for simple selection management.
 * @see MultiSelectionManager
 * @see SingleSelectionManager
 * @see SelectionListener
 */

public interface SelectionManager {
  /**
   * Removes all objects from the selection
   */
  public void clearSelection();

  /**
   * Sets the selection to be a single object
   */

  public void setSelection(Object aObject);

  /**
   * Sets the selection to be the given array of objects
   */

  public void setSelection(Enumeration aObjects);

  /**
   * Adds a single object to the selection. Notifies
   * listeners if the selection changes.
   */

  public void addSelection(Object aObject);

  /**
   * Adds an array of objects to the selection. Notifies
   * listeners if the selection changes.
   */

  public void addSelection(Enumeration aObjects);

  /**
   * Removes a single object from the selection. Notifies
   * listeners if the selection changes.
   */

  public void removeSelection(Object aObject);

  /**
   * Removes an array of objects from the selection. Notifies
   * listeners if the selection changes.
   */

  public void removeSelection(Enumeration aObjects);

  /**
   * Returns true if the object is in the selection.
   */

  public boolean isSelected(Object aObject);

  /**
   * Returns the number of objects in the selection
   */

  public int getSelectionCount();

  /**
   * Returns an enumeration of all objects in the selection
   */

  public Enumeration getSelection();

  /**
   * Passed on to listeners
   */

  public void doubleClickSelection(MouseEvent aEvent);

  /**
   * Passed on to listeners
   */

  public void contextClickSelection(MouseEvent aEvent);

  /**
   * Passed on to listeners
   */

  public void dragSelection(MouseEvent aEvent);

  /**
   * Adds a selection listener
   */

  public void addSelectionListener(SelectionListener aListener);

  /**
   * Removes a selection listener
   */

  public void removeSelectionListener(SelectionListener aListener);
}
