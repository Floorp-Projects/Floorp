/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: DebugOutputStream.java,v 1.1 2001/07/12 20:25:50 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.io.*;

public class DebugOutputStream extends OutputStream 
{
    ConsoleWindow        console;
    FileOutputStream     log;

    public DebugOutputStream(ConsoleWindow console,
			     String        filename) 
    {
	this.console = console;
	if (filename != null) 
	    {
		String wfHome = System.getProperty("jvmp.home");
		try {
		    File file = new File(wfHome, filename);
		    if (file.exists()) file.delete();		
		    log = new FileOutputStream(file);
		} catch (IOException e) {
		    e.printStackTrace();
		}
	    } 
    }

    public synchronized void write(byte b[], int off, int len) 
	throws IOException 
    {
	if (console != null) 
	    console.append(new String(b, off, len));
	if (log != null)
	    {
		try {
		    log.write(b, off, len);
		} catch (IOException e) {
		   if (console != null) 
		       console.append(e.toString());
		}
	    }
    }

    public synchronized void write(int b) throws IOException 
    {
	System.err.println("char write: "+b);
	// inefficient implementation, but short writes shouldn't happen often
	byte buf[] = new byte[1];
	buf[0] = (byte)b;
	write(buf, 0, 1);
    }
    
    public synchronized void flush() throws IOException {
	if (log != null) log.flush();	
    }
}
