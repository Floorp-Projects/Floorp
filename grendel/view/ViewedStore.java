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

public interface ViewedStore extends ViewedFolder {
  /**
   * Value for setVisible() for showing all folders
   */

  public static final int kAll = 0;

  /**
   * Value for setVisible() for showing subscribed folders
   */

  public static final int kSubscribed = 1;

  /**
   * Value for setVisible() for showing active folders
   */

  public static final int kActive = 2;

  /**
   * Returns the associated store.
   */

  public Store getStore();

  /**
   * Returns the store's default folder wrapped in a ViewedFolder
   * object.
   */

  public ViewedFolder getDefaultFolder() throws MessagingException;

  /**
   * Returns the id which identifies this store in the preferences/
   */

  public int getID();

  /**
   * Returns the description for this store
   */

  public String getDescription();

  /**
   * Returns the protocol used by this store.
   */

  public String getProtocol();

  /**
   * Returns the host for this store. Returns null for a local store.
   */

  public String getHost();

  /**
   * Returns the user name used to connect. May return null if
   * no user name was used.
   */

  public String getUsername();

  /**
   * Returns the password.
   */

  public String getPassword();

  /**
   * Returns the port used to connect. Returns -1 for the protocol default.
   */

  public int getPort();

  /**
   * Returns the connected state of this store
   */

  public boolean isConnected();

  /**
   * Sets which children to show for this store
   */

  public void setVisible(int aVisible);

  /**
   * Returns which children are showing for this store
   */

  public int getVisible();

  /**
   * Adds a ViewedStoreListener
   */

  public void addViewedStoreListener(ViewedStoreListener l);

  /**
   * Removes a ViewedStoreListener
   */

  public void removeViewedStoreListener(ViewedStoreListener l);
}
