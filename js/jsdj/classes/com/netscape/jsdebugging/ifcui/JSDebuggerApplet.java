/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
* IFC FoundationApplet startup support class
*/

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import netscape.security.PrivilegeManager;

public class JSDebuggerApplet extends FoundationApplet {
    /** This method must be implemented by the applet developer because
      * there is no way in the standard Java API for system classes (such as,
      * netscape.application) to look up an applet's class by name. The
      * static method <b>Class.forName()</b> simply looks up one level in the
      * stack and gets the ClassLoader associated with the method block of the
      * caller.
      * <p>
      * When the netscape.application classes are installed as
      * system classes, the ClassLoader is <b>null</b>. Thus, when code in
      * netscape.application calls <b>Class.forName()</b> it can only find
      * other system classes.
      * <p>
      * The solution is an API that allows code to
      * find the ClassLoader for an applet by URL, and a public API on
      * ClassLoader to ask it to load classes by name. Until those
      * enhancements can be made and distributed to all the world's Java
      * systems, applets must subclass FoundationApplet and
      * implement the following one-line method:
      * <pre>
      *     public abstract Class classForName(String className)
      *         throws ClassNotFoundException {
      *         return Class.forName(className);
      *     }
      * </pre>
      */
    public Class classForName(String className)
        throws ClassNotFoundException {
        return Class.forName(className);
    }

    public void run() {
        try {
//
// At one time I needed to have this priv. early on in the loading.
//
// I believe it was an issue when I was explicitly loading the native
// code from the debugger.
//
// This is no longer the case
//
//            System.out.println( "first priv req start" );
//            PrivilegeManager.enablePrivilege("Debugger");
//            System.out.println( "first priv req end (success)" );
            super.run();
    	} catch (Exception e) {
            System.out.println("failed to enable Priv in JSDebuggerApplet.run");
            e.printStackTrace(System.out);
    	}
    }
}
