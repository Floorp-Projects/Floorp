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
package org.mozilla.translator.datamodel;

import javax.swing.*;
import java.util.*;
/**
 *
 * @author  Henrik
 * @version 
 */
public class FilterList extends AbstractListModel {

    private static FilterList instance;
    
    private List filters;
    private int localSize;
    
    public static FilterList getDefaultInstance()
    {
        if (instance==null)
        {
            instance = new FilterList();
        }
        return instance;
    }
    
    /** Creates new FilterList */
    public FilterList() 
    {
        filters = new ArrayList();
        localSize = 0;  
    }
    public Object getElementAt(int pos) 
    {
        return filters.get(pos);        
    }
    
    public int getSize()
    {
        return localSize;
    }
    
    public void addFilter(Filter value)
    {
        filters.add(value);
        localSize =filters.size();
    }
    
    public void removeFilter(Filter value)
    {
        filters.remove(value);
        localSize = filters.size();
    }
    
    public boolean check(Phrase phrase)
    {
        return check(phrase, "MT_no_locale");
    }
    
    public boolean check(Phrase phrase,String localeName)
    {
        Filter f;
        Iterator i = filters.iterator();
        boolean result=false;
        
        while (i.hasNext() && !result)
        {
            f = (Filter) i.next();
            
            result = f.check(phrase,localeName);
        }
        return result;
    }        
}