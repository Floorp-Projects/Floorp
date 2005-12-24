/*
 * Account__Interface.java
 *
 * Created on 18 August 2005, 22:31
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.prefs.accounts;

import grendel.prefs.xml.XMLPreferences;
import java.util.Collection;

/**
 *
 * @author hash9
 */
public interface Account {
    void addIdentity(Identity i);

    int getDefaultIdentity();

    XMLPreferences getIdentities();
    
    Collection<Identity> getCollectionIdentities();

    Identity getIdentity(int i);

    String getName();

    void removeIdentity(int i);

    void setDefaultIdentity(int default_identity);

    void setIdentities(XMLPreferences i);

    void setName(String name);
     
    String getType();
    
    java.util.Properties setSessionProperties(java.util.Properties props);
}
