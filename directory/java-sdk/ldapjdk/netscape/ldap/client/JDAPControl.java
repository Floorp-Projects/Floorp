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
package netscape.ldap.client;

import java.util.*;
import netscape.ldap.ber.stream.*;
import java.io.*;

/**
 * This class implements the controls for LDAPv3.
 * <pre>
 * Control ::= SEQUENCE {
 *   controlType   LDAPOID,
 *   criticality   BOOLEAN DEFAULT FALSE,
 *   controlValue  OCTET STRING OPTIONAL
 * }
 * </pre>
 *
 * @version 1.0
 * @see RFC1777
 */
public class JDAPControl {
    /**
     * Internal variables
     */
    String m_type = null;
    boolean m_criticality = false;
    byte m_vals[] = null;

    /**
     * Constructs add request.
     * @param dn distinguished name of adding entry
     * @param attrs list of attribute associated with entry
     */
    public JDAPControl(String type, boolean criticality, byte vals[]) {
        m_type = type;
        m_criticality = criticality;
        m_vals = vals;
    }

    /**
     * Constructs control from BER element.
     * @param e BER element
     */
    public JDAPControl(BERElement e) {
        BERSequence s = (BERSequence)e;
        try{
            m_type = new String(((BEROctetString)s.elementAt(0)).getValue(), "UTF8");
        } catch(Throwable x)
        {}
        Object value = s.elementAt(1);
        if (value instanceof BERBoolean)
            m_criticality = ((BERBoolean)value).getValue();
        else
            m_vals = ((BEROctetString)value).getValue();

        if (s.size() >= 3)
            m_vals = ((BEROctetString)s.elementAt(2)).getValue();
    }

    /**
     * Gets the ber representation of control.
     * @return ber representation of control
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEROctetString (m_type));
        seq.addElement(new BERBoolean (m_criticality));
        if ( (m_vals == null) || (m_vals.length < 1) )
            seq.addElement(new BEROctetString ((byte[])null));
        else {
            seq.addElement(new BEROctetString (m_vals, 0, m_vals.length));
        }
        return seq;
    }

    /**
     * Returns the identifier of the control.
     * @return Returns the type of the Control, as a string.
     */
    public String getID() {
        return m_type;
    }

    /**
     * Reports if  the control is critical.
     * @return true if the LDAP operation should be discarded if
     * the server does not support this Control.
     */
    public boolean isCritical() {
        return m_criticality;
    }

    /**
     * Returns the data of the control.
     * @return Returns the data of the Control, as a byte array.
     */
    public byte[] getValue() {
        return m_vals;
    }

    /**
     * Retrieves the string representation of extended request.
     * @return string representation of extended request
     */
    public String toString() {
        return "Control" + " { type=" + m_type + ", criticality=" + m_criticality;
    }
}
