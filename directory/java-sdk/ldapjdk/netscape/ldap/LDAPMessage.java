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

import java.util.*;
import netscape.ldap.client.opers.*;
import netscape.ldap.ber.stream.*;
import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;


/**
 * Base class for LDAP request and response messages.
 * This class represents the LDAPMessage in RFC2251. The
 * message is the entity that got transferred back and
 * fro between the server and the client interface. Each
 * message has a protocol operation. The protocol operation
 * indicates if it is a request or response.
 * <pre>
 * LDAPMessage ::= SEQUENCE {
 *   messageID MessageID,
 *   protocolOp CHOICE {
 *     bindRequest BindRequest,
 *     ...
 *   }
 *   controls [0] Controls OPTIONAL
 * }
 * </pre>
 *
 * @version 1.0
 */
public class LDAPMessage {

    public final static int BIND_REQUEST        = 0;
    public final static int BIND_RESPONSE       = 1;
    public final static int UNBIND_REQUEST      = 2;
    public final static int SEARCH_REQUEST      = 3;
    public final static int SEARCH_RESPONSE     = 4;
    public final static int SEARCH_RESULT       = 5;
    public final static int MODIFY_REQUEST      = 6;
    public final static int MODIFY_RESPONSE     = 7;
    public final static int ADD_REQUEST         = 8;
    public final static int ADD_RESPONSE        = 9;
    public final static int DEL_REQUEST         = 10;
    public final static int DEL_RESPONSE        = 11;
    public final static int MODIFY_RDN_REQUEST  = 12;
    public final static int MODIFY_RDN_RESPONSE = 13;
    public final static int COMPARE_REQUEST     = 14;
    public final static int COMPARE_RESPONSE    = 15;
    public final static int ABANDON_REQUEST     = 16;
    public final static int SEARCH_RESULT_REFERENCE = 19;
    public final static int EXTENDED_REQUEST    = 23;
    public final static int EXTENDED_RESPONSE   = 24;
        
    /**
     * Internal variables
     */
    private int m_msgid;
    private JDAPProtocolOp m_protocolOp = null;
    private LDAPControl m_controls[] = null;

    // Time Stemp format Hour(0-23):Minute:Second.Milliseconds used for trace msgs
    static SimpleDateFormat m_timeFormat = new SimpleDateFormat("HH:mm:ss.SSS");
    
    /**
     * Constructs a ldap message.
     * @param msgid message identifier
     * @param op operation protocol
     */
    LDAPMessage(int msgid, JDAPProtocolOp op) {
        m_msgid = msgid;
        m_protocolOp = op;
    }

    LDAPMessage(int msgid, JDAPProtocolOp op, LDAPControl controls[]) {
        m_msgid = msgid;
        m_protocolOp = op;
        m_controls = controls; /* LDAPv3 additions */
    }

