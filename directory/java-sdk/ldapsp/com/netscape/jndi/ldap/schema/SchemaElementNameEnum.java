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
package com.netscape.jndi.ldap.schema;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;

import java.util.*;

class SchemaElementNameEnum implements NamingEnumeration {

    /**
     * Enumeration of schema object names packaged into NameClassPair.
     * The class in NameClassPair is DirContext
     */
    Enumeration m_nameEnum; 
    
    static final String _className = "javax.naming.directory.DirContext"; // for class name is bindings";

    public SchemaElementNameEnum(Enumeration nameEnum) {
        m_nameEnum = nameEnum;
    }

    public Object next() throws NamingException{
        return nextElement();
    }

    public Object nextElement() {
        String name = (String) m_nameEnum.nextElement();
        return new NameClassPair(name, _className, /*isRelative=*/true);
    }

    public boolean hasMore() throws NamingException{
        return m_nameEnum.hasMoreElements();
    }

    public boolean hasMoreElements() {
        return m_nameEnum.hasMoreElements();
    }

    public void close() {}
}
