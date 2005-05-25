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
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Mauro Botelho <mabotelh@bellsouth.net>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.Rectangle;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.net.URL;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.MissingResourceException;
import java.util.Properties;
import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.swing.Action;
import javax.swing.ButtonGroup;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenuBar;
/*
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JRadioButtonMenuItem;
import javax.swing.JSeparator;
*/
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JToolBar;
import javax.swing.SwingUtilities;
import javax.swing.SwingConstants;
import javax.swing.UIManager;
import javax.swing.BoxLayout;

import grendel.prefs.base.InvisiblePrefs;
import grendel.widgets.*;

/**
 *This class provides a JFrame containing the default GUI for Grendel windows,
 *such as the menubar, toolbar, etc.
 *
 *<p>Also manages Frame references so that frames are not required to be
 *reinitialized. This is done through frame IDs and the static method
 *FindFrameById.</p>
 */
public class GeneralFrame extends JFrame
{
  GeneralFrame            fThis;

  BiffIcon fBiffIcon;

  protected Container     fPanel;
  protected Animation     fAnimation;
  protected JMenuBar      fMenu;
  protected CollapsiblePanel        fToolBarPanel;
  protected GridBagConstraints fToolBarPanelConstraints;
  protected GrendelToolBar      fToolBar;
  protected Component     fStatusBar;
  protected String        fResourceBase = "grendel.ui";
  protected String        fID;
  protected JLabel        fStatusLabel;

  //  protected netscape.orion.uimanager.UIManager fUIManager;

  private LAFListener     fLAFListener;
  private Properties      prop;

  static Vector fFrameList = new Vector();
  static boolean sExternalShell = false;

  protected ResourceBundle fLabels =
    ResourceBundle.getBundle("grendel.ui.Labels", getLocale());

  public GeneralFrame(String aTitle, String aID) {
    fThis = this;
    fID = aID;

    String title = aTitle;
    try {
      title = fLabels.getString(title);
    } catch (MissingResourceException e) {}

    setTitle(title);

    addWindowListener(new WindowAdapter() {
      public void windowClosing(WindowEvent e) {
        dispose();
      }
    });

    updateUI();
    fLAFListener = new LAFListener();
    UIManager.addPropertyChangeListener(fLAFListener);

    fPanel = getContentPane();

    fAnimation = new Animation();
    fAnimation.setImageTemplate("ui/images/animation/Contest{0,number,00}.gif",
                                30);

    fToolBarPanel = new CollapsiblePanel(true);
    fToolBarPanelConstraints = new GridBagConstraints();
    fPanel.add(fToolBarPanel, BorderLayout.NORTH);
    //    fUIManager = new netscape.orion.uimanager.UIManager(fToolBarPanel);

    // We need to use Class.forName because getClass() might return a child
    // class in another package.

    ImageIcon curImage = new ImageIcon("ui/images/GrendelIcon32.gif");
    setIconImage(curImage.getImage());

    fFrameList.addElement(this);
  }

  public void dispose() {
    if (fBiffIcon != null) {
      fBiffIcon.dispose();
    }

    fFrameList.removeElement(this);

    super.dispose();

    if (!sExternalShell && fFrameList.size() == 0) {
      ActionFactory.GetExitAction().actionPerformed(null);
      System.out.println("Exiting...");

    }

    UIManager.removePropertyChangeListener(fLAFListener);
  }

  public void updateUI() {
    setBackground(UIManager.getColor("control"));
    getContentPane().setBackground(UIManager.getColor("control"));
  }
  /**
   * Sets the frame's id.
   */

  public void setID(String aID) {
    fID = aID;
  }

  /**
   * Returns the frame's id.
   */

  public String getID() {
    return fID;
  }

  /**
   * Finds the last created frame of a given id.
   */

  public static GeneralFrame FindFrameByID(String aID) {
    return FindFrameByID(aID, null);
  }

  /**
   * Finds the last created frame of a given id.
   */

  public static GeneralFrame FindFrameByID(String aID, GeneralFrame aExclude) {
    for (int i = fFrameList.size() - 1; i >= 0; i--) {
      GeneralFrame frame = (GeneralFrame) fFrameList.elementAt(i);
      if (frame.getID().equals(aID) && frame != aExclude) {
        return frame;
      }
    }
    return null;
  }

  /**
   * Returns a frame for use by dialogs
   */

  static public GeneralFrame GetDefaultFrame() {
    if (fFrameList.size() > 0) {
      return (GeneralFrame) fFrameList.elementAt(0);
    }
    return null;
  }

  /**
   * Sets whether or not this frame is working with an external application.
   * Important to prevent exiting the system when all our frames close.
   */

  static public void SetExternalShell(boolean aShell) {
    sExternalShell = aShell;
  }

  static public boolean IsExternalShell() {
    return sExternalShell;
  }

  static public GeneralFrame[] GetFrameList() {
    GeneralFrame res[] = new GeneralFrame[fFrameList.size()];
    fFrameList.copyInto(res);

    return res;
  }

  static public synchronized void CloseAllFrames() {
    while (fFrameList.size() > 0) {
      GeneralFrame frame = (GeneralFrame) fFrameList.elementAt(0);
      frame.dispose();
    }
  }

  protected Component buildStatusBar() {
    JPanel res = new JPanel();
    res.setLayout(new BoxLayout(res, BoxLayout.X_AXIS));

    fBiffIcon = new BiffIcon();
    fBiffIcon.setSize(fBiffIcon.getPreferredSize());

    fStatusLabel = new JLabel("Grendel");
    fStatusLabel.setFont(Font.decode("Dialog-12"));

    res.add(fStatusLabel);
    res.add(fBiffIcon);

    return res;
  }

  protected void startAnimation() {
    fAnimation.start();
  }

  protected void stopAnimation() {
    fAnimation.stop();
  }

  protected void setStatusText(String aString) {
    if (fStatusLabel != null) {
      fStatusLabel.setText(aString);
    }
  }

  private void saveBounds(String aName) {
    InvisiblePrefs.GetMaster().setBounds(aName, getBounds());
  }

  protected void saveBounds() {
    saveBounds(fID);
  }

  private void restoreBounds(String aName, int aWidth, int aHeight) {
    setBounds(InvisiblePrefs.GetMaster().getBounds(aName, aWidth, aHeight));
  }

  protected void restoreBounds(int aWidth, int aHeight) {
    GeneralFrame frame = FindFrameByID(fID, this);
    if (frame == null) {
      restoreBounds(fID, aWidth, aHeight);
    } else {
      Rectangle bounds = frame.getBounds();
      Insets insets = frame.getInsets();
      setBounds(bounds.x + insets.top, bounds.y + insets.top,
                bounds.width, bounds.height);
    }
  }

  protected void restoreBounds() {
    Dimension screenSize = getToolkit().getScreenSize();

    restoreBounds(screenSize.width * 2 / 3,
                  screenSize.height * 2 / 3);
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
