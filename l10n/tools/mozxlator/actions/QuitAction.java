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
import org.mozilla.translator.runners.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.dialog.*;
import org.mozilla.translator.gui.*;
/** 
 *
 * @author  Henrik Lynggaard Hansen
 * @version 
 */
public class QuitAction extends AbstractAction {

  /** Creates new InstallManagerAction */
  public QuitAction() 
  {
    super("Quit",null);    
  }
  
  public void actionPerformed(ActionEvent evt)
  {
    doExit();
  }
  
  public void doExit()
  {
    int result;
    
    
    result = JOptionPane.showConfirmDialog(MainWindow.getDefaultInstance(),"Save glossary before quiting ?","Exit MozillaTranslator",JOptionPane.YES_NO_CANCEL_OPTION,JOptionPane.WARNING_MESSAGE);
    
    if (result==JOptionPane.NO_OPTION)
   {
      Settings.save();
      System.exit(0);
   }
    if (result==JOptionPane.YES_OPTION)
    {
       
        Settings.save();
        SaveGlossaryRunner sgr = new SaveGlossaryRunner();
        sgr.start();
        try
        {
            sgr.join();
            System.exit(0);
        }
        catch (Exception e)
        {
            Log.write("Error with saving during exit");
            Log.write("Exception : " +e);
        }
        
     }
  }
}