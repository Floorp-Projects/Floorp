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

/**
 * An object of this class represents the content of an LDIF record that
 * specifies changes to an RDN or the DN of an entry.  This class
 * implements the <CODE>LDIFContent</CODE> interface.
 * <P>
 *
 * To get this object from an <CODE>LDIFRecord</CODE> object,
 * use the <CODE>getContent</CODE> method and cast the return value as
 * <CODE>LDIFModDNContent</CODE>.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.util.LDIFRecord#getContent
 */
public class LDIFModDNContent extends LDIFBaseContent {
    /**
     * Internal variables
     */
    private String m_newParent = null;
    private String m_rdn = null;
    private boolean m_deleteOldRDN = false;
    static final long serialVersionUID = 1352504898614557791L;

    /**
     * Constructs an empty <CODE>LDIFModDNContent</CODE> object.
     * To specify the modifications to be made to the entry, use
     * the <CODE>setRDN</CODE>, <CODE>setNewParent</CODE>,
     * and <CODE>setDeleteOldRDN</CODE> methods.
     * @see netscape.ldap.util.LDIFModDNContent#setRDN
     * @see netscape.ldap.util.LDIFModDNContent#setNewParent
     * @see netscape.ldap.util.LDIFModDNContent#setDeleteOldRDN
     */
    public LDIFModDNContent() {
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return The content type (which is
     * <CODE>LDIFContent.MODDN_CONTENT</CODE>).
     * @see netscape.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return MODDN_CONTENT;
    }

    /**
     * Sets the new RDN that should be assigned to the entry.
     * @param rdn The new RDN.
     * @see netscape.ldap.util.LDIFModDNContent#getRDN
     */
    public void setRDN(String rdn) {
        m_rdn = rdn;
    }

    /**
     * Returns the new RDN specified in the content of the LDIF record.
     * @return the new RDN.
     * @see netscape.ldap.util.LDIFModDNContent#setRDN
     */
    public String getRDN() {
        return m_rdn;
    }

    /**
     * Sets the new parent DN that should be assigned to the entry.
     * @param parent The new parent DN for the entry.
     * @see netscape.ldap.util.LDIFModDNContent#getNewParent
     */
    public void setNewParent(String parent) {
        m_newParent = parent;
    }

    /**
     * Returns the entry's new parent DN, if specified in the content
     * of the LDIF record.
     * @return The new parent for the entry.
     * @see netscape.ldap.util.LDIFModDNContent#setNewParent
     */
    public String getNewParent() {
        return m_newParent;
    }

    /**
     * Sets whether or not the old RDN should be removed as an
     * attribute in the entry.
     * @param bool If <CODE>true</CODE>, remove the attribute representing
     * the RDN.  If <CODE>false</CODE>, leave the attribute in the entry.
     * @see netscape.ldap.util.LDIFModDNContent#getDeleteOldRDN
     */
    public void setDeleteOldRDN(boolean bool) {
        m_deleteOldRDN = bool;
    }

    /**
     * Determines if the content of the LDIF record specifies that
     * the old RDN should be removed as an attribute in the entry.
     * @return If <CODE>true</CODE>, the change specifies that the
     * the attribute representing the RDN should be removed.
     * If <CODE>false</CODE>, the change specifies that the attribute
     * should be left in the entry.
     * @see netscape.ldap.util.LDIFModDNContent#setDeleteOldRDN
     */
    public boolean getDeleteOldRDN() {
        return m_deleteOldRDN;
    }

    /**
     * Returns string representation of the content of the LDIF record.
     * @return The string representation of the content of the LDIF record.
     */
    public String toString() {
        String s = "";
        if (m_newParent == null)
            s = s + "new parent() ";
        else
            s = s + "new parent( "+m_newParent+" ), ";
        if (m_deleteOldRDN)
            s = s + "deleteOldRDN( true ), ";
        else
            s = s + "deleteOldRDN( false ), ";
        if (m_rdn == null)
            s = s + "new rdn()";
        else
            s = s + "new rdn( "+m_rdn+" )";

        if ( getControls() != null ) {
            s += getControlString();
        }
        return "LDIFModDNContent {" + s + "}";
    }
}
