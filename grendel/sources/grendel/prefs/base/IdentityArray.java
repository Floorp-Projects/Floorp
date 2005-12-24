/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Edwin Woudt
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s):
 *      Hash9 <hash9@eternal.undonet.com>
 */

package grendel.prefs.base;
import grendel.prefs.accounts.Identity;
import grendel.prefs.Preferences;


public class IdentityArray {
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    private static IdentityArray MasterIdentityArray;
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public static synchronized IdentityArray GetMaster() {
        if (MasterIdentityArray == null) {
            MasterIdentityArray = new IdentityArray();
        }
        return MasterIdentityArray;
    }
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    //Vector ids = new Vector();
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    //Preferences prefs;
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    private IdentityArray() {
        /*
        prefs = PreferencesFactory.Get();
        readPrefs();
         **/
    }
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public void readPrefs() {
        /*
        for (int i=0; i<prefs.getInt("identity.count",1); i++) {
            IdentityStructure id = new IdentityStructure();
            id.setDescription(prefs.getString("identity."+i+".description","Default Identity"));
            id.setName(prefs.getString("identity."+i+".name","John Doe"));
            id.setEMail(prefs.getString("identity."+i+".email","nobody@localhost"));
            id.setReplyTo(prefs.getString("identity."+i+".replyto",""));
            id.setOrganization(prefs.getString("identity."+i+".organization",""));
            id.setSignature(prefs.getString("identity."+i+".signature",""));
            add(id);
        }
        writePrefs();
         */
    }
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public void writePrefs() {
        /*
        prefs.putInt("identity.count",size());
         
        for (int i=0; i<size(); i++) {
         
            prefs.putString("identity."+i+".description" ,get(i).getDescription());
            prefs.putString("identity."+i+".name"        ,get(i).getName());
            prefs.putString("identity."+i+".email"       ,get(i).getEMail());
            prefs.putString("identity."+i+".replyto"     ,get(i).getReplyTo());
            prefs.putString("identity."+i+".organization",get(i).getOrganization());
            prefs.putString("identity."+i+".signature"   ,get(i).getSignature());
         
        }
         */
        Preferences.save();
    }
    
    public IdentityStructure get(int Index) {
        Identity id = Preferences.getPreferances().getAccounts().getAccount(0).getIdentity(Index);
        if (Index<0) Index=0;
        if (Index>Preferences.getPreferances().getAccounts().getAccount(0).getIdentities().size()) Index=0;
        if (id == null) {
            return get(Index +1);
        }
        IdentityStructure is = new IdentityStructure(id);
        return is;
    }
    
    public void add(IdentityStructure aIdentity) {
        Preferences.getPreferances().getAccounts().getAccount(0).addIdentity(aIdentity.getID());
        /*if (aIdentity.getParent() == null) {
            aIdentity.setParent(XMLPreferences.getPreferances().getAccounts().getAccount(0).getIdentities());
        }*/
    }
    
    public void remove(int Index) {
        Preferences.getPreferances().getAccounts().getAccount(0).removeIdentity(Index);
    }
    
    public int size() {
        int i =Preferences.getPreferances().getAccounts().getAccount(0).getIdentities().size(); 
        return i;
    }
    
}
