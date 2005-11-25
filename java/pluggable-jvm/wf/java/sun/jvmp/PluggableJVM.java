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
 * $Id: PluggableJVM.java,v 1.3 2005/11/25 08:16:30 timeless%mozdev.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

/** 
 * Main Waterfall Java class. It works as a message queue handler.
 * TODO:
 * - To make it more portable sun.misc.Queue dependency should be removed.
 * - improve synchronyzation policy
 **/

package sun.jvmp;


import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.*;
import sun.misc.Queue;
import sun.jvmp.security.*;
import java.security.*;
import java.lang.reflect.Constructor;

public abstract class PluggableJVM 
  extends      Thread 
  implements   URLStreamHandlerFactory
{
    protected static int          debugLevel = 10;
    protected PluggableJVM        instance = null;    
    protected boolean             initedNative = false; 
    protected boolean             shouldStop   = false;
    protected boolean             running = false;
    protected boolean             inited = false;    
    protected IDList              nativeFrames;
    protected IDList              nativeThreads;
    protected IDList              nativeMonitors;
    protected ConsoleWindow       console;
    protected IDList              javaPeers;
    protected IDList              vendorExtensions;
    protected Queue               event_queue;
    protected MultipleHashtable   extensions;
    protected ExtSecurityManager  securityManager;
    protected Hashtable           handlesFrames;
    protected Hashtable           handlesMonitors;
    protected WFPolicy            policy;
    protected Hashtable           hashCID;
    protected CodeSource          codesource;
    protected ComponentLoader     loader;
    protected Vector              protocolHandlers;

    /* commands implemented by this pluggable JVM 
       - use JVMP_SendSysEvent() call */
    protected final static int CMD_CTL_CONSOLE = 1;

    /* tracing facility levels */
    public final static int LOG_CRITICAL  = 0;
    public final static int LOG_IMPORTANT = 2;
    public final static int LOG_ERROR     = 4;
    public final static int LOG_WARNING   = 6;
    public final static int LOG_INFO      = 8;
    public final static int LOG_DEBUG     = 10;
    
    /*****************************************************************/
    /*************************** PUBLIC METHODS **********************/
    public Frame getFrameWithId(int id) 
    {
	NativeFrame f = (NativeFrame)nativeFrames.get(id);
	if (f == null) return null;
	return f.getFrame();
    }

    public Enumeration getAllFrames()
    {
	return nativeFrames.elements();
    }
    
    public HostObjectPeer getPeerWithId(int id)
    {       
	return (HostObjectPeer)javaPeers.get(id);       
    }

    public Enumeration getAllPeers()
    {
	return javaPeers.elements();
    }

    public HostObjectPeerFactory getFactoryWithId(int id)
    {
	WaterfallExtension ext = 
	    (WaterfallExtension)vendorExtensions.get(id);
	if (ext == null) return null;
	return ext.getFactory();       
    }
    
    public Enumeration getAllExtensions()
    {
	return vendorExtensions.elements();
    }
    
    public Thread getThreadWithId(int id)
    {
	NativeThread t = (NativeThread)nativeThreads.get(id);
	if (t == null) return null;
	return t.getThread();
    }
    
    public Enumeration getAllThreads()
    {
	return nativeThreads.elements();
    }        

    public SynchroObject getSynchroObjectWithID(int id)
    {
      return (SynchroObject)nativeMonitors.get(id);
    }

    public Enumeration getAllSynchroObjects()
    {
	return nativeMonitors.elements();
    }

    public static void trace(String msg, int level) 
    {
      if (level <= debugLevel) 
	  System.err.println("WF trace("+level+"): " + msg);
    }

    public static void trace(Throwable e, int level) 
    {
      if ((level <= debugLevel) && (e != null)) 
	  e.printStackTrace();
    }    
   
    public PluggableJVM getJVM() 
    {
	return instance;
    }

    /**
     * Those functions should be emphasized. Waterfall itself is just a glueing 
     * framework, but good one :). Those functions allows any entity
     * announce itself as a factory able to produce objects ready to serve 
     * for some purpose, specified by contract ID (CID). CID itself can be any string,
     * describing object functionality. For example Mozilla-like contract IDs, such as
     * @sun.com/wf/applets/appletviewer;1  (XXX: what is "1" here? version?)
     * Object generated by such factories could implement some expected by client
     * interfaces, such as sun.jvmp.applet.AppletViewer, but client should be ready
     * to handle ClassCastException and behave properly. Together with ability 
     * to use java beans from both user-wide and system-wide directories it allows
     * Waterfall to be extended very well. Also extensions can register themselves
     * as servers at startup time.
     **/
    /**
     * Register given object factory. Waterfall and object factory use protocol
     * described in sun.jvmp.ObjectFactory to complete registration.
     **/
    public boolean registerService(ObjectFactory f)
    {
	int proceed = 0;
	if (f == null) return false;
	Enumeration e = f.getCIDs();
	if (e == null) return false;
	try {	    
	    for (;e.hasMoreElements();)
		{
		    ObjectFactory f1;
		    String cid = (String) e.nextElement();
		    if (cid == null) continue;
		    if ((f1 = (ObjectFactory)hashCID.get(cid)) != null)
			{
			    // resolve conflict
			    // the same factory - do nothing
			    if (f.equals(f1)) continue;
			    if (!f.handleConflict(cid, f1) || 
				f1.handleConflict(cid, f)) continue;
			}		    
		    hashCID.put(cid, f);
		    trace("registered service "+f+" for contract ID "+cid,
			  LOG_INFO);
		    proceed++;
		}
	} catch (Exception ex) {
	    trace("Exception in registerService: "+ex, LOG_WARNING);
	    trace(ex, LOG_WARNING);	    
	    return false;
	}
	return (proceed > 0);
    } 
    
    /**
     * Create server object using given CID, and arg as argument of 
     * production method
     **/
    public Object getServiceByCID(String cid, Object arg)
    {
	Object o;
	try {
	    if (cid == null) return null;
	    ObjectFactory of = (ObjectFactory)hashCID.get(cid);
	    if (of == null) return null;
	    o = of.instantiate(cid, arg);
	} catch (Exception e) {
	    trace(e, LOG_WARNING);
	    return null;
	}
	return o;
    }

    /**
     * Initiate unregistering of this service. Unregister completed
     * by Waterfall and object factory using protocol described in 
     * sun.jvmp.ObjectFactory
     **/
    public boolean unregisterService(ObjectFactory o)
    {
	int proceed = 0;
	if (o == null) return false;
	// just release our reference to this object.
	for (Enumeration e = hashCID.keys(); e.hasMoreElements();) 
	    {
		Object key = e.nextElement();
		if (o.equals(hashCID.get(key))) 
		    {
			hashCID.put(key, null);
			proceed++;
		    }
	    }
	return (proceed > 0);
    }
    

    public PermissionCollection getExtensionsPermissions(CodeSource codesource)
    {
	Permissions p = new Permissions();
	// return permissions provided by native-based extensions
	// really I'd like to have only one repository of extensions
	// for both native and Java API.
	// rewrite it ASAP
	for (Enumeration e=vendorExtensions.elements(); e.hasMoreElements();) 
	    {
		HostObjectPeerFactory f = 
		    ((WaterfallExtension)e.nextElement()).getFactory();
		PermissionCollection c = f.getPermissions(codesource);
		if (c != null)
		    {
			for (Enumeration e1=c.elements(); e1.hasMoreElements(); )
			    p.add((Permission)e1.nextElement());
		    }
	    }
	// XXX: it could lead to duplication of permissions,
	// but I don't think it's so awful
	for (Enumeration e=hashCID.keys(); e.hasMoreElements();) 
	    {
		ObjectFactory f = 
		    (ObjectFactory)hashCID.get(e.nextElement());
		PermissionCollection c = f.getPermissions(codesource);
		if (c != null)
		    {
			for (Enumeration e1=c.elements(); e1.hasMoreElements(); )
			    p.add((Permission)e1.nextElement());
		    }
	    }
	return p;
    }

    // XXX: kill me ASAP
    public CodeSource getCodeSource()
    {
	return codesource;
    }

    public URLStreamHandler createURLStreamHandler(String protocol)
    {
      //trace("handler for: "+protocol, PluggableJVM.LOG_DEBUG);
      URLStreamHandler handler = null;
      for (Enumeration e=protocolHandlers.elements();e.hasMoreElements();)
	  {
	      ProtoHandlerDesc d = (ProtoHandlerDesc)e.nextElement();
	      handler = tryToGetHandler(d.loader, d.pkgs, protocol);
	      if (handler != null) return handler;
	  }
      // fallback to defaults
      return tryToGetHandler(null, "sun.net.www.protocol", protocol);
    }

    /**
     * It registers packages that can be loader with this classloader
     * as URL handlers, according to standard procedure described 
     * in java.net.URL documentation.
     **/
    public void registerProtocolHandlers(ClassLoader  loader,
					 String       pkgs)
    {
      synchronized (protocolHandlers) 
	{
	  protocolHandlers.addElement(new ProtoHandlerDesc(loader, pkgs));
	}
    }
    /***************** END OF PUBLIC METHODS *************************/
    /*****************************************************************/
    /* methods from here accessible only to JNI calls, or to our subclasses */
    protected PluggableJVM() 
    {
	nativeFrames     = new IDList();
	nativeThreads    = new IDList();
	nativeMonitors   = new IDList();
	javaPeers        = new IDList();
	vendorExtensions = new IDList();
	event_queue      = new Queue();
	extensions       = new MultipleHashtable();
	securityManager  = new ExtSecurityManager(this);
	handlesFrames    = new Hashtable();
	handlesMonitors  = new Hashtable();
	hashCID          = new Hashtable();
	protocolHandlers = new Vector();
	setName("Waterfall system thread");
    }
    
    /* two additional guarded entry points - not part of public API */ 
    public void run() 
    {
	if (running) return;
	// stuff like components autoregistration moved to this thread 
	// to minimize latency
	String wfHome = System.getProperty("jvmp.home");
	synchronized (loader) 
	  {
	    loader.registerAllInDir(new File(wfHome, "ext"));
	  }
	running = true;		
	runMainLoop();
    }
    
    public void init(String codebase, boolean isNative) 
    {
	// init AWT here
	if (inited) return;	
	Frame f = new Frame();
	f.setBounds(0, 0, 1, 1);
	f.addNotify();
	//f.show();
	//f.hide();
	f.removeNotify();	
	f = null;
	
	// create console
	console = new ConsoleWindow(this);	
	trace("stdout and stderr redirected to console", LOG_WARNING);
	// Reassign stderr and stdout to console
	DebugOutputStream myErrOS = new DebugOutputStream(console, "wflog.err");
	PrintStream myErrPS = new PrintStream(myErrOS);
	System.setErr(myErrPS);	
	DebugOutputStream myOutOS = new DebugOutputStream(console, "wflog.out");
	PrintStream myOutPS = new PrintStream(myOutOS);
	System.setOut(myOutPS);
	console.printHelp();

	if (isNative) loadLibrary();
	File c = new File(codebase);
	try {
	    codesource = new CodeSource(c.toURL(), null);
	    policy = new WFPolicy(this, c.toURL());
	    java.security.Policy.setPolicy(policy);
	    //trace("policy is "+java.security.Policy.getPolicy(), LOG_DEBUG);
	} catch (Exception e) {
	    trace("Wow, cannot set system wide policy: "+e, LOG_CRITICAL);
	}	
	URL.setURLStreamHandlerFactory(this);
	loader = new ComponentLoader(this);
	inited = true;
	return;
    }
    

    // start() method inside of platform-specific VM */
    protected int stopJVM(byte caps_raw[]) 
    {
	int c;
	if ((c = vendorExtensions.length()) > 0) return c;
	shouldStop = true;
	/* dummy event to wake up main thread - low priority 
	   to allow pending requests to be completed */
	sendEvent(caps_raw, 0, 0, 0, PJVMEvent.LOW_PRIORITY);
	// let it to be handled, and then allow caller to destroy JVM
	return 0;
    }
    
    protected synchronized int RegisterWindow(int handle, 
					      int width, int height)
    {
	NativeFrame nf;
	// try to find in hash before
	if ((nf = (NativeFrame)handlesFrames.get(new Integer(handle))) != null) 
	  {
	    nf.getFrame().setBounds(0, 0, width, height);
	    return (nf.getID());
	  }	
	nf = SysRegisterWindow(handle, width, height);
	// check if smth failed
	if (nf == null) return 0;
	handlesFrames.put(new Integer(handle), nf);
	return nativeFrames.add(nf);
    }
    
    protected synchronized int UnregisterWindow(int id)
    {
	Frame f = getFrameWithId(id);
	NativeFrame nf = null;
	if (f != null)  nf = SysUnregisterWindow(id);		
	if (nf == null) return 0;
	// remove from ID table
	nativeFrames.remove(id);
	// remove from hash
	Object current;
	for (Enumeration e = handlesFrames.keys(); e.hasMoreElements();) 
	    {
		current = e.nextElement();
		if (((NativeFrame)handlesFrames.get(current)).getFrame() 
		    == nf.getFrame()) 
		    {
	 		handlesFrames.remove(current);
			break;
		    }
	    }
	return nf.getID();
    }

    protected synchronized int RegisterMonitorObject(long handle)
    {
      SynchroObject so;
	// try to find in hash before
      if ((so = (SynchroObject)handlesMonitors.get(new Long(handle))) != null) 
	  {
	    // XXX: maybe adjust state of SO?
	    return (so.getID());
	  }
      so = SysRegisterMonitorObject(handle);
      // check if smth failed
      if (so == null) return 0;
      handlesMonitors.put(new Long(handle), so);
      return nativeMonitors.add(so);
    }
   
    protected synchronized int UnregisterMonitorObject(int id)
    {
      SynchroObject so = SysUnregisterMonitorObject(id);
      if (so == null) return 0;
      // remove from ID table
      nativeMonitors.remove(id);
      // remove from hash
      Object current;
      for (Enumeration e = handlesMonitors.keys(); e.hasMoreElements();) 
	{
	  current = e.nextElement();
	  if (((SynchroObject)handlesMonitors.get(current)) == so) 
	    {
	      handlesMonitors.remove(current);
	      break;
	    }
	}
      return so.getID();
    }

    // those four must be implemented by nonabstract JVM
    abstract protected NativeFrame SysRegisterWindow(int handle, 
						     int width, int height);
    abstract protected NativeFrame SysUnregisterWindow(int id);
    abstract protected SynchroObject SysRegisterMonitorObject(long handle);
    abstract protected SynchroObject SysUnregisterMonitorObject(int id);    
    
    protected int attachThread()
    {
	NativeThread t = new NativeThread(Thread.currentThread());
	int id = nativeThreads.add(t);
	//log("attaching thread: " + t.getThread()+" with ID "+id);
	return id;
    }

    protected int detachThread()
    {
	NativeThread t = new NativeThread(Thread.currentThread());
	t = (NativeThread)nativeThreads.remove(t);
	int id = t.getID();
	//log("detaching thread: " + t + " with id " + id);
	return id;
    }

    protected int registerExtension(String cid, 
				    int version, 
				    String classpath, 
				    String classname,
				    byte caps_raw[], 
				    byte mask_raw[],
				    long data)
    {
	SecurityCaps caps = new SecurityCaps(caps_raw);
	SecurityCaps mask = new SecurityCaps(mask_raw);
	int extID=0;
	try {
	    trace("Registring extension for "+cid
		  +" version="+version+" classpath="+classpath
		  +" classname="+classname, LOG_INFO);
	    WaterfallExtension ext = 
		new WaterfallExtension(this, cid, version, 
				       classpath, classname, data);
	    //securityManager.registerCapsChecker(ext, caps, mask);	    
	    extensions.put(cid, ext);
	    extID = vendorExtensions.add(ext);
	} catch (Exception e)
	    {
		trace("Extension not registred: "+e, LOG_ERROR);
		trace(e, LOG_ERROR);
		return 0;
	    }
	return extID;
    }

    protected synchronized int unregisterExtension(byte raw_caps[], 
						   int id)
    {
	WaterfallExtension ext = 
	    (WaterfallExtension)vendorExtensions.get(id);
	if (ext == null) return 0;
	HostObjectPeerFactory f = ext.getFactory();
	if (f == null) return 0;
	SecurityCaps caps = new SecurityCaps(raw_caps);
	for (Enumeration e=javaPeers.elements(); e.hasMoreElements();) 
	    {
		HostObjectPeer p = (HostObjectPeer)e.nextElement();
		if (p.getFactory() == f)
		    {
			p.destroy(caps);
			javaPeers.remove(p.getID());
		    }
	    }
	f.destroy(caps);
	vendorExtensions.remove(id);
	// XXX: remove from extensions hash
	return 1;
    }

    // post should send message async
    protected int postEventRaw(byte raw_caps[], int target, 
			       int id, long data, int type, int priority)
    {
	SecurityCaps caps = new SecurityCaps(raw_caps);
	PJVMEvent event = new PJVMEvent(target, id, data, false, caps,
					priority, type);
	synchronized(this) 
	    {
		event_queue.enqueue(event);
		notifyAll();
	    }
	return 1;	
    }
    
    // send should wait completion of request
    // maybe add timeout
    protected int sendEventRaw(byte raw_caps[], int target, 
			       int id, long data, int type, int priority)
    {
	SecurityCaps caps = new SecurityCaps(raw_caps);
	PJVMEvent event = new PJVMEvent(target, id, data, true, caps,
					priority, type);
	synchronized(this) 
	    {
		event_queue.enqueue(event);
		notifyAll();
	    }
	synchronized (event)
	    {
		// maybe this event already proceed - 
		// we'll get deadlock if not check
		if (!event.m_completed)
		    { 
			try {
			    event.wait();
			}
			catch (InterruptedException e) {
			    return 0;
			}
		    }
	    }
	return event.m_result;
    }
    
    protected int handleCall(byte raw_caps[], 
			     int type, int target, 
			     int arg1, long arg2)
    {
	SecurityCaps caps = new SecurityCaps(raw_caps);       
	switch (type)
	    {
	    case 1:
		// peer call
		HostObjectPeer p = getPeerWithId(target);
		if (p == null) {
		  trace("null target "+target+" for direct call", LOG_ERROR);
		  return 0;
		}
		trace("peer call "+p+" "+arg1+" "+arg2, LOG_DEBUG);
		return p.handleCall(caps, arg1, arg2);		
	    case 2:
		// extension call
		HostObjectPeerFactory f = getFactoryWithId(target);
		if (f == null) return 0;
		return f.handleCall(caps, arg1, arg2);
	    case 3:
		// JVM call
		return sysCall(caps, arg1, arg2);
	    }
	return 0;
    }
    
    protected int postEvent(byte raw_caps[], int target, 
			    int id, long data, int priority)
    {
	return postEventRaw(raw_caps, target, id, 
			    data, PJVMEvent.PEER_EVENT, priority);
    }
    
    protected int sendEvent(byte raw_caps[], int target, 
			    int id, long data, int priority)
    {
	return sendEventRaw(raw_caps, target, id, 
			    data, PJVMEvent.PEER_EVENT, priority);
    }
    protected int postExtensionEvent(byte raw_caps[], int target, 
				     int id, long data, int priority)
    {
	return postEventRaw(raw_caps, target, id, 
			    data, PJVMEvent.EXT_EVENT, priority);
    }
    
    protected int sendExtensionEvent(byte raw_caps[], int target,
				     int id, long data, int priority)
    {
	return sendEventRaw(raw_caps, target, id, 
			    data, PJVMEvent.EXT_EVENT, priority);
    }
    protected int postSysEvent(byte raw_caps[], int id, 
			       long data, int priority)
    {
	return postEventRaw(raw_caps, 0, id, 
			    data, PJVMEvent.SYS_EVENT, priority);
    }
    
    protected int sendSysEvent(byte raw_caps[], int id, 
			       long data, int priority)
    {
	return sendEventRaw(raw_caps, 0, id, 
			    data, PJVMEvent.SYS_EVENT, priority);
    }
    protected int enableCapabilities(byte raw_caps[], byte principals[][])
    {
	// XXX: implement ME!!!
	SecurityCaps caps = new SecurityCaps(raw_caps);
	if (caps.isPureSystem())
	    {
		String str = new String(principals[0]);
		if ("CAPS".equals(str)) return 1;
		return 0;
	    }
	if (caps.isPureUser())
	    {
		// ask extension deciders
	    }
	return 0;
    }

    protected int disableCapabilities(byte caps[])
    {
	trace("disableCapabilities()", LOG_DEBUG);
	return 1;
    }

    

    /* PRIVATE AREA */
    protected void loadLibrary() 
    {
	String libname = "JVMPnatives";
	try 
	    {
		System.loadLibrary(libname);
		initedNative = true;
	    } 
	catch (UnsatisfiedLinkError ex) 
	    {
		trace("Plugin could not load DLL: " + libname, LOG_CRITICAL);
		trace("Path is:" +  System.getProperty("java.library.path"), LOG_ERROR);
		trace(ex.toString(), LOG_ERROR);	    
	    }; 	
    };
    
    protected PJVMEvent getNextEvent() throws InterruptedException 
    {
        while (event_queue.isEmpty()) 
	    {
		synchronized(this)
		    {
			wait();
		    }
	    }
        return (PJVMEvent)event_queue.dequeue();
    }

    
    private void runMainLoop() 
    {
	Thread curThread = Thread.currentThread();
	while (!shouldStop && !curThread.isInterrupted()) {
            PJVMEvent evt;
            try {
                evt = getNextEvent();
            } catch (InterruptedException e) {
                shouldStop = true;
		continue;
	    }
	    switch(evt.getType())
		{
		case PJVMEvent.PEER_EVENT:
		    HostObjectPeer peer = getPeerWithId(evt.getTarget());
		    if (peer == null) 
			{ 
			  // not show this message on JVM shutdown
			  if (!shouldStop)
			    {
			      trace("target peer " + evt.getTarget() 
				    + " is invalid now", LOG_WARNING);
			    }
			  evt.m_completed = true;
			  break;
			}
		    try {
			evt.m_result = peer.handleEvent(evt.getCaps(), 
							evt.getID(), 
							evt.getData());
			evt.m_completed = true;
		    } catch (Throwable e) {
			evt.setException(e);
			evt.m_completed = true;
			trace("Exception in peer event handling: "+e,
			      LOG_ERROR);
			trace(e, LOG_ERROR);
			// even if exception - notify sender
		    } 
		    break;
		case  PJVMEvent.EXT_EVENT:
		    HostObjectPeerFactory ext = 
			getFactoryWithId(evt.getTarget());
		    if (ext == null) 
			{ 
			    trace("target extension " + evt.getTarget() 
				  + " is invalid now", LOG_ERROR);
			    evt.m_completed = true;
			    break;
			}
		    try {
			evt.m_result = ext.handleEvent(evt.getCaps(), 
						       evt.getID(), 
						       evt.getData());
			evt.m_completed = true;
		    } catch (Throwable e) 
			{
			    evt.setException(e);
			    evt.m_completed = true;
			    trace("Exception in peer event handling: "+e, 
				  LOG_ERROR);
			    trace(e, LOG_ERROR);
			    // even if exception - notify sender
			}
		    break;
		case  PJVMEvent.SYS_EVENT:
		  try {
		    evt.m_result = sysEvent(evt.getCaps(), 
					    evt.getID(), evt.getData());
		    evt.m_completed = true;
		  } catch (Throwable e) {
		      evt.setException(e);
		      evt.m_completed = true;
		      trace("Exception in sysevent handling: "+e, LOG_ERROR);
		      trace(e, LOG_ERROR);
		      // even if exception - notify sender
		    }
		  break;
		default:		    
		    trace("Unknown event type: "+evt.getType(), LOG_ERROR);
		    break;
		}	    
	    if (evt.isSync())
		{
		    synchronized (evt) 
			{
			    evt.notifyAll();
			}
		}
	}
	trace("Main thread of plugin completed", LOG_WARNING);
	running = false;
    };
    
    protected int sysCall(SecurityCaps caps, int arg1, long arg2)
    {
	return 0;
    }

    protected int sysEvent(SecurityCaps caps, int id, long data)
    {
      switch (id)
	  {
	  case CMD_CTL_CONSOLE:
	      //trace("CMD_CTL_CONSOLE: "+data, LOG_DEBUG);
	      switch ((int)data)
		  {
		  case 0:
		      if (console.isVisible()) console.setVisible(false);
		      return 1;
		  case 1:
		      if (!console.isVisible()) console.setVisible(true);
		      return 1;
		  case 2:
		      return (console.isVisible() ? 2 : 1);
		  }
	      break;
	  default:	      
	      trace ("Unknown sysevent: id="+id+" data="+data, LOG_WARNING);
	      return 0;
	  };
      // for stupid compilers :)
      return 0;
    }  

    protected int createPeer(byte raw_caps[], String cid, int version)
    {
	HostObjectPeer peer = null;
	int id = 0;
	// I'd like to optimize it later, checking if caps changed, and
	// create new SecurityCaps object only if caps changed from previous 
	// call.
	SecurityCaps caps = new SecurityCaps(raw_caps);
	Vector v = extensions.getVector(cid);
	if (v != null)
	  {
	    WaterfallExtension we = null;
	    for (Enumeration e = v.elements(); e.hasMoreElements();)
	      {
		// XXX: do smth more meaningful here
		we = (WaterfallExtension)e.nextElement();
	      }
	    if (we != null) peer = we.instantiate(caps, version);
	  }
	else
	  {
	    trace("Extension with cid  "+cid+" not registred", LOG_ERROR);
	  }
	synchronized(this)
	    {
		if (peer != null) id = javaPeers.add(peer);
		return id;
	    }
    }
    
    protected int destroyPeer(byte raw_caps[], int id)
    {
	SecurityCaps caps = new SecurityCaps(raw_caps);
	HostObjectPeer peer = getPeerWithId(id);
	if (peer == null)  return 0;
	if (peer.destroy(caps) != 0) removePeer(id);
	return id;
    }

    protected int removePeer(int id)
    {
	// not sure if this additional lookup is needed
	//HostObjectPeer peer = getPeerWithId(id);
	//trace("unregistering " + id + " peer = " + peer);
	//if (peer == null) return 0;
	javaPeers.remove(id);
	return id;
    }
    void setDebugLevel(int debug)
    {
	if (debug > LOG_DEBUG)    debug = LOG_DEBUG;
	if (debug < LOG_CRITICAL) debug = LOG_CRITICAL;
	debugLevel = debug;
    }

    URLStreamHandler tryToGetHandler(ClassLoader loader, 
				     String      pkgs, 
				     String      proto)
    {
	// XXX: implement me to support several packages
	try {
	    Class handlerClass = Class.forName(pkgs+"."+proto+".Handler",
					       true,
					       loader);	    
	    if (URLStreamHandler.class.isAssignableFrom(handlerClass)) 
		{
		    Constructor c = 
			handlerClass.getDeclaredConstructor(new Class[]{});
		    return (URLStreamHandler)c.newInstance(new Object[]{});
	      }
	  } catch (Exception e) {
	      // do nothing here - nothing special happened
	      //e.printStackTrace();	    
	  }
	return null;
    }
};

