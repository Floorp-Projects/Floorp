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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.event.*;
import javax.naming.spi.*;
import java.util.*;

public class LdapContextFactory implements InitialContextFactory {

    public Context getInitialContext(Hashtable env) throws NamingException {
        Hashtable ctxEnv = (Hashtable)env.clone();

        // Read system properties as well. Add a system property to the
        // env if it's name start with "java.naming." and it is not already
        // present in the env (env has precedence over the System properties)        
        for (Enumeration e = System.getProperties().keys(); e.hasMoreElements();) {
            String key = (String) e.nextElement();
            if (key.startsWith("java.naming.")) {
                if (ctxEnv.get(key) == null) {
                    ctxEnv.put(key,System.getProperty(key));
                }
            }
        }

        EventDirContext ctx = new LdapContextImpl(ctxEnv);
        return ctx;
    }
}

