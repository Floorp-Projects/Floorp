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
 * $Id: WFAppletPanel.java,v 1.1 2001/07/12 20:26:06 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;

import java.applet.*;
import sun.applet.*;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.*;
import java.awt.*;
import sun.jvmp.*;
import sun.jvmp.applet.*;
import java.io.IOException;

// wrapper for JDK's applet viewer implementation
public class WFAppletPanel extends AppletPanel implements ProxyType
{    
    PluggableJVM jvm;
    Frame        viewFrame;
    int          width;
    int          height;
    URL          documentURL;
    URL          baseURL;
    WFAppletContext  appletContext;

    protected Hashtable atts;
    
    private boolean inited = false;
    private String  myJars;

    public  WFAppletPanel(PluggableJVM     jvm,
			  WFAppletContext  appletContext,
			  Hashtable        atts) 
    {
        if (appletContext == null)
            throw new IllegalArgumentException("AppletContext");
	this.atts = atts;
        this.jvm = jvm;
        this.appletContext = appletContext;
        appletContext.addAppletInContext(this);        
    }

    public void init() 
    {
	super.init();
	sendEvent(APPLET_LOAD);
        sendEvent(APPLET_INIT);
    }

    public void appletStart() 
    {
        if (inited) sendEvent(APPLET_START);
    }

    public void appletStop() 
    {
        sendEvent(APPLET_STOP);
    }

    public int getLoadingStatus()
    {
        switch (status)
            {
            case APPLET_START:
                return WFAppletViewer.STATUS_STARTED;
            case APPLET_STOP:
                return WFAppletViewer.STATUS_STOPPED;
            case APPLET_ERROR:
                return WFAppletViewer.STATUS_ERROR;
            case APPLET_LOAD:
                return WFAppletViewer.STATUS_LOADING;
            default:
                return WFAppletViewer.STATUS_UNKNOWN;
            }
    }

    public void onClose(final int timeOut) 
    { 
	appletContext.removeAppletFromContext(this);
        Thread killer = new Thread(new Runnable() {
		public void run() {		    
		    if (status==APPLET_LOAD) stopLoading();
		    sendEvent(APPLET_STOP);
		    sendEvent(APPLET_DESTROY);
		    sendEvent(APPLET_DISPOSE);
		    sendEvent(APPLET_QUIT);
		}
            });
        killer.start();
    }

    public URL getDocumentBase() 
    {
        return documentURL;
    }

    public void setDocumentBase(URL url)
    {
        documentURL = url;
        initCodebase();
        tryToInit();
    }

    public int getWidth() 
    {
        String w = getParameter("width");
        if (w != null) return Integer.valueOf(w).intValue();
        return 0;
    }
 
    public int getHeight() 
    {
        String h = getParameter("height");
        if (h != null) return Integer.valueOf(h).intValue();
        return 0;
    }

    public String getSerializedObject() 
    {
	return null;
    }

    public String getJarFiles() 
    {
        return myJars;
    }

    public String getParameter(String name) {
        name = name.toLowerCase();
        if (atts.get(name) != null) {
            return (String) atts.get(name);
        } else {
            String value=null;
            try {
                value = getParameterFromHTML(name);
            } catch (Throwable e) {
                jvm.trace(e, PluggableJVM.LOG_WARNING);
            }
            if (value != null)
                atts.put(name.toLowerCase(), value);
            return value;
        }
    }
    
    public Hashtable getParameters()
    {
        return (Hashtable) atts.clone();
    }
   
    public URL getCodeBase() 
    {
        return baseURL;
    }
    
    public String getCode() 
    {	 
	String code = getParameter("java_code");
	if (code==null)
	    code=getParameter("code");
	return code;
    }    
    
    public AppletContext getAppletContext()
    {
        return appletContext;
    }
    
    public void setParameter(String name, Object value) {
        name = name.toLowerCase();
        atts.put(name, value.toString());
    }

