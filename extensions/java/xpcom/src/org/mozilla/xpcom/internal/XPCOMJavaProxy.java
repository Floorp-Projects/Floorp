/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.xpcom.internal;

import java.lang.reflect.*;
import org.mozilla.xpcom.*;


/**
 * This class is used to pass XPCOM objects to Java functions.  A
 * <code>java.lang.reflect.Proxy</code> instance is created using the expected
 * interface, and all calls to the proxy are forwarded to the XPCOM object.
 */
public class XPCOMJavaProxy implements InvocationHandler {

  /**
   * Pointer to the XPCOM object for which we are a proxy.
   */
  protected long nativeXPCOMPtr;

  /**
   * Default constructor.
   *
   * @param aXPCOMInstance  address of XPCOM object as a long
   */
  public XPCOMJavaProxy(long aXPCOMInstance) {
    nativeXPCOMPtr = aXPCOMInstance;
  }

  /**
   * Returns the XPCOM object that the given proxy references.
   *
   * @param aProxy  Proxy created by <code>createProxy</code>
   *
   * @return  address of XPCOM object as a long
   */
  protected static long getNativeXPCOMInstance(Object aProxy) {
    XPCOMJavaProxy proxy = (XPCOMJavaProxy) Proxy.getInvocationHandler(aProxy);
    return proxy.nativeXPCOMPtr;
  }

  /**
   * Creates a Proxy for the given XPCOM object.
   *
   * @param aInterface      interface from which to create Proxy
   * @param aXPCOMInstance  address of XPCOM object as a long
   *
   * @return  Proxy of given XPCOM object
   */
  protected static Object createProxy(Class aInterface, long aXPCOMInstance) {
    return Proxy.newProxyInstance(aInterface.getClassLoader(),
            new Class[] { aInterface, XPCOMJavaProxyBase.class },
            new XPCOMJavaProxy(aXPCOMInstance));
  }

  /**
   * All calls to the Java proxy are forwarded to this method.  This method
   * takes care of a few of the <code>Object</code> method calls;  all other
   * calls are forwarded to the XPCOM object.
   *
   * @param aProxy    Proxy created by <code>createProxy</code>
   * @param aMethod   object that describes the called method
   * @param aParams   array of the arguments passed to the method
   *
   * @return  return value as defined by given <code>aMethod</code>
   */
  public Object invoke(Object aProxy, Method aMethod, Object[] aParams)
          throws Throwable {
    String methodName = aMethod.getName();

    // Handle the three java.lang.Object methods that are passed to us.
    if (aMethod.getDeclaringClass() == Object.class)  {
      if (methodName.equals("hashCode"))  {
        return proxyHashCode(aProxy);
      }
      if (methodName.equals("equals")) {
        return proxyEquals(aProxy, aParams[0]);
      }
      if (methodName.equals("toString")) {
        return proxyToString(aProxy);
      }
      System.err.println("WARNING: Unhandled Object method [" +
                         methodName + "]");
      return null;
    }

    // Handle the 'finalize' method called during garbage collection
    if (aMethod.getDeclaringClass() == XPCOMJavaProxyBase.class) {
      if (methodName.equals("finalize")) {
        finalizeProxy(aProxy);
      } else {
        System.err.println("WARNING: Unhandled XPCOMJavaProxyBase method [" +
                           methodName + "]");
      }
      return null;
    }

    // If not already handled, pass method calls to XPCOM object.
    return callXPCOMMethod(aProxy, methodName, aParams);
  }

  /**
   * Handles method calls of <code>java.lang.Object.hashCode</code>
   *
   * @param aProxy  Proxy created by <code>createProxy</code>
   *
   * @return  Integer object representing hash code of given object
   *
   * @see Object#hashCode()
   */
  protected static Integer proxyHashCode(Object aProxy) {
    return new Integer(System.identityHashCode(aProxy));
  }

  /**
   * Handles method calls of <code>java.lang.Object.equals</code>
   *
   * @param aProxy  Proxy created by <code>createProxy</code>
   * @param aOther  another object
   *
   * @return  <code>true</code> if the given objects are the same;
   *          <code>false</code> otherwise
   *
   * @see Object#equals(Object)
   */
  protected static Boolean proxyEquals(Object aProxy, Object aOther) {
    return (aProxy == aOther ? Boolean.TRUE : Boolean.FALSE);
  }

  /**
   * Indicates whether the given object is an XPCOMJavaProxy.
   *
   * @param aObject  object to check
   *
   * @return  <code>true</code> if the given object is an XPCOMJavaProxy;
   *          <code>false</code> otherwise
   */
  protected static boolean isXPCOMJavaProxy(Object aObject) {
    if (Proxy.isProxyClass(aObject.getClass())) {
      InvocationHandler h = Proxy.getInvocationHandler(aObject);
      if (h instanceof XPCOMJavaProxy) {
        return true;
      }
    }
    return false;
  }

  /**
   * Handles method calls of <code>java.lang.Object.toString</code>
   *
   * @param aProxy  Proxy created by <code>createProxy</code>
   *
   * @return  String representation of given object
   *
   * @see Object#toString()
   */
  protected static String proxyToString(Object aProxy) {
    return aProxy.getClass().getInterfaces()[0].getName() + '@' +
           Integer.toHexString(aProxy.hashCode());
  }

  /**
   * Called when the proxy is garbage collected by the JVM.  Allows us to clean
   * up any references to the XPCOM object.
   *
   * @param aProxy  reference to Proxy that is being garbage collected
   */
  protected void finalizeProxy(Object aProxy) throws Throwable {
    finalizeProxyNative(aProxy);
    super.finalize();
  }

  protected static native void finalizeProxyNative(Object aProxy);

  /**
   * Calls the XPCOM object referenced by the proxy with the given method.
   *
   * @param aProxy        Proxy created by <code>createProxy</code>
   * @param aMethodName   name of method that we want to call
   * @param aParams       array of params passed to method
   *
   * @return  return value as defined by given method
   *
   * @exception XPCOMException if XPCOM method failed.  Values of XPCOMException
   *            are defined by the method called.
   */
  protected static native Object callXPCOMMethod(Object aProxy,
          String aMethodName, Object[] aParams);

}

