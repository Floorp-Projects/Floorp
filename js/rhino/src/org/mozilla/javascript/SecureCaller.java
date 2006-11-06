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
 * @version $Id: SecureCaller.java,v 1.1 2006/11/06 08:37:07 szegedia%freemail.hu Exp $
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
