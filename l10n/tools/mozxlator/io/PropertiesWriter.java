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

package org.mozilla.translator.io;

import java.io.*;

import java.util.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;

/**
 *
 * @author  Henrik Lynggaard
 * @version 4.1
 */
public class PropertiesWriter extends MozFileWriter 
{
    private static String header ="Translated with MozillaTranslator " + Settings.getString("System.Version","(unknown version)") + "  ";
   
    public PropertiesWriter(MozFile f,OutputStream o)
    {
        super(f,o);
    }
    
    


    public void writeFile(String localeName) throws IOException
    {
        Properties prop = new Properties();
        
        Iterator phraseIterator;
        Phrase currentPhrase;
        Translation currentTranslation;
        String key,text;
        
        phraseIterator = fil.getChildIterator();
        
        while (phraseIterator.hasNext())
        {
            currentPhrase = (Phrase) phraseIterator.next();
            
            key = currentPhrase.getName();
            
            currentTranslation = (Translation) currentPhrase.getChildByName(localeName);
            
            if (currentTranslation == null || localeName.equals("en-us"))
            {
                text = currentPhrase.getText();
            }
            else
            {
                text = currentTranslation.getText();
            }
            
            prop.setProperty(key,text);
        }
        prop.store(os,header);
        os.close();
        
    }
}
