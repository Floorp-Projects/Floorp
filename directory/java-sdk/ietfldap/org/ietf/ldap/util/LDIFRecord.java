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

import java.io.Serializable;
import org.ietf.ldap.LDAPControl;

/**
 * An object of this class represents an LDIF record in an LDIF
 * file (or in LDIF data). A record can contain a list of attributes
 * (which describes an entry) or a list of modifications (which
 * decribes the changes that need to be made to an entry).
 * Each record also has a distinguished name.
 * <P>
 *
 * You can get an <CODE>LDIFRecord</CODE> object from LDIF data
 * by calling the <CODE>nextRecord</CODE> method of the
 * <CODE>LDIF</CODE> object holding the data.
 * <P>
 *
 * If you are constructing a new <CODE>LDIFRecord</CODE> object,
 * you can specify the content of the record in one of the
 * following ways:
 * <P>
 *
 * <UL>
 * <LI>To create a record that specifies an entry, use an object of
 * the <CODE>LDIFAttributeContent</CODE> class.
 * <LI>To create a record that specifies a modification to be made,
 * use an object of one of the following classes:
 * <UL>
 * <LI>Use <CODE>LDIFAddContent</CODE> to add a new entry.
 * <LI>Use <CODE>LDIFModifyContent</CODE> to modify an entry.
 * <LI>Use <CODE>LDIFDeleteContent</CODE> to delete an entry.
 * </UL>
 * </UL>
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.util.LDIF
 * @see org.ietf.ldap.util.LDIFAddContent
 * @see org.ietf.ldap.util.LDIFModifyContent
 * @see org.ietf.ldap.util.LDIFDeleteContent
 * @see org.ietf.ldap.util.LDIFAttributeContent
 */
public class LDIFRecord implements Serializable {

    /**
     * Internal variables
     */
    private String m_dn = null;
    private LDIFBaseContent m_content = null;
    static final long serialVersionUID = -6537481934870076178L;

    /**
     * Constructs a new <CODE>LDIFRecord</CODE> object with the
     * specified content.
     * @param dn distinguished name of the entry associated with
     * the record
     * @param content content of the LDIF record.  You can specify
     * an object of the <CODE>LDIFAttributeContent</CODE>,
     * <CODE>LDIFAddContent</CODE>, <CODE>LDIFModifyContent</CODE>,
     * or <CODE>LDIFDeleteContent</CODE> classes.
     * @see org.ietf.ldap.util.LDIFAddContent
     * @see org.ietf.ldap.util.LDIFModifyContent
     * @see org.ietf.ldap.util.LDIFDeleteContent
     * @see org.ietf.ldap.util.LDIFAttributeContent
     */
    public LDIFRecord(String dn, LDIFContent content) {
        m_dn = dn;
        m_content = (LDIFBaseContent)content;
    }

    /**
     * Retrieves the distinguished name of the LDIF record.
     * @return the distinguished name of the LDIF record.
     */
    public String getDN() {
        return m_dn;
    }

    /**
     * Retrieves the content of the LDIF record.  The content is
     * an object of the <CODE>LDIFAttributeContent</CODE>,
     * <CODE>LDIFAddContent</CODE>, <CODE>LDIFModifyContent</CODE>,
     * or <CODE>LDIFDeleteContent</CODE> classes.
     * <P>
     *
     * To determine the class of the object, use the <CODE>getType</CODE>
     * method of that object.  <CODE>getType</CODE> returns one of
     * the following values:
     * <UL>
     * <LI><CODE>LDIFContent.ATTRIBUTE_CONTENT</CODE> (the object is an
     * <CODE>LDIFAttributeContent</CODE> object)
     * <LI><CODE>LDIFContent.ADD_CONTENT</CODE> (the object is an
     * <CODE>LDIFAddContent</CODE> object)
     * <LI><CODE>LDIFContent.MODIFICATION_CONTENT</CODE> (the object is an
     * <CODE>LDIFModifyContent</CODE> object)
     * <LI><CODE>LDIFContent.DELETE_CONTENT</CODE> (the object is an
     * <CODE>LDIFDeleteContent</CODE> object)
     * </UL>
     * <P>
     *
     * For example:
     * <PRE>
     * ...
     * import org.ietf.ldap.*;
     * import org.ietf.ldap.util.*;
     * import java.io.*;
     * import java.util.*;
     * ...
     *     try {
     *         // Parse the LDIF file test.ldif.
     *         LDIF parser = new LDIF( "test.ldif" );
     *         // Iterate through each LDIF record in the file.
     *         LDIFRecord nextRec = parser.nextRecord();
     *         while ( nextRec != null ) {
     *             // Based on the type of content in the record,
     *             // get the content and cast it as the appropriate
     *             // type.
     *             switch( nextRec.getContent().getType() ) {
     *                 case LDIFContent.ATTRIBUTE_CONTENT:
     *                     LDIFAttributeContent attrContent =
     *                         (LDIFAttributeContent)nextRec.getContent();
     *                     break;
     *                 case LDIFContent.ADD_CONTENT:
     *                     LDIFAddContent addContent =
     *                         (LDIFAddContent)nextRec.getContent();
     *                     break;
     *                 case LDIFContent.MODIFICATION_CONTENT:
     *                     LDIFModifyContent modifyContent =
     *                         (LDIFModifyContent)nextRec.getContent();
     *                     break;
     *                 case LDIFContent.DELETE_CONTENT:
     *                     LDIFDeleteContent deleteContent =
     *                         (LDIFDeleteContent)nextRec.getContent();
     *                     break;
     *             }
     *             ...
     *             // Iterate through each record.
     *             nextRec = parser.nextRecord();
     *         }
     *     } catch ( IOException e ) {
     *         System.out.println( "Error: " + e.toString() );
     *         System.exit(1);
     *     }
     * ...
     * </PRE>
     *
     * @return the content of the LDIF record.
     * @see org.ietf.ldap.util.LDIFAddContent
     * @see org.ietf.ldap.util.LDIFModifyContent
     * @see org.ietf.ldap.util.LDIFDeleteContent
     * @see org.ietf.ldap.util.LDIFAttributeContent
     */
    public LDIFContent getContent() {
        return m_content;
    }

    /**
     * Retrieves the list of controls specified in the content
     * of the LDIF record, if any.
     * @return an array of <CODE>LDAPControl</CODE> objects that
     * represent any controls specified in the LDIF record,
     * or <CODE>null</CODE> if none were specified.
     */
    public LDAPControl[] getControls() {
        return (m_content == null) ? null : m_content.getControls();
    }

    /**
     * Gets the string representation of the <CODE>LDIFRecord</CODE>
     * object.
     * @return the string representation of the LDIF record.
     */
    public String toString() {
        return "LDIFRecord {dn=" + m_dn + ", content=" + m_content + "}";
    }
}
