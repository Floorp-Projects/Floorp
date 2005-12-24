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
 * Contributor(s): 
 */

package grendel.ui.prefs;

import java.util.Vector;

import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.Insets;
import java.awt.Rectangle;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JEditorPane;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JPanel;
import javax.swing.JPasswordField;
import javax.swing.JRadioButton;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;

import javax.swing.AbstractListModel;
import javax.swing.ButtonGroup;
import javax.swing.ListSelectionModel;

import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;

import grendel.prefs.base.ServerArray;
import grendel.prefs.base.ServerStructure;
import grendel.prefs.base.IdentityArray;
import grendel.prefs.base.IdentityStructure;

import grendel.ui.StoreFactory;


public class Servers extends JFrame {
  
  JPanel         pane;
  
  JList          list;

  JTextField     tfDesc;
  JComboBox      cbType;

  JTextField     tfHost;
  JTextField     tfPort;
  JTextField     tfUser;
  JPasswordField tfPass;
  JComboBox      cbIdent;
  
  JTextField     tfDir;
  JButton        btChoose;
  
  JRadioButton   rbToLocal;
  JRadioButton   rbAsImap;
  JCheckBox      cbLeave;
  JComboBox      cbStore;

  JLabel         lbDesc;
  JLabel         lbHost;
  JLabel         lbPort;
  JLabel         lbUser;
  JLabel         lbPass;
  JLabel         lbDir;
  JLabel         lbIdent;
  
  ServerArray sva;
  ServerListModel slm;
  int currentSelection = -1;
  
  public static void main(String argv[]) {
    
    Servers servers = new Servers();
    servers.setVisible( true );
    
  }
 
  public Servers() {
    
    super();
    
    sva = ServerArray.GetMaster();

    setSize(500,354);
    setDefaultCloseOperation(DISPOSE_ON_CLOSE);
    
    pane = new JPanel();
    pane.setLayout(null);
    getContentPane().add(pane);
    
    slm = new ServerListModel();
    list = new JList(slm);
    list.setSelectedIndex(0);
    list.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
    list.setBounds(12,12,142,306);
    list.addListSelectionListener(new SelectionChangedListener());
    pane.add(list);
    
    lbDesc = new JLabel("Description");
    lbDesc.setBounds(174,12,lbDesc.getPreferredSize().width,lbDesc.getPreferredSize().height);
    pane.add(lbDesc);
    lbHost = new JLabel("Hostname");
    lbHost.setBounds(174,60,lbHost.getPreferredSize().width,lbHost.getPreferredSize().height);
    pane.add(lbHost);
    lbPort = new JLabel("Port");
    lbPort.setBounds(414,60,lbPort.getPreferredSize().width,lbPort.getPreferredSize().height);
    pane.add(lbPort);
    lbUser = new JLabel("Username");
    lbUser.setBounds(174,92,lbUser.getPreferredSize().width,lbUser.getPreferredSize().height);
    pane.add(lbUser);
    lbPass = new JLabel("Password");
    lbPass.setBounds(334,92,lbPass.getPreferredSize().width,lbPass.getPreferredSize().height);
    pane.add(lbPass);
    lbDir = new JLabel("Directory");
    lbDir.setBounds(174,60,lbDir.getPreferredSize().width,lbDir.getPreferredSize().height);
    pane.add(lbDir);
    lbIdent = new JLabel("Default identity");
    lbIdent.setBounds(174,244,lbIdent.getPreferredSize().width,lbIdent.getPreferredSize().height);
    pane.add(lbIdent);
    
    tfDesc = new JTextField();
    tfDesc.setBounds(254,12,156,tfDesc.getPreferredSize().height);
    pane.add(tfDesc);

    tfHost = new JTextField();
    tfHost.setBounds(244,60,160,tfHost.getPreferredSize().height);
    pane.add(tfHost);
    
    tfPort = new JTextField();
    tfPort.setBounds(442,60,40,tfPort.getPreferredSize().height);
    pane.add(tfPort);

    tfUser = new JTextField();
    tfUser.setBounds(244,92,76,tfUser.getPreferredSize().height);
    pane.add(tfUser);

    tfPass = new JPasswordField();
    tfPass.setBounds(404,92,76,tfPass.getPreferredSize().height);
    pane.add(tfPass);
    
    tfDir = new JTextField();
    tfDir.setBounds(254,60,156,tfDir.getPreferredSize().height);
    pane.add(tfDir);

    btChoose = new JButton("Choose");
    btChoose.setMargin(new Insets(0,0,0,0));
    btChoose.setBounds(422,58,60,btChoose.getPreferredSize().height);
    btChoose.addActionListener(new ChooseDirectoryActionListener());
    pane.add(btChoose);

    rbToLocal = new JRadioButton("Download everything to a local store");
    rbToLocal.setBounds(174,140,rbToLocal.getPreferredSize().width,rbToLocal.getPreferredSize().height);
    pane.add(rbToLocal);
    rbAsImap = new JRadioButton("Manipulate everything on the server");
    rbAsImap.setBounds(174,204,rbAsImap.getPreferredSize().width,rbAsImap.getPreferredSize().height);
    pane.add(rbAsImap);
    
    ButtonGroup bg = new ButtonGroup();
    bg.add(rbToLocal);
    bg.add(rbAsImap);
    
    cbLeave = new JCheckBox("But leave all mail on the server");
    cbLeave.setBounds(190,160,cbLeave.getPreferredSize().width,cbLeave.getPreferredSize().height);
    pane.add(cbLeave);

    Vector ids = new Vector();
    IdentityArray idprefs = IdentityArray.GetMaster();
    for (int i=0; i<idprefs.size(); i++) {
      ids.addElement(idprefs.get(i).getDescription());
    }
    cbIdent = new JComboBox(ids);
    cbIdent.setBounds(270,240,212,cbIdent.getPreferredSize().height);
    pane.add(cbIdent);
    
    JButton button = new JButton("Add New");
    button.setBounds(174,290,68,button.getPreferredSize().height);
    button.addActionListener(new AddNewActionListener());
    button.setMargin(new Insets(0,0,0,0));
    pane.add(button);
    button = new JButton("Delete");
    button.setBounds(254,290,68,button.getPreferredSize().height);
    button.addActionListener(new DeleteActionListener());
    button.setMargin(new Insets(0,0,0,0));
    pane.add(button);
    button = new JButton("Cancel");
    button.setBounds(334,290,68,button.getPreferredSize().height);
    button.addActionListener(new FinishActionListener());
    button.setMargin(new Insets(0,0,0,0));
    pane.add(button);
    button = new JButton("Finish");
    button.setBounds(414,290,68,button.getPreferredSize().height);
    button.addActionListener(new FinishActionListener());
    button.setMargin(new Insets(0,0,0,0));
    pane.add(button);

    String[] data = {"type","local","pop3","imap","news"};
    cbType = new JComboBox(data);
    cbType.addActionListener(new TypeChangedListener());
    cbType.setBounds(422,10,60,cbType.getPreferredSize().height);
    pane.add(cbType);

    update();

  }
  
