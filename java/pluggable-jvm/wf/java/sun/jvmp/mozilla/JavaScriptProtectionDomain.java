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
 * $Id: JavaScriptProtectionDomain.java,v 1.1 2001/07/18 20:48:17 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.mozilla;

import java.security.cert.Certificate;
import java.security.*;
import java.net.URL;

public class JavaScriptProtectionDomain extends ProtectionDomain 
{ 
    // just pointer to native JS security context wrapper
    long securityContext = 0;
    
    public JavaScriptProtectionDomain(long securityContext)
    {
        super(getCodeSource(securityContext), null);
        this.securityContext = securityContext;
    }
    
    public boolean implies(Permission permission) 
    {
        if (securityContext == 0) return false;
        
        if (permission instanceof JSPermission)
            return implies(securityContext,
                           permission.getActions(),
                           null);
        else
            return implies(securityContext,
                           JSPermission.AllJavaPermission,
                           null);
    }
    
    protected static CodeSource getCodeSource(long ctx)
    {
        URL u;
        try {
            u = new URL(getCodeBase(ctx));
        } catch (Exception e) {
            u = null;
            //System.err.println("cannot create JS code source: "+e);
        }
        return new CodeSource(u, getCerts(ctx));
    }
    
    private static Certificate[] getCerts(long ctx)
    {
        return null;  // for now
    }
    
    public void finalize() throws Throwable
    {
      // release proper native object 
        if (securityContext != 0) finalize(securityContext);
    }
    
    
    private native static String   getCodeBase(long ctx);
    private native static byte[][] getRawCerts(long ctx);
    private native boolean implies(long   securityContext,
                                   String target,
                                   String action);
    private native void finalize(long securityContext);
}








