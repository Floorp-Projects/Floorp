/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
