package org.mozilla.translator.io;

import java.io.*;
import java.util.*;
import java.util.jar.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.gui.*;
/**
 *
 * @author  Henrik Lynggaard
 * @version 4.20
 */
public class LocaleAccess implements MozInstallAccess
{
    private MozInstall install;
    private String location;
    private String localeName;
    private MainWindow vindue;
    
    public LocaleAccess(MozInstall i,String loc,String ln)
    {
        install=i;
        location=loc;
        localeName=ln;
        vindue = MainWindow.getDefaultInstance();
    }
    
    public void load()
    {
        File locFile = new File (location);
        
        if (locFile.isDirectory())
        {
            readFromDirectory();
        }
        else
        {
            if (location.endsWith("jar"));
            {
                readFromJar();
            }
            if (location.endsWith("xpi"));
            {
                readFromXPI();
            }
        }
        vindue.setStatus("Ready");
    }
        
    private void readFromDirectory()
    {
        int filesDone=0;
        File baseFile;
        String[] components;
        String[] subcomponents;
        String[] files;
        
        File currentComponentFile;
        File currentSubcomponentFile;
        File currentFileFile;
        
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        
        baseFile = new File(location);
        
        components = baseFile.list();
        
        for (int componentCount=0;componentCount<components.length;componentCount++)
        {
            currentComponent = (MozComponent) install.getChildByName(components[componentCount]);
            
            if (currentComponent!=null)
            {
                currentComponentFile = new File(location + File.separator + components[componentCount],"locale");
                
                subcomponents = currentComponentFile.list();
                
                
                for (int subcomponentCount=0;subcomponentCount<subcomponents.length;subcomponentCount++)
                {
                    currentSubcomponentFile = new File (currentComponentFile,subcomponents[subcomponentCount]);
                    
                    if (currentSubcomponentFile.isDirectory())
                    {
                        currentSubcomponent = (MozComponent) currentComponent.getChildByName(subcomponents[subcomponentCount]);
                        
                        if (currentSubcomponent!=null)
                        {
                            files = currentSubcomponentFile.list();
                            
                            for (int filesCount=0;filesCount<files.length;filesCount++)
                            {
                                currentFile = (MozFile) currentSubcomponent.getChildByName(files[filesCount]);
                                
                                if (currentFile!=null)
                                {
                                    currentFileFile = new File(currentSubcomponentFile,files[filesCount]);
                                    vindue.setStatus("files done: " + filesDone + ",currently reading: " +currentFile);
                                    readFile( currentFileFile,currentFile);
                                    filesDone++;
                                }
                            }
                        }
                    }
                    else
                    {
                        currentSubcomponent = (MozComponent) currentComponent.getChildByName("MT_default");
                        if (currentSubcomponent!=null)
                        {
                            currentFile = (MozFile) currentSubcomponent.getChildByName(subcomponents[subcomponentCount]);

                            if (currentFile!=null)
                            {
                                vindue.setStatus("files done: " + filesDone + ",currently reading: " +currentFile);
                                readFile( currentSubcomponentFile,currentFile);
                                filesDone++;
                            }
                        }
                    }
                }
            }
        }        
    }
    
    private void readFromJar()
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
        

        try
        {
            FileInputStream fis = new FileInputStream(location);
        
            JarInputStream jis = new JarInputStream(fis);
            BufferedInputStream bis = new BufferedInputStream(jis);
        
        while (!done)
        {
            je = jis.getNextJarEntry();
        
            if (je!=null)
            {   
                Log.write("je.getName " + je.getName());
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
                    if (currentComponent!=null)
                    {
                        currentSubComponent = (MozComponent) currentComponent.getChildByName(subcomponentToken);
                    
                        if (currentSubComponent!=null)
                        {

                           currentFile = (MozFile) currentSubComponent.getChildByName(fileToken);

                           if (currentFile!=null)
                           {
                    
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
                    }
                }                          
            }
            else
            {
                done=true;
            }
        }
        }
        catch (Exception e)
        {
            Log.write("error reading from jar");
            Log.write("Exception : " + e);
        }
    }
        

  
    
    private void readFromXPI()
    {
        // to be done;
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
                reader.readFile(localeName);
            }                    
        }
        catch (Exception e)
        {
                        Log.write("Reak File = " + r_fil);
            Log.write("MT file   = " + m_fil);
            Log.write("Error during file read");
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
    
        
    public void save()
    {
     // does nothing
    }
    
}
