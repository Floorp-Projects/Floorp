/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

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
 Client QA Team, St. Petersburg, Russia
*/

import org.mozilla.xpcom.*;
import java.lang.*;
import java.lang.reflect.*;
import java.io.*;

public class X2JOUTServerTestComponent implements iX2JOUTServerTestComponent {

 private static String testLocation="";
 private static String logLocation="";
 private String S="";
 private int all=0;
 private short _s=0;
 private short _us=0;
 private int _i=0;
 private int _ui=0;
 private long _l=0;
 private long _ul=0;
 private float _f=0;
 private double _d=0;

 public X2JOUTServerTestComponent() {
  System.out.println("--[java] X2JOUTServerTestComponent constructor");
 }

 public Object queryInterface(IID iid) {
  System.out.println("--[java]X2JOUTServerTestComponent::queryInterface iid="+iid);
  if ( iid.equals(nsISupports.IID)
		|| iid.equals(iX2JOUTServerTestComponent.IID)) {
	return this;
  } else {
	return null;
  }
 }

/**
 *	This method sets locations, which are passed from C++ client
 *	side.
 *	First parameter - test location,
 *	Second parameter - log location. 
 */
 public void setTestLocation(String location, String location2) {
  testLocation=location;
  logLocation=location2;
 } 

/**
 *	This method writes server side test result to a log file.
 *	These test results were accumulated in apropriated test
 *	methods. This method takes tested type as parameter.
 */
 public void flush(String name) {
  try{
	DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/x2j.out.server."+name)); 
	f.writeBytes(S);
  } catch(Exception e) {
	System.out.println("Exception during writing the file");
  }
  S="";
  all=0;
 }

/****************************************/
/*		TEST METHODS		*/
/****************************************/

/**
 *	This method sets short values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testShort(short[] i) {
  switch (all) {
	case 0: i[0]=new Short(_s).MIN_VALUE; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Short(_s).MAX_VALUE; break;
  }
  all++; 
  S=S+new Short(i[0]).toString()+"\n";
  if (all==3) { 
   flush("short");
  }
 }

/**
 *	This method sets int values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testLong(int[] i) {
  switch (all) {
	case 0: i[0]=new Integer(_i).MIN_VALUE; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Integer(_i).MAX_VALUE; break;
  }
  all++; 
  S=S+new Integer(i[0]).toString()+"\n";
  if (all==3) {
   flush("long");
  }
 }

/**
 *	This method sets long values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testLonglong(long[] i) {
  switch (all) {
	case 0: i[0]=new Long(_l).MIN_VALUE; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Long(_l).MAX_VALUE; break;
  }
  all++; 
  S=S+new Long(i[0]).toString()+"\n";
  if (all==3) {
   flush("longlong");
  }
 }

/**
 *	This method sets byte values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testByte(byte[] i) {
  switch (all) {
	case 0: i[0]=0; break;
	case 1: i[0]=10; break;
	case 2: i[0]=127; break;
  }
  all++; 
  S=S+new Byte(i[0]).toString()+"\n";
  if (all==3) {
   flush("octet");
  }
 }

/**
 *	This method sets short values, which will pass to C++
 *	client side as unsigned short and sets test result,
 *	using flush(...) method.
 */
 public void testUShort(short[] i) {
  switch (all) {
	case 0: i[0]=0; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Short(_s).MAX_VALUE; break;
  }
  all++; 
  S=S+new Short(i[0]).toString()+"\n";
  if (all==3) {
   flush("ushort");
  }
 }

/**
 *	This method sets int values, which will pass to C++
 *	client side as unsigned int and sets test result,
 *	using flush(...) method.
 */
 public void testULong(int[] i) {
  switch (all) {
	case 0: i[0]=0; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Integer(_i).MAX_VALUE; break;
  }
  all++; 
  S=S+new Integer(i[0]).toString()+"\n";
  if (all==3) {
   flush("ulong");
  }
 }

/**
 *	This method sets long values, which will pass to C++
 *	client side as unsigned long and sets test result,
 *	using flush(...) method.
 */
 public void testULonglong(long[] i) {
  switch (all) {
	case 0: i[0]=0; break;
	case 1: i[0]=1000; break;
	case 2: i[0]=new Long(_l).MAX_VALUE; break;
  }
  all++; 
  S=S+new Long(i[0]).toString()+"\n";
  if (all==3) {
   flush("ulonglong");
  }
 }

/**
 *	This method sets float values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testFloat(float[] i) {
  switch (all) {
	case 0: i[0]=new Float(_f).MIN_VALUE; break;
	case 1: i[0]=1; break;
	case 2: i[0]=new Float(_f).MAX_VALUE; break;
  }
  all++; 
  S=S+new Float(i[0]).toString()+"\n";
  if (all==3) {
   flush("float");
  }
 }

/**
 *	This method sets double values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testDouble(double[] i) {
  switch (all) {
	case 0: i[0]=new Double(_d).MIN_VALUE; break;
	case 1: i[0]=1; break;
	case 2: i[0]=new Double(_d).MAX_VALUE; break;
  }
  all++; 
  S=S+new Double(i[0]).toString()+"\n";
  if (all==3) {
   flush("double");
  }
 }

/**
 *	This method sets boolean values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testBoolean(boolean[] i) {
  switch (all) {
	case 0: i[0]=true; break;
	case 1: i[0]=false; break;
  }
  all++; 
  S=S+new Boolean(i[0]).toString()+"\n";
  if (all==2) {
   flush("boolean");
  }
 }

/**
 *	This method sets char values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testChar(char[] i) {
  switch (all) {
	case 0: i[0]='0'; break;
	case 1: i[0]='Z'; break;
  }
  all++; 
  S=S+new Character(i[0]).toString()+"\n";
  if (all==2) {
   flush("char");
  }
 }

/**
 *	This method sets char values, which will pass to C++
 *	client side as PRUnichar and sets test result,
 *	using flush(...) method.
 */
 public void testWChar(char[] i) {
  switch (all) {
	case 0: i[0]='A'; break;
	case 1: i[0]='Z'; break;
  }
  all++; 
  S=S+new Character(i[0]).toString()+"\n";
  if (all==2) {
   flush("wchar");
  }
 }

/**
 *	This method sets string values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testString(String[] i) {
  switch (all) {
	case 0: i[0]=""; break;
	case 1: i[0]=null; break;
	case 2: i[0]="abc"; break;
  }
  all++; 
  S=S+i[0]+"\n";
  if (all==3) {
   flush("string");
  }
 }

/**
 *	This method sets string values, which will pass to C++
 *	client side as PRUnichar* and sets test result,
 *	using flush(...) method.
 */
 public void testWString(String[] i) {
  switch (all) {
	case 0: i[0]=""; break;
	case 1: i[0]="Null must be here"; break;
	case 2: i[0]="abc"; break;
  }
  all++; 
  S=S+i[0]+"\n";
  if (all==3) {
   flush("wstring");
  }
 }

/**
 *	This method sets string array, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testStringArray(int count, String[][] valueArray) {
  String[] returnArray={"qqq","aaa","zzz"};
  S=returnArray[0]+"\n"+returnArray[1]+"\n"+returnArray[2]+"\n";
  valueArray[0]=returnArray;
  flush("stringArray");
 }

/**
 *	This method sets int array, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testLongArray(int count, int[][] intArray) {
  int[] returnArray={1,2,3};
  S=returnArray[0]+"\n"+returnArray[1]+"\n"+returnArray[2]+"\n";
  intArray[0]=returnArray;
  flush("longArray");
 }

/**
 *	This method sets char array, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testCharArray(int count, char[][] charArray) {
  char[] returnArray={'a','b','c'};
  S=returnArray[0]+"\n"+returnArray[1]+"\n"+returnArray[2]+"\n";
  charArray[0]=returnArray;
  flush("charArray");
 }

/**
 *	This method sets misc values, which will pass to C++
 *	client side and sets test result, using flush(...) method.
 */
 public void testMixed(boolean[] _bool, char[] _char, byte[] _byte,
			short[] _short,  short[] _ushort, int[] _int,
			int[] _uint, long[] _long,  long []_ulong,
			float[] _float, double[] _double, String[] _string,
			int count, int[][] _intArray) {

  int[] returnArray={1,2,3};
  _intArray[0]=returnArray;
  _bool[0]=true;
  _char[0]='q';
  _byte[0]=10;
  _short[0]=200; _ushort[0]=200;
  _int[0]=3000; _uint[0]=3000;
  _long[0]=40000; _ulong[0]=40000;
  _float[0]=500000; _double[0]=6000000;
  _string[0]="iddqd";
  
  S=new Boolean(_bool[0]).toString()+"\n"+
	new Character(_char[0]).toString()+"\n"+
	new Byte(_byte[0]).toString()+"\n"+
	new Short(_short[0]).toString()+"\n"+new Short(_ushort[0]).toString()+"\n"+
	new Integer(_int[0]).toString()+"\n"+new Integer(_uint[0]).toString()+"\n"+
	new Long(_long[0]).toString()+"\n"+new Long(_ulong[0]).toString()+"\n"+
	new Float(_float[0]).toString()+"\n"+
	new Double(_double[0]).toString()+"\n"+_string[0]+"\n";
       
  for(int i=0;i<count;i++)
   S=S+new Integer(returnArray[i]).toString()+"\n";

  flush("mixed");
 }

/**
 *	This method sets iX2JOUTServerTestComponent object,
 *	which will pass to C++ client side.
 */
 public void testObject(iX2JOUTServerTestComponent[] obj) {
  obj[0]=this;
 }

/**
 *	This method sets test result, when is invoked from C++ side.
 */
 public void testObj() {
  S="!!!Right string!!!"+"\n";
  flush("object");
 }
/*
 public void testIID(IID[] iid) {
  S=iid.toString()+"\n";
  flush("iid");
 }
*/

/*
 public void testCID(CID[] cid) {
  S=cid.toString()+"\n";
  flush("cid");
 }
*/

/****************************************/
/*		/TEST METHODS		*/
/****************************************/

 static {
  try {
	Class nsISupportsStringClass = Class.forName("org.mozilla.xpcom.nsISupportsString");
	Class iX2JOUTServerTestComponentClass = Class.forName("org.mozilla.xpcom.iX2JOUTServerTestComponent");
	Class nsIComponentManagerClass = Class.forName("org.mozilla.xpcom.nsIComponentManager");       
	Class nsIFactoryClass = Class.forName("org.mozilla.xpcom.nsIFactory");       
	Class nsIEnumeratorClass = Class.forName("org.mozilla.xpcom.nsIEnumerator");

	InterfaceRegistry.register(nsIComponentManagerClass);
	InterfaceRegistry.register(nsIFactoryClass);
	InterfaceRegistry.register(nsIEnumeratorClass);
	InterfaceRegistry.register(iX2JOUTServerTestComponentClass);
	InterfaceRegistry.register(nsISupportsStringClass);

  } catch (Exception e) {
	System.out.println(e);
  }
 }

};
