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


/**
 * Represents the message queue associated with a particular LDAP
 * operation or operations.
 * 
 */
public class LDAPResponseListener extends LDAPMessageQueue{

    /**
     * Constructor
     * @param asynchOp A flag whether the object is used for asynchronous
     * LDAP operations
     * @see netscape.ldap.LDAPAsynchronousConnection
     */
    LDAPResponseListener(boolean asynchOp) {
        super(asynchOp);
    }
    
    /**
     * Blocks until a response is available, or until all operations
     * associated with the object have completed or been canceled, and
     * returns the response.
     *
     * @return A response for a LDAP operation or null if there is no
     * more outstanding requests 
     * @exception LDAPException Network error exception
     * @exception LDAPInterruptedException The invoking thread was interrupted
     */
    public LDAPResponse getResponse() throws LDAPException {
        return (LDAPResponse)nextMessage();
    }

    /**
     * Merge two response listeners
     * Move/append the content from another response listener to this one.
     *   
     * To be used for synchronization of asynchronous LDAP operations where
     * requests are sent by one thread but processed by another one
     * 
     * A client may be implemented in such a way that one thread makes LDAP
     * requests and calls l.getIDs(), while another thread is responsible for
     * processing of responses (call l.getResponse()). Both threads are using
     * the same listener objects. In such a case, a race
     * condition may occur, where a LDAP response message is retrieved and
     * the request terminated (request ID removed) before the first thread
     * has a chance to execute l.getIDs().
     * The proper way to handle this scenario is to create a separate listener
     * for each new request, and after l.getIDs() has been invoked, merge the
     * new request with the existing one.
     * @param listener2 The listener to be merged with.
     */
    public void merge(LDAPSearchListener listener2) {
        super.merge(listener2);
    }
    
    /**
     * Reports true if a response has been received from the server.
     *
     * @return A flag whether the response message queue is empty
     */
    public boolean isResponseReceived() {
        return super.isMessageReceived();
    }

    /**
     * Returns message ids for all outstanding requests
     * @return Message id array
     */
    public int[] getIDs() {
        return super.getIDs();
    }
}
