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
 
public class GetSysPropsTest extends BasicSecurityTest implements Test {

 public GetSysPropsTest() {
 }

protected void doGetProperties(String text) throws Exception {

	Properties props;

	if (text.equals("all")) {
		props = System.getProperties();
    
		for (Enumeration enum = props.propertyNames();
		     enum.hasMoreElements();) {
    		    	String key = (String) enum.nextElement();
		}
	    } else {
		System.getProperty(text);
	    }
}

public void execute( TestContext c ) {

 mustPass = false;

 if ( (c.getProperty("GetSysPropsTest.mustPass")).equals( new String("true") ) ) {
	mustPass = true;
 };

 String param = c.getProperty("GetSysPropsTest.propertyName");
 
 try {
 	doGetProperties(param);
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
