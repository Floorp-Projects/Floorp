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

import netscape.ldap.client.*;

/**
 * An object for taking the asynchronous LDAP responses and making them
 * appear somewhat synchronized.  Uses built-in Java monitors to block
 * until a response is received.  This is also used to communicate
 * exceptions across threads.
 *
 * This class is part of the implementation; it is of no interest to
 * developers.
 *
 * @version 1.0
 */
class LDAPResponseListener
{
    /**
     * Internal variables
     */
    private JDAPMessage response;
    private LDAPException exception;
    private boolean responseReceived;
    private boolean exceptionOccured;
    private Thread me;
    private LDAPControl[] controls;
    private LDAPConnection connection;
    private int id;

    /**
     * Constructs a basic response Listener.
     */
    public LDAPResponseListener ( LDAPConnection conn ) {
        connection = conn;
        reset ();
    }

    /**
     * Retrieves response message. This method blocks until the response
     * has been received.
     * @return jdap message
     */
    synchronized JDAPMessage getResponse () throws LDAPException {
        while (!responseReceived) {
            try {
                wait ();
            } catch (InterruptedException e) {
            }
        }

        if (exceptionOccured) throw exception;
        return response;
    }

    /**
     * Posts the LDAP server's response to the object.  This causes
     * anyone waiting on getResponse to unblock.
     * @param response response message
     */
    synchronized void setResponse (JDAPMessage response) {
        this.response = response;
        this.responseReceived = true;
        notifyAll ();
    }

    /**
     * Signals that an LDAPException occured while servicing the
     * request.  This exception will be throw to any threads waiting
     * on getResponse
     * @param e exception
     */
    synchronized void setException (LDAPException e) {
        exceptionOccured = true;
        responseReceived = true;
        exception = e;
        notifyAll ();
    }

    /**
     * Checks if response received.
     * @return true or false
     */
    boolean isResponseReceived() {
        return responseReceived;
    }

    /**
     * Resets the state of this object, so it can be recycled.
     */
    void reset () {
        responseReceived = false;
        exceptionOccured = false;
        me = null;
    }

    /**
     * Keep track of the thread which issued the request.
     *
     */
    void setThread() {
        me = Thread.currentThread();
    }

    /**
     * What thread issued this request?
     * @return The issuing thread
     */
    Thread getThread() {
        return me;
    }

    /**
     * Return owner connection
     * @return Owner connection
     */
    LDAPConnection getConnection() {
        return connection;
    }

    /**
     * Returns message id.
     * @return Message id.
     */
    int getID() {
        return id;
    }

    /**
     * Sets the message id.
     * @param id The message id.
     */
    void setID( int id ) {
        this.id = id;
    }
}
