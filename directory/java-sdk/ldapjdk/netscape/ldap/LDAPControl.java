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
package netscape.ldap;

import java.io.*;
import netscape.ldap.ber.stream.*;

/**
 * Represents arbitrary control data that can be used with a
 * a particular LDAP operation.  LDAP controls are part of version 3
 * of the LDAP protocol.
 * <P>
 *
 * LDAP controls allow you to extend the functionality of
 * an LDAP operation.  For example, you can use an LDAP control
 * for the search operation to sort search results on an LDAP server.
 * <P>
 *
 * An LDAP control can be either a <B>server control</B> or
 * a <B>client control</B>:
 * <P>
 * <UL>
 * <LI><B>Server controls</B> can be sent to the LDAP server or returned
 * by the server on any operation.
 * <LI><B>Client controls</B> are intended to affect only the client side
 * of the operation.
 * </UL>
 * <P>
 *
 * An LDAP control consists of the following information:
 * <P>
 * <UL>
 * <LI>A unique object ID (OID) that identifies the control.<P>
 * <LI>A &quot;criticality&quot; field, which indicates whether or
 * not the control is critical to the operation. (If the control is
 * critical to the operation and the server does not support the control,
 * the server should not execute the operation.)<P>
 * <LI>Data pertaining to the control.<P>
 * </UL>
 * <P>
 *
 * To determine which server controls are supported by a particular server,
 * you need to search for the root DSE (DSA-specific entry, where DSA is
 * another term for &quot;LDAP server&quot;) and find the values of the
 * <CODE>supportedControl</CODE> attribute.  This attribute contains the
 * object IDs (OIDs) of the controls supported by this server.
 * <P>
 *
 * The following section of code demonstrates how to get the list
 * of the server controls supported by an LDAP server.
 * <P>
 *
 * <PRE>
 * public static void main( String[] args )
 * {
 *   LDAPConnection ld = new LDAPConnection();
 *   try {
 *     String MY_HOST = "localhost";
 *     int MY_PORT = 389;
 *     ld.connect( MY_HOST, MY_PORT );
 *     try {
 *       ld.authenticate( 3, "cn=Directory Manager", "23skidoo" );
 *     } catch( LDAPException e ) {
 *       System.out.println( "LDAP server does not support v3." );
 *       ld.disconnect();
 *       System.exit(1);
 *     }
 *
 *     String MY_FILT = "(objectclass=*)";
 *     String MY_BASE = "";
 *     String getAttrs[] = { "supportedControl" };
 *     LDAPSearchResults res = ld.search( MY_BASE,
 *       LDAPConnection.SCOPE_BASE, MY_FILT, getAttrs, false );
 *
 *     while ( res.hasMoreElements() ) {
 *       LDAPEntry findEntry = (LDAPEntry)res.nextElement();
 *       LDAPAttributeSet findAttrs = findEntry.getAttributeSet();
 *       Enumeration enumAttrs = findAttrs.getAttributes();
 *
 *         while ( enumAttrs.hasMoreElements() ) {
 *           LDAPAttribute anAttr = (LDAPAttribute)enumAttrs.nextElement();
 *           String attrName = anAttr.getName();
 *           System.out.println( attrName );
 *           Enumeration enumVals = anAttr.getStringValues();
 *
 *           while ( enumVals.hasMoreElements() ) {
 *             String aVal = ( String )enumVals.nextElement();
 *             System.out.println( "\t" + aVal );
 *           }
 *         }
 *      }
 *   }
 *   catch( LDAPException e ) {
 *     System.out.println( "Error: " + e.toString() );
 *   }
 *   try {
 *     ld.disconnect();
 *   }
 *   catch( LDAPException e ) {
 *     System.exit(1);
 *   }
 *   System.exit(0);
 * }
 * </PRE>
 * <P>
 *
 * If you compile and run this example against an LDAP server that
 * supports v3 of the protocol, you might receive the following results:
 * <P>
 *
 * <PRE>
 * supportedcontrol
 *   2.16.840.1.113730.3.4.2
 *   2.16.840.1.113730.3.4.3
 *   2.16.840.1.113730.3.4.4
 *   2.16.840.1.113730.3.4.5
 *   1.2.840.113556.1.4.473
 * </PRE>
 * <P>
 *
 * For more information on LDAP controls, see the Internet-Draft on
 * the LDAP v3 protocol. (Note that this internet draft is still a
 * work in progress.  You can find the latest draft at the <A
 * HREF="http://www.ietf.cnri.reston.va.us/html.charters/asid-charter.html"
 * TARGET="_blank">ASID home page</A>.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPv3#CLIENTCONTROLS
 * @see netscape.ldap.LDAPv3#SERVERCONTROLS
 * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
 * @see netscape.ldap.LDAPConnection#getOption
 * @see netscape.ldap.LDAPConnection#setOption
 * @see netscape.ldap.LDAPConnection#getResponseControls
 * @see netscape.ldap.LDAPSearchConstraints#getClientControls
 * @see netscape.ldap.LDAPSearchConstraints#getServerControls
 * @see netscape.ldap.LDAPSearchConstraints#setClientControls
 * @see netscape.ldap.LDAPSearchConstraints#setServerControls
 */
