/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


package org.mozilla.pluglet.test.basic.api;

import java.lang.reflect.*;


public class ReflectionTest
{  public static void main(String[] args)
   {  String name = readLine("Please enter a class name (e.g. java.util.Date): ");
      try
      {  Class cl = Class.forName(name);
         Class supercl = cl.getSuperclass();
         System.out.print("class " + name);
         if (supercl != null && !supercl.equals(Object.class))
            System.out.print(" extends " + supercl.getName());
         System.out.print("\n{\n");
         printConstructors(cl);
         System.out.println();
         printMethods(cl);
         System.out.println();
         printFields(cl);
         System.out.println("}");
      }
      catch(ClassNotFoundException e)
      {  System.out.println("Class not found.");
      }
   }

   public static String readLine(String str)
   {  int ch;
      String r = "";
      boolean done = false;
      System.out.println(str);
      while (!done)
      {  try
         {  ch = System.in.read();
            if (ch < 0 || (char)ch == '\n')
               done = true;
            else if ((char)ch != '\r') // weird--it used to do \r\n translation
               r = r + (char) ch;
         }
         catch(java.io.IOException e)
         {  done = true;
         }
      }
      return r;
   }



   public static void printConstructors(Class cl)
   {  Constructor[] constructors = cl.getDeclaredConstructors();

      for (int i = 0; i < constructors.length; i++)
      {  Constructor c = constructors[i];
         Class[] paramTypes = c.getParameterTypes();
         String name = c.getName();
         System.out.print(Modifier.toString(c.getModifiers()));
         System.out.print(" " + name + "(");
         for (int j = 0; j < paramTypes.length; j++)
         {  if (j > 0) System.out.print(", ");
            System.out.print(paramTypes[j].getName());
         }
         System.out.println(");");
      }
   }
   
   public static void printMethods(Class cl)
   {  Method[] methods = cl.getDeclaredMethods();

      for (int i = 0; i < methods.length; i++)
      {  Method m = methods[i];
         Class retType = m.getReturnType();
         Class[] paramTypes = m.getParameterTypes();
         String name = m.getName();
         System.out.print(Modifier.toString(m.getModifiers()));
         System.out.print(" " + retType.getName() + " " + name 
            + "(");
         for (int j = 0; j < paramTypes.length; j++)
         {  if (j > 0) System.out.print(", ");
            System.out.print(paramTypes[j].getName());
         }
         System.out.println(");");
      }
   }

   public static void printFields(Class cl)
   {  Field[] fields = cl.getDeclaredFields();

      for (int i = 0; i < fields.length; i++)
      {  Field f = fields[i];
         Class type = f.getType();
         String name = f.getName();
         System.out.print(Modifier.toString(f.getModifiers()));
         System.out.println(" " + type.getName() + " " + name 
            + ";");
      }
   }


   public Method[] getMethods(Class cl)
   {  
      Method[] methods = cl.getDeclaredMethods();
      return methods;
   }

   public Class[] getParameters(Method m)
   {  
      Class[] paramTypes = m.getParameterTypes();
      return paramTypes;
   }

   public Constructor[] getConstructors(Class cl)
   {  
      Constructor[] constructors = cl.getDeclaredConstructors();
      return constructors;
   }

   
}
