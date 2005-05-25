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
 * Contributor(s): Jeff Galyan <jeffrey.galyan@sun.com>
 */

package grendel.ui;

import javax.swing.JWindow;
import javax.swing.JLabel;
import javax.swing.ImageIcon;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Toolkit;

/**
 *Default screen to be shown while application is initializing. Run the
 *dispose method on this class when you're ready to close the splash screen.
 */
public class Splash extends JWindow {

  public Splash() {
    super();
    ImageIcon image = new ImageIcon("ui/images/GrendelSplash.jpg");
    JLabel splashLabel = new JLabel(image);
    getContentPane().add(splashLabel);

    Dimension screensize = Toolkit.getDefaultToolkit().getScreenSize();
    setLocation(screensize.width/2 - 150, screensize.height/2 - 150);

    pack();
    validate();
    setVisible(true);
  }
}
