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
 * $Id: TestPeerFactory.java,v 1.2 2001/07/12 19:58:04 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.test;

import java.awt.*;
import java.security.*;
import sun.jvmp.*;
import sun.jvmp.security.*;

class TestObjectPeer implements HostObjectPeer
{
  private int  m_id = 0;
  private long m_data = 0;
  private HostObjectPeer m_realPeer = null;
  TestPeerFactory m_factory = null;
  SynchroObject so = null;

  public TestObjectPeer(TestPeerFactory factory, int version)
  {
      m_factory = factory;
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

  public HostObjectPeerFactory getFactory()
  {
      return m_factory;
  }

  protected void performTest(int id) 
    {
	Frame frame = null;
	MyActionListener l1 = new MyActionListener("First button");
	MyActionListener l2 = new MyActionListener("Second button");    
	frame  = m_factory.m_jvm.getFrameWithId(id);
	if (frame == null) return;
	frame.setLayout(new FlowLayout(FlowLayout.CENTER));
	Button b1 = new Button("Test 1");
	Button b2 = new Button("Test 2");
	b1.setBounds(0, 0, 40, 40);
	b2.setBounds(50, 50, 40, 40);
	b1.addActionListener(l1);
	b2.addActionListener(l2);
	frame.add(b1);
	frame.add(b2);
	frame.show();
    }
  protected void performThreadTest(int id) 
    {
	so = m_factory.m_jvm.getSynchroObjectWithID(id);
	if (so == null) { 
	    m_factory.m_jvm.trace("no such synchro object: "+id,  
		      PluggableJVM.LOG_WARNING);
	    return;
	}
	Thread t = new Thread(new Runnable()
	    {
		public void run() 
		{
		    int millis = 7000;
		    PluggableJVM jvm = m_factory.m_jvm;
		    jvm.trace("New thread started. Trying to get lock.",
			      PluggableJVM.LOG_INFO);
		    if (millis > 0)
			{
			    so._lock();
			    jvm.trace("Lock acquired, sleeping "+millis+" millis...",
				       PluggableJVM.LOG_INFO);
			    try {
				Thread.sleep(millis);
			    } catch (InterruptedException e) {}
			    jvm.trace("notifying sleeping thread",
				      PluggableJVM.LOG_INFO);
			    so._notifyAll();
			    so._unlock();
			    jvm.trace("Unlock and done",
				      PluggableJVM.LOG_INFO);
			}
		}
	    });
	t.start();
    }

  public int  handleEvent(SecurityCaps caps, int eventID, long eventData) 
  {
      int retval = 0;
      m_factory.m_jvm.trace("TestObjectPeer.handleEvent: "+eventID,
			     PluggableJVM.LOG_DEBUG);
      switch (eventID) 
	  {
	  case PE_CREATE:
	      retval = 1;
	      break;	      
	  case 20:
	      performTest((int)eventData);
	      retval = 1;
	      break;
	  case 666:
	      performThreadTest((int)eventData);
	      retval = 1;
	      break;
	  default:
	      retval = 0;
	      break;
	  }
      return retval;
  }
  
  public int  handleCall(SecurityCaps caps, int arg1, long arg2) 
  {
      return 1;
  }
    
  public int destroy(SecurityCaps caps)
  {
      return 1;
  }
};

public class TestPeerFactory implements HostObjectPeerFactory,
					AccessControlDecider
{
    protected final String cid = "@sun.com/wf/tests/testextension;1";
    protected final int version  = 1;
    protected PluggableJVM m_jvm = null;
    protected int m_id;
    protected long m_params = 0;
    
    protected TestPeerFactory(PluggableJVM jvm, long data) 
	throws Exception
    {
	m_jvm = jvm;
	m_params = data;
    }

    public static HostObjectPeerFactory start(PluggableJVM jvm, Long data)
    {
	TestPeerFactory factory = null;
	try 
	    {
		factory = new TestPeerFactory(jvm, data.longValue());
	    } 
	catch (Exception e)
	    {
		jvm.trace("FACTORY NOT CREATED",  PluggableJVM.LOG_ERROR);
		return null;
	    }
	factory.m_jvm = jvm;
	return factory;
    }

    public String getCID()
    {
	return cid;
    }

    public HostObjectPeer create(int version)
    {
	return new TestObjectPeer(this, version);
    }
    
    public int handleEvent(SecurityCaps caps, 
			   int eventID,
			   long eventData)
    {
	m_jvm.trace("Event "+eventID+" come to extension "+m_id, 
		     PluggableJVM.LOG_DEBUG);
	return 1;
    }

    public int  handleCall(SecurityCaps caps, int arg1, long arg2) 
    {
	return 1;
    }
    
    public int destroy(SecurityCaps caps)
    {
	return 1;
    }
 
    public PermissionCollection getPermissions(CodeSource codesource)
    {
	return null;
    }
    
    public int  decide(CallingContext ctx, String principal, int cap_no)
    {
	return NA;
    }
    
    public boolean belongs(int cap_no)
    {
	return false;
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
};












