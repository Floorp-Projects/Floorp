/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.mozilla.pluglet.test.basic.security; 

import org.mozilla.pluglet.test.basic.*;

import java.awt.*;
import java.awt.datatransfer.*;
import java.applet.*;
import java.io.*;
import java.net.*;
import java.util.*;
 
public class RunProgramTest extends BasicSecurityTest implements Test {

public RunProgramTest() {
}

protected void doRunProgram(String string) throws Exception {

	Runtime rt = Runtime.getRuntime();
	Process p1 = rt.exec(string);

	InputStream out = p1.getInputStream();

	byte[] buf = new byte[2048];

	while (out.read(buf) != -1) {
		String str = new String(buf, 0);
	}
	out.close();

	InputStream err = p1.getErrorStream();
    	while (err.read(buf) != -1) {
		String str = new String(buf, 0);
	}

	err.close();
}

public void execute( TestContext c ) {

 mustPass = false;

 if ((c.getProperty("RunProgramTest.mustPass")).equals(new String("true")) ) {
	mustPass = true;
 };

 String cmd = c.getProperty("RunProgramTest.command");
 
 try {
 	doRunProgram(cmd);
        if( mustPass )	
		c.registerPASSED(new String("OK")); else
		c.registerFAILED(new String("Illegal operation performed.\nSeems to be a security hole."));
 } catch ( SecurityException e ) {
     if( mustPass )	
		c.registerFAILED(e.toString()); else
		c.registerPASSED(e.toString());
 } catch ( Exception e ) {
	c.registerFAILED(e.toString());
 }

}

}
 
