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
public class FetchAdvancedSearch implements Fetcher 
{

    private Filter first;
    private Filter second;
    private Filter third;
    private boolean all;
    
    /** Creates new FetchUntranslated */
    public FetchAdvancedSearch(Filter f1,Filter f2,Filter f3,boolean al) 
    {
        first = f1;
        second = f2;
        third = f3;
        all=al;
        
    }

    public boolean check(Phrase ph) 
    {       
        boolean realResult,firstResult,secondResult,thirdResult;
        
        if (all)
        {
            if (first!=null)
            {            
                firstResult = first.check(ph);
            }
            else
            {
                firstResult=true;
            }

            if (second!=null)
            {            
                secondResult = second.check(ph);
            }
            else
            {
                secondResult=true;
            }
            
            if (third!=null)
            {            
                thirdResult = third.check(ph);
            }
            else
            {
                thirdResult=true;
            }
            
            if ((firstResult==true) && (secondResult==true) && (thirdResult==true))
            {
                realResult=true;
            }
            else
            {
                realResult=false;
            }
        }
        else
        {
            if (first!=null)
            {            
                firstResult = first.check(ph);
            }
            else
            {
                firstResult=false;
            }

            if (second!=null)
            {            
                secondResult = second.check(ph);
            }
            else
            {
                secondResult=false;
            }
            
            if (third!=null)
            {            
                thirdResult = third.check(ph);
            }
            else
            {
                thirdResult=false;
            }
            
            if ((firstResult==true) || (secondResult==true) || (thirdResult==true))
            {
                realResult=true;
            }
            else
            {
                realResult=false;
            }
        }
        return realResult;
         
    }
}