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

import org.mozilla.translator.gui.dialog.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.runners.*;
import org.mozilla.translator.kernel.*;
/** 
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class UpdateInstallAction extends AbstractAction {

  /** Creates new InstallManagerAction */
  public UpdateInstallAction() 
  {
    super("Update Install",null);    
  }
  
  public void actionPerformed(ActionEvent evt)
  {
    Object selected;
    UpdateInstallRunner ugr;
    Object[] installArray;
    
    installArray =Glossary.getDefaultInstance().toArray();

    if (installArray.length>0)
    {
      selected = JOptionPane.showInputDialog(MainWindow.getDefaultInstance(),"Select Install to update","Update installation",JOptionPane.QUESTION_MESSAGE,null,installArray,installArray[0]);
    
      if (selected!=null)
      {
        ugr = new UpdateInstallRunner((MozInstall) selected);
        ugr.start();
      }
    }
  }
}