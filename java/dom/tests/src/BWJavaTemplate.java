/*
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s):
*/

package org.mozilla.dom.test;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.lang.reflect.*;
import org.mozilla.dom.*;


class BWJavaTemplate
{
   ReflectionTest rt=null;
   String fullclass=null;
   String jclass=null;
   String packageName=null;
   String outputDir=null;
   Vector outputFiles=null;
   private Method[] methods = null;
   private String currMethod=new String();
   private String currParamList=new String();
   private Hashtable rules = null;

   public BWJavaTemplate(String ajclass, String aoutDir, String rulesFile)
   {
	outputDir = aoutDir+System.getProperty("file.separator");
       fullclass = ajclass;
       if (fullclass == null) {
        System.out.println("Class Name cannot be NULL... ");
	return;
       }

	if (rulesFile != null) {
		loadRules(rulesFile);
	}	
       
       int idx = fullclass.lastIndexOf(".");
       if ( idx != -1)
       {
          packageName =  fullclass.substring(0, idx);
          jclass =  fullclass.substring(idx+1);
System.out.println("packageName is " + packageName);
       } else {
          jclass =  fullclass;
       }
System.out.println("class Name is  " + jclass);

       rt = new ReflectionTest();
       if (rt == null) {
         System.out.println("Could not instantiate ReflectionTest...\n");
	 return;
       }
       outputFiles = new Vector();

System.out.println("Parsing constructors and methods ...");
       if (!(parseClassConstructor()) && !(parseClassMethod())) {
	        System.out.println("Can't parse neither COnstructors nor Methods !!!\n");
		return;
	}

       DataOutputStream dos = null;
       String txtFile = jclass + ".txt";
       try {
        dos = new DataOutputStream( new FileOutputStream(outputDir+txtFile));
       } catch (Exception e) {
         System.out.println("ERROR: Could not create File " + txtFile);
         return; 
       }

     try {
        //dos.write("#Class|Method|Parameter|FileName\n".getBytes());
        for(int i=0; i<outputFiles.size(); i++)
	{
           dos.write(((String)outputFiles.elementAt(i)).getBytes());
	   dos.write("\n".getBytes());
	}
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + txtFile);
         return; 
      }

