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
 *
 */

package grendel.widgets;

import javax.swing.JMenuItem;

/**
 * I haven't figured out a good description for this yet. But it works out 
 * nicely to unify the interactions between the MenuBarCtrl and MenuCtrl.
 */
public interface Control {
  public void addItemByName(String name, JMenuItem component);
}
