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
 * The Initial Developer of the Original Code is Edwin Woudt 
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s):  Jeff Galyan <talisman@anamorphic.com> 
 */

package grendel.prefs.ui;

import java.util.Vector;

import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.Insets;
import java.awt.Rectangle;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import java.net.URL;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JRadioButton;
import javax.swing.JTextField;

import javax.swing.AbstractListModel;
import javax.swing.ButtonGroup;
import javax.swing.ComboBoxModel;
import javax.swing.ImageIcon;
import javax.swing.LookAndFeel;
import javax.swing.UIManager;

import grendel.prefs.base.UIPrefs;

import grendel.ui.UnifiedMessageDisplayManager;


public class UI extends JFrame {

  private static boolean DEBUG = false;

  UIPrefs prefs = UIPrefs.GetMaster();

  JRadioButton rb1, rb2, rb3, rb4, rb5;
  JCheckBox    cbTooltips;
  JComboBox    cbLandF;
  // BUGFIX: Fonts were too big (may be specific to Linux JVM,
  //         but this is a safe bet for a Font). --Jeff
  Font baseFont = new Font("Helvetica", Font.PLAIN, 12);
  
  public static void main(String argv[]) {
    
    UI ui = new UI();
    // Edwin: the show() method is deprecated as of JDK 1.1
    // use setVisible(boolean) instead --Jeff
    ui.setVisible(true);
    
  }
 
