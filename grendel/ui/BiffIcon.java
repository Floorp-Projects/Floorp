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
 * Created: Will Scullin <scullin@netscape.com>, 19 Sep 1997.
 */

package grendel.ui;

import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JLabel;
import javax.swing.event.ChangeListener;
import javax.swing.event.ChangeEvent;

import grendel.storage.MailDrop;

/**
 * This is a simple biff label. It watches the BiffThread
 * to update its state.
 */

class BiffIcon extends JLabel implements ChangeListener {
  BiffThread fThread;

  static Icon fIconUnknown;
  static Icon fIconNew;
  static Icon fIconNone;

  public BiffIcon() {
    if (fIconUnknown == null) {
      fIconUnknown =
        new ImageIcon(getClass().getResource("images/biffUnknown.gif"));
      fIconNew =
        new ImageIcon(getClass().getResource("images/biffNew.gif"));
      fIconNone =
        new ImageIcon(getClass().getResource("images/biffNone.gif"));
    }
    setText(null); // Necessary for icon size to be used for preferred size
    setBiffState(MailDrop.UNKNOWN);

    fThread = BiffThread.Get();
    fThread.addChangeListener(this);

    setBiffState(fThread.getBiffState());
  }

  public void dispose() {
    fThread.removeChangeListener(this);
  }

  public void stateChanged(ChangeEvent aEvent) {
    setBiffState(fThread.getBiffState());
    repaint();
  }

  void setBiffState(int aState) {
    switch (aState) {
    case MailDrop.NEW:
      setIcon(fIconNew);
      break;
    case MailDrop.NONE:
      setIcon(fIconNone);
      break;
    case MailDrop.UNKNOWN:
      setIcon(fIconUnknown);
      break;
    }
  }
}
