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

package com.netscape.sasl.mechanisms;

import java.io.*;
import com.netscape.sasl.*;

/**
 * This class provides the implementation of the EXTERNAL mechanism driver.
 * This mechanism is passed in the SASL External bind request to retrieve the
 * current result code from the server.
 */
public class SaslExternal implements SaslClient {

    /**
     * Default constructor
     */
    public SaslExternal() {
    }

    /**
     * Retrieves the initial response.
     *
     * @return The possibly null byte array containing the initial response.
     * It is null if the mechanism does not have an initial response.
     * @exception SaslException If an error occurred while creating
     * the initial response.
     */
    public byte[] createInitialResponse() throws SaslException {
        return null;
    }

    /**
     * Evaluates the challenge data and generates a response.
     *
     * @param challenge The non-null challenge sent from the server.
     *
     * @return The possibly null reponse to send to the server.
     * It is null if the challenge accompanied a "SUCCESS" status
     * and the challenge only contains data for the client to
     * update its state and no response needs to be sent to the server.
     * @exception SaslException If an error occurred while processing
     * the challenge or generating a response.
     */
    public byte[] evaluateChallenge(byte[] challenge) 
        throws SaslException {
        return null;
    }

    /**
     * Returns the name of mechanism driver.
     * @return The mechanism name.
     */
    public String getMechanismName() {
        return MECHANISM_NAME;
    }

    /**
     * The method may be called at any time to determine if the authentication
     * process is finished.
     * @return <CODE>true</CODE> if authentication is complete. For this class,
     * always returns <CODE>true</CODE>.
     */
    public boolean isComplete() {
        return true;
    }

    /**
     * Retrieves an input stream for the session. It may return
	 * the same stream that is passed in, if no processing is to be
	 * done by the client object.
     *
     * This method can only be called if isComplete() returns true.
     * @param is The original input stream for reading from the server.
     * @return An input stream for reading from the server, which
	 * may include processing the original stream. For this class, the
     * input parameter is always returned.
     * @exception IOException If the authentication exchange has not completed
     * or an error occurred while getting the stream.
     */
    public InputStream getInputStream(InputStream is)
        throws IOException {
        return is;
    }

    /**
     * Retrieves an output stream for the session. It may return
	 * the same stream that is passed in, if no processing is to be
	 * done by the client object.
     *
     * This method can only be called if isComplete() returns true.
     * @param is The original output stream for writing to the server.
     * @return An output stream for writing to the server, which
	 * may include processing the original stream. For this class, the
     * input parameter is always returned.
     * @exception IOException If the authentication exchange has not completed
     * or an error occurred while getting the stream.
     */
    public OutputStream getOutputStream(OutputStream os)
        throws IOException {
        return os;
    }

    private final static String MECHANISM_NAME = "EXTERNAL";
}
