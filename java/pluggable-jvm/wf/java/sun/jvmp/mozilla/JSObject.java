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
 * $Id: JSObject.java,v 1.1 2001/07/18 20:48:17 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;
 
import sun.jvmp.PluggableJVM;
import sun.jvmp.applet.*;
import java.security.*;
import netscape.javascript.JSException;
import java.net.URL;

public class JSObject extends netscape.javascript.JSObject {
 
    private int  nativeJSObject = 0;
    private int  jsThreadID = 0;
    private long m_params = 0;
    /* this field used to perform static calls - it is inited from constructor
       of MozillaPeerFactory  with pointer to BrowserSupportWrapper */
    static  long m_evaluator = 0;
    
    // just for reflection
    JSObject(Long params)  {
        this(params.longValue());
    }
 
    JSObject(long params)  {
	this(JSGetThreadID(params), JSGetNativeJSObject(params));
	m_params = params;
    }
 
    JSObject(int jsThreadID, int nativeJSObject)  {
        this.jsThreadID = jsThreadID;
        this.nativeJSObject = nativeJSObject;
    }
    
     
    public Object call(String methodName,
                       Object args[])
        throws JSException
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();

	return JSObjectCall(jsThreadID,
                            nativeJSObject,
                            m_params,
                            ctx.getURL(),
                            ctx.getCertChain(),
                            ctx.getCertLength(),
                            ctx.getNumOfCert(),
                            methodName, args,
                            ctx.getAccessControlContext());
    }
    
    public Object eval(String s) throws JSException
    {
        WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();

        return JSObjectEval(jsThreadID,
                            nativeJSObject,
                            m_params,
                            ctx.getURL(),
                            ctx.getCertChain(),
                            ctx.getCertLength(),
                            ctx.getNumOfCert(),
                            s,
                            ctx.getAccessControlContext());
    }

    public Object getMember(String name) throws JSException
    {
        WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
 
        return JSObjectGetMember(jsThreadID,
                                 nativeJSObject,
                                 m_params,
                                 ctx.getURL(),
                                 ctx.getCertChain(),
                                 ctx.getCertLength(),
                                 ctx.getNumOfCert(),
                                 name,
                                 ctx.getAccessControlContext());
    }

    public void setMember(String name, Object value) throws JSException
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
	
        JSObjectSetMember(jsThreadID,
                          nativeJSObject,
                          m_params,
                          ctx.getURL(),
                          ctx.getCertChain(),
                          ctx.getCertLength(),
                          ctx.getNumOfCert(),
                          name,
                          value,
                          ctx.getAccessControlContext());
    }

    public void removeMember(String name) throws JSException
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
	
        JSObjectRemoveMember(jsThreadID,
                             nativeJSObject,
                             m_params,
                             ctx.getURL(),
                             ctx.getCertChain(),
                             ctx.getCertLength(),
                             ctx.getNumOfCert(),
                             name,
                             ctx.getAccessControlContext());
    }


    public Object getSlot(int index) throws JSException
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
 
        return JSObjectGetSlot(jsThreadID,
                               nativeJSObject,
                               m_params,
                               ctx.getURL(),
                               ctx.getCertChain(),
                               ctx.getCertLength(),
                               ctx.getNumOfCert(),
                               index,
                               ctx.getAccessControlContext());
    }


    public void setSlot(int index, Object value) throws JSException
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
 
        JSObjectSetSlot(jsThreadID,
                        nativeJSObject,
                        m_params,
                        ctx.getURL(),
                        ctx.getCertChain(),
                        ctx.getCertLength(),
                        ctx.getNumOfCert(),
                        index,
                        value,
                        ctx.getAccessControlContext());
    }

    public String toString()
    {
        return JSObjectToString(jsThreadID, nativeJSObject, m_params);
    }
 
    public void finalize()
        throws Throwable
    {
        JSFinalize(jsThreadID, nativeJSObject, m_params);
        super.finalize();
    }

    private static int JSGetNativeJSObject(long params)
    {
	WFSecurityContext ctx = WFSecurityContext.getCurrentSecurityContext();
	
        return JSGetNativeJSObject(params,
                                   ctx.getURL(),
                                   ctx.getCertChain(),
                                   ctx.getCertLength(),
                                   ctx.getNumOfCert(),
                                   ctx.getAccessControlContext());
    }

    private static native int JSGetNativeJSObject(long params,
                                                  String url,
                                                  byte[][] chain,
                                                  int[] certLength,
                                                  int numOfCerts,
                                                  AccessControlContext ctx);
 
    private static native int JSGetThreadID(long params);
 
    private static native void JSFinalize(int jsThreadID,
                                          int nativeJSObject,
                                          long params);

    private static native Object JSObjectCall(int jsThreadID,
                                              int nativeJSObject,
                                              long params,
                                              String url,
                                              byte[][] chain,
                                              int[] certLength, int numOfCerts,
                                              String methodName,
                                              Object args[],
                                              AccessControlContext ctx)
        throws JSException;
 
    private static native Object JSObjectEval(int jsThreadID,
                                              int nativeJSObject,
                                              long params,
                                              String url,
                                              byte[][] chain,
                                              int[] certLength,
                                              int numOfCerts,
                                              String script,
                                              AccessControlContext ctx)
        throws JSException;

     private static native Object JSObjectGetMember(int jsThreadID,
                                                   int nativeJSObject,
                                                   long params,
                                                   String url,
                                                   byte[][] chain,
                                                   int[] certLength,
                                                   int numOfCerts,
                                                   String name,
                                                   AccessControlContext ctx)
        throws JSException;
 
    private static native void JSObjectSetMember(int jsThreadID,
                                                 int nativeJSObject,
                                                 long params,
                                                 String url,
                                                 byte[][] chain,
                                                 int[] certLength,
                                                 int numOfCerts,
                                                 String name,
                                                 Object value,
                                                 AccessControlContext ctx)
        throws JSException;

     private static native void JSObjectRemoveMember(int jsThreadID,
                                                    int nativeJSObject,
                                                    long params,
                                                    String url,
                                                    byte[][] chain,
                                                    int[] certLength,
                                                    int numOfCerts,
                                                    String name,
                                                    AccessControlContext ctx)
        throws JSException;
 
    private static native Object JSObjectGetSlot(int jsThreadID,
                                                 int nativeJSObject,
                                                 long params,
                                                 String url,
                                                 byte[][] chain,
                                                 int[] certLength,
                                                 int numOfCerts,
                                                 int index,
                                                 AccessControlContext ctx)
        throws JSException;

    private static native void JSObjectSetSlot(int jsThreadID,
                                               int nativeJSObject,
                                               long params,
                                               String url,
                                               byte[][] chain,
                                               int[] certLength,
                                               int numOfCerts,
                                               int index,
                                               Object value,
                                               AccessControlContext ctx)
        throws JSException;
 
    private static native String JSObjectToString(int jsThreadID,
                                                  int nativeJSObject,
                                                  long params);
}

