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
package org.ietf.ldap.util;

/** This abstract class is the parent of those classes which implement
    MIME encoding and decoding: base64.
    @see org.ietf.ldap.util.MimeBase64Encoder
    @see org.ietf.ldap.util.MimeBase64Decoder
 */

public abstract class MimeEncoder implements java.io.Serializable {

    static final long serialVersionUID = 5179250095383961512L;

    /** Given a sequence of input bytes, produces a sequence of output bytes.
        Note that some (small) amount of buffering may be necessary, if the
        input byte stream didn't fall on an appropriate boundary.  If there
        are bytes in `out' already, the new bytes are appended, so the
        caller should do `out.setLength(0)' first if that's desired.
     */
    abstract public void translate(ByteBuf in, ByteBuf out);

    /** Tell the decoder that no more input data will be forthcoming.
        This may result in output, as a result of flushing the internal
        buffer.  This object must not be used again after calling eof().
        If there are bytes in `out' already, the new bytes are appended,
        so the caller should do `out.setLength(0)' first if that's desired.
     */
    abstract public void eof(ByteBuf out);
}
