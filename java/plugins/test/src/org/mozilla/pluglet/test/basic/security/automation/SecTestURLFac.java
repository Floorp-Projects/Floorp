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

package org.mozilla.pluglet.test.basic.security.automation;

import org.mozilla.pluglet.test.basic.*;
import java.io.*;
import java.util.*;
import java.net.*;

public class SecTestURLFac implements Test {

private String description = " setURLStreamHandlerFactory";
private String fFac;
private boolean mustPass;

/* 
**This class is dummy stub for test setURLStreamHandlerFactory method.
*/
private class SecTestURLStreamHandlerFactory implements URLStreamHandlerFactory {
  SecTestURLStreamHandlerFactory(){}
  public URLStreamHandler createURLStreamHandler(String protocol) {
    return new URLStreamHandler() {
      public URLConnection openConnection(URL u) {return null;}
    };
  } 
  
}
/*
**End of dummy class declaration
*/
public void doAction() throws Exception {
	

        try { 
		URL.setURLStreamHandlerFactory( new  SecTestURLStreamHandlerFactory()); 
	}
	catch (Error e)
	{
	  throw new SecurityException( "URLFactory already installed" );
	}
}
	
public void execute( TestContext c ) {
 
 mustPass = false;

 if (c.getProperty("SecTestURLFac.mustPass").equals( new String("true") )) {
	mustPass = true;
 };
 fFac = c.getProperty("SecTestURLFac.fFactory");

 try {
 	doAction();
     if( mustPass )	
		c.registerPASSED(new String("OK")); else
		c.registerFAILED(new String(description) + new String(" failed."));

 } catch ( SecurityException e ) {
     if( mustPass )	
		c.registerFAILED(e.toString()); else
		c.registerPASSED(e.toString());
 } catch ( Exception e ) {
        e.printStackTrace();
	c.registerFAILED(e.toString());
 }
 }
}


