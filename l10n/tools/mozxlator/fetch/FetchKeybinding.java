/*
 * FetchKaybinding.java
 *
 * Created on 19. august 2000, 21:07
 */

package org.mozilla.translator.fetch;

import org.mozilla.translator.datamodel.*;

/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class FetchKeybinding implements Fetcher {

    private String localeName;
    
    /** Creates new FetchKaybinding */
    public FetchKeybinding(String ln) 
    {
        localeName =ln;
    }

    public boolean check(Phrase ph) 
    {
        boolean result;
        Phrase commandPhrase,accessPhrase;
        Translation currentTranslation,commandTranslation,accessTranslation;
        String text,key;
        
        result = false;
        // find the correct text
        currentTranslation = (Translation) ph.getChildByName(localeName);
        if (currentTranslation==null)
        {
            text = ph.getText();
        }
        else
        {
                text = currentTranslation.getText();
        }
        
        // check for command 
        commandPhrase = ph.getCommandConnection();
    
        if (commandPhrase!=null)
        {
            commandTranslation = (Translation) commandPhrase.getChildByName(localeName);
            
            if (commandTranslation ==null)
            {
                    key = commandPhrase.getText();
            }
            else
            {
                key = commandTranslation.getText();
            }
            text = text.toLowerCase();
            key  = key.toLowerCase();
            
            if (text.indexOf(key)==-1)
            {
                result=true;
            }
        }
                
        // check for access
        accessPhrase = ph.getAccessConnection();
    
        if (accessPhrase!=null)
        {
            accessTranslation = (Translation) accessPhrase.getChildByName(localeName);
            
            if (accessTranslation ==null)
            {
                    key = accessPhrase.getText();
            }
            else
            {
                key = accessTranslation.getText();
            }
            text = text.toLowerCase();
            key  = key.toLowerCase();
            
            if (text.indexOf(key)==-1)
            {
                result=true;
            }
        }    
        return result;
    }
    
  
}
