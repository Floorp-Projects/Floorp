/*
 * FetchRunner.java
 *
 * Created on 19. august 2000, 13:01
 */

package org.mozilla.translator.fetch;

import java.util.*;
import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.*;
/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class FetchRunner
{

    public static List getFromGlossary(Fetcher fetch)
    {
        Glossary glos;
        Iterator installIterator;
        MozInstall currentInstall;
        List allList,installList;
        
        glos = Glossary.getDefaultInstance();
        allList = new ArrayList();
        
        installIterator = glos.getChildIterator();
        
        while (installIterator.hasNext())
        {
            currentInstall = (MozInstall) installIterator.next(); 
            
            installList = getFromInstall(currentInstall,fetch);
            
            allList.addAll(installList);
        }
        return allList;
    }
          
    public static List getFromInstall(MozInstall install,Fetcher fetch)
    {
        Iterator componentIterator;
        Iterator subcomponentIterator;
        Iterator fileIterator;
        Iterator phraseIterator;
        
        MozComponent currentComponent;
        MozComponent currentSubcomponent;
        MozFile currentFile;
        Phrase currentPhrase;
        MainWindow vindue = MainWindow.getDefaultInstance();
        int filesDone=0;
        List result = new ArrayList();
        
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
                    currentFile = (MozFile) fileIterator.next();
                    vindue.setStatus("Files done: " + filesDone + ", currently handling : " + currentFile);
                    filesDone++;
                    phraseIterator = currentFile.getChildIterator();
                    
                    while (phraseIterator.hasNext())
                    {
                        currentPhrase = (Phrase) phraseIterator.next();
                        
                        if (fetch.check(currentPhrase))
                        {
                            result.add(currentPhrase);
                        }
                    }
                }
            }
        }
        vindue.setStatus("Ready");
        return result;
    }    
}
