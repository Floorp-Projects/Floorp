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

public class X2JINServerTestComponent implements iX2JINServerTestComponent {

 private static String testLocation=null;
 private static String logLocation=null;
 private String S="";

 public X2JINServerTestComponent() {
  System.out.println("--[java] X2JINServerTestComponent constructor");
 }

 public Object queryInterface(IID iid) {
  System.out.println("--[java]X2JINServerTestComponent::queryInterface iid="+iid);
  if (iid.equals(nsISupports.IID)
		|| iid.equals(iX2JINServerTestComponent.IID)) {
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
      DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/x2j.in.server."+name)); 
      f.writeBytes(S);
  } catch(Exception e) {
	System.out.println("Exception during writing the file");
  }
  S="";
 }

/****************************************/
/*		TEST METHODS		*/
/****************************************/

/**
 *	This method accumulates short values, which are passed from C++
 *	client side.
 */
 public void testShort(short i) {
  S=S+new Short(i).toString()+"\n";
 }

/**
 *	This method accumulates int values, which are passed from C++
 *	client side.
 */
 public void testLong(int i) {
  S=S+new Integer(i).toString()+"\n";
 }

/**
 *	This method accumulates long values, which are passed from C++
 *	client side.
 */
 public void testLonglong(long i) {
  S=S+new Long(i).toString()+"\n";
 }

/**
 *	This method accumulates byte values, which are passed from C++
 *	client side.
 */
 public void testByte(byte i) {
  S=S+new Byte(i).toString()+"\n";
 }

/**
 *	This method accumulates short values (unsigned short on C++
 *	side), which are passed from C++ client side.
 */
 public void testUShort(short i) {
  S=S+new Short(i).toString()+"\n";
 }

/**
 *	This method accumulates int values (unsigned int on C++
 *	side), which are passed from C++ client side.
 */
 public void testULong(int i) {
  S=S+new Integer(i).toString()+"\n";
 }

/**
 *	This method accumulates long values (unsigned long on C++
 *	side), which are passed from C++ client side.
 */
 public void testULonglong(long i) {
  S=S+new Long(i).toString()+"\n";
 }

/**
 *	This method accumulates float values, which are passed from C++
 *	client side.
 */
 public void testFloat(float i) {
  S=S+new Float(i).toString()+"\n";
 }

/**
 *	This method accumulates double values, which are passed from
 *	C++ client side.
 */
 public void testDouble(double i) {
  S=S+new Double(i).toString()+"\n";
 }

/**
 *	This method accumulates boolean values, which are passed from
 *	C++ client side.
 */
 public void testBoolean(boolean i) {
  S=S+new Boolean(i).toString()+"\n";
 }

/**
 *	This method accumulates char values, which are passed from
 *	C++ client side.
 */
 public void testChar(char i) {
  S=S+new Character(i).toString()+"\n";
 }

/**
 *	This method accumulates char values (wchar on C++ side),
 *	which are passed from C++ client side.
 */
 public void testWChar(char i) {
  S=S+new Character(i).toString()+"\n";
 }

/**
 *	This method accumulates String values, which are passed from
 *	C++ client side.
 */
 public void testString(String i) {
  S=S+i+"\n";
 }

/**
 *	This method accumulates String values (wstring on C++ side),
 *	which are passed from C++ client side.
 */
 public void testWString(String i) {
  S=S+i+"\n";
 }

/**
 *	This method accumulates String array values, which are passed
 *	from C++ client side.
 *	First parameter - number of an array elements.
 */
 public void testStringArray(int count, String[] stringArray) {
  for(int j=0;j<count;j++)
    S=S+stringArray[j]+"\n";
 }

/**
 *	This method accumulates int array values, which are passed
 *	from C++ client side.
 *	First parameter - number of an array elements.
 */
 public void testLongArray(int count, int[] intArray) {
  for(int j=0;j<count;j++)
    S=S+new Integer(intArray[j]).toString()+"\n";
 }

/**
 *	This method accumulates char array values, which are passed
 *	from C++ client side.
 *	First parameter - number of an array elements.
 */
 public void testCharArray(int count, char[] charArray) {
  for(int j=0;j<count;j++)
    S=S+new Character(charArray[j]).toString()+"\n";
 }

/**
 *	This method accumulates misc values, which are passed from
 *	C++ client side.
 */
 public void testMixed(boolean _bool, char _char, byte _byte,
			short _short,  short _ushort, int _int,
			int _uint, long _long,  long _ulong,
			float _float, double _double, String _string,
			int count, int[] _intArray) {
  S=S+new Boolean(_bool).toString()+"\n";
  S=S+new Character(_char).toString()+"\n";
  S=S+new Byte(_byte).toString()+"\n";
  S=S+new Short(_short).toString()+"\n";
  S=S+new Short(_ushort).toString()+"\n";
  S=S+new Integer(_int).toString()+"\n";
  S=S+new Integer(_uint).toString()+"\n";
  S=S+new Long(_long).toString()+"\n";
  S=S+new Long(_ulong).toString()+"\n";
  S=S+new Float(_float).toString()+"\n";
  S=S+new Double(_double).toString()+"\n";
  S=S+_string+"\n";
  for(int j=0;j<count;j++)	
    S=S+new Integer(_intArray[j]).toString()+"\n";
 }

/**
 *	This method try to invoke testObj() method,
 *	using iX2JINServerTestComponent value, which is passed from
 *	C++ client side.
 */
 public void testObject(iX2JINServerTestComponent obj) {
  try{
	obj.testObj();
  } catch(Exception e) {
System.out.println("EXC in TESTOBJ");
        S=S+"Exception in TestObject: "+e+"\n";
  }
 }

/**
 *	This method sets correct result string for testObject(...)
 *	method.
 */
 public void testObj() {
System.out.println("TESTOBJ");
  S=S+"!!!Right string!!!"+"\n";
 }

/**
 *	This method gets IID value, which is passed from
 *	C++ client side.
 */
 public void testIID(IID iid) {
  S=S+iid.toString()+"\n";
 }

/**
 *	This method gets CID value, which is passed from
 *	C++ client side.
 */
 public void testCID(CID cid) {
  S=S+cid.toString()+"\n";
//CID c=new CID("1ddc5b10-9852-11d4-aa22-00a024a8bbac");
 }

/****************************************/
/*		/TEST METHODS		*/
/****************************************/



 static {
  try {
	Class nsISupportsStringClass = Class.forName("org.mozilla.xpcom.nsISupportsString");
	Class iX2JINServerTestComponentClass = Class.forName("org.mozilla.xpcom.iX2JINServerTestComponent");
	Class nsIComponentManagerClass = Class.forName("org.mozilla.xpcom.nsIComponentManager");       
	Class nsIFactoryClass = Class.forName("org.mozilla.xpcom.nsIFactory");       
	Class nsIEnumeratorClass = Class.forName("org.mozilla.xpcom.nsIEnumerator");

	InterfaceRegistry.register(nsIComponentManagerClass);
	InterfaceRegistry.register(nsIFactoryClass);
	InterfaceRegistry.register(nsIEnumeratorClass);
	InterfaceRegistry.register(iX2JINServerTestComponentClass);
	InterfaceRegistry.register(nsISupportsStringClass);
  } catch (Exception e) {
	System.out.println(e);
  }
 }

};
