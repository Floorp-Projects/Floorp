/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript;

import java.lang.ref.SoftReference;
import java.lang.reflect.UndeclaredThrowableException;
import java.security.AccessController;
import java.security.CodeSource;
import java.security.Policy;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.security.SecureClassLoader;
import java.util.Map;
import java.util.WeakHashMap;

import org.mozilla.classfile.ByteCode;
import org.mozilla.classfile.ClassFileWriter;

/**
 * A security controller relying on Java {@link Policy} in effect. When you use
 * this security controller, your securityDomain objects must be instances of
 * {@link CodeSource} representing the location from where you load your 
 * scripts. Any Java policy "grant" statements matching the URL and certificate
 * in code sources will apply to the scripts. If you specify any certificates 
 * within your {@link CodeSource} objects, it is your responsibility to verify
 * (or not) that the script source files are signed in whatever 
 * implementation-specific way you're using.
 * @author Attila Szegedi
 */
public class PolicySecurityController extends SecurityController
{
    private static final byte[] secureCallerImplBytecode = loadBytecode();

    // We're storing a CodeSource -> (ClassLoader -> SecureRenderer), since we
    // need to have one renderer per class loader. We're using weak hash maps
    // and soft references all the way, since we don't want to interfere with
    // cleanup of either CodeSource or ClassLoader objects.
    private static final Map callers = new WeakHashMap();
    
    private static class Loader extends SecureClassLoader
    implements GeneratedClassLoader
    {
        private final CodeSource codeSource;
        
        Loader(ClassLoader parent, CodeSource codeSource)
        {
            super(parent);
            this.codeSource = codeSource;
        }

        public Class defineClass(String name, byte[] data)
        {
            return defineClass(name, data, 0, data.length, codeSource);
        }
        
        public void linkClass(Class cl)
        {
            resolveClass(cl);
        }
    }
    
    public GeneratedClassLoader createClassLoader(final ClassLoader parent, 
            final Object securityDomain)
    {
        return (Loader)AccessController.doPrivileged(
            new PrivilegedAction()
            {
                public Object run()
                {
                    return new Loader(parent, (CodeSource)securityDomain);
                }
            });
    }

    public Object getDynamicSecurityDomain(Object securityDomain)
    {
        // No separate notion of dynamic security domain - just return what was
        // passed in.
        return securityDomain;
    }

    public Object callWithDomain(final Object securityDomain, final Context cx, 
            Callable callable, Scriptable scope, Scriptable thisObj, 
            Object[] args)
    {
        // Run in doPrivileged as we might be checked for "getClassLoader" 
        // runtime permission
        final ClassLoader classLoader = (ClassLoader)AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    return cx.getApplicationClassLoader();
                }
            });
        final CodeSource codeSource = (CodeSource)securityDomain;
        Map classLoaderMap;
        synchronized(callers)
        {
            classLoaderMap = (Map)callers.get(codeSource);
            if(classLoaderMap == null)
            {
                classLoaderMap = new WeakHashMap();
                callers.put(codeSource, classLoaderMap);
            }
        }
        SecureCaller caller;
        synchronized(classLoaderMap)
        {
            SoftReference ref = (SoftReference)classLoaderMap.get(classLoader);
            if(ref != null)
            {
                caller = (SecureCaller)ref.get();
            }
            else
            {
                caller = null;
            }
            if(caller == null)
            {
                try
                {
                    // Run in doPrivileged as we'll be checked for 
                    // "createClassLoader" runtime permission
                    caller = (SecureCaller)AccessController.doPrivileged(
                            new PrivilegedExceptionAction()
                    {
                        public Object run() throws Exception
                        {
                            Loader loader = new Loader(classLoader, 
                                    codeSource);
                            Class c = loader.defineClass(
                                    SecureCaller.class.getName() + "Impl", 
                                    secureCallerImplBytecode);
                            return c.newInstance();
                        }
                    });
                }
                catch(PrivilegedActionException ex)
                {
                    throw new UndeclaredThrowableException(ex.getCause());
                }
            }
        }
        return caller.call(callable, cx, scope, thisObj, args);
    }
    
    public abstract static class SecureCaller
    {
        public abstract Object call(Callable callable, Context cx, Scriptable scope, 
                Scriptable thisObj, Object[] args);
    }
    
    
    private static byte[] loadBytecode()
    {
        String secureCallerClassName = SecureCaller.class.getName();
        ClassFileWriter cfw = new ClassFileWriter(
                secureCallerClassName + "Impl", secureCallerClassName, 
                "<generated>");
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
        cfw.addALoad(0);
        cfw.addInvoke(ByteCode.INVOKESPECIAL, secureCallerClassName, 
                "<init>", "()V");
        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)1);
        String callableCallSig = 
            "Lorg/mozilla/javascript/Context;" +
            "Lorg/mozilla/javascript/Scriptable;" +
            "Lorg/mozilla/javascript/Scriptable;" +
            "[Ljava/lang/Object;)Ljava/lang/Object;";
        
        cfw.startMethod("call",
                "(Lorg/mozilla/javascript/Callable;" + callableCallSig,
                (short)(ClassFileWriter.ACC_PUBLIC
                        | ClassFileWriter.ACC_FINAL));
        for(int i = 1; i < 6; ++i) {
            cfw.addALoad(i);
        }
        cfw.addInvoke(ByteCode.INVOKEINTERFACE, 
                "org/mozilla/javascript/Callable", "call", 
                "(" + callableCallSig);
        cfw.add(ByteCode.ARETURN);
        cfw.stopMethod((short)6);
        return cfw.toByteArray();
    }
}