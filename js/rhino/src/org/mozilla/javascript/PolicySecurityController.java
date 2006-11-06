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
 * @version $Id: PolicySecurityController.java,v 1.3 2006/11/06 08:37:07 szegedia%freemail.hu Exp $
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