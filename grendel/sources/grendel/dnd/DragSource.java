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
 * Created: Will Scullin <scullin@netscape.com>,  6 Nov 1997.
 */

package grendel.dnd;

import java.awt.*;
import java.awt.datatransfer.*;
import java.awt.event.*;
import java.util.*;

public class DragSource {
  static DragSource fInstance = new DragSource();

  Cursor fDefaultDragCursor =
    Cursor.getPredefinedCursor(Cursor.HAND_CURSOR);
  Cursor fDefaultDropCursor =
    Cursor.getPredefinedCursor(Cursor.CROSSHAIR_CURSOR);
  Cursor fDefaultNoDropCursor =
    Cursor.getPredefinedCursor(Cursor.HAND_CURSOR);

  public static DragSource getDragSource(Component c) {
    return fInstance;
  }

  public DragSourceContext startDrag(Component c,
                                     AWTEvent trigger,
                                     int actions,
                                     Image dragImage,
                                     Point dragImageOffset,
                                     Transferable transferable,
                                     DragSourceListener dsl)
    throws InvalidDnDOperationException {
    DragSourceContext res = null;
    if (trigger instanceof MouseEvent) {
      res = new DragSourceContext(this, c, trigger, transferable);
      res.setSourceActions(actions);
      try {
        res.addDragSourceListener(dsl);
      } catch (TooManyListenersException e) {}
    } else {
      throw new InvalidDnDOperationException();
    }

    return res;
  }

  public void setDefaultDragCursor(Cursor c) {
    fDefaultDropCursor = c;
  }

  public Cursor getDefaultDragCursor() {
    return fDefaultDropCursor;
  }

  public void setDefaultDropCursor(Cursor c) {
    fDefaultDragCursor = c;
  }

  public Cursor getDefaultDropCursor() {
    return fDefaultDragCursor;
  }

  public void setDefaultNoDropCursor(Cursor c) {
    fDefaultNoDropCursor = c;
  }

  public Cursor getDefaultNoDropCursor() {
    return fDefaultNoDropCursor;
  }

  public boolean isDragTrigger(AWTEvent trigger) {
    if (trigger instanceof MouseEvent) {
      MouseEvent e = (MouseEvent) trigger;
      return (e.getID() == MouseEvent.MOUSE_DRAGGED);
    }
    return false;
  }

  public Rectangle getDragThresholdBBox(Component c,
                                        Point hotspot) {
    return new Rectangle(hotspot.x - 4, hotspot.y - 4,
                         8, 8);
  }
}
