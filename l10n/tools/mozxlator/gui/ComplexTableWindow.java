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

import javax.swing.*;
import java.util.*;

import org.mozilla.translator.gui.models.*;
import org.mozilla.translator.datamodel.*;
/** 
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class ComplexTableWindow extends JInternalFrame implements MozFrame{

  private JTable table;
  private JScrollPane scroll;
  private ComplexTableModel model;
  
  
  /** Creates new ComplexTableWindow */
  public ComplexTableWindow(String title,List toModel,List cols,String localeName) 
  {
    super(title);
    setClosable(true);
    setMaximizable(true);
    setIconifiable(true);
    setResizable(true);
    
    model  = new ComplexTableModel(toModel,cols,localeName);
    table  = new JTable(model);
    scroll = new JScrollPane(table);
    
    getContentPane().add(scroll,"Center");
    pack();
    MainWindow.getDefaultInstance().addWindow(this);
    setVisible(true);
  }
  
  public Phrase getSelectedPhrase() 
  {
      int rowIndex;
      rowIndex = table.getSelectedRow();
      return model.getRow(rowIndex);    
  }
  
  public String getLocaleName() 
  {
    return model.getLocaleName();
  }  

    public JTable getTable() 
    {
        return table;
    }
}