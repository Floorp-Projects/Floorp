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
 * Created: Will Scullin <scullin@netscape.com>, 26 Aug 1997.
 */

package grendel.widgets;

import java.awt.event.MouseEvent;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import javax.swing.event.ChangeEvent;

public class MultiSelectionManager implements SelectionManager {
  Vector    fSelection = new Vector();
  Vector    fListeners = new Vector();

  /**
   * Removes all objects from the selection
   */

  public synchronized void clearSelection() {
    if (!fSelection.isEmpty()) {
      Object removedArray[] = new Object[fSelection.size()];
      Object nullArray[] = new Object[0];
      Enumeration removedEnum = fSelection.elements();
      int idx = 0;
      while (removedEnum.hasMoreElements()) {
        removedArray[idx] = removedEnum.nextElement();
        idx++;
      }

      fSelection.setSize(0);
      notifySelectionChanged(nullArray, removedArray);
    }
  }

  /**
   * Sets the selection to be a single object
   */

  public synchronized void setSelection(Object aObject) {
    if (fSelection.size() != 1 || !fSelection.contains(aObject)) {
      Object removedArray[];
      Object addedArray[];
      if (fSelection.contains(aObject)) {
        removedArray = new Object[fSelection.size() - 1];
        addedArray = new Object[0];
      } else {
        removedArray = new Object[fSelection.size()];
        addedArray = new Object[1];
        addedArray[0] = aObject;
      }

      Enumeration removedEnum = fSelection.elements();
      int idx = 0;
      while (removedEnum.hasMoreElements()) {
        Object element = removedEnum.nextElement();
        if (!element.equals(aObject)) {
          removedArray[idx] = element;
          idx++;
        }
      }

      fSelection.setSize(0);
      fSelection.addElement(aObject);
      notifySelectionChanged(addedArray, removedArray);
    }
  }

  public synchronized void setSelection(Enumeration aObjects) {
    Vector addedVector = new Vector();
    Vector removedVector = new Vector();
    Vector noopVector = new Vector();

    // See which items are being removed
    while (aObjects.hasMoreElements()) {
      Object object = aObjects.nextElement();

      if (fSelection.removeElement(object)) {
        addedVector.insertElementAt(object, addedVector.size());
      } else {
        noopVector.insertElementAt(object, noopVector.size());
      }
    }

    // See which items were not being set
    Enumeration removedEnum = fSelection.elements();
    while (removedEnum.hasMoreElements()) {
      Object element = removedEnum.nextElement();
      removedVector.insertElementAt(element, removedVector.size());
    }

    // Remove everything
    fSelection.setSize(0);

    // Set everything
    int i;

    for (i = 0; i < addedVector.size(); i++) {
      fSelection.insertElementAt(addedVector.elementAt(i), fSelection.size());
    }
    for (i = 0; i < noopVector.size(); i++) {
      fSelection.insertElementAt(noopVector.elementAt(i), fSelection.size());
    }

    // If there were deltas, notify
    if (addedVector.size() > 0 || removedVector.size() > 0) {
      Object removedArray[] = new Object[removedVector.size()];
      Object addedArray[] = new Object[addedVector.size()];

      removedVector.copyInto(removedArray);
      addedVector.copyInto(addedArray);

      notifySelectionChanged(addedArray, removedArray);
    }
  }

  /**
   * Adds a single object to the selection. Notifies
   * listeners if the selection changes.
   */

  public synchronized void addSelection(Object aObject) {
    if (!fSelection.contains(aObject)) {
      fSelection.insertElementAt(aObject, fSelection.size());
      Object addedArray[] = new Object[1];
      Object removedArray[] = new Object[0];
      addedArray[0] = aObject;

      notifySelectionChanged(addedArray, removedArray);
    }
  }

  /**
   * NYI
   */

  public synchronized void addSelection(Enumeration aObjects) {
    Vector addedVector = new Vector();
    while (aObjects.hasMoreElements()) {
      Object object = aObjects.nextElement();
      if (!fSelection.contains(object)) {
        fSelection.insertElementAt(object, fSelection.size());
        addedVector.insertElementAt(object, addedVector.size());
      }
    }

    if (addedVector.size() > 0) {
      Object addedArray[] = new Object[addedVector.size()];
      Object removedArray[] = new Object[0];

      addedVector.copyInto(addedArray);
      notifySelectionChanged(addedArray, removedArray);
    }
  }

  /**
   * Removes a single object from the selection. Notifies
   * listeners if the selection changes.
   */

  public synchronized void removeSelection(Object aObject) {
    if (fSelection.removeElement(aObject)) {
      Object addedArray[] = new Object[0];
      Object removedArray[] = new Object[1];
      removedArray[0] = aObject;

      notifySelectionChanged(addedArray, removedArray);
    }
  }

  public synchronized void removeSelection(Enumeration aObjects) {
    Vector removedVector = new Vector();
    while (aObjects.hasMoreElements()) {
      Object object = aObjects.nextElement();
      if (fSelection.removeElement(object)) {
        removedVector.insertElementAt(object, removedVector.size());
      }
    }

    if (removedVector.size() > 0) {
      Object removedArray[] = new Object[removedVector.size()];
      Object addedArray[] = new Object[0];

      removedVector.copyInto(removedArray);
      notifySelectionChanged(addedArray, removedArray);
    }
  }

  /**
   * Returns true if the object is in the selection.
   */

  public synchronized boolean isSelected(Object aObject) {
    return fSelection.contains(aObject);
  }

  /**
   * Returns the number of objects in the selection
   */

  public synchronized int getSelectionCount() {
    return fSelection.size();
  }

  /**
   * Returns an enumeration of all objects in the selection
   */

  public synchronized Enumeration getSelection() {
    return fSelection.elements();
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
