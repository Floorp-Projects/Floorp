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
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.datamodel.*;


/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.01

 */
public class RedundantViewAction extends AbstractAction {

    /** Creates new RedundantViewAction */
    public RedundantViewAction()
    {
        super("Reddundant Strings",null);
    }

    public void actionPerformed(ActionEvent evt)
    {

        List collectedList;
        List redundant;
        List all;
        
        Iterator installIterator;
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;

        MozInstall spaceInstall = new MozInstall("","");
        MozComponent spaceComponent = new MozComponent("",spaceInstall);
        MozComponent spaceSubcomponent = new MozComponent("",spaceComponent);
        MozFile spaceFile = new MozFile("",spaceSubcomponent);
        Phrase spacePhrase = new Phrase("",spaceFile,"","",false);
        
        Glossary glos;
        MozInstall currentInstall;
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase,testPhrase;
        
        String localeName;
        
        ShowWhatDialog swd;
        List cols;

        swd = new ShowWhatDialog();

        if (swd.visDialog())
        {
            localeName = swd.getSelectedLocale();
            cols = swd.getSelectedColumns();
            collectedList = new ArrayList();
            all = new ArrayList();

            // select the correct phrases
            glos = Glossary.getDefaultInstance();
            installIterator = glos.getChildIterator();
            
            
            // first we all all known phrases to a big list
            
            while (installIterator.hasNext())
            {
                currentInstall = (MozInstall) installIterator.next();
                componentIterator = currentInstall.getChildIterator();
                
                while (componentIterator.hasNext())
                {
                    currentComponent = (MozComponent) componentIterator.next();
                    subcomponentIterator = currentComponent.getChildIterator();
                    
                    while (subcomponentIterator.hasNext())
                    {
                        currentSubcomponent = (MozComponent) subcomponentIterator.next();
                        fileIterator = currentSubcomponent.getChildIterator();
                        
                        while (fileIterator.hasNext())
                        {
                            currentFile = (MozFile) fileIterator.next();
                            
                            all.addAll(currentFile.getAllChildren());
                        }
                    }
                }
            }
            
            // check for redundant original texts
            
            while (all.size()>1)
            {
                phraseIterator = all.iterator();
                redundant = new ArrayList();
                
                testPhrase = (Phrase) phraseIterator.next();
                phraseIterator.remove();
                redundant.add(testPhrase);
                
                while (phraseIterator.hasNext())
                {
                    currentPhrase = (Phrase) phraseIterator.next();
                    
                    if (testPhrase.getText().equalsIgnoreCase(currentPhrase.getText()))
                    {
                        redundant.add(currentPhrase);
                        phraseIterator.remove();
                    }
                }
                
                if (redundant.size()>1)
                {
                    collectedList.addAll(redundant);
                    collectedList.add(spacePhrase);
                }
            }
            
            // display the phrases
            ComplexTableWindow ctw = new ComplexTableWindow("Untranslated strings",collectedList,cols,localeName);
        }
    }
}