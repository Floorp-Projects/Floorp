/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * $Id: ActivatorAppletContext.java,v 1.1 2001/05/09 17:29:58 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;

import sun.jvmp.applet.*;
import java.applet.*;
import java.util.*;
import sun.applet.*;
import java.awt.*;
import java.net.*;
import java.io.*;
import java.lang.reflect.*;
import sun.misc.Ref;

public abstract class ActivatorAppletContext implements AppletContext 
{
     
    /*
     * Get an audio clip.
     *
     * @param url url of the desired audio clip
     */
    public AudioClip getAudioClip(URL url) {
        System.getSecurityManager().checkConnect(url.getHost(), url.getPort());
	synchronized (audioClips) {
	    AudioClip clip = (AudioClip)audioClips.get(url);
	    if (clip == null) {
		clip = new AppletAudioClip(url);
		audioClips.put(url, clip);
	    }
	    return clip;
	}
    }

    /*
     * Get an image.
     * 
     * @param url of the desired image
     */
    public Image getImage(URL url) {
	//sun.jvmp.PluggableJVM.trace("Ask for image: "+url);
	return getCachedImage(url);
    }

     static Image getCachedImage(URL url) {
         System.getSecurityManager().checkConnect(url.getHost(), url.getPort());
	 return (Image)getCachedImageRef(url).get();
    }

    /**
     * Get an image ref.
     */
    static Ref getCachedImageRef(URL url) {
        synchronized (imageRefs) {
            AppletImageRef ref = (AppletImageRef)imageRefs.get(url);
            if (ref == null) {
                ref = new AppletImageRef(url);
                imageRefs.put(url, ref);
            }
            return ref;
        }
    }


    /**
     * Get an applet by name.
     */
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
		    if (checkConnect(appletPanel.getCodeBase().getHost(),
				     p.getCodeBase().getHost())==false)
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
    
    /**
     * Return an enumeration of all the accessible
     * applets on this page.
     */
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

    /*
     * <p>
     * Check that a particular applet is authorized to connect to 
     * the target applet.
     * This code is JDK 1.1 and 1.2 dependent
     * </p>
     *
     * @param source Source applet host name requesting the connect
     * @param target Target applet host name to connect to 
     * 
     * @return true if the connection is granted
     */
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

    /*
     * Replaces the Web page currently being viewed with the given URL
     *
     * @param url the address to transfer to
     */
    abstract public void showDocument(URL url);

    /*
     * Requests that the browser or applet viewer show the Web page
     * indicated by the url argument.
     *
     * @param url the address to transfer to
     * @param target One of the value
     *	"_self"  show in the current frame
     *  "_parent"show in the parent frame
     *  "_top"   show in the topmost frame
     *  "_blank" show in a new unnamed top-level windownameshow in a 
     *           new top-level window named name
     */
    abstract public void showDocument(URL url, String target);

    /*
     * Show status.
     * 
     * @param status status message
     */
    abstract public void showStatus(String status);

    /* 
     * Add a applet in this applet context
     * 
     * @param applet the applet to add
     */
    void addAppletInContext(AppletPanel appletPanel) {
	this.appletPanel = appletPanel;
	appletPanels.addElement(appletPanel);
    }

    /* 
     * Remove applet from this applet context
     * 
     * @param applet applet to remove
     */
    void removeAppletFromContext(AppletPanel applet) {
	appletPanels.removeElement(applet);
    }

    private AppletPanel appletPanel;
    
    private static Vector appletPanels = new Vector();

    private static Hashtable audioClips = new Hashtable();
    private static Hashtable imageRefs = new Hashtable();
}

