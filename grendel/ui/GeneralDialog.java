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
 * Created: Will Scullin <scullin@netscape.com>,  6 Jan 1998.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.ui;

import java.awt.Frame;
import java.util.Locale;
import java.util.ResourceBundle;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;

import javax.swing.JDialog;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

public class GeneralDialog extends JDialog {
  static ResourceBundle fLabels = ResourceBundle.getBundle("grendel.ui.Labels",
                                                          Locale.getDefault());
  protected GeneralDialog fThis;

  private LAFListener     fLAFListener;

  public GeneralDialog(Frame f) {
    super(f);
    init();
  }
  public GeneralDialog(Frame f, boolean m) {
    super(f, m);
    init();
  }
  public GeneralDialog(Frame f, String t) {
    super(f, t);
    init();
  }

  void init() {
    fThis = this;

    fLAFListener = new LAFListener();
    UIManager.addPropertyChangeListener(fLAFListener);
  }

  public void dispose() {
    super.dispose();

    UIManager.removePropertyChangeListener(fLAFListener);
  }

  class LAFListener implements PropertyChangeListener {
    public void propertyChange(PropertyChangeEvent aEvent) {
      SwingUtilities.updateComponentTreeUI(fThis);
      invalidate();
      validate();
      repaint();
    }
  }
}
