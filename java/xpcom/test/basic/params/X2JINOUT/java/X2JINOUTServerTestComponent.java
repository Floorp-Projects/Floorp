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

public class X2JINOUTServerTestComponent implements iX2JINOUTServerTestComponent {

 private static String testLocation=null;
 private static String logLocation=null;
 private int all=0;
 private String S="";

 public X2JINOUTServerTestComponent() {
  System.out.println("--[java] X2JINOUTServerTestComponent constructor");
 }

 public Object queryInterface(IID iid) {
  System.out.println("--[java]X2JINOUTServerTestComponent::queryInterface iid="+iid);
  if ( iid.equals(nsISupports.IID)
		|| iid.equals(iX2JINOUTServerTestComponent.IID)) {
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
	DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/x2j.inout.server."+name)); 
	f.writeBytes(S);
  }catch(Exception e) {
	System.out.println("Exception during writing the file");
  }
  S="";
  all=0;
 }

/****************************************/
/*		TEST METHODS		*/
/****************************************/

/**
 *	This method accumulates short values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testShort(short[] i) {
  S=S+new Short(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates int values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testLong(int[] i) {
  S=S+new Integer(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates long values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testLonglong(long[] i) {
  S=S+new Long(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates byte values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testByte(byte[] i) {
  S=S+new Byte(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates short values, which are passed from C++
 *	client side and transfers them back to C++ side as unsigned short.
 */
 public void testUShort(short[] i) {
  S=S+new Short(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates int values, which are passed from C++
 *	client side and transfers them back to C++ side as unsigned int.
 */
 public void testULong(int[] i) {
  S=S+new Integer(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates long values, which are passed from C++
 *	client side and transfers them back to C++ side as unsigned long.
 */
 public void testULonglong(long[] i) {
  S=S+new Long(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates float values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testFloat(float[] i) {
  S=S+new Float(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates double values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testDouble(double[] i) {
  S=S+new Double(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates boolean values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testBoolean(boolean[] i) {
  S=S+new Boolean(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates char values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testChar(char[] i) {
  S=S+new Character(i[0]).toString()+"\n";
 }

/**
 *	This method accumulates char values, which are passed from C++
 *	client side and transfers them back to C++ side as PRUnichar.
 */
 public void testWChar(char[] i) {
  S=S+i[0]+"\n";
 }

/**
 *	This method accumulates string values, which are passed from C++
 *	client side and transfers them back to C++ side.
 */
 public void testString(String[] i) {
  S=S+i[0]+"\n";
 }

/**
 *	This method accumulates string values, which are passed from C++
 *	client side and transfers them back to C++ side as PRUnichar*.
 */
 public void testWString(String[] i) {
  S=S+i[0]+"\n";
  all++;
  if (all==2) {
   flush("wstring");
  }
 }

/**
 *	This method accumulates string array values, which are passed
 *	from C++ client side and transfers them back to C++ side.
 */
 public void testStringArray(int count, String[][] stringArray) {
  for(int i=0;i<count;i++)
   S=S+stringArray[0][i]+"\n";        
  flush("stringArray");
 }

/**
 *	This method accumulates int array values, which are passed
 *	from C++ client side and transfers them back to C++ side.
 */
 public void testLongArray(int count, int[][] intArray) {
  for(int i=0;i<count;i++)
   S=S+new Integer(intArray[0][i]).toString()+"\n";        
  flush("longArray");
 }

/**
 *	This method accumulates char array values, which are passed
 *	from C++ client side and transfers them back to C++ side.
 */
 public void testCharArray(int count, char[][] charArray) {
  for(int i=0;i<count;i++)
   S=S+charArray[0][i]+"\n";        
  flush("charArray");
 }

/**
 *	This method accumulates misc values, which are passed
 *	from C++ client side and transfers them back to C++ side.
 */
 public void testMixed(boolean[] _bool, char[] _char, byte[] _byte,
			short[] _short,  short[] _ushort, int[] _int,
			int[] _uint, long[] _long,  long []_ulong,
			float[] _float, double[] _double, String[] _string,
			int count, int[][] _intArray) {

  S=S+new Boolean(_bool[0]).toString()+"\n"+
	new Character(_char[0]).toString()+"\n"+
	new Byte(_byte[0]).toString()+"\n"+
	new Short(_short[0]).toString()+"\n"+
	new Short(_ushort[0]).toString()+"\n"+
	new Integer(_int[0]).toString()+"\n"+
	new Integer(_uint[0]).toString()+"\n"+
	new Long(_long[0]).toString()+"\n"+
	new Long(_ulong[0]).toString()+"\n"+
	new Float(_float[0]).toString()+"\n"+
	new Double(_double[0]).toString()+"\n"+
	_string[0]+"\n";
  for(int i=0;i<count;i++)	
   S=S+new Integer(_intArray[0][i]).toString()+"\n";
  flush("mixed");
 }

/**
 *	This method gets iX2JINOUTServerTestComponent object, which is
 *	passed from C++ client side and invokes testObj() method to set
 *	java side test result.
 */
 public void testObject(iX2JINOUTServerTestComponent[] obj) {
  obj[0].testObj();
 }

/**
 *	This method sets first java side test result.
 */
 public void testObj() {
  S="!!!Right string!!!";	
  flush("object");
 }

/**
 *	This method sets iX2JINOUTServerTestComponent object, which will
 *	be pass to C++ client side.
 */
 public void testObject2(iX2JINOUTServerTestComponent[] obj) {
  obj[0]=this;
 }

/**
 *	This method sets second java side test result.
 */
 public void testObj2() {
  try{
	DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/x2j.inout.xclient.object")); 
	f.writeBytes("!!!Right string!!!");
  } catch(Exception e) {
	System.out.println("Exception during writing the file");
  }
 }

/**
 *	This method gets IID value, which is passed from
 *	C++ client side and transfers them back to C++ side..
 */
/* public void testIID(IID[] iid) {
  S=S+iid.toString()+"\n";
  flush("iid");
 }*/

/**
 *	This method gets CID value, which is passed from
 *	C++ client side and transfers them back to C++ side..
 */
/* public void testCID(CID[] cid) {
  S=S+cid.toString()+"\n";
  flush("cid");
 }*/


/****************************************/
/*		/TEST METHODS		*/
/****************************************/

 static {
  try {
	Class nsISupportsStringClass = Class.forName("org.mozilla.xpcom.nsISupportsString");
	Class iX2JINOUTServerTestComponentClass = Class.forName("iX2JINOUTServerTestComponent");
	Class nsIComponentManagerClass = Class.forName("org.mozilla.xpcom.nsIComponentManager");
	Class nsIFactoryClass = Class.forName("org.mozilla.xpcom.nsIFactory");
	Class nsIEnumeratorClass = Class.forName("org.mozilla.xpcom.nsIEnumerator");

	InterfaceRegistry.register(nsIComponentManagerClass);
	InterfaceRegistry.register(nsIFactoryClass);
	InterfaceRegistry.register(nsIEnumeratorClass);
	InterfaceRegistry.register(iX2JINOUTServerTestComponentClass);
	InterfaceRegistry.register(nsISupportsStringClass);
  }catch (Exception e) {
	System.out.println(e);
  }
 }
};
