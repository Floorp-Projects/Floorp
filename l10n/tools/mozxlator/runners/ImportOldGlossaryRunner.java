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
package org.mozilla.translator.runners;

import java.io.*;
import java.util.*;
import java.util.zip.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.kernel.*;


/**
 *
 * @author  Henrik Lynggaard Hansen
 * @version 4.0
 */
public class ImportOldGlossaryRunner extends Thread {

    private MozInstall install;
    private String glosFile;

    /** Creates new ImportOldGlossary */
    public ImportOldGlossaryRunner(MozInstall i,String glos)
    {
        install =i;
        glosFile = glos;
    }

    public void run()
    {

        Properties prop;
        FileInputStream fis;
        ZipInputStream zis;
        ZipEntry ze;
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase;
        Translation currentTranslation;
        MozComponent allFiles;

        int componentCount;
        int fileCount;
        String compPrefix;
        String filePrefix;
        String fileName;
        String phrasePrefix;
        String localePrefix;
        String key;
        int phraseCount;
        String locales,singleLocale;
        int qa;
        StringTokenizer localeTokenizer;
        String translated,comment;
        MainWindow vindue = MainWindow.getDefaultInstance();

        prop = new Properties();
        allFiles = new MozComponent("temp",null);
        // build a all file list of install
        vindue.setStatus("Building list of all files");
        try
        {
            componentIterator = install.getChildIterator();

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
                        allFiles.addChild((MozFile) fileIterator.next());
                    }
                }
            }

            // load old glossary in properties
            vindue.setStatus("Loading old glossary");
            fis = new FileInputStream(glosFile);
            zis = new ZipInputStream(fis);
            ze = zis.getNextEntry();

            prop.load(zis);
            zis.closeEntry();
            zis.close();

            componentCount = Integer.parseInt(prop.getProperty("componentCount"));

            // match files in old with the all file List
            for (int c=0;c<componentCount;c++)
            {
                compPrefix = "" + c + ".";
                fileCount = Integer.parseInt(prop.getProperty(compPrefix+ "fileCount"));

                for (int f=0;f<fileCount;f++)
                {
                    filePrefix = compPrefix + f + ".";

                    fileName = prop.getProperty(filePrefix + "name");

                    currentFile = (MozFile) allFiles.getChildByName(fileName);

                    if (currentFile!=null)
                    {
                        vindue.setStatus("Migrateing file : " + currentFile.getName());
                        
                        // match any keys in the matched files
                        
                        phraseCount = Integer.parseInt(prop.getProperty(filePrefix + "phraseCount"));
                        
                        
                        for (int p=0;p<phraseCount;p++)
                        {
                            phrasePrefix = filePrefix + p + ".";
                            key = prop.getProperty(phrasePrefix + "key");
                            currentPhrase = (Phrase) currentFile.getChildByName(key);
                            
                            if (currentPhrase!=null)
                            {
                                
                        
                                // match or add any locales
                                locales = prop.getProperty(phrasePrefix + "locales", "NONE");
                                if (!locales.equals("NONE"))
                                {
                                    localeTokenizer = new StringTokenizer(locales,",",false);

                                    while (localeTokenizer.hasMoreTokens())
                                    {
                                        singleLocale = localeTokenizer.nextToken();
                                        localePrefix = phrasePrefix + singleLocale +".";
                                        translated = prop.getProperty(localePrefix +"text");
                                        qa = Integer.parseInt(prop.getProperty(localePrefix +"qa",""+Translation.QA_NOTSEEN));
                                        comment = prop.getProperty(localePrefix +"comment","");

                                        currentTranslation = (Translation) currentPhrase.getChildByName(singleLocale);

                                        if ((currentTranslation==null) && (!translated.equals("")))
                                        {
                                            currentTranslation = new Translation(singleLocale,currentPhrase,translated,qa,comment);
                                            currentPhrase.addChild(currentTranslation);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        catch (Exception e)
        {
            Log.write("Error importing old glossary");
            Log.write("Exception " + e);
        }
        MainWindow.getDefaultInstance().setStatus("Ready");
    }

}