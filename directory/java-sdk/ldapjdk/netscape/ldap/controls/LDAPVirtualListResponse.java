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
package netscape.ldap.controls;

import java.io.*;
import netscape.ldap.client.JDAPBERTagDecoder;
import netscape.ldap.LDAPControl;
import netscape.ldap.ber.stream.*;
import netscape.ldap.LDAPException;

/**
 * Represents control data for returning paged results from a search.
 *
 * @version 1.0
 *
 *<PRE>
 *   VirtualListViewResponse ::= SEQUENCE {
 *       targetPosition   INTEGER (0 .. maxInt),
 *       contentCount     INTEGER (0 .. maxInt),
 *       virtualListViewResult ENUMERATED {
 *           success                  (0),
 *           operatonsError           (1),
 *           timeLimitExceeded        (3),
 *           adminLimitExceeded       (11),
 *           insufficientAccessRights (50),
 *           busy                     (51),
 *           unwillingToPerform       (53),
 *           sortControlMissing       (60),
 *           offsetRangeError         (61),
 *           other                    (80)
 *       },
 *       contextID     OCTET STRING OPTIONAL 
 *  }
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
     * Contructs an <CODE>LDAPVirtualListResponse</CODE> object.
     * @param oid This parameter must be equal to
     * <CODE>LDAPVirtualListResponse.VIRTUALLISTRESPONSE</CODE> or a 
     * <CODE>LDAPException</CODE>is thrown.
     * @param critical True if this control is critical.
     * @param value The value associated with this control.
     * @exception netscape.ldap.LDAPException If oid is not 
     * <CODE>LDAPVirtualListResponse.VIRTUALLISTRESPONSE</CODE>.
     * @see netscape.ldap.LDAPControl#register
     */ 
    public LDAPVirtualListResponse( String oid, boolean critical, 
                                    byte[] value ) throws LDAPException {
        super( VIRTUALLISTRESPONSE, critical, value );
        if ( !oid.equals( VIRTUALLISTRESPONSE ) ) {
             throw new LDAPException( "oid must be LDAPVirtualListResponse." +
                                      "VIRTUALLISTRESPONSE", 
                                      LDAPException.PARAM_ERROR);
        }
        
	parseResponse();
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
     * Gets the context cookie, if any.
     * @return The result context cookie.
     */
    public String getContext() {
        return m_context;
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
            m_resultCode = ((BEREnumerated)seq.elementAt( 2 )).getValue();
            if( seq.size() > 3 ) {
                BEROctetString str = (BEROctetString)seq.elementAt( 3 );
                m_context = new String(str.getValue(), "UTF8");
            }
        } catch(Exception x) {
            m_firstPosition = m_contentCount = m_resultCode = -1;
            m_context = null;        }
    }

    /**
     * Returns a control returned on a VLV search.
     * @param controls An array of controls that may include a VLV
     * results control.
     * @return The control, if any; otherwise null.
     * @deprecated LDAPVirtualListResponse controls are now automatically 
     * instantiated.
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

    public String toString() {
         StringBuffer sb = new StringBuffer("{VirtListResponseCtrl:");
        
        sb.append(" isCritical=");
        sb.append(isCritical());
        
        sb.append(" firstPosition=");
        sb.append(m_firstPosition);
        
        sb.append(" contentCount=");
        sb.append(m_contentCount);

        sb.append(" resultCode=");
        sb.append(m_resultCode);
        
        if (m_context != null) {
            sb.append(" conext=");
            sb.append(m_context);
        }

        sb.append("}");

        return sb.toString();
    }

    
    private int m_firstPosition = 0;
    private int m_contentCount = 0;
    private int m_resultCode = -1;
    private String m_context = null;

}