     try {
       dos.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + txtFile);
       return; 
     }

   }


	private void loadRules(String rulesFile) {
		try {
			/* Input stream routines*/
			FileInputStream in = new FileInputStream(rulesFile);
			LineNumberReader ln = new LineNumberReader((Reader)(new InputStreamReader((InputStream)in)));

			/*Parsing rules file*/
			rules = new Hashtable();
			String str = null;
			int pos = -1;
			while((str = ln.readLine()) != null) {
				if ((pos = str.indexOf(' ')) <= 0 || pos == str.length()) {
					System.out.println("WARNIG: rules file format error at line "+ln.getLineNumber());
				} else {
					//System.out.println("Rules: "+str.substring(0, pos)+" <-> "+str.substring(pos+1));
					rules.put((Object)str.substring(0, pos), (Object)str.substring(pos+1));
				}
			}
		} catch (Exception e) {
			System.out.println("WARNING: Can't read rules file "+rulesFile+": "+e.fillInStackTrace()+"\n");
			rules = null;
		}
	}


   /**
   * Parses the class constructors and create class files
   *
   * @return           Either true or false
   */
   public boolean parseClassConstructor()
   {
       Class cl = null;
       try {
         cl = Class.forName(fullclass);
       } catch (Exception e) {
         System.out.println("Could not find class file " + fullclass);
         return false;
       }
       if (Modifier.isAbstract(cl.getModifiers())) 
               return false;

       if (Modifier.isInterface(cl.getModifiers()))
               return false;

       Constructor constructors[] = rt.getConstructors(cl);
       String paramString = null;
       int staticFlag=0;
       for (int i = 0; i < constructors.length; i++)
       {  
            staticFlag=0;
	    String returnType = "void";
            Constructor c = constructors[i];
            Class[] paramTypes = c.getParameterTypes();
            String conName = c.getName();
	    currMethod = conName;

            int mod = c.getModifiers();
            if (Modifier.isStatic(mod)) 
                   staticFlag = 1;
            else if (!(Modifier.isPublic(mod)))  
                   continue;

	    paramString = new String();
	    ParamCombination pcomb = null;
            pcomb = new ParamCombination(paramTypes.length);
	    if (pcomb == null)
	    {
               System.out.println("Could not instantiate ParamCombination...\n");
	       return false;
	    }


            for (int j = 0; j < paramTypes.length; j++)
            {  
	       // Construct parameter string (param1_param2_...) 
               String pstr = paramTypes[j].getName();
	       String tstr = new String("");


               // check if param is an Array and accordingly get 
               // component type.
	       if (pstr.startsWith("["))
               {
		    int index = pstr.lastIndexOf("[") + 1;
                    tstr = "Arr" + (new Integer(index)).toString();
                    pstr = (pstr.substring(index)).trim();
                    if (pstr.length() == 1) {
                       if (pstr.compareTo("I") == 0) 
		          tstr = tstr + "int";
                       else if (pstr.compareTo("F") == 0) 
		          tstr = tstr + "float";
                       else if (pstr.compareTo("D") == 0) 
		          tstr = tstr + "double";
                       else if (pstr.compareTo("S") == 0) 
		          tstr = tstr + "short";
                       else if (pstr.compareTo("C") == 0) 
		          tstr = tstr + "char";
                       else if (pstr.compareTo("B") == 0) 
		          tstr = tstr + "byte";
                          
                    } else {
                      // Remove the leading 'L' from the string
                      pstr = pstr.substring(1);
                    }

		    // Remove trailing semicolon
 		    int slen = pstr.length();
                    pstr = pstr.substring(0, slen-1);
               }

               // Remove . from paramname (java.lang.String)
	       int idx = pstr.lastIndexOf(".");
	       if (idx != -1)
	       {
	          pstr = pstr.substring(idx + 1);
	       }
	       tstr = tstr + pstr;
	       paramString = paramString  + "_" + tstr;

	       /* get value list for parameter */
	       pcomb.addElement(getParameterValueList(tstr));
             
            }

	    // remove leading underscore
	    if (paramString.length() > 1)
	      paramString = paramString.substring(1);

	    currParamList = paramString;

            String combstr[] = pcomb.getValueList();
            if (combstr != null)
            {
	      for (int k=0; k< combstr.length; k++)
              {
                 String pstr = paramString + "_" + (new Integer(k)).toString();
                 createTemplate(jclass, jclass, pstr, combstr[k], returnType, staticFlag);
              }
            } else {
                 createTemplate(jclass, jclass, null, null, returnType, staticFlag);
            } 
      }
      return true;
   }

   /**
   * Parses the class methods and create class files
   *
   * @return           Either true or false
   */
   public boolean parseClassMethod()
   {
       Class cl = null;
       try {
         cl = Class.forName(fullclass);
       } catch (Exception e) {
         System.out.println("Could not find class file " + fullclass);
         return false;
       }
       methods = rt.getMethods(cl);
       String paramString = null;
       int staticFlag=0;
       for (int i = 0; i < methods.length; i++)
       {  
System.out.println("Parsing method: "+jclass+" -> "+methods[i].getName());
            staticFlag=0;
            Method m = methods[i];
            Class[] paramTypes = m.getParameterTypes();
            String conName = m.getName();
	    String returnType = (m.getReturnType()).getName();
            currMethod = conName;

            int mod = m.getModifiers();
            if (Modifier.isStatic(mod)) 
                   staticFlag = 1;
            else if (!(Modifier.isPublic(mod)))  
                   continue;

	    paramString = new String();
	    ParamCombination pcomb = null;
System.out.println("Number of parms: "+paramTypes.length);
            pcomb = new ParamCombination(paramTypes.length);
	    if (pcomb == null)
	    {
               System.out.println("Could not instantiate ParamCombination...\n");
	       return false;
	    }

            if (Modifier.isStatic(m.getModifiers())) 
                   staticFlag = 1;

            for (int j = 0; j < paramTypes.length; j++)
            {  
	       // Construct parameter string (param1_param2_...) 
               String pstr = paramTypes[j].getName();
	       String tstr = new String("");


               // check if param is an Array and accordingly get 
               // component type.
	       if (pstr.startsWith("["))
               {
		    int index = pstr.lastIndexOf("[") + 1;
                    tstr = "Arr" + (new Integer(index)).toString();
                    pstr = (pstr.substring(index)).trim();
                    if (pstr.length() == 1) {
                       if (pstr.compareTo("I") == 0) 
		          tstr = tstr + "int";
                       else if (pstr.compareTo("F") == 0) 
		          tstr = tstr + "float";
                       else if (pstr.compareTo("D") == 0) 
		          tstr = tstr + "double";
                       else if (pstr.compareTo("S") == 0) 
		          tstr = tstr + "short";
                       else if (pstr.compareTo("C") == 0) 
		          tstr = tstr + "char";
                       else if (pstr.compareTo("B") == 0) 
		          tstr = tstr + "byte";
                          
                    } else {
                      // Remove the leading 'L' from the string
                      pstr = pstr.substring(1);
                    }

		    // Remove trailing semicolon
 		    int slen = pstr.length();
                    pstr = pstr.substring(0, slen-1);
               }

               // Remove . from paramname (java.lang.String)
	       int idx = pstr.lastIndexOf(".");
	       if (idx != -1)
	       {
	          pstr = pstr.substring(idx + 1);
	       }
	       tstr = tstr + pstr;
	       paramString = paramString  + "_" + tstr;

	       /* get value list for parameter */
	       pcomb.addElement(getParameterValueList(tstr));
             
            }

	    // remove leading underscore
	    if (paramString.length() > 1)
	      paramString = paramString.substring(1);

            String combstr[] = pcomb.getValueList();
            if (combstr != null)
            {
	      for (int k=0; k< combstr.length; k++)
              {
                 String pstr = paramString + "_" + (new Integer(k)).toString();
                 currParamList = paramString;
                 createTemplate(jclass, conName, pstr, combstr[k], returnType, staticFlag);
              }
            } else {
                 createTemplate(jclass, conName, null, null, returnType, staticFlag);
            } 
      }
      return true;
   }


   /**
   * GetParses the class constructors and create class files
   *
   * @param      Name of the paramter class
   * @return     Vector of all permissbile values for this class
   *             viz: for String class it will be null/"dummy"
   */
   public Vector getParameterValueList(String param)
   {
       Vector result = null;

       if (param == null) return null;
       result = new Vector();

       if (param.compareTo("String") == 0)
       {
           result.addElement("null");     
           result.addElement("\"DUMMY_STRING\"");     
       } else
       if (param.compareTo("int") == 0) 
       {
           result.addElement("0");     
           result.addElement("Integer.MIN_VALUE");     
           result.addElement("Integer.MAX_VALUE");     
       } else
       if (param.compareTo("float") == 0) 
       {
           result.addElement("0");     
           result.addElement("Float.MAX_VALUE");     
           result.addElement("Float.NaN");     
       } else
       if (param.compareTo("double") == 0) 
       {
           result.addElement("0");     
           result.addElement("Double.MAX_VALUE");     
           result.addElement("Double.NaN");     
       } else
       if (param.compareTo("long") == 0) 
       {
           result.addElement("0");     
           result.addElement("Long.MIN_VALUE");     
           result.addElement("Long.MAX_VALUE");     
       } else
       if (param.compareTo("short") == 0) 
       {
           result.addElement("0");     
           result.addElement("Short.MIN_VALUE");     
           result.addElement("Short.MAX_VALUE");     
       } else
       if (param.compareTo("char") == 0) 
       {
           result.addElement("Character.MAX_VALUE");
       } else
       if (param.compareTo("boolean") == 0)
       {
           result.addElement("true");     
           result.addElement("false");     
       } else
       if (param.compareTo("byte") == 0) 
       {
           result.addElement("Byte.MIN_VALUE");     
           result.addElement("Byte.MAX_VALUE");     
       } else
       if (param.compareTo("Integer") == 0) 
       {
           result.addElement("null");
           result.addElement("new Integer(0)");     
           result.addElement("new Integer(Integer.MIN_VALUE)");     
           result.addElement("new Integer(Integer.MAX_VALUE)");     
       } else
       if (param.compareTo("Float") == 0) 
       {
           result.addElement("null");
           result.addElement("new Float(0)");     
           result.addElement("new Float(Float.MAX_VALUE)");     
           result.addElement("new Float(Float.NaN)");     
       } else
       if (param.compareTo("Double") == 0) 
       {
           result.addElement("null");
           result.addElement("new Double(0)");     
           result.addElement("new Double(Double.MAX_VALUE)");     
           result.addElement("new Double(Double.NaN)");     
       } else
       if (param.compareTo("Long") == 0) 
       {
           result.addElement("null");
           result.addElement("new Long(0)");     
           result.addElement("new Long(Long.MIN_VALUE)");     
           result.addElement("new Long(Long.MAX_VALUE)");     
       } else
       if (param.compareTo("Short") == 0) 
       {
           result.addElement("null");
           result.addElement("new Short(0)");     
           result.addElement("new Short(Short.MIN_VALUE)");     
           result.addElement("new Short(Short.MAX_VALUE)");     
       } else
       if (param.compareTo("Character") == 0) 
       {
           result.addElement("'" + (new Character(Character.MAX_VALUE)).toString() + "`");
       } else
       if (param.compareTo("Byte") == 0) 
       {
           result.addElement("null");
           result.addElement("new Byte(Byte.MIN_VALUE)");     
           result.addElement("new Byte(Byte.MAX_VALUE)");   
	} else 
	if (rules != null && rules.get(param) != null) {
	   result.addElement((String)rules.get(param));
	   result.addElement("null");  
       } else {
           result.addElement("null");
           result.addElement("NOTNULL");
       }

       return result;
   }


   /**
   * Create Template Java File
   *
   * @param  className       Name of the class file
   * @param  methodName      Name of the method 
   * @param  paramString     Concated string of parameter types separated by 
   *                         underscores.
   * @param  valueList       A concated String of values for the above 
   *                         parameters in the given order
   * @param  returnType      Return Type for the given method.
   *
   *
   * @return           void
   */
   public void createTemplate( String className, 
                               String methodName, 
                               String paramString, 
                               String valueList,
			       String returnType,
			       int    staticFlag
                             )
   {
       if (className  == null) return;
       if (methodName == null) return;
       if (returnType == null) return;
       if (paramString != null)
         if (valueList == null) return;


       String cName = new String();
       String filename = new String();
       if (paramString != null){
         cName = className + "_" + methodName + "_" + paramString; 
         filename =  cName + ".java";
       } else {
         cName = className + "_" + methodName;
         filename =  cName +  ".java";
       }
 

     DataOutputStream raf = null;
     try {
      raf = new DataOutputStream( new FileOutputStream(outputDir+filename));
     } catch (Exception e) {
         System.out.println("ERROR: Could not create File " + filename);
         return; 
     }

      try {
        String fstr = new String();
        fstr = fstr + "/**";
        fstr = fstr + "\n";
        fstr = fstr + " *";
        fstr = fstr + "\n";
        fstr = fstr + " *  @version 1.00 ";
        fstr = fstr + "\n";
        fstr = fstr + " *  @author ";
        fstr = fstr + "\n";
        fstr = fstr + " *";
        fstr = fstr + "\n";
        fstr = fstr + " *  TESTID ";
        fstr = fstr + "\n";
        fstr = fstr + " * ";
        fstr = fstr + "\n";
        fstr = fstr + " *";
        fstr = fstr + "\n";
        fstr = fstr + " */";
        fstr = fstr + "\n";
        fstr = fstr + "package org.mozilla.dom.test;";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "import java.util.*;";
        fstr = fstr + "\n";
        fstr = fstr + "import java.io.*;";
        fstr = fstr + "\n";
        fstr = fstr + "import org.mozilla.dom.test.*;";
        fstr = fstr + "\n";
        fstr = fstr + "import org.mozilla.dom.*;";
        fstr = fstr + "\n";
        fstr = fstr + "import org.w3c.dom.*;";
        fstr = fstr + "\n";
        fstr = fstr + "import org.w3c.dom.html.*;";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "public class " + cName;
        fstr = fstr + " extends BWBaseTest implements Execution";
        fstr = fstr + "\n";
        fstr = fstr + "{";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "   /**";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *  Constructor";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    */";
        fstr = fstr + "\n";
        fstr = fstr + "   public " + cName + "()";   
        fstr = fstr + "\n";
        fstr = fstr + "   {";
        fstr = fstr + "\n";
        fstr = fstr + "   }";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "   /**";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *  Starting point of application";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    *  @param   args    Array of command line arguments";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    */";
        fstr = fstr + "\n";
        fstr = fstr + "   public static void main(String[] args)"; 
        fstr = fstr + "\n";
        fstr = fstr + "   {";
        fstr = fstr + "\n";
        fstr = fstr + "   }";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "   /**";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    *  Execute Method ";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    *  @param   tobj    Object reference (Node/Document/...)";
        fstr = fstr + "\n";
        fstr = fstr + "    *  @return          true or false  depending on whether test passed or failed.";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    */";
        fstr = fstr + "\n";
        fstr = fstr + "   public boolean execute(Object tobj)";
        fstr = fstr + "\n";
        fstr = fstr + "   {"; 
        fstr = fstr + "\n";
        fstr = fstr + "      if (tobj == null)  {";
        fstr = fstr + "\n";
        fstr = fstr + "           TestLoader.logPrint(\"Object is NULL...\");";
        fstr = fstr + "\n";
        fstr = fstr + "           return BWBaseTest.FAILED;";
        fstr = fstr + "\n";
        fstr = fstr + "      }";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "      String os = System.getProperty(\"OS\");"; 
        fstr = fstr + "\n";
        fstr = fstr + "      osRoutine(os);"; 
        fstr = fstr + "\n";

/*        String pString=valueList;
        if (valueList == null)
              pString= new String("");
*/

	if (rules != null && rules.get(className) != null) 
		fstr =fstr + makeTestRoutine(className, returnType, methodName, valueList);       

        fstr = fstr + "      return BWBaseTest.PASSED;";
        fstr = fstr + "\n";
        fstr = fstr + "   }"; 
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "   /**";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *  Routine where OS specific checks are made. ";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    *  @param   os      OS Name (SunOS/Linus/MacOS/...)";
        fstr = fstr + "\n";
        fstr = fstr + "    ***********************************************************";
        fstr = fstr + "\n";
        fstr = fstr + "    *";
        fstr = fstr + "\n";
        fstr = fstr + "    */";
        fstr = fstr + "\n";
        fstr = fstr + "   private void osRoutine(String os)";
        fstr = fstr + "\n";
        fstr = fstr + "   {";
        fstr = fstr + "\n";
        fstr = fstr + "     if(os == null) return;";
        fstr = fstr + "\n";
        fstr = fstr + "\n";
        fstr = fstr + "     os = os.trim();";
        fstr = fstr + "\n";
        fstr = fstr + "     if(os.compareTo(\"SunOS\") == 0) {}";
        fstr = fstr + "\n";
        fstr = fstr + "     else if(os.compareTo(\"Linux\") == 0) {}";
        fstr = fstr + "\n";
        fstr = fstr + "     else if(os.compareTo(\"Windows\") == 0) {}";
        fstr = fstr + "\n";
        fstr = fstr + "     else if(os.compareTo(\"MacOS\") == 0) {}";
        fstr = fstr + "\n";
        fstr = fstr + "     else {}";
        fstr = fstr + "\n";
        fstr = fstr + "   }";

        fstr = fstr + "\n";
        fstr = fstr + "}";
        fstr = fstr + "\n";

        raf.write(fstr.getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + filename);
         return; 
      }

      try {
       raf.close();
      } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + filename);
       return; 
      }


     DataOutputStream dos = null;
     String txtFile = className + ".txt";
     try {
      dos = new DataOutputStream( new FileOutputStream(outputDir+txtFile));
     } catch (Exception e) {
         System.out.println("ERROR: Could not create File " + txtFile);
         return; 
     }

     String str = className + ":" + currMethod + ":" + currParamList + ":" + filename;
     outputFiles.addElement(str);
     try {
        dos.write(str.getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + txtFile);
         return; 
      }

     try {
       dos.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + txtFile);
       return; 
     }
   }



	private Method findMethod(String mn) {
		if (methods == null)
			return null;
		for (int i=0; i < methods.length; i++) {
			//System.out.println("Comparing: "+mn+" <-> "+methods[i].getName());
			if (methods[i].getName().compareTo(mn) == 0)
				return methods[i];
		}
		return null;
	}


	private String makeTestRoutine(String className, String returnType, String methodName, String valueList) {
		String result = new String("");
		String tab = "   ", tab1=tab+tab, tab2=tab1+tab, tab3=tab2+tab1;
		String robj = "", check = "";
		String valList = valueList;

		if (valueList == null)
			valList = "";
		/*Weather method returns object ?*/
		if (returnType.compareTo("void") != 0)  {
			robj = returnType + " rc = ";	
			if (returnType.indexOf('.') > 0)
				check = tab3 + "if (rc == null) return BWBaseTest.FAILED;\n";
		}
		/*Let's try to find set<->get pair for methods with one argument ...*/
		if (methodName.startsWith("set") && valueList.indexOf(',') < 0 && valueList.compareTo("null") != 0) {
			String getMethod = "get" + methodName.substring(3);
			//System.out.println("GET_METHOD "+getMethod);
			Method m;
			if ((m = findMethod(getMethod)) != null && m.getParameterTypes().length == 0) {
				String equals = ".equals";
				/* Weather return type is standard ?*/
				if (m.getReturnType().getName().indexOf('.') < 0) 
					equals = "==";
				check = tab3+"if (!(obj."+getMethod+"()"+equals+"("+valueList+"))) return BWBaseTest.FAILED;\n";
			}
		}
		result += tab1 + "Document d = (Document)tobj;\n";
		result += tab1 + "if (d != null) {\n";
		result += tab2 + "try {\n"; 
		result += tab3 + className+" obj = " + rules.get(className)+";\n";
		result += tab3 + robj + "obj." + methodName + "("+valList+");\n" + check; 
		result += tab2 + "} catch (Exception e) {\n";
		result += tab3 + "return BWBaseTest.FAILED;\n";
		result += tab2 + "}\n";
		result += tab1 + "} else {\n";
		result += tab2 + "return BWBaseTest.FAILED;\n";
		result += tab1 + "}\n";
		return result;
	}

   public static void usage()
   {
        System.out.println("Usage: java JavaTestTemplate [-d <output dir>] [-r <rules_file>] <className>");
   }

   public static void main(String args[])
   {
     if (args.length == 0)
     {
	BWJavaTemplate.usage();
        return;
     } 

     String className = null;
     String outDirectory = null;
     String rulesFile = null;

	switch(args.length) {
	case(0): 
		BWJavaTemplate.usage(); 
		return; 
	case(1): 
		if(args[0].charAt(0) == '-') {
			BWJavaTemplate.usage(); 
			return; 
		}
		className = args[0];
		break;
	default: 
		for(int i=0; i<args.length;) {	
			if(args[i].charAt(0) != '-') {
				if (className != null) {	
					BWJavaTemplate.usage();
					return; 
				}
				className = args[i];
				i++;
			} else {
				switch(args[i].charAt(1)) {
				case('d'): 
					outDirectory = args[i+1];
					i+=2;
					break;
				case('r'):
					rulesFile = args[i+1];
					i+=2;
					break;
				default:
					BWJavaTemplate.usage(); 
					return; 
				} //end of switch
			}
		}
	} //end of switch

/*
     if (args.length == 3) { 
       if (args[0].compareTo("-d") != 0) { 
          BWJavaTemplate.usage(); 
          return; 
       } else {
         outDirectory = args[1];
         className = args[2];
       }
     } else {
        outDirectory = new String(".");
        className = args[0];
     }
*/
     BWJavaTemplate jt = new BWJavaTemplate(className, outDirectory, rulesFile);
     
   }
}
