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
public class LDAP_AttributeSet implements IAttributeSet, Enumeration {
    private LDAPAttributeSet    fMyAttributeSet;
    private Enumeration         fAttrEnum;

    /** Create a new emtpy AttributeSet
     */
    protected LDAP_AttributeSet (LDAPAttributeSet aSet) {
        fMyAttributeSet = aSet;
    }

    /** Add an attrbiute to the set.
     */
//    public void add(AC_Attribute anAttribute) {
//        fSet.put (anAttribute.getName(), anAttribute.getValue());
//    }

    /** Return the named attribute.
     */
    public IAttribute getAttribute(String anAttributeName) {
        return new LDAP_Attribute (fMyAttributeSet.getAttribute(anAttributeName));
    }

    /** Return the number of attributes in the set.
     */
    public int size() {
        return fMyAttributeSet.size();
    }

    /** Return the attribute set enumeration.
     *  We enumerate our own attribute set.
     */
    public Enumeration getEnumeration() {
        //FIX: Not multi-threaded!
        //initiailize the enumeration cursor.
        fAttrEnum = fMyAttributeSet.getAttributes();
        return this;
    }

    /** Enumeration methods
     */
    public boolean hasMoreElements() {
        return fAttrEnum.hasMoreElements();
    }

    /** Enumeration methods
     */
    public Object nextElement() {
        //get the next attribute.
        LDAPAttribute attr = (LDAPAttribute) fAttrEnum.nextElement();

        //cache the element.
//        fMyElementCache.add (card);

        //return the LDAPAttribute rapped in a IAttribute.
        return new LDAP_Attribute (attr);
    }
}
