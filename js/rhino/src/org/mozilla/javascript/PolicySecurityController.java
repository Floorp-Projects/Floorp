package org.mozilla.javascript;

import java.lang.ref.Reference;
import java.lang.ref.SoftReference;
import java.lang.reflect.UndeclaredThrowableException;
import java.security.AccessController;
import java.security.CodeSource;
import java.security.Policy;
import java.security.PrivilegedAction;
import java.security.SecureClassLoader;
import java.util.Map;
import java.util.Random;
import java.util.WeakHashMap;

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
 * @version $Id: PolicySecurityController.java,v 1.1 2006/09/10 12:16:46 szegedia%freemail.hu Exp $
 */
public class PolicySecurityController extends SecurityController
{
    // Use weak map, so stuff related to abandoned code sources gets cleaned up
    // automatically.
    private static final Map secureCallers = new WeakHashMap();
    
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
    
    public GeneratedClassLoader createClassLoader(ClassLoader parentLoader, 
            Object securityDomain)
    {
        return createLoader(parentLoader, (CodeSource)securityDomain);
    }

    private Loader createLoader(
            final ClassLoader parent, final CodeSource securityDomain)
    {
        return (Loader)AccessController.doPrivileged(
            new PrivilegedAction()
            {
                public Object run()
                {
                    return new Loader(parent, securityDomain);
                }
            });
    }

    public Object getDynamicSecurityDomain(Object securityDomain)
    {
        // No separate notion of dynamic security domain - just return what was
        // passed in.
        return securityDomain;
    }

    public Object callWithDomain(Object securityDomain, Context cx, 
            Callable callable, Scriptable scope, Scriptable thisObj, 
            Object[] args)
    {
        SecureCaller secureCaller;
        CodeSource codeSource = (CodeSource)securityDomain;
        ClassLoader appClassLoader = cx.getApplicationClassLoader();
        DomainKey key = new DomainKey(codeSource, appClassLoader);
        synchronized(secureCallers)
        {
            Reference ref = (Reference)secureCallers.get(key);
            if(ref != null)
            {
                secureCaller = (SecureCaller)ref.get();
            }
            else
            {
                secureCaller = null;
            }
            if(secureCaller == null)
            {
                Loader l = createLoader(appClassLoader, codeSource);
                String name = SecureCaller.class.getName() + "$Sub" + 
                // Being a bit paranoid about name clashes - if anyone'd figure
                // out how to get to the class loader this might become a 
                // concern
                Integer.toHexString(key.hashCode()) + "_" + Long.toHexString(
                        new Random().nextLong());
                // Just a trivial "class ... extends SecureCaller {}" 
                // class.
                ClassFileWriter cfw = new ClassFileWriter(name, 
                        SecureCaller.class.getName(), "<generated>");
                Class clazz = l.defineClass(name, cfw.toByteArray());
                try
                {
                    secureCaller = (SecureCaller)clazz.newInstance();
                    // Using a soft reference, so we don't hold strongly on the
                    // code source and allow it to get cleaned up eventually.
                    secureCallers.put(key, new SoftReference(secureCaller));
                }
                catch (InstantiationException e)
                {
                    throw new UndeclaredThrowableException(e);
                }
                catch (IllegalAccessException e)
                {
                    throw new UndeclaredThrowableException(e);
                }
            }
        }
        return secureCaller.call(callable, cx, scope, thisObj, args);
    }
    
    public static class SecureCaller
    {
        public Object call(Callable callable, Context cx, Scriptable scope, 
                Scriptable thisObj, Object[] args)
        {
            return callable.call(cx, scope, thisObj, args);
        }
    }
    
    private static final class DomainKey
    {
        private final CodeSource codeSource;
        private final ClassLoader appClassLoader;
        
        DomainKey(CodeSource codeSource, ClassLoader appClassLoader)
        {
            this.codeSource = codeSource;
            if(codeSource == null)
            {
                throw new IllegalArgumentException("codeSource == null");
            }
            this.appClassLoader = appClassLoader;
        }
        
        public boolean equals(Object obj)
        {
            if(obj == null || obj.getClass() != DomainKey.class)
            {
                return false;
            }
            DomainKey other = (DomainKey)obj;
            return codeSource.equals(other.codeSource) && appClassLoader == other.appClassLoader;
        }
        
        public int hashCode()
        {
            return codeSource.hashCode() ^ System.identityHashCode(appClassLoader);
        }
    }
}
