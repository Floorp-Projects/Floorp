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

import java.io.*;
 
public class DeleteFileTest extends BasicSecurityTest implements Test {

private String filename;

/**
 * a dummy constructor
 */
public DeleteFileTest() {
};

/**
 * do the specified legal or illegal action
 */
protected void doDeleteFile() throws Exception {
	File file = new File(filename);
	file.delete();
};

/**
 * The general test procedure - we define if test failed or passed
 * using the mustPass variable, which was defined in the superclass.
 *
 */
public void execute( TestContext c ) {

 System.out.println("Test execution goes....");
 mustPass = false;

 if ( (c.getProperty("DeleteFileTest.mustPass")).equals(new String("true")) ) {
	mustPass = true;
 };


 filename = c.getProperty("DeleteFileTest.FileName");
 
 try {
 	doDeleteFile();
     	if( mustPass )	
		c.registerPASSED(new String("OK")); else
		c.registerFAILED(new String("Illegal operation performed - file was deleted!.\nSeems to be a security hole."));

 } catch ( SecurityException e ) {
     if( mustPass )	
		c.registerFAILED(e.toString()); else
		c.registerPASSED(e.toString());
 } catch ( Exception e ) {
	c.registerFAILED(e.toString());
 } 
 }
 
}
