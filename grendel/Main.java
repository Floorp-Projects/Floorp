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
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Joel York <yorkjoe@charlie.acc.iit.edu>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel;

import grendel.prefs.base.UIPrefs;

import grendel.ui.MessageDisplayManager;
import grendel.ui.MultiMessageDisplayManager;
import grendel.ui.UnifiedMessageDisplayManager;

/**
 * This launches the Grendel GUI.
 */

public class Main {
  static MessageDisplayManager fManager;

  public static void main(String argv[]) {
    if (UIPrefs.GetMaster().getDisplayManager().equals("multi")) {
      fManager = new MultiMessageDisplayManager();
    } else {
      fManager = new UnifiedMessageDisplayManager();
    }
    MessageDisplayManager.SetDefaultManager(fManager);
    fManager.displayMaster();
  }
}

