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
package com.netscape.jndi.ldap.common;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;
import javax.naming.event.*;
import java.util.Hashtable;

public class LdapContextAdapter extends DirContextAdapter implements EventDirContext, LdapContext {

/* LdapContext methods */
    public ExtendedResponse extendedOperation(ExtendedRequest req)throws NamingException {
        throw new OperationNotSupportedException();
    }

    public Control[] getRequestControls() throws NamingException {
        throw new OperationNotSupportedException();
    }

    public Control[] getResponseControls() throws NamingException {
        throw new OperationNotSupportedException();
    }

    public Control[] getConnectControls() throws NamingException {
        throw new OperationNotSupportedException();
    }

    public LdapContext newInstance(Control[] reqCtls) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void reconnect(Control[] reqCtls) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void setRequestControls(Control[] reqCtls) throws NamingException {
        throw new OperationNotSupportedException();
    }
    /**
     * Naming Event methods
     */
    public void addNamingListener(String target, int scope, NamingListener l) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void addNamingListener(Name target, int scope, NamingListener l) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void addNamingListener(String target, String filter, SearchControls ctls, NamingListener l)throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void addNamingListener(Name target, String filter, SearchControls ctls, NamingListener l)throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void addNamingListener(String target, String filter, Object[] filtrArgs, SearchControls ctls, NamingListener l)throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void addNamingListener(Name target, String filter, Object[] filtrArgs, SearchControls ctls, NamingListener l)throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void removeNamingListener(NamingListener l) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public boolean targetMustExist() {
        return true;
    }
}
