/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
package org.mozilla.xpcom;

import java.net.URLClassLoader;
import java.net.URL;
import java.util.jar.Manifest;
import java.util.jar.Attributes;
import java.io.InputStream;
import java.io.File;

public class ComponentLoader {
    //  path to jar file. Name of main class sould to be in MANIFEST.
    public static Object loadComponent(String location) {
	try {
	    File file = (new File(location)).getCanonicalFile(); //To avoid spelling diffs, e.g c: and C:
	    location = file.getAbsolutePath();
	    if (File.separatorChar != '/') { 
		location = location.replace(File.separatorChar,'/');
	    }
	    if (!location.startsWith("/")) {
		location = "/" + location;
	    }
	    URL url = new URL("file:"+location);
	    URLClassLoader loader = URLClassLoader.newInstance(new URL[]{url});
	    URL manifestURL = new URL("jar:file:"+location+"!/META-INF/MANIFEST.MF");
	    InputStream inputStream = manifestURL.openStream();
	    Manifest manifest = new Manifest(inputStream);
	    Attributes attr = manifest.getMainAttributes();
	    String componentClassName = attr.getValue("Component-Class");
	    if (componentClassName == null) {
		//nb
		return null;
	    }
	    Object object = loader.loadClass(componentClassName).newInstance();
	    return object;
	} catch (Exception e) {
            e.printStackTrace();
	    return null;
	}
    } 
}

