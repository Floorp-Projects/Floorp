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
 * $Id: MozillaAppletPeer.java,v 1.3 2001/07/12 20:26:17 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;

import sun.jvmp.*;
import sun.jvmp.security.*;
import sun.jvmp.applet.*;
import java.util.Hashtable;
import java.net.URL;
import java.net.MalformedURLException;
import java.applet.*;
import java.awt.Frame;

public class MozillaAppletPeer extends WFAppletContext
    implements HostObjectPeer, sun.jvmp.javascript.JSContext
{
    private int                        m_id;
    private int                        m_winid;
    private long                       m_params = 0;
    MozillaPeerFactory                 m_factory;
    WFAppletViewer                     m_viewer = null;
    private boolean                    m_stopped = true;
    protected netscape.javascript.JSObject
                                       m_js = null;
    PluggableJVM                       jvm;

    MozillaAppletPeer(MozillaHostObjectPeer fake, int id)
    {
	m_factory = fake.m_factory;;
	jvm = m_factory.m_jvm;
	jvm.trace("CREATED APPLET OBJECT: "+this,
		  PluggableJVM.LOG_DEBUG);	
	m_id = id;
	m_winid = 0;	
    }
    
    public int getID() 
    {
        return m_id;
    }

    public void setID(int id)
    {
	if (m_id != 0) return;
	m_id = id;       
    }
    public int handleEvent(SecurityCaps caps, int eventID, long eventData) 
    {
	int  retval = 0; 
	//jvm.trace("GOT EVENT "+eventID+"!!!");
	switch (eventID) 
	  {
	  case PE_CREATE:
	      retval = 1;
	      break;	      
	  case  PE_SETTYPE:
	      jvm.trace("SETTYPE called twice - IGNORED", 
			PluggableJVM.LOG_WARNING);
	      retval = 1;
	      break;
	  case PE_SETWINDOW:
	      jvm.trace("PE_SETWINDOW", PluggableJVM.LOG_DEBUG);
	      m_winid = (int)eventData;
	      retval = handleSetWindow();
	      break;
	  case PE_NEWPARAMS:	      
	      m_params = eventData;
	      retval = handleNewParams();
	      break;
	  case PE_DESTROY:
	      retval = destroy(caps);
	      break;
	  case PE_STOP:
	      retval = 1;
	      // do it before stop, as after we can't be sure
	      // our browser peer is still alive
	      m_stopped = true;
	      if (m_viewer != null) m_viewer.stopApplet();	  
	      break;
	  case PE_START:
	      retval = 1;
	      m_stopped = false;
	      m_viewer.startApplet();
	      break;
	  case PE_GETPEER:
	      // this call is handled in both ways - direct and 
	      // queued as policy on place where to handle
	      // JS calls can be changed on Mozilla side
	      // At least for now it works OK with both cases	      
	      retval = handleCall(caps,  PE_GETPEER, eventData);	     
	      break;
	  default:
	      retval = 0;
	  }
	return retval;
    }
    
    public int handleCall(SecurityCaps caps, int arg1, long arg2) 
    {
      int  retval = 0;
      switch (arg1) 
	  {
	  case PE_GETPEER:
	    if (m_viewer == null) break;
	    Object o = null;
	    switch (m_viewer.getLoadingStatus())
		{
		case WFAppletViewer.STATUS_STARTED:
		    o = m_viewer.getApplet();
		    break;
		case WFAppletViewer.STATUS_ERROR: 
		    o = null;
		    break;
		default:
		    jvm.trace("XXX: Applet loading in progress....",
			      PluggableJVM.LOG_WARNING);
		    // here we just return NULL, as for the moment no valid
		    // applet object exist - if someone really needs this 
		    // object - call later
		    break;
		}
	    nativeReturnJObject(o, arg2);
	    retval = 1;
	    break;
	  default:
	    retval = 0;
	  }
      return retval;
    }

    public int destroy(SecurityCaps caps)
    {
	jvm.trace("DESTROY", PluggableJVM.LOG_DEBUG);
	finalizeParams();
	m_params = 0;
	m_stopped = true;
	if (m_viewer != null) { 
	    m_viewer.destroyApplet(2500);	 
	    m_viewer = null; 
	}
	return 1;
    }
    
    public HostObjectPeerFactory getFactory()
    {
	return m_factory;
    }
    
    private int handleNewParams()
    {
	String[][] params = getParams();
	String[]   keys   = params[0];
	String[]   vals   = params[1];
	URL docbase;
	Hashtable  hparams = new Hashtable();
	for (int i=0; i < keys.length; i++)
	    hparams.put(keys[i].toLowerCase(), vals[i]);
	//hparams.put("OWNER", this);
	//hparams.put("ID", new Integer(m_id));
	try 
	    {
		docbase = new URL((String)hparams.get("docbase"));
	    } 
	catch (MalformedURLException e) 
	    {
		jvm.trace("invalid docbase due "+e, 
			  PluggableJVM.LOG_ERROR);
		return 0;
	    }
	m_viewer = m_factory.m_mgr.createAppletViewer(this,
						      m_factory,
						      hparams);
	m_viewer.setDocumentBase(docbase);
	return handleSetWindow();	
    }

    private int handleSetWindow()
    {
      if (m_stopped) return 0;
      if (m_winid != 0) {
	    Frame f = jvm.getFrameWithId(m_winid);
	    if (f != null) m_viewer.setWindow(f);
	}
      return 1;	
    }
    /**
     * From AppletContext 
     */    
    public void showStatus(String string)
    {
      //PluggableJVM.trace("Status: "+string);
      // calls back peer in Mozilla
      if (m_stopped) return;
      nativeShowStatus(string);
    }
    
    public void showDocument(URL url, String target)
    {
      // never pass events after stop() - otherwise
      // we'll crash the browser
      if (m_stopped) return;
      // calls back peer in Mozilla
      nativeShowDocument(url.toString(), target);
    }
    
    public void showDocument(URL url)
    {
	showDocument(url, "_top");
    }
    
    /**
     * From JSContext
     */
    public netscape.javascript.JSObject getJSObject()
    {	
	// check if we got params from browser - fail otherwise
	if (m_params == 0) return null;
	if (m_js == null) 
	    m_js = m_factory.getJSObject(m_params);
	return m_js;
    }

    // duplicates of nsIJavaObjectInfo methods
    // funny way to return 2 arrays of strings
    native protected String[][]     getParams();
    native protected void           finalizeParams();
    native protected boolean        nativeShowStatus(String status);
    native protected boolean        nativeShowDocument(String url, 
						       String target);
    // method to write jobject ret to native pointer ptr
    native protected void           nativeReturnJObject(Object ret, long ptr);
    // XXX: add others
};










