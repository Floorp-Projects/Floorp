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

import javax.naming.ldap.*;
import javax.naming.directory.*;

/**
 * An extension of SearchResult that allows access to controls sent
 * back with the results of a search
 */
class SearchResultWithControls extends SearchResult implements HasControls {

    Control[] m_ctrls;

    /**
     * Enable constructors
     */
    public SearchResultWithControls(String name, Object obj, Attributes attrs) {
        super(name, obj, attrs);
    }

    public SearchResultWithControls(String name, Object obj, Attributes attrs, boolean isRelative) {
        super(name, obj, attrs, isRelative);
    }

    public SearchResultWithControls(String name, String className, Object obj, Attributes attrs) {
        super(name, className, obj, attrs);
    }

    public SearchResultWithControls(String name, String className, Object obj, Attributes attrs, boolean isRelative) {
        super(name, className, obj, attrs, isRelative);
    }

    /**
     * Implements HasControls interface
     */
    public Control[] getControls() {
        return m_ctrls;
    }

    /**
     * Set controls array
     */
    public void setControls(Control[] ctrls) {
        m_ctrls = ctrls;
    }
}
