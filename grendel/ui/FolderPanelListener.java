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
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 */

package grendel.ui;

import java.util.EventListener;

import javax.swing.event.ChangeEvent;

import grendel.widgets.StatusEvent;

/**
 * Interface for information about changes in the <code>FolderPanel</code>
 */

public interface FolderPanelListener {
  /**
   * Called when a folder starts to load
   */
  public void loadingFolder(ChangeEvent aEvent);

  /**
   * Called when a folder is done loading
   */

  public void folderLoaded(ChangeEvent aEvent);

  /**
   * Called when the selection changes.
   */

  public void folderSelectionChanged(ChangeEvent aEvent);

  /**
   * Called for status text updates
   */

  public void folderStatus(StatusEvent aEvent);

  /**
   * Called when the user double clicks.
   */

  public void folderSelectionDoubleClicked(ChangeEvent aEvent);
}
