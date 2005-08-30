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
 * getParameterTypes() results, recover from IllegalAccessException
 * in some cases and provide serialization support.
 *
 * @author Igor Bukanov
 */

final class MemberBox implements Serializable
{
    static final long serialVersionUID = 6358550398665688245L;

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
    {
        Method method = method();
        try {
            try {
                return method.invoke(target, args);
            } catch (IllegalAccessException ex) {
                Method accessible = searchAccessibleMethod(method, argTypes);
                if (accessible != null) {
                    memberObject = accessible;
                    method = accessible;
                } else {
                    if (!VMBridge.instance.tryToMakeAccessible(method)) {
                        throw Context.throwAsScriptRuntimeEx(ex);
                    }
                }
                // Retry after recovery
                return method.invoke(target, args);
            }
        } catch (Exception ex) {
            throw Context.throwAsScriptRuntimeEx(ex);
        }
    }

    Object newInstance(Object[] args)
    {
        Constructor ctor = ctor();
        try {
            try {
                return ctor.newInstance(args);
            } catch (IllegalAccessException ex) {
                if (!VMBridge.instance.tryToMakeAccessible(ctor)) {
                    throw Context.throwAsScriptRuntimeEx(ex);
                }
            }
            return ctor.newInstance(args);
        } catch (Exception ex) {
            throw Context.throwAsScriptRuntimeEx(ex);
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

    private void readObject(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        Member member = readMember(in);
        if (member instanceof Method) {
            init((Method)member);
        } else {
            init((Constructor)member);
        }
    }

    private void writeObject(ObjectOutputStream out)
        throws IOException
    {
        out.defaultWriteObject();
        writeMember(out, memberObject);
    }

    /**
     * Writes a Constructor or Method object.
     *
     * Methods and Constructors are not serializable, so we must serialize
     * information about the class, the name, and the parameters and
     * recreate upon deserialization.
     */
    private static void writeMember(ObjectOutputStream out, Member member)
        throws IOException
    {
        if (member == null) {
            out.writeBoolean(false);
            return;
        }
        out.writeBoolean(true);
        if (!(member instanceof Method || member instanceof Constructor))
            throw new IllegalArgumentException("not Method or Constructor");
        out.writeBoolean(member instanceof Method);
        out.writeObject(member.getName());
        out.writeObject(member.getDeclaringClass());
        if (member instanceof Method) {
            writeParameters(out, ((Method) member).getParameterTypes());
        } else {
            writeParameters(out, ((Constructor) member).getParameterTypes());
        }
    }

    /**
     * Reads a Method or a Constructor from the stream.
     */
    private static Member readMember(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        if (!in.readBoolean())
            return null;
        boolean isMethod = in.readBoolean();
        String name = (String) in.readObject();
        Class declaring = (Class) in.readObject();
        Class[] parms = readParameters(in);
        try {
            if (isMethod) {
                return declaring.getMethod(name, parms);
            } else {
                return declaring.getConstructor(parms);
            }
        } catch (NoSuchMethodException e) {
            throw new IOException("Cannot find member: " + e);
        }
    }

    private static final Class[] primitives = {
        Boolean.TYPE,
        Byte.TYPE,
        Character.TYPE,
        Double.TYPE,
        Float.TYPE,
        Integer.TYPE,
        Long.TYPE,
        Short.TYPE,
        Void.TYPE
    };

    /**
     * Writes an array of parameter types to the stream.
     *
     * Requires special handling because primitive types cannot be
     * found upon deserialization by the default Java implementation.
     */
    private static void writeParameters(ObjectOutputStream out, Class[] parms)
        throws IOException
    {
        out.writeShort(parms.length);
    outer:
        for (int i=0; i < parms.length; i++) {
            Class parm = parms[i];
            out.writeBoolean(parm.isPrimitive());
            if (!parm.isPrimitive()) {
                out.writeObject(parm);
                continue;
            }
            for (int j=0; j < primitives.length; j++) {
                if (parm.equals(primitives[j])) {
                    out.writeByte(j);
                    continue outer;
                }
            }
            throw new IllegalArgumentException("Primitive " + parm +
                                               " not found");
        }
    }

    /**
     * Reads an array of parameter types from the stream.
     */
    private static Class[] readParameters(ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        Class[] result = new Class[in.readShort()];
        for (int i=0; i < result.length; i++) {
            if (!in.readBoolean()) {
                result[i] = (Class) in.readObject();
                continue;
            }
            result[i] = primitives[in.readByte()];
        }
        return result;
    }

    private transient Member memberObject;
    transient Class[] argTypes;
}

