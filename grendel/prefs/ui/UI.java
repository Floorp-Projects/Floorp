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
 * The Initial Developer of the Original Code is Edwin Woudt 
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are 
 * Copyright (C) 1999 Edwin Woudt.  All Rights Reserved.
 *
 * Contributors: 
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

  UIPrefs prefs = UIPrefs.GetMaster();

  JRadioButton rb1, rb2, rb3, rb4, rb5;
  JCheckBox    cbTooltips;
  JComboBox    cbLandF;
  
  public static void main(String argv[]) {
    
    UI ui = new UI();
    ui.show();
    
  }
 
  public UI() {
    
    super();
    
    setSize(500,354);
    setDefaultCloseOperation(DISPOSE_ON_CLOSE);
    getContentPane().setLayout(null);

    JLabel label = new JLabel("Choose your window layout:");
    label.setBounds(12,12,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Look and Feel");
    label.setBounds(12,148,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);
    label = new JLabel("Note: Grendel must be restarted to see any changes");
    label.setBounds(12,294,label.getPreferredSize().width,label.getPreferredSize().height);
    getContentPane().add(label);

    JLabel icon; URL iconUrl;
    
    iconUrl = getClass().getResource("images/multi.gif");
    icon = new JLabel(new ImageIcon(iconUrl));
    icon.setBounds(48,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    iconUrl = getClass().getResource("images/right.gif");
    icon = new JLabel(new ImageIcon(iconUrl));
    icon.setBounds(138,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    iconUrl = getClass().getResource("images/top.gif");
    icon = new JLabel(new ImageIcon(iconUrl));
    icon.setBounds(228,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    iconUrl = getClass().getResource("images/left.gif");
    icon = new JLabel(new ImageIcon(iconUrl));
    icon.setBounds(318,44,icon.getPreferredSize().width,icon.getPreferredSize().height);
    getContentPane().add(icon);
    iconUrl = getClass().getResource("images/stacked.gif");
    icon = new JLabel(new ImageIcon(iconUrl));
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
    cbTooltips.setBounds(12,100,cbTooltips.getPreferredSize().width, cbTooltips.getPreferredSize().height);
    getContentPane().add(cbTooltips);

    cbLandF = new JComboBox(new LAFListModel());
    cbLandF.setBounds(100,144,300,cbLandF.getPreferredSize().height);
    getContentPane().add(cbLandF);

    JButton button = new JButton("Cancel");
    button.setBounds(334,290,68,button.getPreferredSize().height);
    button.addActionListener(new CancelActionListener());
    button.setMargin(new Insets(0,0,0,0));
    getContentPane().add(button);
    button = new JButton("Finish");
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
      hide();
      dispose();
    	
    }
  
  }  
  
  class CancelActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      hide();
      dispose();
    	
    }
  
  }

  class LAFListModel extends AbstractListModel implements ComboBoxModel {
    LookAndFeel fLAFs[] = null;
    Object selection = null;

    LAFListModel() {
      UIManager.LookAndFeelInfo[] info = 
        UIManager.getInstalledLookAndFeels();
      fLAFs = new LookAndFeel[info.length];
      for (int i = 0; i < info.length; i++) {
        try {
          String name = info[i].getClassName();
          Class c = Class.forName(name);
          fLAFs[i] = (LookAndFeel)c.newInstance();
          if (fLAFs[i].getDescription().equals(prefs.getLookAndFeel())) {
            selection = fLAFs[i].getDescription();
          }
        } catch (Exception e){
        }
      }
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
