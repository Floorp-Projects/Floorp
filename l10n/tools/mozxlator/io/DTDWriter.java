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

import org.mozilla.translator.kernel.*;
import org.mozilla.translator.datamodel.*;


/** This class writes *.dtd files in the mozilla Chrome
 * @author Henrik Lynggaard Hansen
 * @version 3.0
 */
public class DTDWriter extends  MozFileWriter
{
 
    private static String header ="<!-- Translated with MozillaTranslator " + Settings.getString("System.Version","(unknown version)") + "  -->";

     /** Creates new DTDWriter
     * @param os The output stream to write to
     */
    public DTDWriter(MozFile f,OutputStream o)
    {
        super(f,o);
    }

    public void writeFile(String localeName) throws IOException
    {
        OutputStreamWriter osw;
        BufferedWriter bw;
        Iterator phraseIterator;
        Phrase currentPhrase;
        Translation currentTranslation;
        String key,text,line;
        osw = new OutputStreamWriter(os,"UTF-8");
        bw = new BufferedWriter(osw);
        
        bw.write(header);
        bw.newLine();
        
        phraseIterator = fil.getChildIterator();
        
        while (phraseIterator.hasNext())
        {
            currentPhrase = (Phrase) phraseIterator.next();
            key = currentPhrase.getName();
            
            currentTranslation = (Translation) currentPhrase.getChildByName(localeName);
            
            if (currentTranslation==null || localeName.equals("en-us"))
            {
                text = currentPhrase.getText();
            }
            else
            {
                text = currentTranslation.getText();
            }
            
            if (text.indexOf("\"")==-1)
            {
                line = "<!ENTITY " +key +" \"" + text +"\">";
            }
            else
            {
                line = "<!ENTITY " +key +" '" + text +"'>";
            }

            bw.write(line,0,line.length());
            bw.newLine();
             
        }
        bw.close();
    }
}