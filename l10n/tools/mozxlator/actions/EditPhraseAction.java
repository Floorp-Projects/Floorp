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

package org.mozilla.translator.actions;

import java.awt.event.*;
import javax.swing.*;
import javax.swing.event.*;

import org.mozilla.translator.gui.dialog.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.datamodel.*;
/** 
 *
 * @author  Henrik Lynggaard Hansen
 * @version 
 */
public class EditPhraseAction extends AbstractAction {

  /** Creates new InstallManagerAction */
  public EditPhraseAction() 
  {
    super("Edit Phrase",null);    
  }
  
  public void actionPerformed(ActionEvent evt)
  {
    MainWindow mw;
    MozFrame mf;
    Phrase ph;
    String name;
    JTable table;
    mw = MainWindow.getDefaultInstance();
    mf = mw.getInnerFrame();
    
    if (mf!=null)
    {
      ph = mf.getSelectedPhrase();
      if (ph!=null)
      {
        name = mf.getLocaleName();
        table = mf.getTable();
        
        table.editingCanceled(new ChangeEvent(this));
        EditPhraseDialog epd = new EditPhraseDialog(); 
    
        epd.visDialog(ph,name);
      }
    }  
  }
}