    /**
     * Creates a ldap message from a BERElement. This method is used
     * to parse LDAP response messages
     *
     * @param element ber element constructed from incoming byte stream
     */
    static LDAPMessage parseMessage(BERElement element) throws IOException {
        int l_msgid;
        JDAPProtocolOp l_protocolOp = null;
        LDAPControl l_controls[] = null;
        
        if (element.getType() != BERElement.SEQUENCE)
            throw new IOException("SEQUENCE in jdap message expected");
        BERSequence seq = (BERSequence)element;
        BERInteger msgid = (BERInteger)seq.elementAt(0);
        l_msgid = msgid.getValue();
        BERElement protocolOp = (BERElement)seq.elementAt(1);
        if (protocolOp.getType() != BERElement.TAG) {
            throw new IOException("TAG in protocol operation is expected");
        }
        BERTag tag = (BERTag)protocolOp;
        switch (tag.getTag()&0x1f) {
            case JDAPProtocolOp.BIND_RESPONSE:
                l_protocolOp = new JDAPBindResponse(protocolOp);
  	        break;
            case JDAPProtocolOp.SEARCH_RESPONSE:
                l_protocolOp = new JDAPSearchResponse(protocolOp);
  	        break;
            /*
             * If doing search without bind,
             * x500.arc.nasa.gov returns tag SEARCH_REQUEST tag
             * in SEARCH_RESULT.
             */
            case JDAPProtocolOp.SEARCH_REQUEST:
            case JDAPProtocolOp.SEARCH_RESULT:
                l_protocolOp = new JDAPSearchResult(protocolOp);
  	        break;
            case JDAPProtocolOp.MODIFY_RESPONSE:
                l_protocolOp = new JDAPModifyResponse(protocolOp);
  	        break;
            case JDAPProtocolOp.ADD_RESPONSE:
                l_protocolOp = new JDAPAddResponse(protocolOp);
      	    break;
            case JDAPProtocolOp.DEL_RESPONSE:
                l_protocolOp = new JDAPDeleteResponse(protocolOp);
          	break;
            case JDAPProtocolOp.MODIFY_RDN_RESPONSE:
                l_protocolOp = new JDAPModifyRDNResponse(protocolOp);
          	break;
            case JDAPProtocolOp.COMPARE_RESPONSE:
                l_protocolOp = new JDAPCompareResponse(protocolOp);
          	break;
            case JDAPProtocolOp.SEARCH_RESULT_REFERENCE:
                l_protocolOp = new JDAPSearchResultReference(protocolOp);
          	break;
            case JDAPProtocolOp.EXTENDED_RESPONSE:
                l_protocolOp = new JDAPExtendedResponse(protocolOp);
          	break;
            default:
                throw new IOException("Unknown protocol operation");
        }

        /* parse control */
        if (seq.size() >= 3) {
            tag = (BERTag)seq.elementAt(2);
            if ( tag.getTag() == (BERTag.CONSTRUCTED|BERTag.CONTEXT|0) ) {
                BERSequence controls = (BERSequence)tag.getValue();
                l_controls = new LDAPControl[controls.size()];
                for (int i = 0; i < controls.size(); i++) {
                    l_controls[i] = LDAPControl.parseControl(controls.elementAt(i));
    		    }
            }
        }
        
        if (l_protocolOp instanceof JDAPSearchResponse) {
            return new LDAPSearchResult(l_msgid,
                (JDAPSearchResponse) l_protocolOp, l_controls);
        }            
        else if (l_protocolOp instanceof JDAPSearchResultReference) {
            return new LDAPSearchResultReference(l_msgid,
                (JDAPSearchResultReference) l_protocolOp, l_controls);
        }            
        else if (l_protocolOp instanceof JDAPExtendedResponse) {
            return new LDAPExtendedResponse(l_msgid,
                (JDAPExtendedResponse) l_protocolOp, l_controls);
        }
        else {
            return new LDAPResponse(l_msgid, l_protocolOp, l_controls);
        }            
    }

    /**
     * Returns the message identifer.
     * @return message identifer
     */
    public int getID(){
        return m_msgid;
    }

    /**
     * Returns the LDAP operation type of the message
     * @return message type
     */
    public int getType(){
        return m_protocolOp.getType();
    }

    /**
     * Retrieves the protocol operation.
     * @return protocol operation
     */
    JDAPProtocolOp getProtocolOp() {
        return m_protocolOp;
    }

    /**
     * Retrieves list of controls.
     * @return controls
     */
    public LDAPControl[] getControls() {
        return m_controls;
    }

    /**
     * Writes the ber encoding to stream.
     * @param s output stream
     */
    void write(OutputStream s) throws IOException {
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
        StringBuffer sb = new StringBuffer("[LDAPMessage] ");
        sb.append(m_msgid);
        sb.append(" ");
        sb.append(m_protocolOp.toString());

        for (int i =0; m_controls != null && i < m_controls.length; i++) {
            sb.append(" ");
            sb.append(m_controls[i].toString());
        }
        return sb.toString();
    }
    
    /**
     * Returns string representation of a ldap message with
     * the time stamp. Used for message trace
     * @return ldap message with the time stamp
     */
    String toTraceString() {
        String timeStamp = m_timeFormat.format(new Date());
        StringBuffer sb = new StringBuffer(timeStamp);
        sb.append(" ");
        sb.append(m_msgid);
        sb.append(" ");
        sb.append(m_protocolOp.toString());

        for (int i =0; m_controls != null && i < m_controls.length; i++) {
            sb.append(" ");
            sb.append(m_controls[i].toString());
        }
        return sb.toString();
    }
}
