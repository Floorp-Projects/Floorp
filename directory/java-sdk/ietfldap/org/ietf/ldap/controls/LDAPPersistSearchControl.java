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
package org.ietf.ldap.controls;

import java.io.*;
import org.ietf.ldap.client.JDAPBERTagDecoder;
import org.ietf.ldap.LDAPControl;
import org.ietf.ldap.ber.stream.*;

/**
 * Represents an LDAP v3 server control that specifies a persistent
 * search (an ongoing search operation), which allows your LDAP client
 * to get notification of changes to the directory.  (The OID for this
 * control is 2.16.840.1.113730.3.4.3.)  You can use this control in
 * conjunction with an "entry change notification" control (represented
 * by <CODE>LDAPEntryChangeControl</CODE> object.
 * <P>
 *
 * To use persistent searching for change notification, you create a
 * "persistent search" control that specifies the types of changes that
 * you want to track.  You include the control in a search request.
 * If an entry in the directory is changed, the server determines if
 * the entry matches the search criteria in your request and if the
 * change is the type of change that you are tracking.  If both of
 * these are true, the server sends the entry to your client.
 * <P>
 *
 * The server can also include an "entry change notification" control
 * with the entry.  (The OID for this control is 2.16.840.1.113730.3.4.7.)
 * This control contains additional information about the
 * change made to the entry, including the type of change made,
 * the change number (which corresponds to an item in the server's
 * change log, if the server supports a change log), and, if the
 * entry was renamed, the old DN of the entry.
 * <P>
 *
 * When constructing an <CODE>LDAPPersistSearchControl</CODE> object,
 * you can specify the following information:
 * <P>
 *
 * <UL>
 * <LI>the type of change you want to track (added, modified, deleted,
 * or renamed entries)
 * <LI>a preference indicating whether or not you want the server to
 * return all entries that initially matched the search criteria
 * (rather than only the entries that change)
 * <LI>a preference indicating whether or not you want entry change
 * notification controls included with every entry returned by the
 * server
 * </UL>
 * <P>
 *
 * For example:
 * <PRE>
 * ...
 * LDAPConnection ld = new LDAPConnection();
 *
 * try {
 *     // Connect to server.
 *     ld.connect( 3, hostname, portnumber, "", "" );
 *
 *     // Create a persistent search control.
 *     int op = LDAPPersistSearchControl.ADD |
 *         LDAPPersistSearchControl.MODIFY |
 *         LDAPPersistSearchControl.DELETE |
 *         LDAPPersistSearchControl.MODDN;
 *     boolean changesOnly = true;
 *     boolean returnControls = true;
 *     boolean isCritical = true;
 *     LDAPPersistSearchControl persistCtrl = new
 *         LDAPPersistSearchControl( op, changesOnly,
 *         returnControls, isCritical );
 *
 *     // Set the search constraints to use that control.
 *     LDAPSearchConstraints cons = ld.getSearchConstraints();
 *     cons.setBatchSize( 1 );
 *     cons.setServerControls( persistCtrl );
 *
 *     // Start the persistent search.
 *     LDAPSearchResults res = ld.search( "o=Airius.com",
 *               LDAPConnection.SCOPE_SUB, "(cn=Barbara*)", null, false, cons );
 *
 *     // Loop through the incoming results.
 *     while ( res.hasMore() ) {
 *     ...
 *     }
 * ...
 * }
 * </PRE>
 *
 * @see org.ietf.ldap.LDAPControl
 * @see org.ietf.ldap.controls.LDAPEntryChangeControl
 */

public class LDAPPersistSearchControl extends LDAPControl {

    /**
     * Default constructor
     */
    public LDAPPersistSearchControl()
    {
        super(PERSISTENTSEARCH, true, null);
    }

