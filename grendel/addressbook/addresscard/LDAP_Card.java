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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Lester Schueler <lesters@netscape.com>, 14 Nov 1997.
 *
 * Contributors: Christoph Toshok <toshok@netscape.com>
 */

package grendel.addressbook.addresscard;

import netscape.ldap.*;
import java.util.Enumeration;

//************************
//************************
public class LDAP_Card implements ICard {
    private LDAP_Server fParentSource;
    private LDAPEntry   fMyEntry;

    /** Constructor for LDAP server.
     */
    protected LDAP_Card(LDAP_Server aParentSource, LDAPEntry anEntry) {
        fParentSource = aParentSource;
        fMyEntry = anEntry;
    }

    /** Return the parent card source from where this card came from (if any)
     */
    public ICardSource getParent() {
        return fParentSource;
    }

    /** Add an attribute to the card.
     * FIX (this updates the server!)
     */
    public void addAttribute (IAttribute anAttribute) {
        //get the native attribute set.
//        LDAPAttributeSet nativeAttrSet = fMyEntry.getAttributeSet();

        //convert the local attribute into a native LDAP attribute and add.
//        nativeAttrSet.add(localAttr.getLDAPAttribute());
    }

    /** Get a value of the address card.
     */
    public IAttribute getAttribute (String anAttributeName) {
        return new LDAP_Attribute (fMyEntry.getAttribute(anAttributeName));
    }

    /** Get the full attribute set for this card.
     */
    public IAttributeSet getAttributeSet () {
        return new LDAP_AttributeSet (fMyEntry.getAttributeSet());
    }

    //LDAP specific methods:

    /** Rerturn the DN
     */
    public String getDN() {
        return fMyEntry.getDN();
    }
}
