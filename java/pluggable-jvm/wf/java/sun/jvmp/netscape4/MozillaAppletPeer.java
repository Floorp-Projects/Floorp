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
 * $Id: MozillaAppletPeer.java,v 1.2 2001/07/12 19:58:01 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.netscape4;

import sun.jvmp.*;
import sun.jvmp.security.*;
import sun.jvmp.applet.*;
import java.util.Hashtable;
import java.net.URL;
import java.net.MalformedURLException;
import java.lang.reflect.*;

public class MozillaAppletPeer extends WFAppletContext
    implements HostObjectPeer
{
    private int                        m_id;
    private int                        m_winid;
    //private long                       m_params;
    NetscapePeerFactory                m_factory;
    AppletViewer                       m_viewer = null;
    private boolean                    m_stopped = true;

    MozillaAppletPeer(MozillaHostObjectPeer fake, int id)
    {
	//PluggableJVM.trace("CREATED APPLET OBJECT");
	m_factory = fake.m_factory;
	m_id = id;
	m_winid = 0;
	// XXX: get it from browser
	/*
	ActivatorAppletPanel.initEnvironment(ActivatorAppletPanel.PROXY_TYPE_MANUAL,
					     "http=webcache-cup:8080",
					     "dubhe1",
					     null);
	*/
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
	PluggableJVM jvm = m_factory.m_jvm;
	jvm.trace("GOT EVENT "+eventID+"!!!", PluggableJVM.LOG_DEBUG);
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
	      jvm.trace("PE_NEWPARAMS", PluggableJVM.LOG_DEBUG);
	      //m_params = eventData;
	      // execute it async
	      retval = handleNewParams(this, caps);
	      //retval = 1;
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
	  default:
	      retval = 0;
	  }
	return retval;
    }

    public int  handleCall(SecurityCaps caps, int arg1, long arg2) 
    {
	return 1;
    }
    

    public int destroy(SecurityCaps caps)
    {
	m_factory.m_jvm.trace("DESTROY", PluggableJVM.LOG_DEBUG);
	m_stopped = true;
	if (m_viewer != null) { 
	    m_viewer.destroyApplet(5000);	 
	    m_viewer = null; 
	}
	return 1;
    }
    
    int handleNewParams(Object arg, SecurityCaps caps)
    {
	if (arg != null) 
	    {
		Method m;
		try {
		m = arg.getClass().getDeclaredMethod("handleNewParams", 
						     new Class[] {Object.class});
		} catch (Exception e) {
		    m_factory.m_jvm.trace("No handleNewParams: "+e,
					   PluggableJVM.LOG_ERROR); 
		    return 0; 
		}
		AsyncExecutor ex = 
		    new AsyncExecutor(arg, m, new Object[] {null});
		ex.start();
		return 1;
	    }
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
		m_factory.m_jvm.trace("invalid docbase due "+e,
				       PluggableJVM.LOG_WARNING);
		return 0;
	    }
	m_viewer = m_factory.m_mgr.createAppletViewer(this,
						      m_factory,
						      hparams);
	m_viewer.setDocumentBase(docbase);
	handleEvent(caps, PE_START, 0);
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
    
    public void showStatus(String string)
    {
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
      //PluggableJVM.trace("showDocument: "+url);
      nativeShowDocument(url.toString(), target);
    }
    
    public void showDocument(URL url)
    {
	showDocument(url, "_top");
    }

    public HostObjectPeerFactory getFactory()
    {
	return m_factory;
    }
    
    // duplicates of nsIJavaObjectInfo methods
    // funny way to return 2 arrays of strings
    native protected String[][]     getParams();
    native protected boolean        nativeShowStatus(String status);
    native protected boolean        nativeShowDocument(String url, 
						       String target);
    // XXX: add others
};


class AsyncExecutor extends Thread
{
    Object o;
    Method m;
    Object[] args;
    AsyncExecutor(Object o, Method m, Object[] args)
    {
	this.o = o; this.m = m; this.args = args;
    }
    public void run() 
    {
	try {
	    m.invoke(o, args);
	} catch (Exception e) {
	}
    }
}



