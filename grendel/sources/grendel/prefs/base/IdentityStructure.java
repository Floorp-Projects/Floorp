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


public class IdentityStructure {
    private Identity id;
    /*private Prefs parent;*/
    public IdentityStructure() {
        id = new Identity();
    }
    
    public IdentityStructure(Identity id/*, Prefs parent*/) {
        this.id = id;
        /*this.parent = parent;*/
    }
    
    public IdentityStructure(String aDescription) {
        id = new Identity();
        id.setDescription(aDescription);
    }
    
    public String getOrganization() {
        return id.getOrganisation();
    }
    
    public String getSignature() {
        String s = id.getSigs().getProperty(Integer.toString(0),"");
        if (s == null) {
            return "";
        } else {
            return s;
        }
    }
    
    public void setOrganization(String aOrganization) {
        id.setOrganisation(aOrganization);
    }
    
    public void setSignature(String aSignature) {
        id.getSigs().setProperty(Integer.toString(0),aSignature);
    }
    
    public void setName(String name) {
        id.setName(name);
    }
    
    public void setEMail(String email) {
        id.setEMail(email);
    }
    
    public void setOrganisation(String organisation) {
        id.setOrganisation(organisation);
    }
    
    public void setDescription(String description) {
        /*if (parent != null) {
            Object o = parent.getProperty(getDescription());
            parent.remove(getDescription());
            parent.put(description,o);
        }*/
        id.setDescription(description);
    }
    
    public void setReplyTo(String reply_to) {
        id.setReplyTo(reply_to);
    }
    public String getName() {
        return id.getName();
    }
    
    public String getDescription() {
        return id.getDescription();
    }
    
    public String getReplyTo() {
        return id.getReplyTo();
    }
    
    public String getOrganisation() {
        return id.getOrganisation();
    }
    
    public String getEMail() {
        return id.getEMail();
    }
    
    public Identity getID() {
        return id;
    }
    
    /*public Prefs getParent() {
        return parent;
    }*/
    
    /*public void setParent(Prefs p) {
        parent = p;
    }**/
    
}