class WFSecurityContext
{
    private ProtectionDomain domain;
    private AccessControlContext ctx;
 
    WFSecurityContext(ProtectionDomain domain, AccessControlContext ctx)
    {
        this.domain = domain;
        this.ctx = ctx;
    }
 
    String getURL()
    {
        if (domain != null)
        {
            CodeSource src = domain.getCodeSource();
            URL u = src.getLocation();
            if (u != null) return u.toString();
	}
	
	return null;
    }
    
    byte[][] getCertChain()
    {
        return null;
    }
 
    int[] getCertLength()
    {
        return null;
    }
 
    int getNumOfCert()
    {
        return 0;
    }
 
    AccessControlContext getAccessControlContext()
    {
        return ctx;
    }

    static WFSecurityContext getCurrentSecurityContext()
    {
        final AccessControlContext ctx = AccessController.getContext();
	
        try {
	    return (WFSecurityContext)
		AccessController.doPrivileged(new PrivilegedExceptionAction() {
                             public Object run() throws PrivilegedActionException
				{
				    SecurityManager sm = System.getSecurityManager();
				    // do anything only if installed SM is subclass 
				    // of our applet security manager
				    if (sm != null && sm instanceof sun.jvmp.applet.WFAppletSecurityManager)
					{
					    WFAppletSecurityManager m = (WFAppletSecurityManager)sm;
					    Class[] stack = m.getExecutionStackContext();
					    for (int i=0; i < stack.length; i++)
						{
						    ClassLoader loader = stack[i].getClassLoader();
						    if (loader !=null && 
							loader instanceof sun.jvmp.applet.WFAppletClassLoader)
							return new WFSecurityContext(stack[i].getProtectionDomain(), 
										     ctx);
						}
					}
				    return new WFSecurityContext(null, ctx);
				}
			    });
	    
	} catch (PrivilegedActionException e) {
            PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
            return new WFSecurityContext(null, ctx);
        }
    }

}




