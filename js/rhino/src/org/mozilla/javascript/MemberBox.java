/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
 * Felix Meschberger
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

package org.mozilla.javascript;

import java.lang.reflect.*;
import java.io.*;

/**
 * Wrappper class for Method and Constructor instances to cache
 * getParameterTypes() results and to recover from IllegalAccessException
 * in some cases.
 *
 * @author Igor Bukanov
 */

final class MemberBox implements Serializable
{

    MemberBox(Method method)
    {
        init(method);
    }

    MemberBox(Constructor constructor)
    {
        init(constructor);
    }

    private void init(Method method)
    {
        this.memberObject = method;
        this.argTypes = method.getParameterTypes();
    }

    private void init(Constructor constructor)
    {
        this.memberObject = constructor;
        this.argTypes = constructor.getParameterTypes();
    }

    Method method()
    {
        return (Method)memberObject;
    }

    Constructor ctor()
    {
        return (Constructor)memberObject;
    }

    boolean isMethod()
    {
        return memberObject instanceof Method;
    }

    boolean isCtor()
    {
        return memberObject instanceof Constructor;
    }

    boolean isStatic()
    {
        return Modifier.isStatic(memberObject.getModifiers());
    }

    String getName()
    {
        return memberObject.getName();
    }

    Class getDeclaringClass()
    {
        return memberObject.getDeclaringClass();
    }

    String toJavaDeclaration()
    {
        StringBuffer sb = new StringBuffer();
        if (isMethod()) {
            Method method = method();
            sb.append(method.getReturnType());
            sb.append(' ');
            sb.append(method.getName());
        } else {
            Constructor ctor = ctor();
            String name = ctor.getDeclaringClass().getName();
            int lastDot = name.lastIndexOf('.');
            if (lastDot >= 0) {
                name = name.substring(lastDot + 1);
            }
            sb.append(name);
        }
        sb.append(JavaMembers.liveConnectSignature(argTypes));
        return sb.toString();
    }

    public String toString()
    {
        return memberObject.toString();
    }

    Object invoke(Object target, Object[] args)
        throws IllegalAccessException, InvocationTargetException
    {
        Method method = method();
        try {
            return method.invoke(target, args);
        } catch (IllegalAccessException ex) {
            Method accessible = searchAccessibleMethod(method, argTypes);
            if (accessible != null) {
                memberObject = accessible;
                method = accessible;
            } else {
                if (!tryToMakeAccessible(method)) {
                    throw ex;
                }
            }
            return method.invoke(target, args);
        }
    }

    Object newInstance(Object[] args)
        throws InvocationTargetException, IllegalAccessException,
               InstantiationException
    {
        Constructor ctor = ctor();
        try {
            return ctor.newInstance(args);
        } catch (IllegalAccessException ex) {
            if (!tryToMakeAccessible(ctor)) {
                throw ex;
            }
            return ctor.newInstance(args);
        }
    }

    private static Method searchAccessibleMethod(Method method, Class[] params)
    {
        int modifiers = method.getModifiers();
        if (Modifier.isPublic(modifiers) && !Modifier.isStatic(modifiers)) {
            Class c = method.getDeclaringClass();
            if (!Modifier.isPublic(c.getModifiers())) {
                String name = method.getName();
                Class[] intfs = c.getInterfaces();
                for (int i = 0, N = intfs.length; i != N; ++i) {
                    Class intf = intfs[i];
                    if (Modifier.isPublic(intf.getModifiers())) {
                        try {
                            return intf.getMethod(name, params);
                        } catch (NoSuchMethodException ex) {
                        } catch (SecurityException ex) {  }
                    }
                }
                for (;;) {
                    c = c.getSuperclass();
                    if (c == null) { break; }
                    if (Modifier.isPublic(c.getModifiers())) {
                        try {
                            Method m = c.getMethod(name, params);
                            int mModifiers = m.getModifiers();
                            if (Modifier.isPublic(mModifiers)
                                && !Modifier.isStatic(mModifiers))
                            {
                                return m;
                            }
                        } catch (NoSuchMethodException ex) {
                        } catch (SecurityException ex) {  }
                    }
                }
            }
        }
        return null;
    }

    private static boolean tryToMakeAccessible(Member member)
    {
        /**
         * Due to a bug in Sun's VM, public methods in private
         * classes are not accessible by default (Sun Bug #4071593).
         * We have to explicitly set the method accessible
         * via method.setAccessible(true) but we have to use
         * reflection because the setAccessible() in Method is
         * not available under jdk 1.1.
         */
        if (method_setAccessible != null) {
            try {
                Object[] args_wrapper = { Boolean.TRUE };
                method_setAccessible.invoke(member, args_wrapper);
                return true;
            } catch (Exception ex) { }
        }
        return false;
    }

    private void readObject(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        Member member = FunctionObject.readMember(in);
        if (member instanceof Method) {
            init((Method)member);
        } else {
            init((Constructor)member);
        }
    }

    private void writeObject(ObjectOutputStream out)
        throws IOException
    {
        FunctionObject.writeMember(out, memberObject);
    }

    private transient Member memberObject;
    transient Class[] argTypes;

    private static Method method_setAccessible;

    static {
        try {
            Class MethodClass = Class.forName("java.lang.reflect.Method");
            method_setAccessible = MethodClass.getMethod(
                "setAccessible", new Class[] { Boolean.TYPE });
        } catch (Exception ex) {
            // Assume any exceptions means the method does not exist.
        }
    }
}

