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

import java.util.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.translator.gui.dialog.*;
import org.mozilla.translator.gui.*;

import org.mozilla.translator.fetch.*;


/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version
 */
public class KeybindingCheckerAction extends AbstractAction {

    /** Creates new InstallManagerAction */
    public KeybindingCheckerAction()
    {
    super("Check keybindings",null);
    }

    public void actionPerformed(ActionEvent evt)
    {
        List collectedList;
        String localeName;
        ShowWhatDialog swd;
        List cols;
        swd = new ShowWhatDialog();

        if (swd.visDialog())
        {
            localeName = swd.getSelectedLocale();
            cols = swd.getSelectedColumns();

            Fetcher kbf = new FetchKeybinding(localeName);
            collectedList = FetchRunner.getFromGlossary(kbf);
            

            Collections.sort(collectedList);
            ComplexTableWindow ctw = new ComplexTableWindow("String with bad keybinding",collectedList,cols,localeName);
        }


    }
}