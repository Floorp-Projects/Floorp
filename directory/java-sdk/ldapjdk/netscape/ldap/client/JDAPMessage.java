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
import netscape.ldap.client.opers.*;
import netscape.ldap.ber.stream.*;
import java.io.*;
import java.net.*;

/**
 * This class represents the LDAPMessage in RFC1777. The
 * message is the entity that got transferred back and
 * fro between the server and the client interface. Each
 * message has a protocol operation. The protocol operation
 * indicates if it is a request or response.
 * <pre>
 * LDAPMessage ::= SEQUECE {
 *   messageID MessageID,
 *   protocolOp CHOICE {
 *     bindRequest BindRequest,
 *     ...
 *   }
 * }
 * </pre>
 *
 * @version 1.0
 */
public class JDAPMessage {
    /**
     * Internal variables
     */
    protected int m_msgid;
    protected JDAPProtocolOp m_protocolOp = null;
    protected JDAPControl m_controls[] = null;

    /**
     * Constructs a ldap message.
     * @param msgid message identifier
     * @param op operation protocol
     */
    public JDAPMessage(int msgid, JDAPProtocolOp op) {
        m_msgid = msgid;
        m_protocolOp = op;
    }

    public JDAPMessage(int msgid, JDAPProtocolOp op, JDAPControl controls[]) {
        m_msgid = msgid;
        m_protocolOp = op;
        m_controls = controls; /* LDAPv3 additions */
    }

    /**
     * Constructs a ldap message with BERElement. This constructor
     * is used by JDAPClient to build JDAPMessage from byte
     * stream.
     * @param element ber element constructed from incoming byte stream
     */
    public JDAPMessage(BERElement element) throws IOException {
        if (element.getType() != BERElement.SEQUENCE)
            throw new IOException("SEQUENCE in jdap message expected");
        BERSequence seq = (BERSequence)element;
        BERInteger msgid = (BERInteger)seq.elementAt(0);
        m_msgid = msgid.getValue();
        BERElement protocolOp = (BERElement)seq.elementAt(1);
        if (protocolOp.getType() != BERElement.TAG) {
            throw new IOException("TAG in protocol operation is expected");
        }
        BERTag tag = (BERTag)protocolOp;
        switch (tag.getTag()&0x1f) {
            case JDAPProtocolOp.BIND_RESPONSE:
                m_protocolOp = new JDAPBindResponse(protocolOp);
  	        break;
            case JDAPProtocolOp.SEARCH_RESPONSE:
                m_protocolOp = new JDAPSearchResponse(protocolOp);
  	        break;
            /*
             * If doing search without bind,
             * x500.arc.nasa.gov returns tag SEARCH_REQUEST tag
             * in SEARCH_RESULT.
             */
            case JDAPProtocolOp.SEARCH_REQUEST:
            case JDAPProtocolOp.SEARCH_RESULT:
                m_protocolOp = new JDAPSearchResult(protocolOp);
  	        break;
            case JDAPProtocolOp.MODIFY_RESPONSE:
                m_protocolOp = new JDAPModifyResponse(protocolOp);
  	        break;
            case JDAPProtocolOp.ADD_RESPONSE:
                m_protocolOp = new JDAPAddResponse(protocolOp);
      	    break;
            case JDAPProtocolOp.DEL_RESPONSE:
                m_protocolOp = new JDAPDeleteResponse(protocolOp);
          	break;
            case JDAPProtocolOp.MODIFY_RDN_RESPONSE:
                m_protocolOp = new JDAPModifyRDNResponse(protocolOp);
          	break;
            case JDAPProtocolOp.COMPARE_RESPONSE:
                m_protocolOp = new JDAPCompareResponse(protocolOp);
          	break;
            case JDAPProtocolOp.SEARCH_RESULT_REFERENCE:
                m_protocolOp = new JDAPSearchResultReference(protocolOp);
          	break;
            case JDAPProtocolOp.EXTENDED_RESPONSE:
                m_protocolOp = new JDAPExtendedResponse(protocolOp);
          	break;
            default:
                throw new IOException("Unknown rotocol operation");
        }

        /* parse control */
        if (seq.size() >= 3) {
            tag = (BERTag)seq.elementAt(2);
            if ( tag.getTag() == (BERTag.CONSTRUCTED|BERTag.CONTEXT|0) ) {
                BERSequence controls = (BERSequence)tag.getValue();
                m_controls = new JDAPControl[controls.size()];
                for (int i = 0; i < controls.size(); i++) {
                    m_controls[i] = new JDAPControl(controls.elementAt(i));
    		        }
            }
        }
    }

    /**
     * Retrieves the message identifer.
     * @return message identifer
     */
    public int getId(){
        return m_msgid;
    }

    /**
     * Retrieves the protocol operation.
     * @return protocol operation
     */
    public JDAPProtocolOp getProtocolOp() {
        return m_protocolOp;
    }

    /**
     * Retrieves list of controls.
     * @return controls
     */
    public JDAPControl[] getControls() {
        return m_controls;
    }

    /**
     * Writes the ber encoding to stream.
     * @param s output stream
     */
    public void write(OutputStream s) throws IOException {
        BERSequence seq = new BERSequence();
        BERInteger i = new BERInteger(m_msgid);
        seq.addElement(i);
        BERElement e = m_protocolOp.getBERElement();
        if (e == null) {
            throw new IOException("Bad BER element");
        }
        seq.addElement(e);
        if (m_controls != null) { /* LDAPv3 additions */
            BERSequence c = new BERSequence();
            for (int j = 0; j < m_controls.length; j++) {
                c.addElement(m_controls[j].getBERElement());
            }
            BERTag t = new BERTag(BERTag.CONTEXT|BERTag.CONSTRUCTED|0, c, true);
            seq.addElement(t);
        }
        seq.write(s);
    }

    /**
     * Returns string representation of a ldap message.
     * @return ldap message
     */
    public String toString() {
        return "[JDAPMessage] " + m_msgid + " " + m_protocolOp.toString();
    }
}
