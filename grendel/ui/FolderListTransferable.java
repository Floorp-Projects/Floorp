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
 * Created: Will Scullin <scullin@netscape.com>, 17 Nov 1997.
 */

package grendel.ui;

import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.io.IOException;
import java.util.Vector;

import javax.mail.Message;

public class FolderListTransferable implements Transferable {
  Vector fList;

  static DataFlavor fFlavors[] = {
    new DataFlavor(FolderListTransferable.class, "Netscape Folder List")
  };

  static DataFlavor GetDataFlavor() {
    return fFlavors[0];
  }

  public FolderListTransferable(Vector aList) {
    fList = aList;
  }

  public Vector getFolderList() {
    return fList;
  }

  public Object getTransferData(DataFlavor aFlavor)
    throws UnsupportedFlavorException, IOException {
    if (aFlavor.equals(fFlavors[0])) {
      return this;
    } else {
      throw new UnsupportedFlavorException(aFlavor);
    }
  }

  public DataFlavor[] getTransferDataFlavors() {
    return fFlavors;
  }

  public boolean isDataFlavorSupported(DataFlavor aFlavor) {
    return aFlavor.equals(fFlavors[0]);
  }
}
