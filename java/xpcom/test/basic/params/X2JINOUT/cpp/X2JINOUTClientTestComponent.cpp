/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

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

#include "plstr.h"
#include "prenv.h"
#include "prmem.h"
#include "stdio.h"
#include "nsIComponentManager.h"
#include "X2JINOUTClientTestComponent.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringUtil.h"

 iX2JINOUTServerTestComponent *serverComponent, *obj;
 char* testLocation=NULL;
 char* logLocation=NULL;
 char* val=NULL;
 char* xval=NULL;
 char** hash=NULL;
 PRUint8 hashCount=0;
 PseudoHash* exclusionHash;

X2JINOUTClientTestComponentImpl::X2JINOUTClientTestComponentImpl() {    
 NS_INIT_REFCNT();
 testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
 logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
 printf("DEbug:avm:X2JINOUTClientTestComponentImpl::X2JINOUTClientTestComponentImp\n");
 InitStackVars();
  hash=(char**)PR_Malloc(sizeof(char*)*50);
  exclusionHash=new PseudoHash();
}

X2JINOUTClientTestComponentImpl::~X2JINOUTClientTestComponentImpl() {
}

 NS_IMPL_ISUPPORTS3(X2JINOUTClientTestComponentImpl,
			iX2JINOUTClientTestComponent,
			iClientTestComponent, iExclusionSupport);
 IMPL_PROCEED_RESULTS(X2JINOUTClientTestComponentImpl);
 IMPL_VAR_STACKS(X2JINOUTClientTestComponentImpl);
 IMPL_PSEUDOHASH(X2JINOUTClientTestComponentImpl);

/**
 *	This method creates an instance of server test component and
 *	transfers test and log locations to that instance.
 */
NS_IMETHODIMP X2JINOUTClientTestComponentImpl::Initialize(const char* serverProgID){

 nsresult rv;
 rv = nsComponentManager::CreateInstance(serverProgID,
					nsnull,
					NS_GET_IID(iX2JINOUTServerTestComponent),
					(void**)&serverComponent);   
 if (NS_FAILED(rv)) {
   printf("Create instance failed!!!");
   return rv;
 }

 serverComponent->SetTestLocation(testLocation, logLocation);
 return NS_OK;
}

 NS_IMETHODIMP X2JINOUTClientTestComponentImpl::Exclude(PRUint32 count, const char **exclusionList) {
  printf("Debug:ovk:X2JINOUTClientTestComponentImpl::exclude\n");
  for(int i=0;i<count;i++) {
    exclusionHash->put((char*)exclusionList[i]);
    hashCount++;
  }
  return NS_OK;
 }

/**
 *	This method executes all tests. See more comments below.
 */
