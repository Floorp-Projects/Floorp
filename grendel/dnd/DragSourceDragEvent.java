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

public class DragSourceDragEvent extends java.util.EventObject {
  DragSourceContext fContext;
  int fActions;
  int fModifiers;

  public DragSourceDragEvent(DragSourceContext context,
                             int actions, int modifiers) {
    super(context);
    fActions = actions;
    fModifiers = modifiers;
  }

  public DragSourceContext getDragSourceContext() {
    return (DragSourceContext) getSource();
  }

  public int getTargetActions() {
    return 0;
  }

  public int getGestureModifiers() {
    return 0;
  }
}