  private void update() {
    
    String type = "";
    
    if (currentSelection > -1) {
      sva.get(currentSelection).setDescription(tfDesc.getText());
      sva.get(currentSelection).setHost       (tfHost.getText());
      sva.get(currentSelection).setPort       (Integer.parseInt(tfPort.getText()));
      sva.get(currentSelection).setUsername   (tfUser.getText());
      sva.get(currentSelection).setPassword   (new String( tfPass.getPassword() ));
      int index = cbType.getSelectedIndex();
      if (index == 1) { type = "berkeley"; }
      if (index == 2) { type = "pop3"; }
      if (index == 3) { type = "imap"; }
      if (index == 4) { type = "news"; }
      sva.get(currentSelection).setType(index);
      sva.get(currentSelection).setBerkeleyDirectory(tfDir.getText());
      sva.get(currentSelection).setDefaultIdentity(cbIdent.getSelectedIndex());
    }
    
    tfDesc.setText(sva.get(list.getSelectedIndex()).getDescription());
    tfHost.setText(sva.get(list.getSelectedIndex()).getHost());
    tfPort.setText("" + sva.get(list.getSelectedIndex()).getPort());
    tfUser.setText(sva.get(list.getSelectedIndex()).getUsername());
    tfPass.setText(sva.get(list.getSelectedIndex()).getPassword());
    int index = 0;
    type = sva.get(list.getSelectedIndex()).getType();
    if (type.equals("berkeley")) { index = 1; }
    if (type.equals("pop3"    )) { index = 2; }
    if (type.equals("imap"    )) { index = 3; }
    if (type.equals("news"    )) { index = 4; }
    cbType.setSelectedIndex(index);
    tfDir.setText(sva.get(list.getSelectedIndex()).getBerkeleyDirectory());
    cbIdent.setSelectedIndex(sva.get(list.getSelectedIndex()).getDefaultIdentity());
    
    currentSelection = list.getSelectedIndex();
    
  }
  
  void showNothing() {
    tfHost.setVisible( false ); lbHost.setVisible( false );
    tfPort.setVisible( false ); lbPort.setVisible( false );
    tfUser.setVisible( false ); lbUser.setVisible( false );
    tfPass.setVisible( false ); lbPass.setVisible( false );
    cbIdent.setVisible( false );lbIdent.setVisible( false );

    tfDir.setVisible( false );  lbDir.setVisible( false );  btChoose.setVisible( false );
  
    rbToLocal.setVisible( false );
    rbAsImap.setVisible( false );
    cbLeave.setVisible( false );

    pane.repaint(new Rectangle(pane.getSize()));
  }
  
