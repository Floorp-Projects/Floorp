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
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 */

package grendel.ui;

import javax.mail.Folder;
import javax.mail.Message;

/**
 * Handles attempts to view messages, folders and their contents,
 * without assuptions about the UI layout.
 * Potential side affects for given actions are detailed.
 * The resulting window state should be order independent <b>if</b>
 * overlapping arguments are the same with the exception
 * of window focus. That is, <pre>
 * displayMaster(folderA);
 * displayFolder(folderA, messageA);
 * </pre>
 * should result in the same windows and selections as <pre>
 * displayFolder(foldarA, messageA);
 * displayMaster(folderA);
 * </pre>
 * but the result of <pre>
 * displayFolder(folderA, messageA);
 * displayMaster(folderB):
 * </pre>
 * is undefined. If multiple windows are opened as the result of
 * multiple calls, focus will be shifted to the last opened
 * window. Focus will always be shifted to the last acted upon
 * view.
 */

public abstract class MessageDisplayManager {
  static volatile MessageDisplayManager fManager;

  static {
    System.out.println("MessageDisplayManager.<clinit>");
  }

  /**
   * Sets the default message display manager
   */

  public static synchronized void SetDefaultManager(MessageDisplayManager
                                                    aManager) {
    fManager = aManager;
    System.out.println("Setting manager: " + aManager);
  }

  /**
   * Gets the default message display manager
   */

  public static synchronized MessageDisplayManager GetDefaultManager() {
    System.out.println("Getting manager: " + fManager);
    return fManager;
  }

  /**
   * Displays a message given a Message object. This
   * may affect displayed folders.
   */

  public abstract void displayMessage(Message aMessage);

  /**
   * Displays a folder given a folder object. This may
   * affect displayed messages.
   */

  public abstract void displayFolder(Folder aFolder);

  /**
   * Displays folder given a Folder object and
   * selects a message in that folder given a Message
   * object. This may or may not also display the message, but
   * it will not open multiple windows.
   */

  public abstract void displayFolder(Folder aFolder, Message aMessage);

  /**
   * Displays the master (A folder tree, for now). This should not
   * affect displayed folders or messages.
   */

  public abstract void displayMaster();

  /**
   * Displays the master with the given folder selected. This
   * may affect displayed folders and messages.
   */

  public abstract void displayMaster(Folder aFolder);
}
