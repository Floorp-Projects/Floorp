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
package org.ietf.ldap;

/*
 * This object represents the value of the LDAPConnection.m_responseControlTable hashtable.
 * It stores the response controls and its corresponding LDAPConnection and
 * the message ID for its corresponding LDAPMessage.
 */
class LDAPResponseControl implements java.io.Serializable {
    static final long serialVersionUID = 389472019686058593L;
    private LDAPConnection m_connection;
    private int m_messageID;
    private LDAPControl[] m_controls;

    public LDAPResponseControl(LDAPConnection conn, int msgID, 
      LDAPControl[] controls) {

        m_connection = conn;
        m_messageID = msgID;
        m_controls = new LDAPControl[controls.length];
        for (int i=0; i<controls.length; i++) 
            m_controls[i] = controls[i];
    }

    public int getMsgID() {
        return m_messageID;
    }

    public LDAPControl[] getControls() {
        return m_controls;
    }

    public LDAPConnection getConnection() {
        return m_connection;
    }
}
