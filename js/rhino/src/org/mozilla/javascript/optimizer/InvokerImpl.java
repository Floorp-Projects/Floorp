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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * David C. Navas
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

package org.mozilla.javascript.optimizer;

import java.util.Hashtable;
import java.io.IOException;
import java.lang.reflect.Method;

import org.mozilla.javascript.Invoker;
import org.mozilla.classfile.ByteCode;
import org.mozilla.classfile.ClassFileWriter;
import org.mozilla.classfile.DefiningClassLoader;

/**
 * Avoid cost of java.lang.reflect.Method.invoke() by compiling a class to 
 * perform the method call directly.
 */
public class InvokerImpl extends Invoker {
    
    public Invoker createInvoker(Method method, Class[] types) {
        
        Invoker result = (Invoker)invokersCache.get(method);
        if (result != null) { return result; }
        
        synchronized (this) {
            ++classNumber;
        }
        
        String className = "inv" + classNumber;
            
        ClassFileWriter cfw = new ClassFileWriter(className, 
                                "org.mozilla.javascript.Invoker", "");
        cfw.setFlags((short)(ClassFileWriter.ACC_PUBLIC | 
                             ClassFileWriter.ACC_FINAL));
            
        // Add our instantiator!
        cfw.startMethod("<init>", "()V", ClassFileWriter.ACC_PUBLIC);
        cfw.add(ByteCode.ALOAD_0);
        cfw.add(ByteCode.INVOKESPECIAL,
                "org.mozilla.javascript.Invoker",
                "<init>", "()", "V");
        cfw.add(ByteCode.RETURN);
        cfw.stopMethod((short)1, null); // one argument -- this???

        // Add the invoke() method call
        cfw.startMethod("invoke", 
                        "(Ljava/lang/Object;[Ljava/lang/Object;)"+
                         "Ljava/lang/Object;",
                        (short)(ClassFileWriter.ACC_PUBLIC | 
                                ClassFileWriter.ACC_FINAL));
            
        // If we return a primitive type, then do something special!
        String declaringClassName = method.getDeclaringClass().getName
            ().replace('.', '/');
        Class returnType = method.getReturnType();
        String invokeSpecial = null;
        String invokeSpecialType = null;
        boolean returnsVoid = false;
        boolean returnsBoolean = false;
            
        if (returnType.isPrimitive()) {
            if (returnType == Boolean.TYPE) {
                returnsBoolean = true;
                invokeSpecialType = "(Z)";
            } else if (returnType == Void.TYPE) {
                returnsVoid = true;
                invokeSpecialType = "(V)";
            } else if (returnType == Integer.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Integer");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(I)";
            } else if (returnType == Long.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Long");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(J)";
            } else if (returnType == Short.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Short");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(S)";
            } else if (returnType == Float.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Float");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(F)";
            } else if (returnType == Double.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Double");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(D)";
            } else if (returnType == Byte.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial = "java/lang/Byte");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(B)";
            } else if (returnType == Character.TYPE) {
                cfw.add(ByteCode.NEW, invokeSpecial 
                            = "java/lang/Character");
                cfw.add(ByteCode.DUP);
                invokeSpecialType = "(C)";
            }
        }
            
        // handle setup of call to virtual function (if calling non-static)
        if (!java.lang.reflect.Modifier.isStatic(method.getModifiers())) {
            cfw.add(ByteCode.ALOAD_1);
            cfw.add(ByteCode.CHECKCAST, declaringClassName);
        }
            
        // Handle parameters!
        StringBuffer params = new StringBuffer(2 + ((types!=null)?(20 *
                                types.length):0));
            
