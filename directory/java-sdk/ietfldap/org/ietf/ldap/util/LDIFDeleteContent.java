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

/**
 * An object of this class represents the content of an LDIF record that
 * specifies the deletion of an entry.  This class implements the
 * <CODE>LDIFContent</CODE> interface.
 * <P>
 *
 * To get this object from an <CODE>LDIFRecord</CODE> object,
 * use the <CODE>getContent</CODE> method and cast the return value as
 * <CODE>LDIFDeleteContent</CODE>.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.util.LDIFRecord#getContent
 */
public class LDIFDeleteContent extends LDIFBaseContent {

    static final long serialVersionUID = -6581979396116035503L;

    /**
     * Constructs an <CODE>LDIFDeleteContent</CODE> object
     * to specify that an entry should be deleted.  (The DN
     * identifying the entry is part of the <CODE>LDIFRecord</CODE>
     * object.
     */
    public LDIFDeleteContent() {
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return the content type (which is
     * <CODE>LDIFContent.DELETE_CONTENT</CODE>).
     * @see org.ietf.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return DELETE_CONTENT;
    }

    /**
     * Returns the string representation of the content of the LDIF record.
     * @return the string representation of the content of the LDIF record.
     */
    public String toString() {
        String s = "";
        if ( getControls() != null ) {
            s += getControlString();
        }
        return "LDIFDeleteContent {" + s + "}";
    }
}
