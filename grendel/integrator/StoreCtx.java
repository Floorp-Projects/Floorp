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
 * Created: Will Scullin <scullin@netscape.com>, 14 Nov 1997.
 */

package grendel.integrator;

import java.awt.Image;
import java.text.MessageFormat;

//import netscape.shell.IShellIntegrator;
//import netscape.shell.IShellView;
//import netscape.shell.IShellViewCtx;

import grendel.view.ViewedFolder;
import grendel.view.ViewedStore;

public class StoreCtx extends FolderCtx {
  ViewedStore fStore;
  public StoreCtx(ViewedStore aStore) {
    super(aStore);
    fStore = aStore;
  }

  /**
   * Returns a human readable name for this view
   *
   * @return a string representation of this view
   */
  public String getDisplayName() {
    String host = fStore.getHost();
    if (host != null) {
      return MessageFormat.format("Messages on {0}", new Object[] {host});
    } else {
      return "Local Messages";
    }
  }
}
