/*
 * PartialAccess.java
 *
 * Created on 2. september 2000, 13:54
 */

package org.mozilla.translator.io;

import java.io.*;
import java.util.*;
import java.util.zip.*;

import org.mozilla.translator.kernel.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.gui.*;


/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class PartialAccess implements MozInstallAccess {

    Object[] subcomponents;
    String fileName;
    MozInstall install;
    
    /** Creates new PartialAccess */
    public PartialAccess(String fn,MozInstall i) 
    {
        fileName=fn;
        install =i;
        subcomponents = null;
    }

    public PartialAccess(String fn,MozInstall i,Object[] sc) 
    {
        fileName=fn;
        install =i;
        subcomponents = sc;
    }
    
    public void load() 
    {
        MainWindow vindue = MainWindow.getDefaultInstance();
        
        Properties prop;
        FileInputStream fis;
        ZipInputStream zis;
        ZipEntry ze;

        int componentMax;
        int subcomponentMax;
        int fileMax;
        int phraseMax;
        int translationMax;

        String componentPrefix;
        String subcomponentPrefix;
        String filePrefix;
        String phrasePrefix;
        String translationPrefix;

        int qa_status;
        String qa_comment;
        String loc_name;
        String text;

        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase;
        Translation currentTranslation;
        
        try
        {
            vindue.setStatus("Loading file");
            prop = new Properties();
            fis = new FileInputStream(fileName);
            zis = new ZipInputStream(fis);
            ze = zis.getNextEntry();

            prop.load(zis);
            zis.closeEntry();
            zis.close();

            subcomponentMax =Integer.parseInt(prop.getProperty("subcomponent.count"));

            for (int subcomponentCount=0;subcomponentCount<subcomponentMax;subcomponentCount++)
            {
                vindue.setStatus("Parsing subcomponent " + (subcomponentCount+1) + " of " + subcomponentMax); // set the status 
                
                subcomponentPrefix = ""+ subcomponentCount + ".";
                currentComponent = (MozComponent) install.getChildByName(prop.getProperty(subcomponentPrefix + "parent"));
                
                if (currentComponent!=null)
                {
                    currentSubcomponent = (MozComponent) currentComponent.getChildByName(prop.getProperty(subcomponentPrefix + "name"));

                    if (currentSubcomponent!=null)
                    {
                        fileMax = Integer.parseInt(prop.getProperty(subcomponentPrefix + "count"));
                        for (int fileCount=0;fileCount<fileMax;fileCount++)
                        {
                            filePrefix = subcomponentPrefix + fileCount + ".";
                            

                            currentFile = (MozFile) currentSubcomponent.getChildByName(prop.getProperty(filePrefix + "name"));
                            if (currentFile!=null)
                            {
                                phraseMax = Integer.parseInt(prop.getProperty(filePrefix + "count"));
                                for (int phraseCount=0;phraseCount<phraseMax;phraseCount++)
                                {
                                    phrasePrefix = filePrefix + phraseCount + ".";
                                    currentPhrase = (Phrase) currentFile.getChildByName(prop.getProperty(phrasePrefix + "key"));
                                    if (currentPhrase!=null)
                                    {
                                        translationMax = Integer.parseInt(prop.getProperty(phrasePrefix + "count"));
                                        for (int translationCount=0;translationCount<translationMax;translationCount++)
                                        {
                                            translationPrefix = phrasePrefix + translationCount + ".";
                                            
                                            qa_status = Integer.parseInt(prop.getProperty(translationPrefix + "status"));
                                            qa_comment = prop.getProperty(translationPrefix + "comment");
                                            loc_name = prop.getProperty(translationPrefix + "name");
                                            text = prop.getProperty(translationPrefix + "text");
                                            currentTranslation = (Translation) currentPhrase.getChildByName(loc_name);

                                            if (currentTranslation==null)
                                            {
                                                currentTranslation= new Translation(loc_name,currentPhrase,text,qa_status,qa_comment);
                                                currentPhrase.addChild(currentTranslation);
                                            }
                                            else
                                            {
                                                currentTranslation.setStatus(qa_status);
                                                currentTranslation.setComment(qa_comment);
                                                currentTranslation.setText(text);
                                            }
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
            Log.write("Exception " + e); 
            Log.write("Error importing glossary");
        }
        MainWindow.getDefaultInstance().setStatus("Ready");        
    }
    
    public void save() 
    {
        MainWindow vindue = MainWindow.getDefaultInstance();
        
        Properties prop = new Properties();
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        Iterator fileIterator;
        Iterator phraseIterator;
        Iterator translationIterator;
        MozFile currentFile;
        Phrase currentPhrase;
        Translation currentTranslation;
        int fileCounter;
        int phraseCounter;
        int translationCounter;
        String subPrefix;
        String filePrefix;
        String phrasePrefix;
        String translationPrefix;
        FileOutputStream fos;
        ZipOutputStream zos;
        ZipEntry ze;
        
        int subcomponentMax =subcomponents.length;
        
        for (int subCount=0;subCount<subcomponentMax;subCount++)
        {
            vindue.setStatus("Parsing subcomponent " + (subCount+1) + " of " + subcomponentMax); // set the status 
            
            subPrefix = "" + subCount +".";
            currentSubcomponent = (MozComponent) subcomponents[subCount];

            currentComponent = (MozComponent) currentSubcomponent.getParent();

            prop.setProperty(subPrefix + "name",currentSubcomponent.getName());
            prop.setProperty(subPrefix + "parent", currentComponent.getName());

            fileIterator = currentSubcomponent.getChildIterator();

            fileCounter=0;
            while (fileIterator.hasNext())
            {
                filePrefix = subPrefix + fileCounter + ".";
                fileCounter++;
                currentFile = (MozFile) fileIterator.next();

                prop.setProperty(filePrefix + "name", currentFile.getName());

                phraseIterator = currentFile.getChildIterator();

                phraseCounter=0;
                while (phraseIterator.hasNext())
                {
                    phrasePrefix = filePrefix + phraseCounter + ".";
                    phraseCounter++;
                    currentPhrase = (Phrase) phraseIterator.next();

                    prop.setProperty(phrasePrefix + "key",currentPhrase.getName());

                    translationIterator = currentPhrase.getChildIterator();

                    translationCounter=0;
                    while (translationIterator.hasNext())
                    {
                        translationPrefix = phrasePrefix + translationCounter + ".";
                        translationCounter++;
                        currentTranslation = (Translation) translationIterator.next();

                        prop.setProperty(translationPrefix + "name", currentTranslation.getName());
                        prop.setProperty(translationPrefix + "comment", currentTranslation.getComment());
                        prop.setProperty(translationPrefix + "status", ""+currentTranslation.getStatus());
                        prop.setProperty(translationPrefix + "text", currentTranslation.getText());
                    }
                    prop.setProperty(phrasePrefix + "count",""+translationCounter);
                }
                prop.setProperty(filePrefix + "count",""+phraseCounter);
            }
            prop.setProperty(subPrefix + "count" , "" +fileCounter);
        }
        prop.setProperty("subcomponent.count",""+subcomponentMax);
        
        
        try 
        {
            vindue.setStatus("Saving file");
            fos = new FileOutputStream(fileName);
            zos = new ZipOutputStream(fos);
            ze = new ZipEntry("glossary.txt");
            zos.putNextEntry(ze);
            prop.store(zos,"Partial glossary file for MozillaTranslator");
            zos.closeEntry();
            zos.close();
        }
        catch(Exception e)
        {
            Log.write("error writeing partial glossary file");
        }
        vindue.setStatus("Ready");
            
    }
    
}