class ProtoHandlerDesc
{
    ClassLoader loader;
    String      pkgs;
    
    public ProtoHandlerDesc(ClassLoader loader, String pkgs)
    {
	this.loader = loader;
	this.pkgs = pkgs;
    }
};

class PJVMEvent 
{
    public static final int PEER_EVENT = 1;
    public static final int EXT_EVENT = 2;
    public static final int SYS_EVENT = 3;
    public static final int HIGHEST_PRIORITY = 100;
    public static final int HIGH_PRIORITY = 80;
    public static final int NORMAL_PRIORITY = 50;
    public static final int LOW_PRIORITY = 20;
    public static final int LOWEST_PRIORITY = 1;   
    public    boolean m_completed = false;
    protected int  m_target;
    protected int  m_id;
    protected long m_data;
    protected boolean m_sync;
    protected SecurityCaps m_caps;
    protected int  m_priority;
    protected int  m_type;
    protected Throwable m_exception;
    public    int m_result = 0;
    PJVMEvent(int target, int id, long data, boolean sync, SecurityCaps caps,
	      int priority, int type)
    {
	m_target = target;
	m_id = id; 
	m_data = data;
	m_sync = sync;
	m_caps = caps;
	m_type = type;
	m_exception = null;
	m_priority = priority;
    }
    
    public int getTarget()
    {
	return m_target;
    }    
    
    public int getID()
    {
	return m_id;
    }
    
    public int getType()
    {
	return m_type;
    }    
    
    public long getData()
    {
	return m_data;
    }
    public boolean isSync()
    {
	return m_sync;
    }
    public SecurityCaps getCaps()
    {
	return m_caps;
    }
    public Throwable getException()
    {
	return m_exception;
    }
    public void setException(Throwable e)
    {
	m_exception = e;
    }
    public int getPriority()
    {
      return m_priority;
    }
}





