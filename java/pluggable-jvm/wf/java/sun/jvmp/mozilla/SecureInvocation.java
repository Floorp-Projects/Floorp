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
 * $Id: SecureInvocation.java,v 1.1 2001/07/18 20:48:17 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;

import java.security.*; 
import java.lang.reflect.*;
import sun.jvmp.PluggableJVM;

public class SecureInvocation {
 
    public static Object ConstructObject(final Constructor constructor,
                                         final Object[]    args,
                                         long              handle) throws Exception
    {
	ProtectionDomain[] d = new ProtectionDomain[1]; 
	d[0] = new JavaScriptProtectionDomain(handle);
	AccessControlContext context = new AccessControlContext(d);
	try {
            // Perform the object constructor.
            return AccessController.doPrivileged(new PrivilegedExceptionAction() {
		    public Object run() throws Exception
		    {
			return constructor.newInstance(args);
		    }
		}); 
	} catch (PrivilegedActionException e) {
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	    throw e;
	}
	// never be here 
    }

    public static Object CallMethod(final Object   obj,
                                    final Method   method,
                                    final Object[] args,
                                    long           handle) throws Exception
    {
	ProtectionDomain[] d = new ProtectionDomain[1]; 
	d[0] = new JavaScriptProtectionDomain(handle);
	AccessControlContext context = new AccessControlContext(d);
	try {
            // Perform the object constructor.
            return AccessController.doPrivileged(new PrivilegedExceptionAction() {
		    public Object run() throws Exception
		    {
			return method.invoke(obj, args);
		    }
		}); 
	} catch (PrivilegedActionException e) {
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	    throw e;
	}
	// never be here 
    }

    public static Object GetField(final Object obj,
                                  final Field  field,
                                  long         handle) throws Exception
    {
	ProtectionDomain[] d = new ProtectionDomain[1]; 
	d[0] = new JavaScriptProtectionDomain(handle);
	AccessControlContext context = new AccessControlContext(d);
	try {
            // Perform the object constructor.
            return AccessController.doPrivileged(new PrivilegedExceptionAction() {
		    public Object run() throws Exception
		    {
			return field.get(obj);
		    }
		}); 
	} catch (PrivilegedActionException e) {
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	    throw e;
	}
	// never be here 
    }

    public static void SetField(final Object  obj,
                                final Field   field,
                                final Object  val,
                                long          handle) throws Exception
    {
	ProtectionDomain[] d = new ProtectionDomain[1]; 
	d[0] = new JavaScriptProtectionDomain(handle);
	AccessControlContext context = new AccessControlContext(d);
	try {
            // Perform the object constructor.
            AccessController.doPrivileged(new PrivilegedExceptionAction() {
		    public Object run() throws Exception
		    {
			field.set(obj, val);
			return null;
		    }
		}); 
	} catch (PrivilegedActionException e) {
	    PluggableJVM.trace(e, PluggableJVM.LOG_WARNING);
	    throw e;
	}
	// never be here 
    }
}




