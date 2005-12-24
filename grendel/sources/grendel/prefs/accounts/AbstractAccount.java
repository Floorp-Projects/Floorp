/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Account.java
 *
 * Created on 09 August 2005, 21:17
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package grendel.prefs.accounts;

import grendel.prefs.xml.XMLPreferences;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Set;

/**
 *
 * @author hash9
 */
abstract class AbstractAccount extends XMLPreferences implements Account {
    
    /**
     * Creates a new instance of AbstractAccount
     */
    public AbstractAccount() {
    }
    
    /**
     * Creates a new instance of AbstractAccount
     */
    public AbstractAccount(String name) {
        super();
        setName(name);
    }
    
    public String getName() {
        return getProperty("name","");
    }
    
    public void setName(String name) {
        setProperty("name",name);
    }
    
    public void setIdentities(XMLPreferences i) {
        this.setProperty("identities", i);
    }
    
    private int getNextEmptyIDIndex() {
        XMLPreferences p = getIdentities();
        Set<String> keys = p.keySet();
        boolean in_use = false;
        int i = -1;
        do {
            i++;
            in_use =  keys.contains(Integer.toString(i));
        } while(in_use);
        return i;
    }
    
    public void addIdentity(Identity i) {
        getIdentities().setProperty(Integer.toString(getNextEmptyIDIndex()),i);
    }
    
    public void removeIdentity(int i) {
        getIdentities().remove(Integer.toString(i));
    }
    
    public XMLPreferences getIdentities() {
        return this.getPropertyPrefs("identities");
    }
    
    public Collection<Identity> getCollectionIdentities() {
        ArrayList<Identity> i_a = new ArrayList<Identity>();
        Collection<Object> c_o = getIdentities().values();
        for (Object o: c_o) {
            if (o instanceof Identity) {
                i_a.add((Identity) o);
            }
        }
        return i_a;
    }
    
    public Identity getIdentity(int i) {
        XMLPreferences p = getPropertyPrefs("identities");
        p = p.getPropertyPrefs(Integer.toString(i));
        if (p instanceof Identity) {
            return (Identity) p;
        } else {
            return null;
        }
    }
    
    public int getDefaultIdentity() {
        return this.getPropertyInt("default_identity");
    }
    
    public void setDefaultIdentity(int default_identity) {
        this.setProperty("default_identity",new Integer(default_identity));
    }
    
    public String getType() {
       return getType(this.getClass());
    }
    
    static String getType(Class c) {
        if (AbstractAccount.class.isAssignableFrom(c)) {
            String name = c.getCanonicalName();
            name = name.substring(name.lastIndexOf('.')+1);
            if (name.startsWith("Account__")) { // These aren't account types
                return "";
            } else if (name.startsWith("Account_")) {  // Starts Account_
                String type = name.substring(8).toLowerCase();
                return type;
            } else {// This isn't an account
                return "";
            }
        }
        return "";
    }
}
