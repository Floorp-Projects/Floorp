/*
 * FetchUntranslated.java
 *
 * Created on 19. august 2000, 20:32
 */

package org.mozilla.translator.fetch;

import org.mozilla.translator.datamodel.*;


/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class FetchUntranslated implements  Fetcher 
{

    private String localeName;
    
    /** Creates new FetchUntranslated */
    public FetchUntranslated(String ln) 
    {
        localeName = ln;
    }

    public boolean check(Phrase ph) 
    {
        boolean result;
        Translation currentTranslation;
        
        currentTranslation = (Translation) ph.getChildByName(localeName);
        
        if (currentTranslation==null)
        {
            result=true;
        }
        else
        {
            result=false;
        }
        return result;                
    }
}