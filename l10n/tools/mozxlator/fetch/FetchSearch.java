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
public class FetchSearch implements Fetcher 
{

    private Filter filt;
    
    /** Creates new FetchUntranslated */
    public FetchSearch(Filter f) 
    {
        filt=f;
    }

    public boolean check(Phrase ph) 
    {        
        return filt.check(ph);
    }
}