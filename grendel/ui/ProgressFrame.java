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
 * Created: Will Scullin <scullin@netscape.com>, 26 Sep 1997.
 */

package grendel.ui;

import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.Panel;
import java.awt.event.ActionEvent;
import java.util.Locale;
import java.util.ResourceBundle;

import javax.swing.AbstractAction;
import javax.swing.Box;
import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JProgressBar;
import javax.swing.UIManager;

public abstract class ProgressFrame extends GeneralFrame implements Runnable {
  int             fMax;
  JProgressBar    fMeter;
  JButton         fCancel;
  JLabel          fStatus;
  Thread          fThread;
  boolean         fDone;
  boolean         fCanceled;
  boolean         fDisposed;
  Container       fContentPanel;

  public ProgressFrame(String aCaption) {
    super("", "mail.progress");

    setTitle(fLabels.getString(aCaption));
    setBackground(UIManager.getColor("control"));

    fContentPanel = new Panel();
    getContentPane().add(fContentPanel);

    GridBagLayout gridbag = new GridBagLayout();
    GridBagConstraints c = new GridBagConstraints();

    fContentPanel.setLayout(gridbag);

    fStatus = new JLabel("Status");
    fMeter = new JProgressBar();
    fMeter.setMinimum(0);
    fMeter.setMaximum(1);
    fMeter.setValue(0);
    fCancel = new JButton("Cancel");
    fCancel.addActionListener(new AbstractAction() {
      public void actionPerformed(ActionEvent aEvent) {
        setCanceled(true);
      }
    });

    c.weightx = 1;
    c.weighty = 1;
    c.gridheight = 1;

    c.fill = GridBagConstraints.HORIZONTAL;
    c.gridwidth = GridBagConstraints.REMAINDER;
    c.insets = new Insets(5,5,5,5);
    gridbag.setConstraints(fStatus, c);
    fContentPanel.add(fStatus);

    c.fill = GridBagConstraints.HORIZONTAL;
    c.gridwidth = GridBagConstraints.REMAINDER;
    c.insets = new Insets(0,5,0,5);
    gridbag.setConstraints(fMeter, c);
    fContentPanel.add(fMeter);

    c.fill = GridBagConstraints.NONE;
    c.gridwidth = 1;
    c.insets = new Insets(5,5,5,5);
    gridbag.setConstraints(fCancel, c);
    fContentPanel.add(fCancel);

    // Dimension size = getPreferredSize();
    restoreBounds(320, 200);
    setVisible(true);
  }

  public synchronized void dispose() {
    if (!fDisposed) {
      super.dispose();

      fDisposed = true;

      setCanceled(true);
    }
  }

  public synchronized void setMax(int aMax) {
    fMax = aMax;
    fMeter.setMaximum(aMax);
  }

  public synchronized int getMax() {
    return fMax;
  }

  public synchronized void setProgress(int aProgress) {
    fMeter.setValue(aProgress);
  }

  public synchronized int getProgress() {
    return fMeter.getValue();
  }

  public synchronized void setStatus(String aStatus) {
    fStatus.setText(aStatus);
  }

  public synchronized String getStatus() {
    return fStatus.getText();
  }

  public synchronized void setDone(boolean aDone) {
    fDone = aDone;
  }

  public synchronized boolean getDone() {
    return fDone;
  }

  public synchronized void setCanceled(boolean aCanceled) {
    fCanceled = aCanceled;
  }

  public synchronized boolean isCanceled() {
    return fCanceled;
  }

  public void start() {
    fThread = new Thread(this);
    fThread.start();
  }

  public void run() {
    progressLoop();

    fThread = null;
    this.dispose();
  }

  public abstract void progressLoop();
}
