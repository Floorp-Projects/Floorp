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
 * $Id: NetscapePeerFactory.java,v 1.2 2001/07/12 19:58:01 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.netscape4;

import sun.jvmp.*;
import sun.jvmp.security.*;
import java.security.*;

public class NetscapePeerFactory implements HostObjectPeerFactory,
					    AccessControlDecider
{
    protected final String cid = "@sun.com/wf/netscape4/appletpeer;1";
    protected final int version  = 1;
    protected int m_id;
    PluggableJVM  m_jvm;

    protected NetscapePeerFactory(PluggableJVM jvm) 
	throws Exception
    {
	m_jvm = jvm;
	loadLibrary();	
    }

    public static HostObjectPeerFactory start(PluggableJVM jvm)
    {
	NetscapePeerFactory factory = null;
	try 
	    {
		factory = new NetscapePeerFactory(jvm);
	    } 
	catch (Exception e)
	    {
		jvm.trace("NETSCAPE 4 FACTORY NOT CREATED", 
			  PluggableJVM.LOG_ERROR);
		return null;
	    }
	return factory;
    }
    public String getCID()
    {
	return cid;
    }
    public HostObjectPeer create(int version)
    {
	return new MozillaHostObjectPeer(this, version);
    }
    
    public int handleEvent(SecurityCaps caps, 
			   int eventID,
			   long eventData)
    {
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
	Permissions perms = new Permissions();
	perms.add(new RuntimePermission("accessClassInPackage.sun.jvmp.netscape4"));
	return perms;
    }

    public int  decide(CallingContext ctx, String principal, int cap_no)
    {
	return NA;
    }
    
    public boolean belongs(int cap_no)
    {
	return false;
    }       

    private void loadLibrary() throws UnsatisfiedLinkError
    {
	String libname = "wf_netscape4";
	try 
	    {
		System.loadLibrary(libname);
	  } 
	catch (UnsatisfiedLinkError ex) 
	    {
		m_jvm.trace("System could not load DLL: " + libname,
			    PluggableJVM.LOG_ERROR);
		m_jvm.trace("Path is:" + 
			    System.getProperty("java.library.path"),
			    PluggableJVM.LOG_WARNING);
		m_jvm.trace(ex.toString(),
			    PluggableJVM.LOG_WARNING);
		throw ex;
	    };
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
}






