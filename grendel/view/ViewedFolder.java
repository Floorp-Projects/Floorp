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
 * Created: Will Scullin <scullin@netscape.com>,  2 Dec 1997.
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
 */

package grendel.view;

import javax.mail.Folder;
import javax.mail.MessagingException;
import javax.mail.Store;

public interface ViewedFolder {
  /**
   * Returns the associated folder
   */

  public Folder getFolder();

  /**
   * Returns the name of the associated folder
   */

  public String getName();

  /**
   * Returns the ViewedFolder associated with the given folder.
   * The Folder object inside the ViewedFolder may not be the
   * same as the object passed in, but it will always represent
   * the same folder
   */

  public ViewedFolder getViewedFolder(Folder aFolder)
    throws MessagingException;

  /**
   * Get cached message count data, since some protocols
   * will hit the server for each call.
   */

  public int getMessageCount();

  /**
   * Get cached unread count, since some protocols will
   * hit the server for each call.
   */

  public int getUnreadMessageCount();

  /**
   * Get cached undeleted message count, since some protocols will
   * hit the server for each call.
   */

  public int getUndeletedMessageCount();

  /**
   * Returns the next folder at this level.
   */

  public ViewedFolder getNextFolder();

  /**
   * Returns the first subfolder of this folder.
   */

  public ViewedFolder getFirstSubFolder();

  /**
   * Returns the parent folder. Returns null for the default
   * folder for a session.
   */

  public ViewedFolder getParentFolder();

  /**
   * Returns the associated session
   */

  public ViewedStore getViewedStore();

  /**
   * Returns whether this is an inbox or not
   */

  public boolean isInbox();
}
