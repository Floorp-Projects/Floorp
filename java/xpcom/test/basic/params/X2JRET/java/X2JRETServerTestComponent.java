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

public class X2JRETServerTestComponent implements iX2JRETServerTestComponent {

 private static String testLocation="";
 private static String logLocation="";
 private String S="";
 private int all=0;

 public X2JRETServerTestComponent() {
  System.out.println("--[java] X2JRETServerTestComponent constructor");
 }

 public Object queryInterface(IID iid) {
  System.out.println("--[java]X2JRETServerTestComponent::queryInterface iid="+iid);
  if (iid.equals(nsISupports.IID)
		|| iid.equals(iX2JRETServerTestComponent.IID)) {
	return this;
  }else {
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
	DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/x2j.ret.server."+name)); 
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
 *	This method sets short values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public short testShort() {
  short _short=0;
  switch (all) {
	case 0: _short=java.lang.Short.MIN_VALUE; break;
	case 1: _short=1000; break;
	case 2: _short=java.lang.Short.MAX_VALUE; break;
  }
  all++; 
  S=S+new Short(_short).toString()+"\n";
  if (all==3) {
   flush("short");
  }
  return _short;
 }

/**
 *	This method sets int values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public int testLong() {
  int _int=0;
  switch (all) {
	case 0: _int=java.lang.Integer.MIN_VALUE; break;
	case 1: _int=1000; break;
	case 2: _int=java.lang.Integer.MAX_VALUE; break;
  }
  all++; 
  S=S+new Integer(_int).toString()+"\n";
  if (all==3) {
   flush("long");
  }
  return _int;
 }

/**
 *	This method sets long values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public long testLonglong() {
  long _long=0;
  switch (all) {
	case 0: _long=java.lang.Long.MIN_VALUE; break;
	case 1: _long=1000; break;
	case 2: _long=java.lang.Long.MAX_VALUE; break;
  }
  all++; 
  S=S+new Long(_long).toString()+"\n";
  if (all==3) {
   flush("longlong");
  }
  return _long;
 }

/**
 *	This method sets byte values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public byte testByte() {
  byte _byte=0;
  switch (all) {
	case 0: _byte=0; break;
	case 1: _byte=10; break;
	case 2: _byte=127; break;
  }
  all++; 
  S=S+new Byte(_byte).toString()+"\n";
  if (all==3) {
   flush("octet");
  }
  return _byte;
 }

/**
 *	This method sets short values and returns them to C++
 *	client side as unsigned short and sets java side test result,
 *	using flush(...) method.
 */
 public short testUShort() {
  short _ushort=0;
  switch (all) {
	case 0: _ushort=0; break;
	case 1: _ushort=1000; break;
	case 2: _ushort=java.lang.Short.MAX_VALUE; break;
  }
  all++; 
  S=S+new Short(_ushort).toString()+"\n";
  if (all==3) {
   flush("ushort");
  }
  return _ushort;
 }

/**
 *	This method sets int values and returns them to C++
 *	client side as unsigned int and sets java side test result,
 *	using flush(...) method.
 */
 public int testULong() {
  int _uint=0;
  switch (all) {
	case 0: _uint=0; break;
	case 1: _uint=1000; break;
	case 2: _uint=java.lang.Integer.MAX_VALUE; break;
  }
  all++; 
  S=S+new Integer(_uint).toString()+"\n";
  if (all==3) {
   flush("ulong");
  }
  return _uint;    
 }

/**
 *	This method sets long values and returns them to C++
 *	client side as unsigned long and sets java side test result,
 *	using flush(...) method.
 */
 public long testULonglong() {
  long _ulong=0;
  switch (all) {
	case 0: _ulong=0; break;
	case 1: _ulong=1000; break;
	case 2: _ulong=java.lang.Long.MAX_VALUE; break;
  }
  all++; 
  S=S+new Long(_ulong).toString()+"\n";
  if (all==3) {
   flush("ulonglong");
  }
  return _ulong;    
 }

/**
 *	This method sets float values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public float testFloat() {
  float _float=0;
  switch (all) {
	case 0: _float=java.lang.Float.MIN_VALUE; break;
	case 1: _float=1; break;
	case 2: _float=java.lang.Float.MAX_VALUE; break;
	case 3: _float=0; break;
  }
  all++; 
  S=S+new Float(_float).toString()+"\n";
  if (all==4) {
   flush("float");
  }
  return _float;    
 }

/**
 *	This method sets double values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public double testDouble() {
  double _double=0;
  switch (all) {
	case 0: _double=java.lang.Double.MIN_VALUE; break;
	case 1: _double=1; break;
	case 2: _double=java.lang.Double.MAX_VALUE; break;
	case 3: _double=0; break;
  }
  all++; 
  S=S+new Double(_double).toString()+"\n";
  if (all==4) {
   flush("double");
  }
  return _double;    
 }

/**
 *	This method sets boolean values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public boolean testBoolean() {
  boolean _bool=true;
  switch (all) {
	case 0: _bool=true; break;
	case 1: _bool=false; break;
  }
  all++; 
  S=S+new Boolean(_bool).toString()+"\n";
  if (all==2) {
   flush("boolean");
  }
  return _bool;    
 }

/**
 *	This method sets char values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public char testChar() {
  char _char='0';
  switch (all) {
	case 0: _char='0'; break;
	case 1: _char='Z'; break;
  }
  all++; 
  S=S+new Character(_char).toString()+"\n";
  if (all==2) {
   flush("char");
  }
  return _char;    
 }

/**
 *	This method sets char values and returns them to C++
 *	client side as PRUnichar and sets java side test result,
 *	using flush(...) method.
 */
 public char testWChar() {
  char _wchar='0';
  switch (all) {
	case 0: _wchar='A'; break;
	case 1: _wchar='Z'; break;
  }
  all++; 
  S=S+new Character(_wchar).toString()+"\n";
  if (all==2) {
   flush("wchar");
  }
  return _wchar;    
 }

/**
 *	This method sets string values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public String testString() {
  String _string="0";
  switch (all) {
	case 0: _string=""; break;
	case 1: _string="abc"; break;
	case 2: _string="NULL string must be here"; break;
  }
  all++; 
  S=S+_string+"\n";
  if (all==3) {
   flush("string");
  }
  return _string;    
 }

/**
 *	This method sets string values and returns them to C++
 *	client side as PRUnichar* and sets java side test result,
 *	using flush(...) method.
 */
 public String testWString() {
  String _wstring="0";
  switch (all) {
	case 0: _wstring=""; break;
	case 1: _wstring="abc"; break;
	case 2: _wstring="ctrl"; break;
  }
  all++; 
  S=S+_wstring+"\n";
  if (all==3) {
   flush("wstring");
  }
  return _wstring;    
 }

/**
 *	This method sets string array values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public String[] testStringArray(int count) {
  String[] stringArray={"iddqd","idkfa","ctrl-x"};
  S=stringArray[0]+"\n"+stringArray[1]+"\n"+stringArray[2]+"\n";
  flush("stringArray");
  return stringArray;
 }

/**
 *	This method sets int array values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public int[] testLongArray(int count) {
  int[] intArray={11,22,33};
  S=intArray[0]+"\n"+intArray[1]+"\n"+intArray[2]+"\n";
  flush("longArray");
  return intArray;
 }

/**
 *	This method sets char array values and returns them to C++
 *	client side and sets java side test result,
 *	using flush(...) method.
 */
 public char[] testCharArray(int count) {
  char[] charArray={'a','b','c'};
  S=charArray[0]+"\n"+charArray[1]+"\n"+charArray[2]+"\n";
  flush("charArray");
  return charArray;
 }

/**
 *	This method returns iX2JRETServerTestComponent object to C++
 *	client side and sets java side test result, using flush(...)
 *	method.
 */
 public iX2JRETServerTestComponent testObject() {
  return this;
 }

/**
 *	This method is invoked from C++ client side, using
 *	iX2JRETServerTestComponent object.
 *	It sets java side test result, using flush(...) method.
 */
 public void testObj() {
  S="!!!Right string!!!"+"\n";
  flush("object");
 }

/*
 public IID testIID() {
  IID _iid=new IID("5e946b90-3a31-11d5-b653-005004552ed1");
  S=_iid.toString()+"\n";
  flush("iid");
  return _iid;
 }

 public CID testCID() {
  CID _cid=new CID("5e946b90-3a31-11d5-b653-005004552ed1");
  S=_cid.toString()+"\n";
  flush("cid");
  return _cid;
 }
*/
/****************************************/
/*		/TEST METHODS		*/
/****************************************/

 static {
  try {
	Class nsISupportsStringClass = Class.forName("org.mozilla.xpcom.nsISupportsString");
	Class iX2JRETServerTestComponentClass = Class.forName("org.mozilla.xpcom.iX2JRETServerTestComponent");
	Class nsIComponentManagerClass = Class.forName("org.mozilla.xpcom.nsIComponentManager");       
	Class nsIFactoryClass = Class.forName("org.mozilla.xpcom.nsIFactory");       
	Class nsIEnumeratorClass = Class.forName("org.mozilla.xpcom.nsIEnumerator");

	InterfaceRegistry.register(nsIComponentManagerClass);
	InterfaceRegistry.register(nsIFactoryClass);
	InterfaceRegistry.register(nsIEnumeratorClass);
	InterfaceRegistry.register(iX2JRETServerTestComponentClass);
	InterfaceRegistry.register(nsISupportsStringClass);

  }catch (Exception e) {
	System.out.println(e);
  }
 }
};
