/* -*- Mode: Java; tab-width: 8; c-basic-offset: 4 -*-
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
/* ** */

package netscape.javascript;
import java.io.*;

public class JSUtil {

    /* Return the stack trace of an exception or error as a String */
    public static String getStackTrace(Throwable t) {
	ByteArrayOutputStream captureStream;
	PrintWriter p;
	
	captureStream = new ByteArrayOutputStream();
	p = new PrintWriter(captureStream);

	t.printStackTrace(p);
	p.flush();

	return captureStream.toString();
    }

    /**
     * This method is used to work around a bug in AIX JDK1.1.6, in which
     * static initializers are not run when a static field is referenced from
     * native code.  The problem does not manifest itself if the field is
     * accessed from Java code.
     */
    private static void workAroundAIXJavaBug() {
        if (java.lang.Void.TYPE == null)
            java.lang.System.out.println("JDK bug: " +
                                         "java.lang.Void.TYPE uninitialized");
    }
}
