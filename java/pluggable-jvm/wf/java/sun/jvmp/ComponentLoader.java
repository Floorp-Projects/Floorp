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
 * $Id: ComponentLoader.java,v 1.2 2001/07/12 19:57:52 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.net.URL;
import java.net.URLClassLoader;
import java.beans.*;
import java.io.*;
import java.util.jar.*;
import java.lang.reflect.*;
import java.security.CodeSource;

class ComponentLoader
{
    protected PluggableJVM jvm;

    ComponentLoader(PluggableJVM jvm)
    {
	this.jvm = jvm;
    }

    /**
     * Load component from the given URL.
     * It should be a JAR with attribute "Factory-Class" set to the name of 
     * factory class of this component.
     **/
    public ObjectFactory getComponent(URL url)
    {
	try {
	    //jvm.trace("url="+url, jvm.LOG_DEBUG);
	     JarInputStream jis = new JarInputStream(url.openStream());
	     URL wfcp = jvm.getCodeSource().getLocation();
	     // yes - own classloader for every component - must be safer
	     URLClassLoader comploader = new URLClassLoader(new URL [] {url, wfcp},
							    this.getClass().getClassLoader());
	     Class      factory = null;
	     Manifest   mf  = jis.getManifest();
	     Attributes a = mf.getMainAttributes();
	     String classname = 
	       a.getValue(new Attributes.Name("Factory-Class"));
	     // if this component not pretend to be autoregistered
	     // just skip it
	     if (classname == null) return null;	   
	     factory = Class.forName(classname, true, comploader);
	     Method m = factory.getDeclaredMethod("getFactory",
						  new Class[]{PluggableJVM.class,
							      CodeSource.class});
	     CodeSource cs = new CodeSource(url, null);
	     Object o = m.invoke(null, new Object[]{jvm, cs});	     
	     return (ObjectFactory)o;
	} catch (Exception e) {
	    // as I'm such a lazy butt to perform all checks here
	    jvm.trace("getComponent failed: "+e, PluggableJVM.LOG_WARNING);
	    jvm.trace(e, PluggableJVM.LOG_WARNING);
 	    return null;
	}	
    }    

    public void register(URL url)
    {
	ObjectFactory of = getComponent(url);
	if (of == null) return;
	jvm.registerService(of);
    }

     /**
     * Autoregister components from those locations
     **/
    public void register(URL[] urls)
    {
	if (urls == null) return;
	for (int i=0; i<urls.length; i++)
	    register(urls[i]);
    }

    /**
     * Autoregister all components from some directory
     **/
    public void registerAllInDir(File dir)
    {
	if (dir == null || !dir.isDirectory()) return;
	File[] comps = dir.listFiles();
	if (comps == null) return;
	for (int i=0; i<comps.length; i++)
	  {
	    try {
	      if (comps[i].isFile()) register(comps[i].toURL());
	    } catch(Exception e) {
	      jvm.trace("registerAllInDir failed: "+e, 
			PluggableJVM.LOG_WARNING);
	    }
	  }
    }
};






