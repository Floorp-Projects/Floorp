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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap.util;

import netscape.ldap.LDAPControl;

/**
 * This interface represents the content of an LDIF record.
 * An LDIF record can specify an entry or modifications to be
 * made to an entry.
 * <P>
 *
 * The following classes implement this interface:
 * <P>
 *
 * <UL>
 * <LI><CODE>LDIFAttributeContent</CODE> (represents the content
 * of an LDIF record that specifies an entry)
 * <LI><CODE>LDIFAddContent</CODE> (represents the content
 * of an LDIF record that adds a new entry)
 * <LI><CODE>LDIFModifyContent</CODE> (represents the content
 * of an LDIF record that modifies an entry)
 * <LI><CODE>LDIFDeleteContent</CODE> (represents the content
 * of an LDIF record that deletes an entry)
 * <LI><CODE>LDIFModDNContent</CODE> (represents the content
 * of an LDIF record that changes the RDN or DN of an entry)
 * </UL>
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.util.LDIFRecord
 * @see netscape.ldap.util.LDIFAttributeContent
 * @see netscape.ldap.util.LDIFAddContent
 * @see netscape.ldap.util.LDIFModifyContent
 * @see netscape.ldap.util.LDIFDeleteContent
 * @see netscape.ldap.util.LDIFModDNContent
 */
public interface LDIFContent {

   /**
    * The LDIF record specifies an entry and its attributes.
    */
    public final static int ATTRIBUTE_CONTENT = 0;

   /**
    * The LDIF record specifies a new entry to be added.
    */
    public final static int ADD_CONTENT = 1;

   /**
    * The LDIF record specifies an entry to be deleted.
    */
    public final static int DELETE_CONTENT = 2;

   /**
    * The LDIF record specifies modifications to an entry.
    */
    public final static int MODIFICATION_CONTENT = 3;

   /**
    * The LDIF record specifies changes to the DN or RDN of an entry.
    */
    public final static int MODDN_CONTENT = 4;

    /**
     * Determine the content type.
     * @return The content type, identified by one of the following values:
     * <UL>
     * <LI>ATTRIBUTE_CONTENT (specifies an entry and its attributes)
     * <LI>ADD_CONTENT (specifies a new entry to be added)
     * <LI>DELETE_CONTENT (specifies an entry to be deleted)
     * <LI>MODIFICATION_CONTENT (specifies an entry to be modified)
     * <LI>MODDN_CONTENT (specifies a change to the RDN or DN of an entry)
     * </UL>
     */
    public int getType();

    /**
     * Retrieves the list of controls specified in the content
     * of the LDIF record, if any
     * @return An array of <CODE>LDAPControl</CODE> objects that
     * represent any controls specified in the the LDIF record,
     * or <CODE>null</CODE> if none were specified.
     */
    public LDAPControl[] getControls();

    /**
     * Sets the list of controls
     * @param controls An array of <CODE>LDAPControl</CODE> objects
     * or <CODE>null</CODE> if none are to be specified.
     */
    public void setControls( LDAPControl[] controls );

    /**
     * Returns the string representation of the content of the LDIF record.
     * @return String representation of the content of the LDIF record.
     */
    public String toString();
}