    public void setWindow(Frame f)
    {
        if (f == null)
          {
	      // maybe create new top level Frame
	      jvm.trace("Got zero Frame for SetWindow",
			PluggableJVM.LOG_ERROR);
	      return;
          }
        f.setLayout(null);
        Applet a = getApplet();
        if (a == null) {
            try {
                int wd = Integer.parseInt(getParameter("width"));
                int ht = Integer.parseInt(getParameter("height"));
                this.width = wd;
                this.height = ht;
            } catch (NumberFormatException e) {
                setParameter("width", new Integer(width));
                setParameter("height", new Integer(height));
	    }	    
	} else {
	    setParameter("width", new Integer(width));
            setParameter("height", new Integer(height));
            setSize(width, height);
            a.resize(width, height);
            a.setVisible(true);
	}
	setBounds(0, 0, width, height);
        if (this.viewFrame == f) return;
	this.viewFrame = f;
        f.add(this);
        f.show();
        f.setBounds(0, 0, width, height);
        tryToInit();
    }


    public void paint(Graphics g)
    {
        if (getApplet() == null && g != null) {
            setBackground(Color.lightGray);
            g.setColor(Color.black);
            Dimension d = getSize();
            Font fontOld = g.getFont();
            FontMetrics fm = null;
            if (fontOld != null)
                fm = g.getFontMetrics(fontOld);
            String str = getStatusString();;
	    
            // Display message on the screen if the applet is not started yet.
            if (d != null && fm != null)
                g.drawString(str, (d.width - fm.stringWidth(str))/ 2,
                             (d.height + fm.getAscent())/ 2);
        }
    }
    
    protected String getParameterFromHTML(String name)
    {
        return null;
    }    

    protected void loadJarFiles(AppletClassLoader loader)
        throws IOException, InterruptedException
    {
	
	
        // Cache option as read from the Html page tag
        String copt;
	
        // Cache Archive value as read from the Html page tag
        String carch;
	
        // Cache Version value as read from the Html page tag
        String cver;
	
        // Vector to store the names of the jar files to be cached
        Vector jname = new Vector();
 
        // Vector to store actual version numbers
        Vector jVersion = new Vector();
 
	
        // Figure out the list of all required JARs.
        String raw = getParameter("java_archive");
        if (raw == null)
	    {
		raw = getParameter("archive");
	    }
	
	// no jars required
	if (raw == null)
            {
                return;
            }
	myJars = raw;
	super.loadJarFiles(loader);
	return;
    }

    protected AppletClassLoader createClassLoader(final URL codebase) 
    {
        return new AVClassLoader(codebase);
    }

    private String getStatusString()
    {
        switch (status)
            {
            case APPLET_START:
                return "Applet started.";
            case APPLET_STOP:
                return "Applet stopped.";
            case APPLET_ERROR:
                return "Error loading applet.";
            case APPLET_LOAD:
                return "WF loading applet...";
            default:
		// just for user's convinience ;)
		return "loading applet...";
                //return "Unknown status";
            }
    }
    

    private void tryToInit() {
        //jvm.trace("tryToInit()", PluggableJVM.LOG_DEBUG);
        if (!inited &&  viewFrame != null && getDocumentBase() != null)
            {
                inited = true;
		Thread starter = new Thread(new Runnable() {
			public void run() {
			    //jvm.trace("real Init()", PluggableJVM.LOG_DEBUG);
			    init();
			    sendEvent(APPLET_START);
			}
		    });
		starter.start();
            }
    }

    private void initCodebase()
    {
        String att = getParameter("java_codebase");
        if (att == null) att = getParameter("codebase");
        if (att != null) {
            if (!att.endsWith("/")) att += "/";
            try {
                baseURL = new URL(documentURL, att);
            } catch (MalformedURLException e) {
            }
        }
        if (baseURL == null) {
            String file = documentURL.getFile();
            int i = file.lastIndexOf('/');
            if (i > 0 && i < file.length() - 1) {
                try {
                    baseURL = new URL(documentURL, file.substring(0, i + 1));
                } catch (MalformedURLException e) {
                }
            }
        }
	if (baseURL == null) baseURL = documentURL;
    }
    

}




