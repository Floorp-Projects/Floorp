/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
package org.mozilla.pluglet;

import java.net.URLClassLoader;
import java.net.URL;
import java.util.jar.Manifest;
import java.util.jar.Attributes;
import java.io.InputStream;

public class PlugletLoader {
    //  path to jar file. Name of main class sould to be in MANIFEST.
    public static Pluglet getPluglet(String path) {
	try {
	    org.mozilla.util.Debug.print("-- PlugletLoader.getPluglet("+path+")\n"); 
	    URL url = new URL("file://"+path);
	    URLClassLoader loader = URLClassLoader.newInstance(new URL[]{url});
	    URL manifestURL = new URL("jar:file://"+path+"!/META-INF/MANIFEST.MF");
	    InputStream inputStream = manifestURL.openStream();
	    Manifest manifest = new Manifest(inputStream);
	    Attributes attr = manifest.getMainAttributes();
	    String plugletClassName = attr.getValue("Pluglet-Class");
	    org.mozilla.util.Debug.print("-- PlugletLoader.getPluglet class name "+plugletClassName+"\n");
            org.mozilla.util.Debug.print("-- PL url[0] "+loader.getURLs()[0]);
	    if (plugletClassName == null) {
		//nb
		return null;
	    }
	    Object pluglet = loader.loadClass(plugletClassName).newInstance();
	    if (pluglet instanceof Pluglet) {
		return (Pluglet) pluglet;
	    } else {
		return null;
	    }
	} catch (Exception e) {
	    org.mozilla.util.Debug.print("-- PlugletLoader.getPluglet exc "+e); 
	    return null;
	}
    } 
    
    public static String getMIMEDescription(String path) {
	try {
	    org.mozilla.util.Debug.print("-- PlugletLoader.getMIMEDescription("+path+")\n");
	    URL manifestURL = new URL("jar:file://"+path+"!/META-INF/MANIFEST.MF");
	    InputStream inputStream = manifestURL.openStream();
	    Manifest manifest = new Manifest(inputStream);
	    Attributes attr = manifest.getMainAttributes();
	    String mimeDescription = attr.getValue("MIMEDescription");
            org.mozilla.util.Debug.print("-- PlugletLoader.getMIMEDescription desc "+mimeDescription+"\n"); 
	    return mimeDescription;
	} catch (Exception e) {
	    org.mozilla.util.Debug.print(e+"\n");
	    return null;
	}
    }
}