        params.append("(");
        if (types != null) {
            for(int i = 0; i < types.length; i++) {
                Class type = types[i];

                cfw.add(ByteCode.ALOAD_2);
                    
                if (i <= 5) {
                    cfw.add((byte) (ByteCode.ICONST_0 + i));
                } else if (i <= Byte.MAX_VALUE) {
                    cfw.add(ByteCode.BIPUSH, i);
                } else if (i <= Short.MAX_VALUE) {
                    cfw.add(ByteCode.SIPUSH, i);
                } else {
                    cfw.addLoadConstant((int)i);
                }
                    
                cfw.add(ByteCode.AALOAD);
                    
                if (type.isPrimitive()) {
                    // Convert enclosed type back to primitive.
                        
                    if (type == Boolean.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Boolean");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Boolean", 
                                "booleanValue", "()", "Z");
                        params.append("Z");
                    } else if (type == Integer.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                "intValue", "()", "I");
                        params.append("I");
                    } else if (type == Short.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                "shortValue", "()", "S");
                        params.append("S");
                    } else if (type == Character.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Character");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Character", 
                                "charValue", "()", "C");
                        params.append("C");
                    } else if (type == Double.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                "doubleValue", "()", "D");
                        params.append("D");
                    } else if (type == Float.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Number");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Number", 
                                "floatValue", "()", "F");
                        params.append("F");
                    } else if (type == Byte.TYPE) {
                        cfw.add(ByteCode.CHECKCAST, "java/lang/Byte");
                        cfw.add(ByteCode.INVOKEVIRTUAL, "java/lang/Byte", 
                                "byteValue", "()", "B");
                        params.append("B");
                    }
                } else {
                    String typeName = type.getName().replace('.', '/');
                    cfw.add(ByteCode.CHECKCAST, typeName);
                        
                    if (!type.isArray()) {
                        params.append('L');
                    }
                    params.append(typeName);
                        
                    if (!type.isArray()) {
                        params.append(';');
                    }
                }
            }
        }
        params.append(")");
            
        // Call actual function!
        if (!java.lang.reflect.Modifier.isStatic(method.getModifiers())) {
            cfw.add(ByteCode.INVOKEVIRTUAL, declaringClassName, 
                    method.getName(), params.toString(),
                    (invokeSpecialType!=null?invokeSpecialType.substring(1,2)
                                             :returnType.isArray()?
                                              returnType.getName().replace('.','/')
                                              :"L".concat
                (returnType.getName().replace('.', '/').concat(";"))));
        } else {
            cfw.add(ByteCode.INVOKESTATIC, declaringClassName, 
                    method.getName(), params.toString(),
                    (invokeSpecialType!=null?invokeSpecialType.substring(1,2)
                                             :returnType.isArray()?
                                              returnType.getName().replace('.','/')
                                              :"L".concat
                (returnType.getName().replace('.', '/').concat(";"))));
        }
            
        // Handle return value
        if (returnsVoid) {
            cfw.add(ByteCode.ACONST_NULL);
            cfw.add(ByteCode.ARETURN);
        } else if (returnsBoolean) {
            // HACK
            //check to see if true;
            // '7' is the number of bytes of the ifeq<branch> plus getstatic<TRUE> plus areturn instructions
            cfw.add(ByteCode.IFEQ, 7);
            cfw.add(ByteCode.GETSTATIC,
                    "java/lang/Boolean",
                    "TRUE",
                    "Ljava/lang/Boolean;");
            cfw.add(ByteCode.ARETURN);
            cfw.add(ByteCode.GETSTATIC,
                    "java/lang/Boolean",
                    "FALSE",
                    "Ljava/lang/Boolean;");
            cfw.add(ByteCode.ARETURN);
        } else if (invokeSpecial != null) {
            cfw.add(ByteCode.INVOKESPECIAL,
                    invokeSpecial,
                    "<init>", invokeSpecialType, "V");
            cfw.add(ByteCode.ARETURN);
        } else {
            cfw.add(ByteCode.ARETURN);
        }
        cfw.stopMethod((short)3, null); // three arguments, including the this pointer???
            
        // Add class to our classloader.
        java.io.ByteArrayOutputStream bos = 
            new java.io.ByteArrayOutputStream(550);
            
        try {
            cfw.write(bos);
        }
        catch (IOException ioe) {
            throw new RuntimeException("unexpected IOException" + ioe.toString());
        }
            
        try {
            byte[] bytes = bos.toByteArray();
                
            classLoader.defineClass(className, bytes);
            Class c = classLoader.loadClass(className, true);
            result = (Invoker)c.newInstance();
                
            if (false) {
                System.out.println("Generated method delegate for: " + method.getName() 
                                   + " on " + method.getDeclaringClass().getName() + " :: " + params.toString() 
                                   + " :: " + types);
            }
        } catch (ClassNotFoundException e) {
            throw new RuntimeException("unexpected " + e.toString());
        } catch (InstantiationException e) {
            throw new RuntimeException("unexpected " + e.toString());
        } catch (IllegalAccessException e) {
            throw new RuntimeException("unexpected " + e.toString());
        }

        invokersCache.put(method, result);
        return result;
    }
        
    public Object invoke(Object that, Object [] args) {
        return null;
    }

    int classNumber;
    Hashtable invokersCache = new Hashtable();
    DefiningClassLoader classLoader = new DefiningClassLoader();
}