public class LDAPControl implements Cloneable {
    public final static String MANAGEDSAIT       = "2.16.840.1.113730.3.4.2";
    /* Password information sent back to client */
    public final static String PWEXPIRED         = "2.16.840.1.113730.3.4.4";
    public final static String PWEXPIRING        = "2.16.840.1.113730.3.4.5";

    /**
     * Default constructor for the <CODE>LDAPControl</CODE> class.
     */
    public LDAPControl()
    {
    }

    /**
     * Constructs a new <CODE>LDAPControl</CODE> object using the
     * specified object ID (OID), &quot;criticality&quot; field, and
     * data to be used by the control.
     * <P>
     *
     * @param id The object ID (OID) identifying the control.
     * @param critical <CODE>true</CODE> if the LDAP operation should be
     * cancelled when the server does not support this control (in other
     * words, this control is critical to the LDAP operation).
     * @param vals Control-specific data.
     * @see netscape.ldap.LDAPSearchConstraints#setClientControls
     * @see netscape.ldap.LDAPSearchConstraints#setServerControls
     */
    public LDAPControl(String id,
             boolean critical,
             byte vals[]) {
        m_oid = id;
        m_critical = critical;
        m_value = vals;
    }

    /**
     * Gets the object ID (OID) of the control.
     * @return Object ID (OID) of the control.
     */
    public String getID() {
        return m_oid;
    }

    /**
     * Specifies whether or not the control is critical to the LDAP operation.
     * @return <CODE>true</CODE> if the LDAP operation should be cancelled when
     * the server does not support this control.
     */
    public boolean isCritical() {
        return m_critical;
    }

    /**
     * Gets the data in the control.
     * @return Returns the data in the control as a byte array.
     */
    public byte[] getValue() {
        return m_value;
    }

    /**
     * Creates a copy of the control.
     * @return Copy of the control.
     */
    public Object clone() {
        byte[] vals = null;
        if ( m_value != null ) {
            vals = new byte[m_value.length];
            for( int i = 0; i < m_value.length; i++ )
                vals[i] = m_value[i];
        }
        LDAPControl control = new LDAPControl( m_oid, m_critical, vals );
        return control;
    }

    /**
     * Create a "flattened" BER encoding from a BER,
     * and return it as a byte array.
     * @param ber A BER encoded sequence.
     * @return The byte array of encoded data.
     */
    protected byte[] flattenBER( BERSequence ber ) {
        /* Suck out the data and return it */
        ByteArrayOutputStream outStream = new ByteArrayOutputStream();
        try {
            ber.write( outStream );
        } catch ( IOException e ) {
            return null;
        }
        return outStream.toByteArray();
    }

    private String m_oid;
    protected boolean m_critical = false;
    protected byte[] m_value = null;
}

