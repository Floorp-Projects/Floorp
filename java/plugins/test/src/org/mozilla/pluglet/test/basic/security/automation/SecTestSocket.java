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
import java.net.*;

public class SecTestSocket implements Test {

private String description = " setSecurityManager";
private String fHost;
private String fPort;
private boolean mustPass;

void dumpInputStream( InputStream is, boolean doAll ) throws java.lang.Exception {
	BufferedReader	data = new BufferedReader( new InputStreamReader(is) );

        String line;
        while ((line = data.readLine()) != null) {
	  System.out.println( line );
	  if (!doAll) break;
	}
}

public void doAction() throws Exception {

      Socket		es = new Socket( fHost, Integer.parseInt(fPort) );
      DataOutputStream	os = new DataOutputStream( es.getOutputStream() );

      os.writeBytes( "GET / HTTP/1.0\n\n" );

      dumpInputStream( es.getInputStream(), true );
}
	
public void execute( TestContext c ) {
 
 mustPass = false;

 if (c.getProperty("SecTestSocket.mustPass").equals( new String("true") )) {
	mustPass = true;
 };

 fHost = c.getProperty("SecTestSocket.fHost");
 fPort = c.getProperty("SecTestSocket.fPort");

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
	c.registerFAILED(e.toString());
 }
 }
}



