/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.util;

import java.util.Vector;
import netscape.ldap.LDAPAttribute;

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
 * @see netscape.ldap.util.LDIFRecord#getContent
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
     * @see netscape.ldap.util.LDIFAttributeContent#addElement
     */
    public LDIFAttributeContent() {
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return The content type (which is
     * <CODE>LDIFContent.ATTRIBUTE_CONTENT</CODE>).
     * @see netscape.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return ATTRIBUTE_CONTENT;
    }

    /**
     * Adds an attribute to the content of the LDIF record.
     * @param attr The attribute that you want to add.
     */
    public void addElement(LDAPAttribute attr) {
        m_attrs.addElement(attr);
    }

    /**
     * Retrieves the list of the attributes specified in the content
     * of the LDIF record.
     * @return An array of <CODE>LDAPAttribute</CODE> objects that
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
     * @return The string representation of the content of the LDIF record.
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
