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

/** This class reads *.properties files from the mozilla chrome
 * @author Henrik Lynggaard Hansen
 * @version 3.0
 */
public class PropertiesReader extends MozFileReader {

    public PropertiesReader(MozFile f,InputStream i)
    {
        super(f,i);
    }

    public void readFile(String localeName) throws IOException
    {
        Properties prop;
        Enumeration enum;
        String key,text;
        Phrase currentPhrase;
        Translation currentTranslation;
        
        prop = new Properties();
        prop.load(is);
        is.close();
        
        enum = prop.propertyNames();

        while (enum.hasMoreElements())
        {
            key = (String) enum.nextElement();
            text = prop.getProperty(key);

            currentPhrase = (Phrase) fil.getChildByName(key);
            
            if (localeName.equalsIgnoreCase("en-us"))
            {
                if (currentPhrase==null )
                {
                    currentPhrase = new Phrase(key,fil,text,"",false);
                    fil.addChild(currentPhrase);
                }
                else
                {
                    currentPhrase.setText(text);
                }
                currentPhrase.setMarked(true);                
                
            }
            else
            {
                if (currentPhrase!=null)
                {
                    currentTranslation = (Translation) currentPhrase.getChildByName(localeName);
                    if (currentTranslation==null)
                    {
                        currentTranslation = new Translation(localeName,currentPhrase,text);
                        currentPhrase.addChild(currentTranslation);
                    }
                    else
                    {
                        currentTranslation.setText(text);
                    }
                }
            }    
        }
    }
}