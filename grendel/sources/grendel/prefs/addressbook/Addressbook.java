/*
 * Addressbook.java
 *
 * Created on 18 August 2005, 22:31
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.prefs.addressbook;

import grendel.prefs.xml.XMLPreferences;
import java.util.Collection;

/**
 *
 * @author hash9
 */
public interface Addressbook {
    
    String getName();

    void setName(String name);
    
    String getSuperType();
    
    String getType();
}
