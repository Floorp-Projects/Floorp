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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

/**
 * The <CODE>LDAPTraceWriter</CODE> interface enables logging of LDAP
 * trace messages in environments where an OutputStream can not be used.
 * <P>
 * The interface is primarily meant for integrating LDAP tracing with the
 * servlet log facility:
 * <P>
 * <PRE>
 *     servletCtx = config.getServletContext();
 *     ...
 *     LDAPConnection ld = new LDAPConnection();
 *     ld.setProperty(ld.TRACE_PROPERTY, 
 *            new LDAPTraceWriter() { 
 *                public void write (String msg) {
 *                    servletCtx.log(msg);
 *                }
 *            });
 * <PRE>
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPConnection#setProperty(java.lang.String, java.lang.Object)
 */
public interface LDAPTraceWriter {

     /**
     * Writes an LDAP trace message.
     * 
     * @param msg An incoming or outgoing LDAP message
     *
     * @version 1.0
     * @see org.ietf.ldap.LDAPConnection#setProperty(java.lang.String, java.lang.Object)
     */
    public void write (String msg);

}
