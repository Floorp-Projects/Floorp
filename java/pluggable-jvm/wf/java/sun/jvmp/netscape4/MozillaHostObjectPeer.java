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
 * $Id: MozillaHostObjectPeer.java,v 1.2 2001/07/12 19:58:01 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.netscape4;

import sun.jvmp.*;
import sun.jvmp.security.*;

public class MozillaHostObjectPeer implements HostObjectPeer
{
  private int  m_id = 0;
  private long m_data = 0;
  private HostObjectPeer m_realPeer = null;
  NetscapePeerFactory m_factory;

  public MozillaHostObjectPeer(NetscapePeerFactory factory,
			       int version)
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
    

  public int  handleEvent(SecurityCaps caps, int eventID, long eventData) 
  {
      int retval = 0;
      //Plugin.trace("MozillaHostObjectPeer.handleEvent: "+eventID);
      // forward functionality to the real peer
      if (m_realPeer != null) 
	  return m_realPeer.handleEvent(caps, eventID, eventData);
      switch (eventID) 
	  {
	  case PE_CREATE:
	      retval = 1;
	      break;	      
	  case  PE_SETTYPE:
	      switch((int)eventData) 
		  {
		  case PT_APPLET:
		  case PT_OBJECT:
		      m_realPeer = new MozillaAppletPeer(this, m_id);
		      // maybe send CREATE event from here?
		      retval = 1;
		      break;
		  default:
		      m_factory.m_jvm.trace("Unknown tag type",  
					    PluggableJVM.LOG_WARNING);
		      break;
		  }
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
	if (m_realPeer != null) 
	    return m_realPeer.destroy(caps);
	return 0;
    }

    public HostObjectPeerFactory getFactory()
    {
	return m_factory;
    }
};


