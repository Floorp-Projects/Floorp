/*
 * GlossaryAccess.java
 *
 * Created on 27. august 2000, 17:03
 */

package org.mozilla.translator.io;

import java.io.*;
import java.util.*;

import java.util.zip.*;


import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.*;
/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class GlossaryAccess implements  MozInstallAccess {

    /** Creates new GlossaryAccess */
    public GlossaryAccess() 
    {
        // empty
    }

    public void load()
    {
        FileInputStream fis;
        ZipInputStream zis;
        ZipEntry ze;
        
        int installMax,installCount;
        int componentMax,componentCount;
        int subcomponentMax,subcomponentCount;
        int fileMax,fileCount;
        int phraseMax,phraseCount;
        int translationMax,translationCount;
        
        String installPrefix;
        String componentPrefix;
        String subcomponentPrefix;
        String filePrefix;
        String phrasePrefix;
        String translationPrefix;

        
        MozInstall currentInstall;
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase,labelPhrase,accessPhrase,commandPhrase;
        Translation currentTranslation;
        Properties prop;
        
        String name;
        String path;
        String locale;
        String key;
        String note;
        String text;
        boolean keep;
        String access;
        String command;
        Phrase accessLink;
        Phrase commandLink;
        String comment;
        int status;
        
        String statusPrefix,statusText;
        MainWindow vindue = MainWindow.getDefaultInstance();
        
        try
        {
            vindue.setStatus("Loading file");
            prop= new Properties();
            fis = new FileInputStream(Settings.getString("System.Glossaryfile","glossary.zip"));
            zis = new ZipInputStream(fis);
            ze = zis.getNextEntry();

            prop.load(zis);
            zis.closeEntry();
            zis.close();
            
            installMax = Integer.parseInt(prop.getProperty("install.count"));
            
            for (installCount=0;installCount<installMax;installCount++)
            {
                statusPrefix = "Parsing install " + (installCount+1) + " of " + installMax + " : ";
                installPrefix= "" + installCount + ".";
            
                name   = prop.getProperty(installPrefix + "name","");
                path   = prop.getProperty(installPrefix + "path","");
                componentMax = Integer.parseInt(prop.getProperty(installPrefix + "count","0"));
               
                currentInstall = new MozInstall(name,path);
                Glossary.getDefaultInstance().addChild(currentInstall);

               
                for (componentCount=0;componentCount<componentMax;componentCount++)
                {
                    
                    componentPrefix=installPrefix + componentCount + ".";
                    name = prop.getProperty(componentPrefix + "name","");
                    
                    statusText = statusPrefix + "Component " + (componentCount+1) + " of " +componentMax + " ( " + name + ")";
                    vindue.setStatus(statusText);
                    
                    subcomponentMax = Integer.parseInt(prop.getProperty(componentPrefix + "count","0"));
                    
                    currentComponent = new MozComponent(name,currentInstall);
                    currentInstall.addChild(currentComponent);

                    for (subcomponentCount=0;subcomponentCount<subcomponentMax;subcomponentCount++)
                    {
                        subcomponentPrefix = componentPrefix + subcomponentCount + ".";
                        
                        name = prop.getProperty(subcomponentPrefix + "name","");
                        fileMax = Integer.parseInt(prop.getProperty(subcomponentPrefix + "count","0"));
                        
                        currentSubcomponent = new MozComponent(name,currentComponent);
                        currentComponent.addChild(currentSubcomponent);
                        
                        
                        for (fileCount=0;fileCount<fileMax;fileCount++)
                        {
                            filePrefix = subcomponentPrefix + fileCount + ".";
                            
                            name = prop.getProperty(filePrefix + "name","");
                            phraseMax = Integer.parseInt(prop.getProperty(filePrefix + "count","0"));
                            
                            currentFile = new MozFile(name,currentSubcomponent);
                            currentSubcomponent.addChild(currentFile);

                            for (phraseCount=0;phraseCount<phraseMax;phraseCount++)
                            {
                                phrasePrefix = filePrefix + phraseCount + ".";
                                
                                key = prop.getProperty(phrasePrefix + "key","");
                                note = prop.getProperty(phrasePrefix + "note","");
                                text = prop.getProperty(phrasePrefix + "text","");
                                keep = Boolean.valueOf(prop.getProperty(phrasePrefix + "keep")).booleanValue();
                                translationMax = Integer.parseInt(prop.getProperty(phrasePrefix + "count",""));
                               
                                currentPhrase = new Phrase(key,currentFile,text,note,keep);
                                currentPhrase.setAccessConnection(null);
                                currentPhrase.setCommandConnection(null);
                                currentFile.addChild(currentPhrase);

                                if (key.endsWith(".label"))
                                {

                                    accessPhrase = (Phrase) currentFile.getChildByName(key.substring(0,key.length()-6)+ ".accesskey" );
                                    commandPhrase =(Phrase) currentFile.getChildByName(key.substring(0,key.length()-6)+ ".commandkey" );
                                    
                                    currentPhrase.setAccessConnection(accessPhrase);
                                    currentPhrase.setCommandConnection(commandPhrase);

                                }
                                if (key.endsWith(".accesskey"))
                                {

                                    labelPhrase = (Phrase) currentFile.getChildByName(key.substring(0,key.length()-10)+ ".label" );
                                    if (labelPhrase!=null)
                                    {
                                        labelPhrase.setAccessConnection(currentPhrase);
                                    }

                                }
                                if (key.endsWith(".commandkey"))
                                {

                                    labelPhrase = (Phrase) currentFile.getChildByName(key.substring(0,key.length()-11)+".label" );                                   
                                    if (labelPhrase!=null)
                                    {
                                        labelPhrase.setCommandConnection(currentPhrase);
                                    }

                                }                                
                                
                                
                                for (translationCount=0;translationCount<translationMax;translationCount++)
                                {
                                    translationPrefix = phrasePrefix + translationCount + ".";
                                    
                                    name    = prop.getProperty(translationPrefix + "name","");
                                    comment = prop.getProperty(translationPrefix + "comment","");
                                    text    = prop.getProperty(translationPrefix + "text","");
                                    status  = Integer.parseInt(prop.getProperty(translationPrefix + "status","0"));
                                    
                                                                     
                                    currentTranslation = new Translation(name,currentPhrase,text,status,comment);
                                    currentPhrase.addChild(currentTranslation);
                                    
                                }
                            }
                        }
                    }
                }
            }
            MainWindow.getDefaultInstance().setStatus("Ready");
        }
        catch (Exception e)
        {
            Log.write("Exception : " +e);
            Log.write("Error loading the glossary file");
        }                
    }
    
    public void save() 
    {
        FileOutputStream fos;
        ZipOutputStream zos;
        ZipEntry ze;
        
        Glossary glos;
        MozInstall currentInstall;
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase;
        Translation currentTranslation;
        
        Iterator installIterator;
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;
        Iterator translationIterator;
        
        int installCount;
        int componentCount;
        int subcomponentCount;
        int fileCount;
        int phraseCount;
        int translationCount;
        
        String installPrefix;
        String componentPrefix;
        String subcomponentPrefix;
        String filePrefix;
        String phrasePrefix;
        String translationPrefix;
       String statusPrefix,statusText;
        Properties prop;
        
        MainWindow vindue = MainWindow.getDefaultInstance();
        prop = new Properties();

        installCount=0;
        installIterator = Glossary.getDefaultInstance().getChildIterator();

        while (installIterator.hasNext())
        {
            installPrefix = "" + installCount + ".";
            currentInstall = (MozInstall) installIterator.next();

            prop.setProperty(installPrefix + "name",currentInstall.getName());
            prop.setProperty(installPrefix + "path",currentInstall.getPath());

            componentIterator = currentInstall.getChildIterator();
            componentCount=0;
            statusPrefix = "Parsing install " + (installCount+1) + " : ";
            
            while (componentIterator.hasNext())
            {
               componentPrefix = installPrefix + componentCount + ".";

                currentComponent = (MozComponent) componentIterator.next();
                statusText = statusPrefix + "Component " + (componentCount+1) + " ( " + currentComponent.getName() + ")";
                vindue.setStatus(statusText);

                prop.setProperty(componentPrefix +"name",currentComponent.getName());

                subcomponentIterator = currentComponent.getChildIterator();
                subcomponentCount = 0;

                while (subcomponentIterator.hasNext())
                {
                    subcomponentPrefix = componentPrefix + subcomponentCount + ".";
                    currentSubcomponent = (MozComponent) subcomponentIterator.next();

                    prop.setProperty(subcomponentPrefix + "name",currentSubcomponent.getName());

                    fileIterator = currentSubcomponent.getChildIterator();
                    fileCount =0;

                    while (fileIterator.hasNext())
                    {
                        filePrefix = subcomponentPrefix + fileCount + ".";
                        currentFile = (MozFile) fileIterator.next();

                        prop.setProperty(filePrefix + "name",currentFile.getName());

                        phraseIterator = currentFile.getChildIterator();
                        phraseCount=0;

                        while (phraseIterator.hasNext())
                        {
                            phrasePrefix = filePrefix + phraseCount + ".";
                            currentPhrase = (Phrase) phraseIterator.next();

                            prop.setProperty(phrasePrefix + "key",currentPhrase.getName());
                            prop.setProperty(phrasePrefix + "note",currentPhrase.getNote());
                            prop.setProperty(phrasePrefix + "text",currentPhrase.getText());
                            prop.setProperty(phrasePrefix + "keep",""+currentPhrase.getKeepOriginal());

                            translationIterator = currentPhrase.getChildIterator();
                            translationCount=0;

                            while (translationIterator.hasNext())
                            {
                                translationPrefix = phrasePrefix + translationCount + ".";
                                currentTranslation = (Translation) translationIterator.next();

                                prop.setProperty(translationPrefix + "name",currentTranslation.getName());
                                if (!currentTranslation.getComment().equals(""))
                                {
                                    prop.setProperty(translationPrefix + "comment",currentTranslation.getComment());
                                }
                                prop.setProperty(translationPrefix + "text",currentTranslation.getText());
                                if (currentTranslation.getStatus()!=Translation.QA_NOTSEEN)
                                {
                                    prop.setProperty(translationPrefix + "status",""+currentTranslation.getStatus());
                                }
                                translationCount++;
                            }

                            prop.setProperty(phrasePrefix + "count",""+translationCount);
                            phraseCount++;
                        }
                        prop.setProperty(filePrefix + "count",""+phraseCount);
                        fileCount++;

                    }
                    prop.setProperty(subcomponentPrefix + "count","" + fileCount);
                    subcomponentCount++;

                }
                prop.setProperty(componentPrefix + "count","" +subcomponentCount);
                componentCount++;

            }
            prop.setProperty(installPrefix + "count",""+componentCount);
            installCount++;

        }
        prop.setProperty("install.count","" + installCount);

        try
        {
            vindue.setStatus("Saving file");
            fos = new FileOutputStream(Settings.getString("System.Glossaryfile","glossary.zip"));
            zos = new ZipOutputStream(fos);
            ze = new ZipEntry("glossary.txt");
            zos.putNextEntry(ze);
            prop.store(zos,"Translated with MozillaTranslator " + Settings.getString("System.Version","(unknown version)"));
            zos.closeEntry();
            zos.close();
        }
        catch(Exception e)
        {
            Log.write("error writeing glossary file");
        }
        vindue.setStatus("Ready");
        
    }
    
}
