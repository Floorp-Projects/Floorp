/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/

 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Henrik Lynggaard Hansen (Initial Code)
 *
 */
package org.mozilla.translator.gui;

import java.awt.*;
import javax.swing.*;

import org.mozilla.translator.kernel.*;
import org.mozilla.translator.actions.*;

/** 
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class MainWindow extends JFrame {

  private static MainWindow instance;
  
  private JLabel statusLine;
  private JDesktopPane desktop;
  private JMenuBar menuBar;
  private JMenu fileMenu;
  private JMenu glossaryMenu;
  private JMenu commandMenu;
  private JMenu viewMenu;
  private JMenu helpMenu;
  
  
  public  static void init()
  {
    instance = new MainWindow();
  }
  
  public static MainWindow getDefaultInstance()
  {
    return instance;
  }
  
  
  /** Creates new MainWindow */
  private MainWindow() 
  {
    super("MozillaTranslator, Version " + Settings.getString("System.Version","X.xx"));
   
    setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
    desktop = new JDesktopPane();
    statusLine = new JLabel("Status: Loading...");
    
    menuBar = new JMenuBar();
    
    // the file menu
    fileMenu = new JMenu("File");
    addMenuItem(fileMenu,new InstallManagerAction(),"");
    fileMenu.addSeparator();
    addMenuItem(fileMenu,new SetupAction(),"");
    fileMenu.addSeparator();
    addMenuItem(fileMenu,new QuitAction(),"");
    
    menuBar.add(fileMenu);
    
    // the glossary menu
    glossaryMenu = new JMenu("Glossary");

    addMenuItem(glossaryMenu , new UpdateInstallAction(), "alt shift U");
    addMenuItem(glossaryMenu , new SaveGlossaryAction(), "alt shift S");
    glossaryMenu.addSeparator();
    addMenuItem(glossaryMenu , new WritePackageAction(), "alt shift I");
    addMenuItem(glossaryMenu , new WriteXpiAction(), "alt shift X"); 
    glossaryMenu.addSeparator();
    addMenuItem(glossaryMenu , new ImportPartialGlossaryAction(), "control shift I");
    addMenuItem(glossaryMenu , new ExportPartialGlossaryAction(), "control shift E");
    glossaryMenu.addSeparator();
    addMenuItem(glossaryMenu , new ImportLocaleAction(),"");
//    addMenuItem(glossaryMenu , new NoOpAction("ImportMozExpAction()"), "");
//    addMenuItem(glossaryMenu , new NoOpAction("ExportMozExpAction()"), "");
    glossaryMenu.addSeparator();
    addMenuItem(glossaryMenu , new ImportOldGlossaryAction(),"");
    menuBar.add(glossaryMenu);
    
    // the Command Menu
    commandMenu = new JMenu("Functions");
    
    addMenuItem(commandMenu , new NoOpAction("SummaryReportAction()"), "");
    addMenuItem(commandMenu , new KeybindingCheckerAction(), "");
    commandMenu.addSeparator();
    addMenuItem(commandMenu , new NoOpAction("HTMLExportAction()"), "");
    commandMenu.addSeparator();
    addMenuItem(commandMenu , new EditPhraseAction(), "alt E");
    
    menuBar.add(commandMenu);
    
    //The view menu
    viewMenu = new JMenu("Views");
    
    addMenuItem(viewMenu , new ChromeViewAction(), "alt C");
    addMenuItem(viewMenu , new UntranslatedViewAction() , "alt U");
    addMenuItem(viewMenu , new RedundantViewAction(), "alt R");
    addMenuItem(viewMenu , new KeepViewAction(), "alt K");
    addMenuItem(viewMenu , new SearchViewAction(), "alt S");
    addMenuItem(viewMenu , new AdvancedSearchAction() , "");
    
    
    menuBar.add(viewMenu);
    
    helpMenu = new JMenu("Help");
    
    addMenuItem(helpMenu, new AboutAction(),"");
    
    
    menuBar.add(helpMenu);
    
    
    // build the window
    setJMenuBar(menuBar);
    
    getContentPane().add(desktop, BorderLayout.CENTER);
    getContentPane().add(statusLine, BorderLayout.SOUTH);
    
    setSize(800,600);
    Utils.placeFrameAtCenter(this);

  }

  public void setStatus(String text)
  {
    statusLine.setText(text);
  }
  
  public void addWindow(JInternalFrame win)
  {
    desktop.add(win);
  }

  
  
  private void addMenuItem(JMenu adMenu,Action action,String key)
  {
    JMenuItem item;
    item = adMenu.add(action);
    if (!key.equals(""))
    {
      item.setAccelerator(KeyStroke.getKeyStroke(key));
    }
  }

  public MozFrame getInnerFrame()
  {
    return (MozFrame) desktop.getSelectedFrame();
  }
  
}