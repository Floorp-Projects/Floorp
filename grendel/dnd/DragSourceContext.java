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
 * Created: Will Scullin <scullin@netscape.com>,  6 Nov 1997.
 */

package grendel.dnd;

import java.awt.*;
import java.awt.datatransfer.*;
import java.awt.event.*;
import java.util.*;
import javax.swing.*;

public class DragSourceContext {
  static final int ACTION_MASK =
    DnDConstants.ACTION_MOVE|
    DnDConstants.ACTION_COPY|
    DnDConstants.ACTION_REFERENCE;

  DragSource fSource;
  Component fComponent;
  AWTEvent fTrigger;
  Transferable fTransferable;
  int fActions;
  int fModifiers;
  Cursor fCursor;
  Cursor fOldCursor;
  DragSourceListener fListener;

  public DragSourceContext(DragSource aSource, Component aComponent,
                           AWTEvent aTrigger, Transferable aTransferable) {
    fSource = aSource;
    fComponent = aComponent;
    fTrigger = aTrigger;
    fTransferable = aTransferable;
    fOldCursor = fComponent.getCursor();
  }

  public DragSource getDragSource() {
    return fSource;
  }

  public Component getComponent() {
    return fComponent;
  }

  public AWTEvent getTrigger() {
    return fTrigger;
  }

  public Transferable getTransferable() {
    return fTransferable;
  }

  public void cancelDrag() throws InvalidDnDOperationException {
    notifyDropEnd(true, false);
    fComponent.setCursor(fOldCursor);
  }

  public void commitDrop() throws InvalidDnDOperationException {
    notifyDropEnd(false, true);
    fComponent.setCursor(fOldCursor);
  }

  public int getSourceActions() {
    return fActions;
  }

  public void setSourceActions(int actions)
    throws InvalidDnDOperationException {
    if ((actions & (~ACTION_MASK)) != 0) {
      throw new InvalidDnDOperationException();
    }
    fActions = actions;
  }

  public void setCursor(Cursor cursor)
    throws InvalidDnDOperationException {
    fCursor = cursor;
    fComponent.setCursor(fCursor);
  }

  public Cursor getCursor() {
    return fCursor;
  }

  public void addDragSourceListener(DragSourceListener dsl)
    throws TooManyListenersException {
    if (fListener != null) {
      throw new TooManyListenersException();
    }
    fListener = dsl;
  }

  public void removeDragSourceListener(DragSourceListener dsl) {
    if (fListener == dsl) {
      fListener = null;
    }
  }

  void notifyDragEnter() {
    DragSourceDragEvent event = new DragSourceDragEvent(this, 0, 0);
    if (fListener != null) {
      fListener.dragEnter(event);
    }
  }

  void notifyDragOver() {
    DragSourceDragEvent event = new DragSourceDragEvent(this, 0, 0);
    if (fListener != null) {
      fListener.dragOver(event);
    }
  }

  void notifyDragGestureChanged() {
    DragSourceDragEvent event = new DragSourceDragEvent(this, 0, 0);
    if (fListener != null) {
      fListener.dragGestureChanged(event);
    }
  }

  void notifyDragExit() {
    DragSourceDragEvent event = new DragSourceDragEvent(this, 0, 0);
    if (fListener != null) {
      fListener.dragExit(event);
    }
  }

  void notifyDrop() {
    DragSourceDragEvent event = new DragSourceDragEvent(this, 0, 0);
    if (fListener != null) {
      fListener.drop(event);
    }
  }

  void notifyDropEnd(boolean aCancelled, boolean aSuccessful) {
    DragSourceDropEvent event =
      new DragSourceDropEvent(this, aCancelled, aSuccessful);
    if (fListener != null) {
      fListener.dragDropEnd(event);
    }
  }

  class DragMouseListener extends MouseAdapter implements MouseMotionListener {
    public void mouseReleased(MouseEvent aEvent) {

    }

    public void mouseDragged(MouseEvent aEvent) {
      Container top = findTopContainer(fComponent);
      Point pt = SwingUtilities.convertPoint(fComponent, aEvent.getPoint(),
                                             top);
      Component mouseOver =
        findTopContainer(fComponent).getComponentAt(pt);

      if (mouseOver != null && mouseOver instanceof DropTargetComponent) {
        DropTarget target = ((DropTargetComponent) mouseOver).getDropTarget();
        if (target != null) {

        }
      }

    }

    public void mouseMoved(MouseEvent aEvent) {

    }

    Container findTopContainer(Component c) {
      while (c.getParent() != null) {
        c = c.getParent();
      }
      return (Container) c;
    }
  }
}