    /**
     * Constructs an <CODE>LDAPPersistSearchControl</CODE> object
     * that specifies a persistent search.
     *
     * @param changeTypes the change types to monitor. You can perform
     * a bitwise OR on any of the following values and specify the result as
     * the change types:
     * <UL>
     * <LI><CODE>LDAPPersistSearchControl.ADD</CODE> (to track new entries
     * added to the directory)
     * <LI><CODE>LDAPPersistSearchControl.DELETE</CODE> (to track entries
     * removed from the directory)
     * <LI><CODE>LDAPPersistSearchControl.MODIFY</CODE> (to track entries
     * that have been modified)
     * <LI><CODE>LDAPPersistSearchControl.MODDN</CODE> (to track entries
     * that have been renamed)
     * </UL>
     * @param changesOnly <code>true</code> if you do not want the server
     * to return all existing entries in the directory that match the
     * search criteria.  (Use this if you just want the changed entries 
     * to be returned.)
     * @param returnControls <code>true</code> if you want the server to return
     * entry change controls with each entry in the search results
     * @param isCritical <code>true</code> if this control is critical to
     * the search operation. (If the server does not support
     * this control, you may not want the server to perform the search
     * at all.)
     * @see org.ietf.ldap.LDAPControl
     * @see org.ietf.ldap.controls.LDAPEntryChangeControl
     */
    public LDAPPersistSearchControl(int changeTypes, boolean changesOnly,
        boolean returnControls, boolean isCritical) {
        super(PERSISTENTSEARCH, isCritical, null);
        _value = createPersistSearchSpecification(changeTypes,
          changesOnly, returnControls);
        _changeTypes = changeTypes;
        _changesOnly = changesOnly;
        _returnECs = returnControls;
    }

    /**
     * Gets the change types monitored by this control.
     * @return integer representing the change types to monitor.
     * This value can be the bitwise OR of <code>ADD, DELETE, MODIFY,</code>
     * and/or <code>MODDN</code>. If the change type is unknown,
     * this method returns -1.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangeTypes
     */
    public int getChangeTypes() {
        return _changeTypes;
    }

    /**
     * Indicates whether you want the server to send any existing
     * entries that already match the search criteria or only the
     * entries that have changed.
     * @return if <code>true</code>, the server returns only the
     * entries that have changed.  If <code>false</code>, the server
     * also returns any existing entries that match the search criteria
     * but have not changed.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangesOnly
     */
    public boolean getChangesOnly() {
        return _changesOnly;
    }

    /**
     * Indicates whether or not the server includes an "entry change
     * notification" control with each entry it sends back to the client
     * during the persistent search.
     * @return <code>true</code> if the server includes "entry change
     * notification" controls with the entries it sends during the
     * persistent search.
     * @see org.ietf.ldap.controls.LDAPEntryChangeControl
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setReturnControls
     */
    public boolean getReturnControls() {
        return _returnECs;
    }

    /**
     * Sets the change types that you want monitored by this control.
     * @param types integer representing the change types to monitor
     * This value can be the bitwise OR of <code>ADD, DELETE, MODIFY,</code>
     * and/or <code>MODDN</code>.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangeTypes
     */
    public void setChangeTypes(int types) {
        _changeTypes = types;
    }

    /**
     * Specifies whether you want the server to send any existing
     * entries that already match the search criteria or only the
     * entries that have changed.
     * @param changesOnly if <code>true</code>, the server returns only the
     * entries that have changed.  If <code>false</code>, the server
     * also returns any existing entries that match the search criteria
     * but have not changed.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangesOnly
     */
    public void setChangesOnly(boolean changesOnly) {
        _changesOnly = changesOnly;
    }

    /**
     * Specifies whether you want the server to include an "entry change
     * notification" control with each entry it sends back to the client
     * during the persistent search.
     * @param returnControls if <code>true</code>, the server includes
     * "entry change notification" controls with the entries it sends
     * during the persistent search
     * @see org.ietf.ldap.controls.LDAPEntryChangeControl
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setReturnControls
     */
    public void setReturnControls(boolean returnControls) {
        _returnECs = returnControls;
    }

