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

/**
 *
 * @author  Henrik
 * @version 4.15
 */
public class AboutAction extends AbstractAction {
  

    /** Creates new AboutAction */
    public AboutAction()
    {
        super("About MozillaTranslator",null);
    }

    public void actionPerformed(ActionEvent evt)
    {
          
        AboutDialog ad = new AboutDialog();
        ad.visDialog();
    }
}