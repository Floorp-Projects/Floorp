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
import java.util.jar.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.io.*;
import org.mozilla.translator.gui.*;
import org.mozilla.translator.gui.dialog.*;
/**
 *
 * @author  Henrik
 * @version 4.0
 */
public class UpdateInstallRunner extends Thread
{
    private MozInstall install;


    /** Creates new UpdateInstallRunner */
    public UpdateInstallRunner(MozInstall i)
    {
        this.install=i;
    }

    public void run() 
    {
        String baseName;
        File baseFile;
        String[] components;
        String[] subcomponents;
        String[] files;
        String currentComponentString;
        String currentSubcomponentString;
        String currentFileString;
        File currentComponentFile;
        File currentSubComponentFile;
        File currentFileFile;
        MozComponent currentComponent;
        MozComponent currentSubComponent;
        MozFile currentFile;
        Phrase currentPhrase;

        Iterator componentIterator;
        Iterator subComponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;

        MainWindow vindue = MainWindow.getDefaultInstance();
        
        baseName = install.getPath();


        baseFile = new File(baseName);
        
        if (baseFile.isDirectory())
        {
            updateFromDirectory();
        }
        else
        {
            try
            {
            updateFromJar();
            }
            catch (Exception e) { Log.write("Exception in updateFromJar() " + e); }
            
        }
        
        vindue.setStatus("Removing dead phrases");
        componentIterator = install.getChildIterator();

        while (componentIterator.hasNext())
        {
            currentComponent = (MozComponent) componentIterator.next();

            if (currentComponent.isMarked())
            {
                subComponentIterator = currentComponent.getChildIterator();

                while (subComponentIterator.hasNext())
                {
                    currentSubComponent = (MozComponent) subComponentIterator.next();

                    if (currentSubComponent.isMarked())
                    {
                        fileIterator = currentSubComponent.getChildIterator();

                        while (fileIterator.hasNext())
                        {
                            currentFile = (MozFile) fileIterator.next();

                            if (currentFile.isMarked())
                            {
                                phraseIterator = currentFile.getChildIterator();

                                while (phraseIterator.hasNext())
                                {
                                    currentPhrase = (Phrase) phraseIterator.next();

                                    if (currentPhrase.isMarked())
                                    {
                                        currentPhrase.setMarked(false);
                                    }
                                    else
                                    {
                                        phraseIterator.remove();

                                    }
                                }
                                currentFile.setMarked(false);

                            }
                            else
                            {
                                fileIterator.remove();

                            }
                        }
                        currentSubComponent.setMarked(false);
                    }
                    else
                    {
                        subComponentIterator.remove();

                    }
                }
                currentComponent.setMarked(false);
            }
            else
            {
                componentIterator.remove();

            }
        }
        
        MainWindow.getDefaultInstance().setStatus("Ready");
        //ShowWhatDialog swd = new ShowWhatDialog();
        // (swd.visDialog())
        //
            //mplexTableWindow ctw = new ComplexTableWindow("Changed Strings", collectedList,swd.getSelectedColumns(),swd.getSelectedLocale());
       //
        
    }
        
    private void updateFromDirectory()
    {
        String baseName;
        File baseFile;
        String[] components;
        String[] subcomponents;
        String[] files;
        String currentComponentString;
        String currentSubcomponentString;
        String currentFileString;
        File currentComponentFile;
        File currentSubComponentFile;
        File currentFileFile;
        MozComponent currentComponent;
        MozComponent currentSubComponent;
        MozFile currentFile;
        Phrase currentPhrase;

        Iterator componentIterator;
        Iterator subComponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;

        MainWindow vindue = MainWindow.getDefaultInstance();
        baseName = install.getPath();

        baseFile = new File(baseName);
        int filesDone=0;

        components = baseFile.list();

        for (int i=0;i<components.length;i++)
        {
            currentComponentFile = new File(baseName + File.separator + components[i],"locale");


            if (currentComponentFile.isDirectory())
            {
                currentComponent = (MozComponent) install.getChildByName(components[i]);

                if (currentComponent==null)
                {
                    currentComponent = new MozComponent(components[i],install);
                    install.addChild(currentComponent);
                }
                currentComponent.setMarked(true);


                subcomponents = currentComponentFile.list();

                for (int j=0;j<subcomponents.length;j++)
                {
                    currentSubComponentFile = new File(currentComponentFile,subcomponents[j]);

                    

                    if (currentSubComponentFile.isDirectory())
                    {
                        currentSubComponent = (MozComponent) currentComponent.getChildByName(subcomponents[j]);

                        if (currentSubComponent==null)
                        {
                            currentSubComponent = new MozComponent(subcomponents[j],currentComponent);
                            currentComponent.addChild(currentSubComponent);
                        }
                        currentSubComponent.setMarked(true);

                        files = currentSubComponentFile.list();

                        for (int k=0;k<files.length;k++)
                        {
                            currentFileFile = new File(currentSubComponentFile,files[k]);
                            currentFile = (MozFile) currentSubComponent.getChildByName(files[k]);

                            if (currentFile==null)
                            {
                                currentFile = new MozFile(files[k],currentSubComponent);
                                currentSubComponent.addChild(currentFile);
                            }
                            currentFile.setMarked(true);

                            vindue.setStatus("Files done : " + filesDone + ", currently reading : " +currentFile);
                            readFile(currentFileFile,currentFile);
                            filesDone++;
                        }
                    }
                    else
                    {
                        currentSubComponent = (MozComponent) currentComponent.getChildByName("MT_default");

                        if (currentSubComponent==null)
                        {
                            currentSubComponent = new MozComponent("MT_default",currentComponent);
                            currentComponent.addChild(currentSubComponent);
                        }
                        currentSubComponent.setMarked(true);

                        currentFile = (MozFile) currentSubComponent.getChildByName(subcomponents[j]);

                        if (currentFile==null)
                        {
                            currentFile = new MozFile(subcomponents[j],currentSubComponent);
                            currentSubComponent.addChild(currentFile);
                        }
                        currentFile.setMarked(true);

                        vindue.setStatus("Files done : " + filesDone + ", currently reading : " +currentFile);
                        readFile(currentSubComponentFile,currentFile);
                        filesDone++;
                    }
                }
            }
        }
    }
    
