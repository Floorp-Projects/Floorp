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

import java.util.Vector;
import java.util.Enumeration;

//************************
//************************
public class AC_Attribute implements IAttribute {
    private String  fName;
    private Vector  fValues;

    /** Create an attribute with mutliple values.
     */
    public AC_Attribute (String aName, String[] aValues) {
        fName = aName;
        fValues = new Vector();

        if (null != aValues) {
            for (int i = 0; i < aValues.length; i++) {
                if (null != aValues[i])
                    add(aValues[i]);
            }
        }
    }

    /** Create an attribute with one value.
     */
    public AC_Attribute (String aName, String aValue) {
        fName = aName;
        fValues = new Vector();

        if (null != aValue)
            add(aValue);
    }

    /** Create an attribute with no values.
     */
    public AC_Attribute (String aName) {
        fName = aName;
        fValues = new Vector();
    }

    /** Add an value to this attribute
     */
    public void add (String aValue) {
        fValues.addElement (aValue);
    }

    /** Return the name of this attribute.
     */
    public String getName() {
        return fName;
    }

    /** Return the first string value of this attribute.
     */
    public String getValue () {
        return (String) fValues.elementAt(0);
    }

    /** Return the string values of this attribute.
     */
    public Enumeration getStringValues () {
        return fValues.elements();
    }

    /** Has the attribute new to the parent?
     */
    public boolean isNew() {
        return false;
    }

    /** Has the attribute been deleted since the last update with it's parent.
     */
    public boolean isDeleted() {
        return false;
    }

    /** Has the attribute been modified since the last update with it's parent.
     */
    public boolean isModified() {
        return false;
    }
}
