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
import java.util.*;

public class DropTarget {
  Component fComponent = null;
  Insets fScrollInsets = null;
  int fTargetActions = DnDConstants.ACTION_COPY_OR_MOVE;
  boolean fActive = true;

  public DropTarget() {

  }

  public DropTarget(Component c) {
    fComponent = c;
  }

  public Component getComponent() {
    return fComponent;
  }

  public void setComponent(Component c)
    throws IllegalArgumentException {
    fComponent = c;
  }

  public DropTargetContext getDropTargetContext() {
    return null;
  }

  public boolean supportsAutoScrolling() {
    return fScrollInsets != null;
  }

  public void setAutoScrollInsets(Insets scrollInsets) {
    fScrollInsets = scrollInsets;
  }

  public Insets getAutoScrollInsets() {
    return fScrollInsets;
  }

  public void setDefaultTargetActions(int actions) {
    fTargetActions = actions;
  }

  public int getDefaultTargetActions() {
    return fTargetActions;
  }

  public void addDropTargetListener(DropTargetListener dte)
    throws TooManyListenersException {
  }

  public void removeDropTargetListener(DropTargetListener dte) {
  }

  public void setActive(boolean active) {
    fActive = active;
  }

  public boolean isActive() {
    return fActive;
  }
}