  void showBerkeley() {
    tfHost.setVisible( false ); lbHost.setVisible( false );
    tfPort.setVisible( false ); lbPort.setVisible( false );
    tfUser.setVisible( false ); lbUser.setVisible( false );
    tfPass.setVisible( false ); lbPass.setVisible( false );
setVisible( true );
    tfDir.setVisible( true );  lbDir.setVisible( true );  btChoose.setVisible( true );
  
    rbToLocal.setVisible( false );
    rbAsImap.setVisible( false );
    cbLeave.setVisible( false );

    pane.repaint(new Rectangle(pane.getSize()));
  }
  
  void showPOP3() {
    tfHost.setVisible( true ); lbHost.setVisible( true );
    tfPort.setVisible( true ); lbPort.setVisible( true );
    tfUser.setVisible( true ); lbUser.setVisible( true );
    tfPass.setVisible( true ); lbPass.setVisible( true );
    cbIdent.setVisible( true );lbIdent.setVisible( true );

    tfDir.setVisible( false );  lbDir.setVisible( false );  btChoose.setVisible( false );
  
    rbToLocal.setVisible( true );
    rbAsImap.setVisible( true );
    cbLeave.setVisible( true );

    pane.repaint(new Rectangle(pane.getSize()));
  }
  
  void showIMAP() {
    tfHost.setVisible( true ); lbHost.setVisible( true );
    tfPort.setVisible( true ); lbPort.setVisible( true );
    tfUser.setVisible( true ); lbUser.setVisible( true );
    tfPass.setVisible( true ); lbPass.setVisible( true );
    cbIdent.setVisible( true );lbIdent.setVisible( true );

    tfDir.setVisible( false );  lbDir.setVisible( false );  btChoose.setVisible( false );
  
    rbToLocal.setVisible( false );
    rbAsImap.setVisible( false );
    cbLeave.setVisible( false );

    pane.repaint(new Rectangle(pane.getSize()));
  }
  
  void showNews() {
    tfHost.setVisible( true ); lbHost.setVisible( true );
    tfPort.setVisible( true ); lbPort.setVisible( true );
    tfUser.setVisible( false ); lbUser.setVisible( false );
    tfPass.setVisible( false ); lbPass.setVisible( false );
    cbIdent.setVisible( true );lbIdent.setVisible( true );

    tfDir.setVisible( false );  lbDir.setVisible( false );  btChoose.setVisible( false );
  
    rbToLocal.setVisible( false );
    rbAsImap.setVisible( false );
    cbLeave.setVisible( false );
    
    pane.repaint(new Rectangle(pane.getSize()));
  }
  
  void chooseDir() {
    JFileChooser fc = new JFileChooser();
    fc.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
    if (fc.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
      tfDir.setText(fc.getSelectedFile().getAbsolutePath());
    }
  }
    
  class SelectionChangedListener implements ListSelectionListener {
  
    public void valueChanged(ListSelectionEvent e) {
       
      if (e.getValueIsAdjusting()) {
      	
        update();
        
      }
       
    }
  	
  }
  
  class AddNewActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      sva.add(new ServerStructure("New Server"));
      slm.fireAdded(sva.size()-1);
      list.setSelectedIndex(sva.size()-1);
      update();
    	
    }
  
  }
  
  class DeleteActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
    	int index = list.getSelectedIndex();
      sva.remove(index);
      slm.fireRemoved(index);
      currentSelection = -1;
      if (index >= sva.size()) { 
        index = sva.size()-1; 
      }
      if (sva.size() <= 0) {
        sva.add(new ServerStructure("New Server"));
        index = 0;
      }
      list.setSelectedIndex(index);
      update();
    	
    }
  
  }
  
  class FinishActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      update();
      sva.writePrefs();
      StoreFactory.Instance().refreshStores();
      setVisible( false );
      dispose();
    	
    }
  
  }
  
  class CancelActionListener implements ActionListener {
  
    public void actionPerformed(ActionEvent e) {
    	
      sva.readPrefs();
      setVisible( false );
      dispose();
    	
    }
  
  }
  
  class TypeChangedListener implements ActionListener {
    
    public void actionPerformed(ActionEvent e) {
      int index = cbType.getSelectedIndex();
      if (index == 0) { showNothing(); }
      if (index == 1) { showBerkeley(); }
      if (index == 2) { showPOP3(); }
      if (index == 3) { showIMAP(); }
      if (index == 4) { showNews(); }
    }
    
  }
  
  class ChooseDirectoryActionListener implements ActionListener {
    
    public void actionPerformed(ActionEvent e) {
      chooseDir();
    }
    
  }
  
  class ServerListModel extends AbstractListModel {
    
    public Object getElementAt(int index) {
      return sva.get(index).getDescription();
    }
    
    public int getSize() {
      return sva.size();
    }
    
    public void fireAdded(int index) {
      fireIntervalAdded(this, index, index);
    }
    
    public void fireRemoved(int index) {
      fireIntervalRemoved(this, index, index);
    }
    
  }
  
}