NS_IMETHODIMP X2JINOUTClientTestComponentImpl::Execute(){

  if (!exclusionHash->containsKey("octet")) TestByte();
  if (!exclusionHash->containsKey("short")) TestShort();
  if (!exclusionHash->containsKey("long")) TestLong();
  if (!exclusionHash->containsKey("longlong")) TestLonglong();
  if (!exclusionHash->containsKey("ushort")) TestUShort();
  if (!exclusionHash->containsKey("ulong")) TestULong();
  if (!exclusionHash->containsKey("ulonglong")) TestULonglong();
  if (!exclusionHash->containsKey("float")) TestFloat();
  if (!exclusionHash->containsKey("double")) TestDouble();
  if (!exclusionHash->containsKey("boolean")) TestBoolean();
  if (!exclusionHash->containsKey("char")) TestChar();
  if (!exclusionHash->containsKey("wchar")) TestWChar();
  if (!exclusionHash->containsKey("string")) TestString();
  if (!exclusionHash->containsKey("wstring")) TestWString();
  if (!exclusionHash->containsKey("stringArray")) TestStringArray();
  if (!exclusionHash->containsKey("longArray")) TestLongArray();
  if (!exclusionHash->containsKey("charArray")) TestCharArray();
  if (!exclusionHash->containsKey("mixed")) TestMixed();
  if (!exclusionHash->containsKey("object")) TestObject();
//  if (!exclusionHash->containsKey("iid")) TestIID();
//  if (!exclusionHash->containsKey("cid")) TestCID();

 return NS_OK;
}
/**
 *	This method sets C++ side short values and transfers them
 *	to java side.
 *	After that this method gets setted short values from java side and
 *	sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestShort(){

  PRInt16 i;
  val=NULL;
  xval=NULL;

  while(PRInt16Vars.size()) {
	i = PRInt16Vars.top();
	PRInt16Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestShort(&i);
	xval=PR_sprintf_append(xval,"%d\n",i);
  } 

  PrintResult("x2j.inout.client.short",val);
  PrintResult("x2j.inout.xclient.short",xval);
  serverComponent->Flush("short");
}

/**
 *	This method sets C++ side byte values and transfers them
 *	to java side.
 *	After that this method gets setted byte values from java side
 *	and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestByte(){

  PRUint8 i;
  val=NULL;
  xval=NULL;

  while(PRUint8Vars.size()) {
	i = PRUint8Vars.top();
	PRUint8Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestByte(&i);
	xval=PR_sprintf_append(xval,"%d\n",i);
  } 

  PrintResult("x2j.inout.client.octet",val);
  PrintResult("x2j.inout.xclient.octet",xval);
  serverComponent->Flush("octet");
}

/**
 *	This method sets C++ side int values and transfers them
 *	to java side.
 *	After that this method gets setted int values from java side
 *	and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestLong(){

  PRInt32 i;
  val=NULL;
  xval=NULL;

  while(PRInt32Vars.size()) {
	i = PRInt32Vars.top();
	PRInt32Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestLong(&i);
	xval=PR_sprintf_append(xval,"%d\n",i);
  } 

  PrintResult("x2j.inout.client.long",val);
  PrintResult("x2j.inout.xclient.long",xval);
  serverComponent->Flush("long");
}

/**
 *	This method sets C++ side long values and transfers them
 *	to java side.
 *	After that this method gets setted long values from java side
 *	and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestLonglong(){

  PRInt64 i;
  val=NULL;
  xval=NULL;

  while(PRInt64Vars.size()) {
	i = PRInt64Vars.top();
	PRInt64Vars.pop();
	val=PR_sprintf_append(val,"%lld\n",i);
	serverComponent->TestLonglong(&i);
	xval=PR_sprintf_append(xval,"%lld\n",i);
  } 

  PrintResult("x2j.inout.client.longlong",val);
  PrintResult("x2j.inout.xclient.longlong",xval);
  serverComponent->Flush("longlong");
}

/**
 *	This method sets C++ side unsigned short values and transfers
 *	them to java side.
 *	After that this method gets setted unsigned short values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestUShort(){

  PRUint16 i;
  val=NULL;
  xval=NULL;

  while(PRUint16Vars.size()) {
	i = PRUint16Vars.top();
	PRUint16Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestUShort(&i);
	xval=PR_sprintf_append(xval,"%d\n",i);
  } 

  PrintResult("x2j.inout.client.ushort",val);
  PrintResult("x2j.inout.xclient.ushort",xval);
  serverComponent->Flush("ushort");
}

/**
 *	This method sets C++ side unsigned int values and transfers
 *	them to java side.
 *	After that this method gets setted unsigned int values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestULong(){

  PRUint32 i;
  val=NULL;
  xval=NULL;

  while(PRUint32Vars.size()) {
	i = PRUint32Vars.top();
	PRUint32Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestULong(&i);
	xval=PR_sprintf_append(xval,"%d\n",i);
  } 

  PrintResult("x2j.inout.client.ulong",val);
  PrintResult("x2j.inout.xclient.ulong",xval);
  serverComponent->Flush("ulong");
}

/**
 *	This method sets C++ side unsigned long values and transfers
 *	them to java side.
 *	After that this method gets setted unsigned long values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestULonglong(){

  PRUint64 i;
  val=NULL;
  xval=NULL;

  while(PRUint64Vars.size()) {
	i = PRUint64Vars.top();
	PRUint64Vars.pop();
	val=PR_sprintf_append(val,"%lld\n",i);
	serverComponent->TestULonglong(&i);
	xval=PR_sprintf_append(xval,"%lld\n",i);
  } 

  PrintResult("x2j.inout.client.ulonglong",val);
  PrintResult("x2j.inout.xclient.ulonglong",xval);
  serverComponent->Flush("ulonglong");
}

/**
 *	This method sets C++ side float values and transfers
 *	them to java side.
 *	After that this method gets setted float values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestFloat(){

  float i;
  val=NULL;
  xval=NULL;

  while(floatVars.size()) {
	i = floatVars.top();
	floatVars.pop();
	val=PR_sprintf_append(val,"%.3e\n",i);
	serverComponent->TestFloat(&i);
	xval=PR_sprintf_append(xval,"%.3e\n",i);
  } 

  PrintResult("x2j.inout.client.float",val);
  PrintResult("x2j.inout.xclient.float",xval);
  serverComponent->Flush("float");
}

/**
 *	This method sets C++ side double values and transfers
 *	them to java side.
 *	After that this method gets setted double values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestDouble(){

  double i;
  val=NULL;
  xval=NULL;

  while(doubleVars.size()) {
	i = doubleVars.top();
	doubleVars.pop();
	val=PR_sprintf_append(val,"%.3e\n",i);
	serverComponent->TestDouble(&i);
	xval=PR_sprintf_append(xval,"%.3e\n",i);
  } 

  PrintResult("x2j.inout.client.double",val);
  PrintResult("x2j.inout.xclient.double",xval);
  serverComponent->Flush("double");
}

/**
 *	This method sets C++ side boolean values and transfers
 *	them to java side.
 *	After that this method gets setted boolean values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestBoolean(){

  PRBool i;
  val=NULL;
  xval=NULL;

  while(PRBoolVars.size()) {
	i = PRBoolVars.top();
	PRBoolVars.pop();
	if (i==PR_TRUE) val=PR_sprintf_append(val,"true\n");
	 else if (i==PR_FALSE) val=PR_sprintf_append(val,"false\n");
	  else val=PR_sprintf_append(val,"strange result\n");
	serverComponent->TestBoolean(&i);
	if (i==PR_TRUE) xval=PR_sprintf_append(xval,"true\n");
	 else if (i==PR_FALSE) xval=PR_sprintf_append(xval,"false\n");
	  else xval=PR_sprintf_append(xval,"strange result\n");
  } 

  PrintResult("x2j.inout.client.boolean",val);
  PrintResult("x2j.inout.xclient.boolean",xval);
  serverComponent->Flush("boolean");
}
 
/**
 *	This method sets C++ side char values and transfers
 *	them to java side.
 *	After that this method gets setted char values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestChar(){

  char i;
  val=NULL;
  xval=NULL;

  while(charVars.size()) {
	i = charVars.top();
	charVars.pop();
	val=PR_sprintf_append(val,"%c\n",i);
	serverComponent->TestChar(&i);
	xval=PR_sprintf_append(xval,"%c\n",i);
  } 

  PrintResult("x2j.inout.client.char",val);
  PrintResult("x2j.inout.xclient.char",xval);
  serverComponent->Flush("char");
}

/**
 *	This method sets C++ side PRUnichar values and transfers
 *	them to java side.
 *	After that this method gets setted PRUnichar values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestWChar(){

  PRUnichar i;
  val=NULL;
  xval=NULL;

  while(wcharVars.size()) {
	i = wcharVars.top();
	wcharVars.pop();
	val=PR_sprintf_append(val,"%c\n",i);
	serverComponent->TestWChar(&i);
	xval=PR_sprintf_append(xval,"%c\n",i);
  } 

  PrintResult("x2j.inout.client.wchar",val);
  PrintResult("x2j.inout.xclient.wchar",xval);
  serverComponent->Flush("wchar");
}

/**
 *	This method sets C++ side char* values and transfers
 *	them to java side.
 *	After that this method gets setted char* values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestString(){

  char* i;
  val=NULL;
  xval=NULL;

  while(stringVars.size()) {
	i = stringVars.top();
	stringVars.pop();
	val=PR_sprintf_append(val,"%s\n",i);
	serverComponent->TestString(&i);
	xval=PR_sprintf_append(xval,"%s\n",i);
  } 

  PrintResult("x2j.inout.client.string",val);
  PrintResult("x2j.inout.xclient.string",xval);
  serverComponent->Flush("string");
}

/**
 *	This method sets C++ side PRUnichar* values and transfers
 *	them to java side.
 *	After that this method gets setted PRUnichar* values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestWString(){

  PRUnichar* _wstring;
  val=NULL; xval=NULL;

  char* _string="";
  nsCString* cstr=new nsCString(_string);
  _wstring=cstr->ToNewUnicode();
  nsString str = *(new nsString(_wstring));
  {
   NS_ALLOC_STR_BUF(aBuf1,str,100)
   val=PR_sprintf_append(val,"%s\n",aBuf1);
   NS_FREE_STR_BUF(aBuf1)
  }
  serverComponent->TestWString(&_wstring);     
  str = *(new nsString(_wstring));
  {
   NS_ALLOC_STR_BUF(aBuf2,str,100)
   xval=PR_sprintf_append(xval,"%s\n",aBuf2);
   NS_FREE_STR_BUF(aBuf2)
  }

  _string="Test string.";
  cstr=new nsCString(_string);
  _wstring=cstr->ToNewUnicode();
  str = *(new nsString(_wstring));
  {
   NS_ALLOC_STR_BUF(aBuf3,str,100)
   val=PR_sprintf_append(val,"%s\n",aBuf3);
   NS_FREE_STR_BUF(aBuf3)
  }
  serverComponent->TestWString(&_wstring);     
  str = *(new nsString(_wstring));
  {
   NS_ALLOC_STR_BUF(aBuf4,str,100)
   xval=PR_sprintf_append(xval,"%s\n",aBuf4);
   NS_FREE_STR_BUF(aBuf4)
  }
  
  PrintResult("x2j.inout.client.wstring",val);
  PrintResult("x2j.inout.xclient.wstring",xval);
}
 
/**
 *	This method sets C++ side char* array and transfers
 *	it to java side.
 *	After that this method gets setted char* array from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestStringArray(){

  int count=3;
  char** _stringArray=(char**)PR_Malloc(sizeof(char*)*count);
  _stringArray[0]="aaa";
  _stringArray[1]="bbb";
  _stringArray[2]="ccc";

  PrintResultArray("x2j.inout.client.stringArray",count,(const char**)_stringArray);
  serverComponent->TestStringArray(count,&_stringArray);
  PrintResultArray("x2j.inout.xclient.stringArray",count,(const char**)_stringArray);
}

/**
 *	This method sets C++ side int array and transfers
 *	it to java side.
 *	After that this method gets setted int array from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestLongArray(){

  int count=3;
  PRInt32* _intArray=(PRInt32*)PR_Malloc(sizeof(PRInt32)*count);
  _intArray[0]=3;
  _intArray[1]=4;
  _intArray[2]=5;

  PrintResultArray("x2j.inout.client.longArray",count, _intArray);
  serverComponent->TestLongArray(count,&_intArray);
  PrintResultArray("x2j.inout.xclient.longArray",count, _intArray);
}

/**
 *	This method sets C++ side char array and transfers
 *	it to java side.
 *	After that this method gets setted char array from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestCharArray(){

  int count=3;
  char* _charArray=(char*)PR_Malloc(sizeof(char)*count);
  _charArray[0]=120;
  _charArray[1]=121;
  _charArray[2]=122;

  PrintResultArray("x2j.inout.client.charArray",count,_charArray);
  serverComponent->TestCharArray(count,&_charArray);
  PrintResultArray("x2j.inout.xclient.charArray",count,_charArray);
}               

/**
 *	This method sets C++ side misc values and transfers
 *	them to java side.
 *	After that this method gets setted misc values from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
void X2JINOUTClientTestComponentImpl::TestMixed(){

  val=NULL; xval=NULL;
  PRBool _bool=true;
  char _char='s';
  PRUint8 _byte=3;
  PRInt16 _short=3;	PRUint16 _ushort=3;
  PRInt32 _int=3;	PRUint32 _uint=3;
  PRInt64 _long=3;	PRUint64 _ulong=3;
  float _float=7;
  double _double=7;
  char* _string="abc";
  int count=3;
  PRInt32* _intArray=(PRInt32*)PR_Malloc(sizeof(PRInt32)*count);
  _intArray[0]=33;
  _intArray[1]=44;
  _intArray[2]=55;


  PrintResultMixed("x2j.inout.client.mixed",_bool, _char, _byte, _short,
		   _ushort, _int, _uint, _long, _ulong, _float, 
 		   _double, _string, count, _intArray);
  serverComponent->TestMixed(&_bool, &_char, &_byte, &_short, &_ushort,
			      &_int, &_uint, &_long, &_ulong, &_float, 
			      &_double, &_string, count, &_intArray);
  PrintResultMixed("x2j.inout.xclient.mixed",_bool, _char, _byte, _short,
		   _ushort, _int, _uint, _long, _ulong, _float, 
 		   _double, _string, count, _intArray);

}

/**
 *	This method sets C++ side iX2JINOUTServerTestComponent value
 *	and transfers it to java side.
 *	After that this method gets setted iX2JINOUTServerTestComponent
 *	value from java side.
 *	TestObj2() is invoked on java side, using that value.
 */
