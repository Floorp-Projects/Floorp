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
public class FetchKeepOriginal implements Fetcher 
{

   
    /** Creates new FetchUntranslated */
    public FetchKeepOriginal() 
    {
        // do nothing
    }

    public boolean check(Phrase ph) 
    {        
        return ph.getKeepOriginal();                
    }
}