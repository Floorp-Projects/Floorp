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
 * Created: Terry Weissman <terry@netscape.com>, 28 Oct 1997.
 */

package grendel.storage;

import calypso.util.Assert;

import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.MessagingException;

/** If you want to call any of the FolderExtra methods on a Folder, you
  should get the FolderExtra object using this call.  Some Folder objects
  will implement that interface directly; the rest need to have them
  painfully computed. */

public class FolderExtraFactory {
  static public FolderExtra Get(Folder f) {
    if (f instanceof FolderExtra) return (FolderExtra) f;
    return new FolderExtraWrapper(f);
  }
}

class FolderExtraWrapper implements FolderExtra {
  Folder f;
  protected FolderExtraWrapper(Folder fold) {
    f = fold;
  }

  public int getUndeletedMessageCount() {
    try {
      return f.getMessageCount(); // ### WRONG WRONG WRONG!
    } catch (MessagingException e) {
    } catch (IllegalStateException e) {
    }
    return -1;
  }

}
