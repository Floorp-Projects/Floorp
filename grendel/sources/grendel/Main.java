/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL
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
 * Contributor(s): Jeff Galyan <talisman@anamorphic.com>
 *               Joel York <yorkjoe@charlie.acc.iit.edu>
 *               Edwin Woudt <edwin@woudt.nl>
 */
package grendel;

import grendel.prefs.base.UIPrefs;

import grendel.ui.MessageDisplayManager;
import grendel.ui.MultiMessageDisplayManager;
import grendel.ui.Splash;
import grendel.ui.UnifiedMessageDisplayManager;

import javax.swing.LookAndFeel;
import javax.swing.UIManager;


/**
 * This launches the Grendel GUI.
 */
public class Main
{
  static MessageDisplayManager fManager;

  public static void main(String[] argv)
  {
    System.setProperty(
                       "java.protocol.handler.pkgs",
                       (System.getProperty("java.protocol.handler.pkgs")+
                       "|grendel.renderer.URL"));

    Splash splash=new Splash();

    UIPrefs prefs=UIPrefs.GetMaster();

    UIManager.LookAndFeelInfo[] info=UIManager.getInstalledLookAndFeels();
    LookAndFeel laf;

    for (int i=0; i<info.length; i++) {
      try {
        String name=info[i].getClassName();
        Class c=Class.forName(name);
        laf=(LookAndFeel) c.newInstance();

        if (laf.getDescription().equals(prefs.getLookAndFeel())) {
          UIManager.setLookAndFeel(laf);
        }
      } catch (Exception e) {
      }
    }

    if (prefs.getDisplayManager().equals("multi")) {
      fManager=new MultiMessageDisplayManager();
    } else {
      fManager=new UnifiedMessageDisplayManager();
    }

    MessageDisplayManager.SetDefaultManager(fManager);
    fManager.displayMaster();
    splash.dispose();
  }
}
