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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.io.Serializable;
import java.util.Vector;

/**
 * Represents the message queue associated with a particular LDAP
 * operation or operations.
 * 
 */
public class LDAPResponseQueue extends LDAPMessageQueueImpl {

    static final long serialVersionUID = 901897097111294329L;

    /**
     * Constructor
     *
     * @param asynchOp a boolean flag that is true if the object is used for 
     * asynchronous LDAP operations
     * @see org.ietf.ldap.LDAPAsynchronousConnection
     */
    LDAPResponseQueue( boolean asynchOp ) {
        super( asynchOp );
    }
}
