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
package com.netscape.jndi.ldap.common;


/**
 * Class used to selectivly enable debug statements
 */
public class Debug {
    
    // lower number is a higher priority message. Level 0 is the
    // highest priority to be used ONLY for errors
    private static int m_level = 0;
    
    static {
        try {
            String level = System.getProperty("jndi.netscape.debug");
            if (level != null) {
                m_level = Integer.parseInt(level);
            }
        }
        catch (Exception e) {}
    }    
    
    
    /**
     * Set the debug level. To disable debugging set the level to -1
     */
    public static void setDebugLevel(int level) {
        m_level = level;
    }
    
    /**
     * Get the debug level. If -1 is returned, then debugging is disabled
     */
    public static int getDebugLevel() {
        return m_level;
    }
    
    /**
     * Print the message if its debug level is enabled
     */
    public static void println(int level, String msg) {
        if (m_level >= 0 && level <= m_level) {
            System.err.println(msg);
        }
    }
}
