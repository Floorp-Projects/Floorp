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

    private String localeName;
    private Filter filt;
    
    /** Creates new FetchUntranslated */
    public FetchSearch(String ln,Filter f) 
    {
        localeName = ln;
        filt=f;
    }

    public boolean check(Phrase ph) 
    {        
        return filt.check(ph,localeName);
    }
}