  public UI() {
    
    super();
    // Edwin: explicitly setting the window size and using a null layout
    // manager to hardcode pixel coordinates for UI components is by 
    // definition not cross-platform compatible. Consider using a 
    // GridBagLayout instead. Also, the layout isn't very consistent in terms
    // of placement of components. --Jeff
    setSize(500,354);
    setDefaultCloseOperation(DISPOSE_ON_CLOSE);
    getContentPane().setLayout(null);

    JLabel label = new JLabel("Choose your window layout:");
    label.setFont(baseFont);
    label.setBounds(12,12,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Look and Feel");
    label.setFont(baseFont);
    label.setBounds(12,148,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Note: Grendel must be restarted to see any changes");
    label.setFont(baseFont);
    label.setBounds(12,294,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);

    JLabel icon;
    
    icon = new JLabel(new ImageIcon("prefs/ui/images/multi.gif"));
    icon.setBounds(48,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    icon = new JLabel(new ImageIcon("prefs/ui/images/right.gif"));
    icon.setBounds(138,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    icon = new JLabel(new ImageIcon("prefs/ui/images/top.gif"));
    icon.setBounds(228,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    icon = new JLabel(new ImageIcon("prefs/ui/images/left.gif"));
    icon.setBounds(318,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    icon = new JLabel(new ImageIcon("prefs/ui/images/stacked.gif"));
    icon.setBounds(408,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    
    rb1 = new JRadioButton("");
    rb1.setBounds(26,44,rb1.getPreferredSize().width,rb1.getPreferredSize().height);
    getContentPane().add(rb1);
    rb2 = new JRadioButton("");
    rb2.setBounds(116,44,rb2.getPreferredSize().width,rb2.getPreferredSize().height);
    getContentPane().add(rb2);
    rb3 = new JRadioButton("");
    rb3.setBounds(206,44,rb3.getPreferredSize().width,rb3.getPreferredSize().height);
    getContentPane().add(rb3);
    rb4 = new JRadioButton("");
    rb4.setBounds(296,44,rb4.getPreferredSize().width,rb4.getPreferredSize().height);
    getContentPane().add(rb4);
    rb5 = new JRadioButton("");
    rb5.setBounds(386,44,rb5.getPreferredSize().width,rb5.getPreferredSize().height);
    getContentPane().add(rb5);
    
    ButtonGroup bg = new ButtonGroup();
    bg.add(rb1); bg.add(rb2); bg.add(rb3); bg.add(rb4); bg.add(rb5);

    cbTooltips = new JCheckBox("Show tooltips");
    cbTooltips.setFont(baseFont);
    cbTooltips.setBounds(12,100,cbTooltips.getPreferredSize().width, cbTooltips.getPreferredSize().height);
    getContentPane().add(cbTooltips);

    cbLandF = new JComboBox(new LAFListModel());
    cbLandF.setFont(baseFont);
    cbLandF.setBounds(100,144,300,cbLandF.getPreferredSize().height);
    getContentPane().add(cbLandF);

    JButton button = new JButton("Cancel");
    button.setFont(baseFont);
    button.setBounds(334,290,68,button.getPreferredSize().height);
    button.addActionListener(new CancelActionListener());
    button.setMargin(new Insets(0,0,0,0));
    getContentPane().add(button);
    button = new JButton("Finish");
    button.setFont(baseFont);
    button.setBounds(414,290,68,button.getPreferredSize().height);
    button.addActionListener(new FinishActionListener());
    button.setMargin(new Insets(0,0,0,0));
    getContentPane().add(button);

    getData();
  }
  
  void getData() {
    if (prefs.getDisplayManager().equals("multi")) {
      rb1.setSelected(true);
    } else {
      if (prefs.getMultiPaneLayout().equals(UnifiedMessageDisplayManager.SPLIT_RIGHT)) {
        rb2.setSelected(true);
      }
      if (prefs.getMultiPaneLayout().equals(UnifiedMessageDisplayManager.SPLIT_TOP)) {
        rb3.setSelected(true);
      }
      if (prefs.getMultiPaneLayout().equals(UnifiedMessageDisplayManager.SPLIT_LEFT)) {
        rb4.setSelected(true);
      }
      if (prefs.getMultiPaneLayout().equals(UnifiedMessageDisplayManager.STACKED)) {
        rb5.setSelected(true);
      }
    }
    if (prefs.getTooltips()) {
      cbTooltips.setSelected(true);
    }
  }
  
  void setData() {
    if (rb1.isSelected()) {
      prefs.setDisplayManager("multi");
    }
    if (rb2.isSelected()) {
      prefs.setDisplayManager("unified");
      prefs.setMultiPaneLayout(UnifiedMessageDisplayManager.SPLIT_RIGHT);
    }
    if (rb3.isSelected()) {
      prefs.setDisplayManager("unified");
      prefs.setMultiPaneLayout(UnifiedMessageDisplayManager.SPLIT_TOP);
    }
    if (rb4.isSelected()) {
      prefs.setDisplayManager("unified");
      prefs.setMultiPaneLayout(UnifiedMessageDisplayManager.SPLIT_LEFT);
    }
    if (rb5.isSelected()) {
      prefs.setDisplayManager("unified");
      prefs.setMultiPaneLayout(UnifiedMessageDisplayManager.STACKED);
    }
    if (cbTooltips.isSelected()) {
      prefs.setTooltips(true);
    } else {
      prefs.setTooltips(false);
    }
    prefs.setLookAndFeel((String)cbLandF.getSelectedItem());
  }
  
  class FinishActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      setData();
      prefs.writePrefs();
      // Edwin: the hide() method is deprecated as of JDK 1.1
      // use setVisible(boolean) instead --Jeff
      setVisible(false);
      dispose();
    	
    }
  
  }  
  
  class CancelActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      setVisible(false);
      dispose();
    	
    }
  
  }

  class LAFListModel extends AbstractListModel implements ComboBoxModel {
    LookAndFeel fLAFs[] = null;
    Object selection = null;
    String os_name = System.getProperty("os.name");

    LAFListModel() {
      UIManager.LookAndFeelInfo[] info = 
        UIManager.getInstalledLookAndFeels();
      Vector LAFVec = new Vector();
      for (int i = 0; i < info.length; i++) {
        try {
          String name = info[i].getClassName();
          if ((name.endsWith("WindowsLookAndFeel") && !os_name.startsWith("Win")) 
              || (name.endsWith("MacLookAndFeel") && !os_name.startsWith("Mac"))) {
            // don't add it to the list
          } else {
            Class c = Class.forName(name);
            LAFVec.addElement((LookAndFeel)c.newInstance());
          }
        } catch (Exception e){
          // Edwin: please at least provide a static private boolean at 
          // class scope to test if we want debug output so if we get 
          // exceptions where we don't expect them, we can handle them
          // gracefully (and hopefully find out what's causing them).
          // We can always set DEBUG to "false" for a release.  --Jeff
          if (DEBUG) {
            e.printStackTrace();
          }
        }
      }
      fLAFs = new LookAndFeel[LAFVec.size()];
      for (int j = 0; j < LAFVec.size(); j++) {
        fLAFs[j] = (LookAndFeel)LAFVec.elementAt(j);
        if (fLAFs[j].getDescription().equals(prefs.getLookAndFeel())) {
          selection = fLAFs[j].getDescription();
        }
      }
      setFont(baseFont);
    }

    public int getSize() {
      if (fLAFs != null) {
        return fLAFs.length;
      }
      return 0;
    }

    public Object getElementAt(int index) {
      if (fLAFs != null && index < fLAFs.length) {
        // this is a hack. the toString() returns a string which is
        // best described as "unwieldly"
        // Actually, I would say this is the right way to do this. --Jeff
        return fLAFs[index].getDescription();
      }
      return null;
    }

    public void setSelectedItem(Object anItem) {
      selection = anItem;
    }
    
    public Object getSelectedItem() {
      return selection;
    }
  }
  
}
