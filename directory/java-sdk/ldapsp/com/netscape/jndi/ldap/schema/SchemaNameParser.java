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

import java.util.Hashtable;
import java.util.Properties;
import javax.naming.*;

class SchemaNameParser implements NameParser {

    private static SchemaNameParser m_parser;
    
    // A table with compound name syntax properties
    static Properties nameSyntax;
    static {
        nameSyntax = new Properties();
        nameSyntax.put("jndi.syntax.direction", "left_to_right");
        nameSyntax.put("jndi.syntax.separator", "/");
        nameSyntax.put("jndi.syntax.ignorecase", "true");
    }

    // Can not be constructed
    private SchemaNameParser() {}
    
    // Shared instance must be used
    public static SchemaNameParser getParser() {
        if (m_parser == null) {
            m_parser = new SchemaNameParser();
        }
        return m_parser;
    }
    
    // implements parse
    public Name parse(String name) throws NamingException {
        return new CompoundName(name, nameSyntax);
    }
    
}
