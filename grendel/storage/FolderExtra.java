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
 * Created: Terry Weissman <terry@netscape.com>, 22 Oct 1997.
 */

package grendel.storage;

import javax.mail.MessagingException;

/** These are extra interfaces that our folder objects implement that are not
  implemented as part of the standard Folder class. If you want to call
  any of these, and you just have a base Folder object, use
  FolderExtraFactory to find or create the FolderExtra object for you. */

public interface FolderExtra {
  /** The number of messages in this folder that have not been deleted. */
  public int getUndeletedMessageCount() throws MessagingException;
};

