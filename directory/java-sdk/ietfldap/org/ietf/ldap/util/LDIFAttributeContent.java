/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap.util;

import java.util.Vector;
import org.ietf.ldap.LDAPAttribute;

/**
 * An object of this class represents the content of an LDIF record that
 * specifies an entry and its attributes.  This class implements the
 * <CODE>LDIFContent</CODE> interface.
 * <P>
 *
 * To get this object from an <CODE>LDIFRecord</CODE> object,
 * use the <CODE>getContent</CODE> method and cast the return value as
 * <CODE>LDIFAttributeContent</CODE>.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.util.LDIFRecord#getContent
 */
public class LDIFAttributeContent extends LDIFBaseContent {

    /**
     * Internal variables
     */
    private Vector m_attrs = new Vector();
    static final long serialVersionUID = -2912294697848028220L;

    /**
     * Constructs an empty <CODE>LDIFAttributeContent</CODE> object with
     * no attributes specified.  You can use the <CODE>addElement</CODE>
     * method to add attributes to this object.
     * @see org.ietf.ldap.util.LDIFAttributeContent#addElement
     */
    public LDIFAttributeContent() {
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return the content type (which is
     * <CODE>LDIFContent.ATTRIBUTE_CONTENT</CODE>).
     * @see org.ietf.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return ATTRIBUTE_CONTENT;
    }

    /**
     * Adds an attribute to the content of the LDIF record.
     * @param attr the attribute to add
     */
    public void addElement(LDAPAttribute attr) {
        m_attrs.addElement(attr);
    }

    /**
     * Retrieves the list of the attributes specified in the content
     * of the LDIF record.
     * @return an array of <CODE>LDAPAttribute</CODE> objects that
     * represent the attributes specified in the content of the LDIF record.
     */
    public LDAPAttribute[] getAttributes() {
        LDAPAttribute attrs[] = new LDAPAttribute[m_attrs.size()];
        for (int i = 0; i < m_attrs.size(); i++) {
            attrs[i] = (LDAPAttribute)m_attrs.elementAt(i);
        }
        return attrs;
    }

    /**
     * Returns the string representation of the content of the LDIF record.
     * @return the string representation of the content of the LDIF record.
     */
    public String toString() {
        String s = "";
        for (int i = 0; i < m_attrs.size(); i++) {
            s = s + ((LDAPAttribute)m_attrs.elementAt(i)).toString();
        }
        if ( getControls() != null ) {
            s += getControlString();
        }
        return "LDIFAttributeContent {" + s + "}";
    }
}
