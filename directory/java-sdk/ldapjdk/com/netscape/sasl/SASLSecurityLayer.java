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
package com.netscape.sasl;

import com.netscape.sasl.SASLException;

/**
 * An object of this type translates buffers back and forth during a
 * session, after the authentication process has completed.
 */
public interface SASLSecurityLayer {
    /**
     * Take a protocol-dependent byte array and encode it (encrypt, for
     * example) for sending to the server.
     * @param vals Byte array to be encoded.
     * @return Encoded byte array.
     * @exception SASLException if an encoded array could not be returned,
     * e.g. because of insufficient memory.
     */
    public byte[] encode( byte[] vals ) throws SASLException;
    /**
     * Take an encoded byte array received from the server and decode it.
     * @param vals Encoded byte array.
     * @return Decoded byte array.
     * @exception SASLException if a decoded array could not be returned,
     * e.g. because of insufficient memory.
     */
    public byte[] decode( byte[] vals ) throws SASLException;
}
