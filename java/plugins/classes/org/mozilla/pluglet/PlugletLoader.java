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
package org.mozilla.pluglet;

import java.net.URLClassLoader;
import java.net.URL;
import java.util.jar.Manifest;
import java.util.jar.Attributes;
import java.io.InputStream;
import java.io.File;
import org.mozilla.pluglet.mozilla.*;
import java.security.*;
import java.awt.*;

public class PlugletLoader {
    static {
	System.loadLibrary("plugletjni");
	Dimension screenSize = new Button().getToolkit().getScreenSize();
    }
    
    //  path to jar file. Name of main class sould to be in MANIFEST.
    public static PlugletFactory getPluglet(String path) {
	try {
	    org.mozilla.util.DebugPluglet.print("-- PlugletLoader.getPluglet("+path+")\n");
	    File file = (new File(path)).getCanonicalFile(); //To avoid spelling diffs, e.g c: and C:
	    path = file.getAbsolutePath();
	    if (File.separatorChar != '/') {
		path = path.replace(File.separatorChar,'/');
		
	    }
	    if (!path.startsWith("/")) {
		path = "/" + path;
		
	    }
	    URL url = new URL("file:"+path);
	    URLClassLoader loader = URLClassLoader.newInstance(new URL[]{url});
	    URL manifestURL = new URL("jar:file:"+path+"!/META-INF/MANIFEST.MF");
	    InputStream inputStream = manifestURL.openStream();
	    Manifest manifest = new Manifest(inputStream);
	    Attributes attr = manifest.getMainAttributes();
	    String plugletClassName = attr.getValue("Pluglet-Class");
	    org.mozilla.util.DebugPluglet.print("-- PlugletLoader.getPluglet class name "+plugletClassName+"\n");
            org.mozilla.util.DebugPluglet.print("-- PL url[0] "+loader.getURLs()[0]+"\n");
	    if (plugletClassName == null) {
		//nb
		return null;
	    }
	    if (policy == null) {
		policy = new PlugletPolicy(Policy.getPolicy());
	    }
	    if (policy != Policy.getPolicy()) {
		Policy.setPolicy(policy);
	    }
	    CodeSource codesource = new CodeSource(url,(java.security.cert.Certificate []) null);
	    Permission perm = new AllPermission();
	    PermissionCollection collection = perm.newPermissionCollection();
	    collection.add(perm);
	    policy.grantPermission(codesource,collection);
	    Class clazz = loader.loadClass(plugletClassName);
	    if (plugletClass == null) {
		plugletClass = Class.forName("org.mozilla.pluglet.Pluglet");
		plugletFactoryClass = Class.forName("org.mozilla.pluglet.PlugletFactory");
	    }
	    if (isSubclassOf(clazz, plugletFactoryClass)) {
                org.mozilla.util.DebugPluglet.print("-- ok we have a PlugletFactory"+"\n");
		return (PlugletFactory)clazz.newInstance();
	    } else {
                org.mozilla.util.DebugPluglet.print("-- we do not have a PlugletFactory. Maybe it is Pluglet"+"\n");
		if (isSubclassOf(clazz,plugletClass)) {
		    org.mozilla.util.DebugPluglet.print("-- it is Pluglet. Creating Generic Factory"+"\n");
		    return new PlugletFactoryGeneric(clazz);
		} else {
		    org.mozilla.util.DebugPluglet.print("-- That is nor Pluglet nor PlugletFactory\n");
		    return null;
		}
		    
	    }
	} catch (Exception e) {
	    org.mozilla.util.DebugPluglet.print("-- PlugletLoader.getPluglet exc "+e); 
            e.printStackTrace();
	    return null;
	}
    } 
    
    public static String getMIMEDescription(String path) {
	try {
	    org.mozilla.util.DebugPluglet.print("-- PlugletLoader.getMIMEDescription("+path+")\n");
	    URL manifestURL = new URL("jar:file:"+path+"!/META-INF/MANIFEST.MF");
	    InputStream inputStream = manifestURL.openStream();
	    Manifest manifest = new Manifest(inputStream);
	    Attributes attr = manifest.getMainAttributes();
	    String mimeDescription = attr.getValue("MIMEDescription");
            org.mozilla.util.DebugPluglet.print("-- PlugletLoader.getMIMEDescription desc "+mimeDescription+"\n"); 
	    return mimeDescription;
	} catch (Exception e) {
	    org.mozilla.util.DebugPluglet.print(e+"\n");
	    e.printStackTrace();
	    return null;
	}
    }
    
    private static boolean isSubclassOf(Class a, Class b) {
	if (a == null
	    || b == null) {
	    return false;
	}
	Class[] interfaces = a.getInterfaces();
	//org.mozilla.util.DebugPluglet.print("-- PlugletLoader.isSubclassOf "+interfaces.length+" "+a+" "+b+"\n"); 
	boolean res = false;
	if (isSubclassOf(a.getSuperclass(),b)) {
	    res = true;
	} else {
	    for (int i = 0; i < interfaces.length; i++) {
		//org.mozilla.util.DebugPluglet.print("-- PlugletLoader.isSubclassOf "+interfaces[i]+"\n"); 
		if (b.equals(interfaces[i])) {
		    res = true;
		    break;
		} else if (isSubclassOf(interfaces[i],b)) {
		    res = true;
		    break;
		}
	    }
	}
	return res;
    }
    private static PlugletPolicy policy = null;
    private static Class plugletClass = null;
    private static Class plugletFactoryClass = null;

}