    /**
     * Takes an input byte array and extracts the ber elements, assigning
     * them to appropriate fields in the entry change control.
     *
     * @param c byte array that contains BER elements
     * @return the entry change control.
     * @deprecated LDAPEntryChangeControl controls are now automatically 
     * instantiated.
     */
    public LDAPEntryChangeControl parseResponse(byte[] c) {
        LDAPEntryChangeControl con = new LDAPEntryChangeControl();

        ByteArrayInputStream inStream = new ByteArrayInputStream(c);
        BERSequence seq = new BERSequence();
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();
        int[] numRead = new int[1];
        numRead[0] = 0;

        try {
            /* a sequence */
            BERSequence s = (BERSequence)BERElement.getElement(decoder, inStream,
              numRead);

            BEREnumerated enum = (BEREnumerated)s.elementAt(0);

            con.setChangeType(enum.getValue());

            if (s.size() > 1) {
                if (s.elementAt(1) instanceof BEROctetString) {
                    BEROctetString str = (BEROctetString)s.elementAt(1);
                    con.setPreviousDN(new String(str.getValue(), "UTF8"));
                } else if (s.elementAt(1) instanceof BERInteger) {
                    BERInteger num = (BERInteger)s.elementAt(1);
                    con.setChangeNumber(num.getValue());
                }
            }
            if (s.size() > 2) {
                BERInteger num = (BERInteger)s.elementAt(2);
                con.setChangeNumber(num.getValue());
            }
        } catch (Exception e) {
            return null;
        }

        return con;
    }

    /**
     * Returns an "entry change notification" control if the control is in
     * the specified array of controls.  Use this method to retrieve an "entry
     * change notification" control included with an entry sent by the server.
     * <P>
     *
     * You can get the controls returned by the server by using the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * <P>
     *
     * For example:
     * <PRE>
     * ...
     * LDAPConnection ld = new LDAPConnection();
     * try {
     *     // Connect to the server, set up the persistent search control,
     *     // and set up the search constraints.
     *     ...
     *
     *     // Search the directory.
     *     LDAPSearchResults res = ld.search( "o=Airius.com",
     *         LDAPConnection.SCOPE_SUB, "(cn=Barbara*)", attrs, false, cons );
     *
     *     // Determine if the server sent a control back to you.
     *     LDAPControl[] returnedControls = ld.getResponseControls();
     *     if ( returnedControls != null ) {
     *
     *         // Get the entry change control.
     *         LDAPEntryChangeControl entryCtrl = null;
     *         for ( int i = 0; i < returnedControls.length; i++ ) {
     *             if ( returnedControls[i] instanceof LDAPEntryChangeControl ) {
     *                 entryCtrl = (LDAPEntryChangeControl)returnedControls[i];
     *                 break;
     *             }
     *         }
     *         if ( entryCtrl != null ) {
     *
     *             // Get and print the type of change made to the entry.
     *             int changeType = entryCtrl.getChangeType();
     *             if ( changeType != -1 ) {
     *                 System.out.print( "Change made: " );
     *                 switch ( changeType ) {
     *                 case LDAPPersistSearchControl.ADD:
     *                     System.out.println( "Added new entry." );
     *             break;
     *                 case LDAPPersistSearchControl.MODIFY:
     *                     System.out.println( "Modified entry." );
     *             break;
     *                 case LDAPPersistSearchControl.DELETE:
     *                     System.out.println( "Deleted entry." );
     *             break;
     *                 case LDAPPersistSearchControl.MODDN:
     *                     System.out.println( "Renamed entry." );
     *             break;
     *                 }
     *             }
     *
     *             // Get and print the change number corresponding
     *             // to the change.
     *             int changeNumber = entryCtrl.getChangeNumber();
     *             if ( changeNumber != -1 )
     *                 System.out.println( "Change log number: " + changeNumber);
     *
     *             // Get and print the previous DN of the entry,
     *             // if the entry was renamed.
     *             LDAPDN oldDN = entryCtrl.getPreviousDN();
     *             if ( oldDN != null )
     *                     System.out.println( "Previous DN: " + oldDN );
     *
     *         } else {
     *
     *             System.out.println( "No entry change control." );
     *         }
     *     }
     *     ...
     * }
     * ...
     *
     * </PRE>
     *
     * @param controls an array of <CODE>LDAPControl</CODE> objects,
     * representing the controls returned by the server
     * with an entry. To get these controls, use the
     * <CODE>getResponseControls</CODE> method of the
     * <CODE>LDAPConnection</CODE> class.
     * @return an <CODE>LDAPEntryChangeControl</CODE> object representing
     * the entry change control sent by the server.  If no entry change
     * control was sent, this method returns null.
     * @see org.ietf.ldap.controls.LDAPEntryChangeControl
     * @see org.ietf.ldap.LDAPConnection#getResponseControls
     * @deprecated LDAPEntryChangeControl controls are now automatically 
     * instantiated.
     */
    public static LDAPEntryChangeControl parseResponse(LDAPControl[] controls) {

        LDAPPersistSearchControl con = new LDAPPersistSearchControl();

        for (int i=0; (controls != null) && (i < controls.length); i++) {

            // get the entry change control
            if (controls[i].getID().equals(LDAPEntryChangeControl.ENTRYCHANGED)) {
                return con.parseResponse(controls[i].getValue());
            }
        }

        return null;
    }

