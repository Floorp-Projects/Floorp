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
 * $Id: WaterfallExtension.java,v 1.2 2001/07/12 19:57:54 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import sun.jvmp.security.*;
import java.lang.reflect.*;
import java.net.*;
import java.util.*;
import sun.jvmp.security.*;
import java.security.*;
import java.io.*;

class WaterfallExtension implements AccessControlDecider, IDObject
{
    protected int    m_version;
    protected String m_cid;
    protected int    m_id;
    Class            bootstrapClass = null;
    ClassLoader      extClassLoader;
    boolean          inited;
    HostObjectPeerFactory factory;
    AccessControlDecider  decider;
    PluggableJVM     m_jvm;
    String           m_classpath, m_classname;

    public WaterfallExtension(PluggableJVM jvm, String cid, int version,
			      String classpath, String classname,
			      long data)
	throws ClassNotFoundException, 
	       MalformedURLException, 
	       NoSuchMethodException,
	       SecurityException,
	       IllegalAccessException,
	       IllegalArgumentException,
	       InvocationTargetException
    {
	inited = false;
	m_cid = cid;
	m_version = version;
	m_jvm = jvm;
	m_classpath = classpath;
	m_classname = classname;
	try 
	    {
	      	// XXX: I dunno, if default should be in classpath 
	        classpath = //getDefaultClassPath() + 
		  classpath;
		URL[] urls = getURLs(classpath);
		ClassLoader loader =  this.getClass().getClassLoader();
		
		extClassLoader = new URLClassLoader(urls, loader);
		bootstrapClass = 
		    Class.forName(classname, true, extClassLoader);
		
		Method start = 
		    bootstrapClass.getDeclaredMethod("start",
						     new Class[]{PluggableJVM.class, Long.class});
		// start() is a static method - returns factory
		// to produce instances of desired type
		factory = (HostObjectPeerFactory)
		    start.invoke(null, new Object[]{m_jvm, new Long(data)});
		decider = (AccessControlDecider)factory;
		
		if (!m_cid.equals(factory.getCID()))
		    {
			// smth is wrong, created object 
			// is of incorrect type
			m_jvm.trace("RETURNED CONTRACT ID ISN'T CORRECT",
				    PluggableJVM.LOG_ERROR);
			// XXX: correct exception?
			throw new 
			    IllegalArgumentException("Mismatch contract ID: "+
						     factory.getCID() + 
						     " instead of "+ m_cid);
		    }
	    }
	catch (ClassCastException e)
	    {
		m_jvm.trace("Extension class not implements required interfaces",
			    PluggableJVM.LOG_ERROR);
		m_jvm.trace(e, PluggableJVM.LOG_ERROR);
		throw e;
	    }
	catch (MalformedURLException e)
	    {
		m_jvm.trace("Oops, can't create URLs from\""
			    +classpath+"\"",  PluggableJVM.LOG_ERROR);
		throw e;
	    }
	catch (ClassNotFoundException e)
	    {
		m_jvm.trace("Oops, can't find class \""
			    +classname+"\"",  PluggableJVM.LOG_ERROR);
		throw e;
	    }
	catch (NoSuchMethodException e)
	    {
		m_jvm.trace("No start method in class "+classname,
			    PluggableJVM.LOG_ERROR);
		throw e;
	    }
	//PluggableJVM.trace("+++++++ Extension inited");
	inited = true;
    }
    
    public void setID(int id)
    {
	if (m_id != 0) return;
	if (factory != null) factory.setID(id);
	m_id = id;
    }
    public int getID()
    {
	return m_id;
    }
    
    public String getCID()
    {
	return m_cid;
    }

    public int getVersion()
    {
	return m_version;
    }
    
    public HostObjectPeer instantiate(SecurityCaps caps, int version)
    {
	if (!inited)
	    return null;
	return factory.create(version);
    }

    public int  decide(CallingContext ctx, String principal, int cap_no)
    {
	if (decider == null) return NA;
	return decider.decide(ctx, principal, cap_no);
    }
    
    public boolean belongs(int cap_no)
    {
	if (decider == null) return false;
	return decider.belongs(cap_no);
    }
    

    protected URL[] getURLs(String classpath) 
      throws MalformedURLException
    {
	int i, pos, count;
	URL url;
	if (classpath == null || "".equals(classpath)) return new URL[]{};
	Vector v = new Vector();
	pos = count = 0;
	do 
	    {
		count++;
		i = classpath.indexOf("|", pos);
		if (i == -1)
		  url = new URL(classpath.substring(pos));
		else 
		  url = new URL(classpath.substring(pos, i));
		m_jvm.trace("adding URL: "+ url,  PluggableJVM.LOG_DEBUG);
		v.addElement(url);
		pos = i+1;
	    }
	while (i > -1);
	
	URL[] urls = (URL[])(v.toArray(new URL[v.size()]));
	return urls;
    }
    
    public HostObjectPeerFactory getFactory()
    {
	return factory;
    }

    public String toString()
    {
	StringBuffer buf = new StringBuffer();
	buf.append("Waterfall extension: ");
	buf.append("classpath=\""+m_classpath+"\" classname="+m_classname+"\n");
	buf.append("cid="+m_cid+" version="+m_version);
	if (factory != null) buf.append(" factory=" + factory.toString());
	return buf.toString();
    }

    protected String getDefaultClassPath() 
    {
	StringBuffer sb = new StringBuffer();
	String wfHome = System.getProperty("jvmp.home");
	File dir = new File(wfHome, "ext");
	if (dir == null || !dir.isDirectory()) return "";
	File[] comps = dir.listFiles();
	if (comps == null) return "";
//      try {
//  	    for (int i=0; i<comps.length; i++)
//  		if (comps[i].isFile()) 
//  		    sb.append(comps[i].toURL().toString()+"|");
//  	} catch(Exception e) {
//  	}
//  	return sb.toString();
	return "";
    }
}



