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
import java.util.jar.*;


import java.util.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;

/**
 *
 * @author  Henrik Lynggaard
 * @version 4.1
 */
public class CopyWriter extends MozFileWriter 
{
    private static String header ="Translated with MozillaTranslator " + Settings.getString("System.Version","(unknown version)") + "  ";
   
    private MozInstall install;
    private MozComponent component;
    private MozComponent subcomponent;
    
    public CopyWriter(MozFile f,OutputStream o)
    {
        super(f,o);
    }
    
    public void writeFile(String localeName) throws IOException
    {
       
        String baseName;
        File baseFile;
        InputStream is;
        boolean theEnd;
        int trans;
        

        
        subcomponent = (MozComponent) fil.getParent();
        
        component = (MozComponent) subcomponent.getParent();
        
        install = (MozInstall) component.getParent();
        
       baseName = install.getPath();
        
        baseFile = new File(baseName);
        
        if (baseFile.isDirectory())
        {
            is = getInputFromDirectory();
        }
        else
        {
            is = getInputFromJar();
        }
        
        theEnd=false;
        while (!theEnd)
        {
            trans = is.read();
            if (trans==-1)
            {
                theEnd = true;
            }
            else
            {
                os.write(trans);
            }
        }
        is.close();
        os.close();
        
    }
    
    private InputStream getInputFromDirectory()
    {
        FileInputStream result=null;
        String fileName;
        
       fileName = install.getPath();
        fileName = fileName + File.separator + component.getName();
        fileName = fileName + File.separator + "locale";
        
        if (subcomponent.getName().equals("MT_default"))
        {
            fileName= fileName + File.separator + fil.getName();
        }
        else
        {
            fileName= fileName + File.separator + subcomponent.getName();
            fileName= fileName + File.separator + fil.getName();
        }
        try 
        {    
            result = new FileInputStream(fileName);
        }
        catch(Exception e)
        {
            Log.write("Error reading original file for copy: "+ fileName);
        }
        return result;
    }

    private InputStream getInputFromJar()
    {
        InputStream result=null;
        FileInputStream jarFile;
        JarInputStream jis;
        JarEntry je;
        String fileName;
        String jarName;
        String compareText;
        boolean done;
        
        jarName = install.getPath();
        fileName = component.getName();
        fileName = fileName + "/locale";
        
        if (subcomponent.getName().equals("MT_default"))
        {
            fileName= fileName + "/" + fil.getName();
        }
        else
        {
            fileName= fileName + "/" + subcomponent.getName();
            fileName= fileName + "/" + fil.getName();
        }
        try
        {
            jarFile = new FileInputStream(jarName);
            jis     = new JarInputStream(jarFile);
            
            done = false;
            
            while (!done)
            {
                je = jis.getNextJarEntry();
                if (je!=null)
                {
                    compareText = je.getName();
                    
                    if (compareText.equalsIgnoreCase(fileName))
                    {
                        result= jis;
                        done=true;
                    }
                }
                else
                {
                    result=null;
                    done=true;
                }
            }
        }
        catch (Exception e)
        {
            Log.write("Error reading original file for copy: "+ fileName);
        }
        return result;
             
                        
            
            
        
    }    
    
    
}