    /**
     * Creates a "flattened" BER encoding of the persistent search
     * specifications and returns it as a byte array.
     * @param changeTypes the change types to monitor on the server
     * @param changesOnly <code>true</code> to skip the initial search
     * @param returnECs <code>true</code> if entry change controls are to be
     * returned with the search results
     * @return the BER-encoded data.
     */
    private byte[] createPersistSearchSpecification(int changeTypes,
        boolean changesOnly, boolean returnECs) {

        /* A sequence */
        BERSequence seq = new BERSequence();
        seq.addElement(new BERInteger(changeTypes));
        seq.addElement(new BERBoolean(changesOnly));
        seq.addElement(new BERBoolean(returnECs));

        /* return a byte array */
        return flattenBER( seq );
    }

    public String toString() {
        StringBuffer sb = new StringBuffer("{PersistSearchCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        sb.append(" returnEntryChangeCtrls=");
        sb.append(_returnECs);
        
        sb.append(" changesOnly=");
        sb.append(_changesOnly);

        sb.append(" changeTypes=");
        sb.append(typesToString(_changeTypes));
        
        sb.append("}");

        return sb.toString();
    }


    /**
     * This method is also used by LDAPentryChangeControl.toString()
     */
    static String typesToString(int changeTypes) {
        String types = "";

        if ((changeTypes & ADD) != 0) {
            types += (types.length() > 0) ? "+ADD" : "ADD";
        }
        if ((changeTypes & DELETE) != 0) {
            types += (types.length() > 0) ? "+DEL" : "DEL";
        }
        if ((changeTypes & MODIFY) != 0) {
            types += (types.length() > 0) ? "+MOD" : "MOD";
        }
        if ((changeTypes & MODDN) != 0) {
            types += (types.length() > 0) ? "+MODDN" : "MODDN";
        }
        return types;
    }        

    private int _changeTypes = 1;
    private boolean _changesOnly = false;
    private boolean _returnECs = false;

    /**
     * Change type specifying that you want to track additions of new
     * entries to the directory.  You can either specify this change type
     * when constructing an <CODE>LDAPPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangeTypes
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangeTypes
     */
    public static final int ADD = 1;

    /**
     * Change type specifying that you want to track removals of
     * entries from the directory.  You can either specify this change type
     * when constructing an <CODE>LDAPPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangeTypes
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangeTypes
     */
    public static final int DELETE = 2;

    /**
     * Change type specifying that you want to track modifications of
     * entries in the directory.  You can either specify this change type
     * when constructing an <CODE>LDAPPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangeTypes
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangeTypes
     */
    public static final int MODIFY = 4;

    /**
     * Change type specifying that you want to track modifications of the
     * DNs of entries in the directory.  You can either specify this change type
     * when constructing an <CODE>LDAPPersistSearchControl</CODE> or
     * by using the <CODE>setChangeTypes</CODE> method.
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#getChangeTypes
     * @see org.ietf.ldap.controls.LDAPPersistSearchControl#setChangeTypes
     */
    public static final int MODDN = 8;
    public final static String PERSISTENTSEARCH  = "2.16.840.1.113730.3.4.3";
}
