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
 * $Id: WFAppletContext.java,v 1.1 2001/07/12 20:25:58 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.applet;

import java.applet.*;
import java.util.*;
import sun.applet.*;
import java.awt.*;
import java.net.*;
import java.io.*;
import java.lang.reflect.*;
import sun.awt.image.URLImageSource;

public abstract class WFAppletContext implements AppletContext 
{
    public AudioClip getAudioClip(URL url) {
        System.getSecurityManager().checkConnect(url.getHost(), url.getPort());
	synchronized (audioClips) {
	    // first try cached data
	    AudioClip clip = (AudioClip)getFromCache(audioClips, url);
	    if (clip == null) {
		clip = new AppletAudioClip(url);
		putToCache(audioClips, url, clip);
	    }
	    return clip;
	}
    }

    public Image getImage(URL url) {
	System.getSecurityManager().checkConnect(url.getHost(), url.getPort());
	synchronized (images) {
	    // first try cached data
	    Image img = (Image)getFromCache(images, url);
	    if (img == null) {
		img = 
		    Toolkit.getDefaultToolkit().createImage(new URLImageSource(url));;
		putToCache(images, url, img);
	    }
	    return img;
	}	
    }

    public Applet getApplet(String name) {
	name = name.toLowerCase();
	for (Enumeration e = appletPanels.elements() ; e.hasMoreElements() ;) {
	    AppletPanel p = (AppletPanel)e.nextElement();
	    String param = p.getParameter("name");
	    if (param != null) {
		param = param.toLowerCase();
	    }
	    if (name.equals(param) && 
		p.getDocumentBase().equals(appletPanel.getDocumentBase())) {
		try {
		    if (!checkConnect(appletPanel.getCodeBase().getHost(),
				      p.getCodeBase().getHost()))
			return null;
		} catch (InvocationTargetException ee) {
		    showStatus(ee.getTargetException().getMessage());
		    return null;
		} catch (Exception ee) {
		    showStatus(ee.getMessage());
		    return null;
		}
		return p.getApplet();
	    }
	}
	return null;
    }
    
    public Enumeration getApplets() {
	Vector v = new Vector();
	for (Enumeration e = appletPanels.elements() ; e.hasMoreElements() ;) {
	    AppletPanel p = (AppletPanel)e.nextElement();
	    if (p.getDocumentBase().equals(appletPanel.getDocumentBase())) {
		try {
		    checkConnect(appletPanel.getCodeBase().getHost(), 
				 p.getCodeBase().getHost());
		    v.addElement(p.getApplet());
		} catch (InvocationTargetException ee) {
		    showStatus(ee.getTargetException().getMessage());
		} catch (Exception ee) {
		    showStatus(ee.getMessage());
		}
	    }
	}
	return v.elements();
    }


    private boolean checkConnect(String sourceHostName, String targetHostName)
	throws SecurityException, InvocationTargetException, Exception
    {
	SocketPermission panelSp = 
	    new SocketPermission(sourceHostName,
		       		 "connect");
	SocketPermission sp =
	    new SocketPermission(targetHostName,
	       			 "connect");
	if (panelSp.implies(sp)) {
	    return true;
	}	
	return false;
    }

    // separate methods to easily work around situation when weak refs
    // not accesible
    Object getFromCache(Hashtable cache, 
			Object key) 
    {
	Object result = null;
	try {
	    java.lang.ref.WeakReference ref = 
		(java.lang.ref.WeakReference)cache.get(key);
	    if (ref != null) result = ref.get();
	} catch (Exception e) {
	    // if no weak refs - use cache what gives memory impact
	    result = cache.get(key);
	}
	return result;
    }

    void  putToCache(Hashtable cache, 
		     Object    key,
		     Object    value)
    {
	try {
	    cache.put(key,  new java.lang.ref.WeakReference(value));
	} catch (Exception e) {
	    // if no weak refs - use cache what gives memory impact
	    cache.put(key, value);
	}
    }

    abstract public void showDocument(URL url);
    abstract public void showDocument(URL url, String target);
    abstract public void showStatus(String status);

    public void addAppletInContext(AppletPanel appletPanel) {
	this.appletPanel = appletPanel;
	appletPanels.addElement(appletPanel);
    }

    public void removeAppletFromContext(AppletPanel applet) {
	appletPanels.removeElement(applet);
    }

    public  java.util.Iterator getStreamKeys() {
	return null;
    }

    public java.io.InputStream getStream(String s) {
	return null;
    }

    public void setStream(String s, java.io.InputStream str) {
    }

    private AppletPanel   appletPanel;    
    private static Vector appletPanels  = new Vector();
    private static Hashtable audioClips = new Hashtable();
    private static Hashtable images     = new Hashtable();
}





