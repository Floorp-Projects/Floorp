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
package netscape.ldap.controls;

import java.io.*;
import netscape.ldap.client.JDAPBERTagDecoder;
import netscape.ldap.LDAPControl;
import netscape.ldap.ber.stream.*;

/**
 * Represents control data for returning paged results from a search.
 *
 * @version 1.0
 *
 *<PRE>
 *      VirtualListViewResponse ::= SEQUENCE {
 *               firstPosition    INTEGER,
 *               contentCount     INTEGER,
 *               virtualListViewResult ENUMERATED {
 *                 success                  (0),
 *                 unwillingToPerform       (53),
 *                 insufficientAccessRights (50),
 *                 operationsError          (1),
 *                 busy                     (51),
 *                 timeLimitExceeded        (3),
 *                 adminLimitExceeded       (11),
 *                 sortControlMissing       (60),
 *                 indexRangeError          (?),
 *               }
 *     }
 *</PRE>
 */

public class LDAPVirtualListResponse extends LDAPControl {
    public final static String VIRTUALLISTRESPONSE = "2.16.840.1.113730.3.4.10";

    /**
     * Blank constructor for internal use in <CODE>LDAPVirtualListResponse</CODE>.
     * @see netscape.ldap.LDAPControl
     */
    LDAPVirtualListResponse() {
        super( VIRTUALLISTRESPONSE, true, null );
    }

    /**
     * Constructs a new <CODE>LDAPVirtualListResponse</CODE> object.
     * @param value A BER encoded byte array.
     * @see netscape.ldap.LDAPControl
     */
    public LDAPVirtualListResponse( byte[] value ) {
        super( VIRTUALLISTRESPONSE, true, null );
        m_value = value;
        parseResponse();
    }

    /**
     * Gets the size of the virtual result set.
     * @return The size of the virtual result set, or -1 if not known.
     */
    public int getContentCount() {
        return m_contentCount;
    }

    /**
     * Gets the index of the first entry returned.
     * @return The index of the first entry returned.
     */
    public int getFirstPosition() {
        return m_firstPosition;
    }

    /**
     * Gets the result code.
     * @return The result code.
     */
    public int getResultCode() {
        return m_resultCode;
    }

    /**
     * Returns a control useful for subsequent paged results searches.
     * "this" should be a control returned on a previous paged results
     * search, so it contains information on the virtual result set
     * size.
     * @return A control useful for subsequent paged results searches.
     */
    private void parseResponse() {
        /* Suck out the data and parse it */
        ByteArrayInputStream inStream =
            new ByteArrayInputStream( getValue() );
        BERSequence ber = new BERSequence();
        JDAPBERTagDecoder decoder = new JDAPBERTagDecoder();
        int[] nRead = new int[1];
        nRead[0] = 0;
        try  {
            /* A sequence */
            BERSequence seq = (BERSequence)BERElement.getElement(
                                                      decoder, inStream,
                                                      nRead );
            /* First is firstPosition */
            m_firstPosition = ((BERInteger)seq.elementAt( 0 )).getValue();
            m_contentCount = ((BERInteger)seq.elementAt( 1 )).getValue();
            BEREnumerated enum = (BEREnumerated)seq.elementAt( 2 );
            m_resultCode = enum.getValue();
        } catch(Throwable x) {
            m_firstPosition = m_contentCount = m_resultCode = -1;
        }
    }

    /**
     * Returns a control returned on a VLV search.
     * @param controls An array of controls that may include a VLV
     * results control.
     * @return The control, if any; otherwise null.
     */
    public static LDAPVirtualListResponse parseResponse(
        LDAPControl[] controls ) {
        LDAPVirtualListResponse con = null;
        /* See if there is a VLV response control in the array */
        for( int i = 0; (controls != null) && (i < controls.length); i++ ) {
            if ( controls[i].getID().equals( VIRTUALLISTRESPONSE ) ) {
                con = new LDAPVirtualListResponse( controls[i].getValue() );
                con.parseResponse();
                break;
            }
        }
        if ( con != null ) {
            con.parseResponse();
        }
        return con;
    }

    private int m_firstPosition = 0;
    private int m_contentCount = 0;
    private int m_resultCode = -1;
}
