/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Identity.java
 *
 * Created on 09 August 2005, 21:16
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

import grendel.prefs.xml.*;
import java.util.Set;

/**
 *
 * @author hash9
 */
public class Identity extends XMLPreferences {
    
    /** Creates a new instance of Identity */
    public Identity() {
    }
    
    /** Creates a new instance of Identity */
    public Identity(String name, String email, String reply_to, String description, String organisation) {
        setName(name);
        setEMail(email);
        setReplyTo(reply_to);
        setDescription(description);
        setOrganisation(organisation);
    }
    
    public Identity(String name, String email, String description, String organisation) {
        setName(name);
        setEMail(email);
        setDescription(description);
        setOrganisation(organisation);
    }
    
    public Identity(String name, String email, String description) {
        setName(name);
        setEMail(email);
        setDescription(description);
    }
    
    public String getDescription() {
        return this.getProperty("description","");
    }
    
    public String getEMail() {
        return this.getProperty("email","");
    }
    
    public String getName() {
        return this.getProperty("name","");
    }
    
    public String getOrganisation() {
        return this.getProperty("organisation","");
    }
    
    public String getReplyTo() {
        return this.getProperty("reply_to","");
    }
    
    public XMLPreferences getSigs() {
        return this.getPropertyPrefs("sigs");
    }
        
    public void setDescription(String description) {
        this.setProperty("description",description);
    }
    
    public void setEMail(String email) {
        this.setProperty("email",email);
    }
    
    public void setName(String name) {
        this.setProperty("name",name);
    }
    
    public void setOrganisation(String organisation) {
        this.setProperty("organisation",organisation);
    }
    
    public void setReplyTo(String reply_to) {
        this.setProperty("reply_to",reply_to);
    }
    
    public void setSigs(XMLPreferences sigs) {
        this.setProperty("sigs",sigs);
    }
    
    private int getNextEmptySigIndex() {
        XMLPreferences p = getSigs();
        Set<String> keys = p.keySet();
        boolean in_use = false;
        int i = -1;
        do {
            i++;
            in_use =  keys.contains(Integer.toString(i));
        } while(in_use);
        return i;
    }
    
    public int addSig(String sig) {
        int i = getNextEmptySigIndex();
        getSigs().setProperty(Integer.toString(i),sig);
        return i;
    }
    
    public void removeSig(int i) {
        getSigs().remove(i);
    }
    
}