    private void updateFromJar() throws FileNotFoundException,IOException
    {
        String baseName;
        File baseFile;
        String[] components;
        String[] subcomponents;
        String[] files;
        String currentComponentString;
        String currentSubcomponentString;
        String currentFileString;
        File currentComponentFile;
        File currentSubComponentFile;
        File currentFileFile;
        MozComponent currentComponent;
        MozComponent currentSubComponent;
        MozFile currentFile;
        Phrase currentPhrase;

        Iterator componentIterator;
        Iterator subComponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;
        JarEntry je;
        boolean done=false;
        StringTokenizer tokens;
        
        String componentToken=null;
        String localeToken=null;
        String subcomponentToken=null;
        String fileToken=null;
        int filesDone=0;
        
        MainWindow vindue = MainWindow.getDefaultInstance();
        baseName = install.getPath();
        
        FileInputStream fis = new FileInputStream(baseName);
        
        JarInputStream jis = new JarInputStream(fis);
        BufferedInputStream bis = new BufferedInputStream(jis);
        
        while (!done)
        {
            je = jis.getNextJarEntry();
        
            if (je!=null)
            {   
                tokens = new StringTokenizer(je.getName(),"/",false);
                
                componentToken = tokens.nextToken();
                if (tokens.hasMoreTokens())
                {
                    localeToken = tokens.nextToken();
                  
                    if (tokens.hasMoreTokens())
                    {
                        subcomponentToken = tokens.nextToken();
                    }
                    if (tokens.hasMoreTokens())
                    {
                        fileToken = tokens.nextToken();
                    }
                    else
                    {
                        fileToken = subcomponentToken;
                        subcomponentToken="MT_default";
                    }
                }
                if (fileToken!=null)
                {
                    currentComponent = (MozComponent) install.getChildByName(componentToken);
                    if (currentComponent==null)
                    {
                        currentComponent = new MozComponent(componentToken,install);
                        install.addChild(currentComponent);
                    }
                    currentComponent.setMarked(true);
                    
                    currentSubComponent = (MozComponent) currentComponent.getChildByName(subcomponentToken);
                    
                    if (currentSubComponent==null)
                    {
                        currentSubComponent = new MozComponent(subcomponentToken,currentComponent);
                        currentComponent.addChild(currentSubComponent);
                    }
                    currentSubComponent.setMarked(true);

                   currentFile = (MozFile) currentSubComponent.getChildByName(fileToken);

                    if (currentFile==null)
                    {
                        currentFile = new MozFile(fileToken,currentSubComponent);
                        currentSubComponent.addChild(currentFile);
                    }
                    currentFile.setMarked(true);
                    vindue.setStatus("Files done : " + filesDone + ", currently reading : " +currentFile);
                    filesDone++;
                    File tempFile = copyToTemp(je,bis);
                    readFile(tempFile,currentFile);
                    tempFile.delete();
                    componentToken=null;
                    localeToken=null;
                    subcomponentToken=null;
                    fileToken=null;
                }               

            }
            else
                {
                    done=true;
            }
    }
                    
        
    }


    private void readFile(File r_fil,MozFile m_fil)
    {
       
        try 
        {
            MozFileReader reader;
            FileInputStream fis;
        
            fis = new FileInputStream(r_fil);
        
            
            reader = MozIo.getFileReader(m_fil,fis);
            if (reader!= null)
            {
                reader.readFile("en-US");
            }
            else
            {
                Phrase tempPhrase = new Phrase("MT_unsupported",m_fil,"MT_unsupported","MT_unsupported",false);
                m_fil.addChild(tempPhrase);
            }
                    
        }
        catch (Exception e)
        {
            Log.write("Error during file read");
            Log.write("Reak File = " + r_fil);
            Log.write("MT file   = " + m_fil);
        }
    }
    
        private File copyToTemp(JarEntry e,BufferedInputStream bis) throws IOException
    {
        File temp = File.createTempFile("MT_",null);
        FileOutputStream fos = new FileOutputStream(temp);
        BufferedOutputStream bos = new BufferedOutputStream(fos);
        boolean theEnd;
        int trans;

        theEnd=false;
        while (!theEnd)
        {
            trans = bis.read();
            if (trans==-1)
            {
                theEnd = true;
            }
            else
            {
                bos.write(trans);
            }
        }
        bos.close();
        return temp;
    }
}