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
 * Created: Will Scullin <scullin@netscape.com>,  4 Sep 1997.
 */

package grendel.widgets;

import java.awt.event.MouseEvent;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import javax.swing.event.ChangeEvent;

public class SingleSelectionManager implements SelectionManager {
  Object fSelection = null;
  Vector fListeners = new Vector();

  Object fNullArray[] = new Object[0];
  Object fAddedArray[] = new Object[1];
  Object fRemovedArray[] = new Object[1];

  /**
   * Removes all objects from the selection
   */

  public void clearSelection() {
    if (fSelection != null) {
      fRemovedArray[0] = fSelection;
      fSelection = null;
      notifySelectionChanged(fNullArray, fRemovedArray);
    }
  }

  /**
   * Sets the selection to be a single object
   */

  public void setSelection(Object aObject) {
    Object removed[] = fNullArray;
    Object added[] = fNullArray;

    if (fSelection != null && !fSelection.equals(aObject)) {
      fRemovedArray[0] = fSelection;
      removed = fRemovedArray;
    }
    if (aObject != null && !aObject.equals(fSelection)) {
      fAddedArray[0] = aObject;
      added = fAddedArray;
    }

    fSelection = aObject;
    if (added != fNullArray || removed != fNullArray) {
      notifySelectionChanged(added, removed);
    }
  }

  public void setSelection(Enumeration aObjects) {
    if (aObjects.hasMoreElements()) {
      setSelection(aObjects.nextElement());
    }
  }

  /**
   * Adds a single object to the selection. Overwrites
   * previous selection if a selection already exists. Notifies
   * listeners if the selection changes.
   */

  public void addSelection(Object aObject) {
    if (fSelection != aObject && aObject != null) {
      Object removed[];
      fAddedArray[0] = aObject;
      if (fSelection != null) {
         fRemovedArray[0] = fSelection;
         removed = fRemovedArray;
      } else {
        removed = fNullArray;
      }

      fSelection = aObject;
      notifySelectionChanged(fAddedArray, removed);
    }
  }

  public void addSelection(Enumeration aObjects) {
    if (aObjects.hasMoreElements()) {
      addSelection(aObjects.nextElement());
    }
  }

  /**
   * Removes a single object from the selection. Notifies
   * listeners if the selection changes.
   */

  public void removeSelection(Object aObject) {
    if (fSelection == aObject) {
      fRemovedArray[0] = aObject;

      fSelection = null;
      notifySelectionChanged(fNullArray, fRemovedArray);
    }
  }

  public void removeSelection(Enumeration aObjects) {
    if (aObjects.hasMoreElements()) {
      removeSelection(aObjects.nextElement());
    }
  }

  /**
   * Returns true if the object is in the selection.
   */

  public boolean isSelected(Object aObject) {
    return fSelection == aObject;
  }

  /**
   * Returns the number of objects in the selection
   */

  public int getSelectionCount() {
    return fSelection != null ? 1 : 0;
  }

  /**
   * Returns an enumeration of all objects in the selection
   */

  public Enumeration getSelection() {
    return new LameEnumeration(fSelection);
  }

  /**
   * Passed on to listeners
   */

  public void doubleClickSelection(MouseEvent aEvent) {
    notifySelectionDoubleClicked(aEvent);
  }

  /**
   * Passed on to listeners
   */

  public void contextClickSelection(MouseEvent aEvent) {
    notifySelectionContextClicked(aEvent);
  }

  /**
   * Passed on to listeners
   */

  public void dragSelection(MouseEvent aEvent) {
    notifySelectionDragged(aEvent);
  }

  /**
   * Adds a selection listener
   */

  public synchronized void addSelectionListener(SelectionListener aListener) {
    fListeners.addElement(aListener);
  }

  /**
   * Removes a selection listener
   */

  public synchronized void removeSelectionListener(SelectionListener aListener) {
    fListeners.removeElement(aListener);
  }

  void notifySelectionChanged(Object aAdded[], Object aRemoved[]) {
    Enumeration listeners = fListeners.elements();
    SelectionEvent event = new SelectionEvent(this, aAdded, aRemoved);
    while (listeners.hasMoreElements()) {
      ((SelectionListener) listeners.nextElement()).selectionChanged(event);
    }
  }

  void notifySelectionDoubleClicked(MouseEvent aEvent) {
    Enumeration listeners = fListeners.elements();
    while (listeners.hasMoreElements()) {
      ((SelectionListener) listeners.nextElement()).selectionDoubleClicked(aEvent);
    }
  }

  void notifySelectionContextClicked(MouseEvent aEvent) {
    Enumeration listeners = fListeners.elements();
    while (listeners.hasMoreElements()) {
      ((SelectionListener) listeners.nextElement()).selectionContextClicked(aEvent);
    }
  }

  void notifySelectionDragged(MouseEvent aEvent) {
    Enumeration listeners = fListeners.elements();
    while (listeners.hasMoreElements()) {
      ((SelectionListener) listeners.nextElement()).selectionDragged(aEvent);
    }
  }
}

//
// LameEnumeration class
// We only have one object, so the enumeration is trivial
//

class LameEnumeration implements Enumeration {
  Object fObject;

  public LameEnumeration(Object aObject) {
    fObject = aObject;
  }

  public boolean hasMoreElements() {
    return fObject != null;
  }

  public Object nextElement() {
    Object res = fObject;
    fObject = null;
    return res;
  }
}
