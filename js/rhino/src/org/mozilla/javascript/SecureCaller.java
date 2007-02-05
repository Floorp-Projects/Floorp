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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.SoftReference;
import java.lang.reflect.UndeclaredThrowableException;
import java.net.URL;
import java.security.AccessController;
import java.security.CodeSource;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.security.SecureClassLoader;
import java.util.Map;
import java.util.WeakHashMap;

/**
 * @author Attila Szegedi
 */
public abstract class SecureCaller
{
    private static final byte[] secureCallerImplBytecode = loadBytecode();
    
    // We're storing a CodeSource -> (ClassLoader -> SecureRenderer), since we
    // need to have one renderer per class loader. We're using weak hash maps
    // and soft references all the way, since we don't want to interfere with
    // cleanup of either CodeSource or ClassLoader objects.
    private static final Map callers = new WeakHashMap();
    
    public abstract Object call(Callable callable, Context cx, 
            Scriptable scope, Scriptable thisObj, Object[] args);
    
    /**
     * Render the specified template element with the specified environment,
     * using a protection domain belonging to the specified code source. 
     * @param codeSource the code source defining the protection domain
     * @param env the processing environment
     * @param t the processed template element
     * @throws TemplateException if environment's processing of the template
     * element throws the exception
     * @throws IOException if environment's processing of the template
     * element throws the exception
     */
    static Object callSecurely(final CodeSource codeSource, Callable callable, 
            Context cx, Scriptable scope, Scriptable thisObj, Object[] args)
    {
        final Thread thread = Thread.currentThread();
        // Run in doPrivileged as we might be checked for "getClassLoader" 
        // runtime permission
        final ClassLoader classLoader = (ClassLoader)AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    return thread.getContextClassLoader();
                }
            });
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
                            ClassLoader effectiveClassLoader;
                            Class thisClass = getClass();
                            if(classLoader.loadClass(thisClass.getName()) != thisClass) {
                                effectiveClassLoader = thisClass.getClassLoader();
                            } else {
                                effectiveClassLoader = classLoader;
                            }  
                            SecureClassLoaderImpl secCl = 
                                new SecureClassLoaderImpl(effectiveClassLoader);
                            Class c = secCl.defineAndLinkClass(
                                    SecureCaller.class.getName() + "Impl", 
                                    secureCallerImplBytecode, codeSource);
                            return c.newInstance();
                        }
                    });
                    classLoaderMap.put(classLoader, new SoftReference(caller));
                }
                catch(PrivilegedActionException ex)
                {
                    throw new UndeclaredThrowableException(ex.getCause());
                }
            }
        }
        return caller.call(callable, cx, scope, thisObj, args);
    }
    
    private static class SecureClassLoaderImpl extends SecureClassLoader
    {
        SecureClassLoaderImpl(ClassLoader parent)
        {
            super(parent);
        }
        
        Class defineAndLinkClass(String name, byte[] bytes, CodeSource cs)
        {
            Class cl = defineClass(name, bytes, 0, bytes.length, cs);
            resolveClass(cl);
            return cl;
        }
    }
    
    private static byte[] loadBytecode()
    {
        return (byte[])AccessController.doPrivileged(new PrivilegedAction()
        {
            public Object run()
            {
                return loadBytecodePrivileged();
            }
        });
    }
    
    private static byte[] loadBytecodePrivileged()
    {
        URL url = SecureCaller.class.getResource("SecureCallerImpl.clazz");
        try
        {
            InputStream in = url.openStream();
            try
            {
                ByteArrayOutputStream bout = new ByteArrayOutputStream();
                for(;;)
                {
                    int r = in.read();
                    if(r == -1)
                    {
                        return bout.toByteArray();
                    }
                    bout.write(r);
                }
            }
            finally
            {
                in.close();
            }
        }
        catch(IOException e)
        {
            throw new UndeclaredThrowableException(e);
        }
    }
}
