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

import org.ietf.ldap.LDAPAttribute;

/**
 *
 * An object of this class represents the content of an LDIF record that
 * specifies a new entry to be added.  This class implements the
 * <CODE>LDIFContent</CODE> interface.
 * <P>
 *
 * To get this object from an <CODE>LDIFRecord</CODE> object,
 * use the <CODE>getContent</CODE> method and cast the return value as
 * <CODE>LDIFAddContent</CODE>.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.util.LDIFRecord#getContent
 */
public class LDIFAddContent extends LDIFBaseContent {

    /**
     * Internal variables
     */
    private LDAPAttribute m_attrs[] = null;
    static final long serialVersionUID = -665548826721177756L;

    /**
     * Constructs a new <CODE>LDIFAddContent</CODE> object with
     * the specified attributes.
     * @param attrs an array of <CODE>LDAPAttribute</CODE> objects
     * representing the attributes of the entry to be added
     */
    public LDIFAddContent(LDAPAttribute attrs[]) {
        m_attrs = attrs;
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return the content type (which is
     * <CODE>LDIFContent.ADD_CONTENT</CODE>).
     * @see org.ietf.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return ADD_CONTENT;
    }

    /**
     * Retrieves the list of the attributes specified in the content
     * of the LDIF record.
     * @return an array of <CODE>LDAPAttribute</CODE> objects that
     * represent the attributes specified in the content of the LDIF record.
     */
    public LDAPAttribute[] getAttributes() {
        return m_attrs;
    }

    /**
     * Returns the string representation of the content of the LDIF record.
     * @return the string representation of the content of the LDIF record.
     */
    public String toString() {
        String s = "";
        for (int i = 0; i < m_attrs.length; i++) {
            s = s + m_attrs[i].toString();
        }
        if ( getControls() != null ) {
            s += getControlString();
        }
        return "LDIFAddContent {" + s + "}";
    }
}
