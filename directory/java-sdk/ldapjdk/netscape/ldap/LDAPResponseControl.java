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
package netscape.ldap;

/*
 * This object represents the value of the LDAPConnection.m_responseControlTable hashtable.
 * It stores the response controls and its corresponding LDAPConnection and
 * the message ID for its corresponding LDAPMessage.
 */
class LDAPResponseControl {
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