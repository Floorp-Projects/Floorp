/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.tools.shell;

import java.security.*;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Hashtable;
import java.util.Enumeration;

import org.mozilla.javascript.*;
import org.mozilla.classfile.*;

public class JavaPolicySecurity extends SecurityProxy
{

    private static class Loader extends ClassLoader
        implements GeneratedClassLoader
    {
        private ProtectionDomain domain;

        Loader(ClassLoader parent, ProtectionDomain domain) {
            super(parent != null ? parent : getSystemClassLoader());
            this.domain = domain;
        }

        public Class defineClass(String name, byte[] data) {
            return super.defineClass(name, data, 0, data.length, domain);
        }

        public void linkClass(Class cl) {
            resolveClass(cl);
        }
    }

    private static class ContextPermissions extends PermissionCollection
    {
// Construct PermissionCollection that permits an action only
// if it is permitted by staticDomain and by security context of Java stack on
// the moment of constructor invocation
        ContextPermissions(ProtectionDomain staticDomain) {
            _context = AccessController.getContext();
            if (staticDomain != null) {
                _statisPermissions = staticDomain.getPermissions();
            }
            setReadOnly();
        }

        public void add(Permission permission) {
            throw new RuntimeException("NOT IMPLEMENTED");
        }

        public boolean implies(Permission permission) {
            if (_statisPermissions != null) {
                if (!_statisPermissions.implies(permission)) {
                    return false;
                }
            }
            try {
                _context.checkPermission(permission);
                return true;
            }catch (AccessControlException ex) {
                return false;
            }
        }

        public Enumeration elements()
        {
            return new Enumeration() {
                public boolean hasMoreElements() { return false; }
                public Object nextElement() { return null; }
            };
        }

        public String toString() {
            StringBuffer sb = new StringBuffer();
            sb.append(getClass().getName());
            sb.append('@');
            sb.append(Integer.toHexString(System.identityHashCode(this)));
            sb.append(" (context=");
            sb.append(_context);
            sb.append(", static_permitions=");
            sb.append(_statisPermissions);
            sb.append(')');
            return sb.toString();
        }

        AccessControlContext _context;
        PermissionCollection _statisPermissions;
    }

    public JavaPolicySecurity() {
        // To trigger error on jdk-1.1 with lazy load
        new CodeSource(null, null);
    }

    protected void callProcessFileSecure(final Context cx,
                                         final Scriptable scope,
                                         final String filename)
    {
        AccessController.doPrivileged(new PrivilegedAction() {
            public Object run() {
                URL url = getUrlObj(filename);
                ProtectionDomain staticDomain = getUrlDomain(url);
                Main.processFileSecure(cx, scope, url.toExternalForm(),
                                       staticDomain);
                return null;
            }
        });
    }

    private URL getUrlObj(String url) {
        URL urlObj;
        try {
            urlObj = new URL(url);
        } catch (MalformedURLException ex) {
            // Assume as Main.processFileSecure it is file, need to build its
            // URL
            String curDir = System.getProperty("user.dir");
            curDir = curDir.replace('\\', '/');
            if (!curDir.endsWith("/")) {
                curDir = curDir+'/';
            }
            try {
                URL curDirURL = new URL("file:"+curDir);
                urlObj = new URL(curDirURL, url);
            } catch (MalformedURLException ex2) {
                throw new RuntimeException
                    ("Can not construct file URL for '"+url+"':"
                     +ex2.getMessage());
            }
        }
        return urlObj;
    }

    private ProtectionDomain getUrlDomain(URL url) {
        CodeSource cs = new CodeSource(url, null);
        PermissionCollection pc = Policy.getPolicy().getPermissions(cs);
        return new ProtectionDomain(cs, pc);
    }

    public GeneratedClassLoader
    createClassLoader(ClassLoader parentLoader, Object securityDomain) {
        ProtectionDomain domain = (ProtectionDomain)securityDomain;
        return new Loader(parentLoader, domain);
    }

    public Object getDynamicSecurityDomain(Object securityDomain)
    {
        ProtectionDomain staticDomain = (ProtectionDomain)securityDomain;
        return getDynamicDomain(staticDomain);
    }

    private ProtectionDomain getDynamicDomain(ProtectionDomain staticDomain) {
        ContextPermissions p = new ContextPermissions(staticDomain);
        ProtectionDomain contextDomain = new ProtectionDomain(null, p);
        return contextDomain;
    }

    public Object callWithDomain(Object securityDomain,
                                 final Context cx,
                                 final Callable callable,
                                 final Scriptable scope,
                                 final Scriptable thisObj,
                                 final Object[] args)
    {
        ProtectionDomain staticDomain = (ProtectionDomain)securityDomain;
        // There is no direct way in Java to intersect permitions according
        // stack context with additional domain.
        // The following implementation first constructs ProtectionDomain
        // that allows actions only allowed by both staticDomain and current
        // stack context, and then constructs AccessController for this dynamic
        // domain.
        // If this is too slow, alternative solution would be to generate
        // class per domain with a proxy method to call to infect
        // java stack.
        // Another optimization in case of scripts coming from "world" domain,
        // that is having minimal default privileges is to construct
        // one AccessControlContext based on ProtectionDomain
        // with least possible privileges and simply call
        // AccessController.doPrivileged with this untrusted context

        ProtectionDomain dynamicDomain = getDynamicDomain(staticDomain);
        ProtectionDomain[] tmp = { dynamicDomain };
        AccessControlContext restricted = new AccessControlContext(tmp);

        PrivilegedAction action = new PrivilegedAction() {
            public Object run() {
                return callable.call(cx, scope, thisObj, args);
            }
        };

        return AccessController.doPrivileged(action, restricted);
    }
}
