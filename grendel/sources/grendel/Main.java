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

import grendel.addressbook.jdbc.JDBCRegister;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;

import grendel.prefs.base.UIPrefs;

import grendel.ui.MessageDisplayManager;
import grendel.ui.MultiMessageDisplayManager;
import grendel.ui.Splash;
import grendel.ui.UnifiedMessageDisplayManager;

import java.io.IOException;

import javax.swing.LookAndFeel;
import javax.swing.UIManager;


/**
 * This launches the Grendel GUI.
 */
public class Main
{
  public static void main(String[] argv)
  {
    Splash splash = new Splash();
    
    System.setProperty("java.protocol.handler.pkgs",
        ("grendel.URL|" + System.getProperty("java.protocol.handler.pkgs")));
        try {
            
            /*long t = System.currentTimeMillis();
            try
            {
              Class.forName("grendel.TaskManager");
            } catch (ClassNotFoundException ex)
            {
              ex.printStackTrace();
            }
            System.out.println("d = "+(System.currentTimeMillis()-t));*/
            //TaskManager.runStartup();
            TaskList.getStartupList().run();
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    
    
    
    splash.dispose();
  }
}