void X2JINOUTClientTestComponentImpl::TestObject(){

  PrintResult("x2j.inout.client.object","!!!Right string!!!"); 
  serverComponent->TestObject(&serverComponent);
  serverComponent->TestObject2(&obj);
  obj->TestObj2();
}
 

/**
 *	This method sets C++ side test results for IID and transfers
 *	IID object to java side.
 *	After that this method gets setted char array from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
// void X2JINOUTClientTestComponentImpl::TestIID() {
/*
   const nsIID& iid=NS_GET_IID(iX2JINOUTServerTestComponent);
   val=NULL;

   val=PR_sprintf_append(val,"%s\n",iid.ToString());
   PrintResult("x2j.inout.client.iid",val);
   serverComponent->TestIID(&iid);
   xval=PR_sprintf_append(xval,"%s\n",iid.ToString());
   PrintResult("x2j.inout.xclient.iid",xval);
*/
// }

/**
 *	This method sets C++ side test results for CID and transfers
 *	CID object to java side.
 *	After that this method gets setted char array from
 *	java side and sets test results for C++ side.
 *	Both values must be equal.
 */
/* void X2JINOUTClientTestComponentImpl::TestCID() {

   NS_DEFINE_CID(cid,X2JINOUTCLIENTTESTCOMPONENT_CID);
   val=NULL;

   val=PR_sprintf_append(val,"%s\n",cid.ToString());
   PrintResult("x2j.inout.client.cid",val);
   serverComponent->TestCID(&cid);
   xval=PR_sprintf_append(val,"%s\n",cid.ToString());
   PrintResult("x2j.inout.xclient.cid",xval);

 }*/
