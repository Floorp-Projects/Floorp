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
#include "X2JINClientTestComponent.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringUtil.h"

 iX2JINServerTestComponent* serverComponent;
 char* testLocation=NULL;
 char* logLocation=NULL;
 char* val=NULL;
 char** hash=NULL;
 PRUint8 hashCount=0;
 PseudoHash* exclusionHash;

 X2JINClientTestComponentImpl::X2JINClientTestComponentImpl() {    
  NS_INIT_REFCNT();
  testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
  logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
  printf("X2JINClientTestComponentImpl::X2JINClientTestComponentImp\n");
  InitStackVars();
  hash=(char**)PR_Malloc(sizeof(char*)*50);
  exclusionHash=new PseudoHash();
 }

 X2JINClientTestComponentImpl::~X2JINClientTestComponentImpl() {

 }

 NS_IMPL_ISUPPORTS3(X2JINClientTestComponentImpl, iX2JINClientTestComponent,
			iClientTestComponent,iExclusionSupport);
 IMPL_PROCEED_RESULTS(X2JINClientTestComponentImpl);
 IMPL_VAR_STACKS(X2JINClientTestComponentImpl);
 IMPL_PSEUDOHASH(X2JINClientTestComponentImpl);

/**
 *	This method creates an instance of server test component and
 *	transfers test and log locations to that instance.
 */
 NS_IMETHODIMP X2JINClientTestComponentImpl::Initialize(const char* serverProgID){
  nsresult rv;
  rv = nsComponentManager::CreateInstance(serverProgID,
					nsnull,
					NS_GET_IID(iX2JINServerTestComponent),
					(void**)&serverComponent);
  if (NS_FAILED(rv)) {
	printf("Create instance failed!!!");
	return rv;
  }

  serverComponent->SetTestLocation(testLocation, logLocation);
  return NS_OK;
 }

 NS_IMETHODIMP X2JINClientTestComponentImpl::Exclude(PRUint32 count, const char **exclusionList) {
  printf("Debug:ovk:X2JINClientTestComponentImpl::exclude\n");
  for(int i=0;i<count;i++) {
    exclusionHash->put((char*)exclusionList[i]);
    hashCount++;
  }
  return NS_OK;
 }

/**
 *	This method executes all tests. See more comments below.
 */
 NS_IMETHODIMP X2JINClientTestComponentImpl::Execute(){

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
  if (!exclusionHash->containsKey("nsIIDRef")) TestIID();
  if (!exclusionHash->containsKey("nsCIDRef")) TestCID();

  return NS_OK;
 }

/**
 *	This method sets C++ side test results for short and transfers
 *	different short values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestShort() {

  PRInt16 i;
  val=NULL;

  while(PRInt16Vars.size()) {
	i = PRInt16Vars.top();
	PRInt16Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestShort(i);        
  } 

  PrintResult("x2j.in.client.short",val);
  serverComponent->Flush("short");
 }

/**
 *	This method sets C++ side test results for int and transfers
 *	different int values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestLong() {

  PRInt32 i;
  val=NULL;

  while(PRInt32Vars.size()) {
	i = PRInt32Vars.top();
	PRInt32Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestLong(i);        
  } 

  PrintResult("x2j.in.client.long",val);
  serverComponent->Flush("long");
 }

/**
 *	This method sets C++ side test results for long and transfers
 *	different long values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestLonglong() {

  PRInt64 i;
  val=NULL;

  while(PRInt64Vars.size()) {
	i = PRInt64Vars.top();
	PRInt64Vars.pop();
	val=PR_sprintf_append(val,"%lld\n",i);
	serverComponent->TestLonglong(i);        
  } 

  PrintResult("x2j.in.client.longlong",val);
  serverComponent->Flush("longlong");
 }

/**
 *	This method sets C++ side test results for byte and transfers
 *	different byte values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestByte() {

  PRUint8 i;
  val=NULL;

  while(PRUint8Vars.size()) {
	i = PRUint8Vars.top();
	PRUint8Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestByte(i);        
  } 

  PrintResult("x2j.in.client.octet",val);
  serverComponent->Flush("octet");
 } 

/**
 *	This method sets C++ side test results for unsigned short and
 *	transfers different ushort values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestUShort() {

  PRUint16 i;
  val=NULL;

  while(PRUint16Vars.size()) {
	i = PRUint16Vars.top();
	PRUint16Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestUShort(i);        
  } 

  PrintResult("x2j.in.client.ushort",val);
  serverComponent->Flush("ushort");
 }

/**
 *	This method sets C++ side test results for unsigned int and
 *	transfers different uint values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestULong() {

  PRUint32 i;
  val=NULL;

  while(PRUint32Vars.size()) {
	i = PRUint32Vars.top();
	PRUint32Vars.pop();
	val=PR_sprintf_append(val,"%d\n",i);
	serverComponent->TestULong(i);        
  } 

  PrintResult("x2j.in.client.ulong",val);
  serverComponent->Flush("ulong");
 }

/**
 *	This method sets C++ side test results for unsigned long and
 *	transfers different ulong values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestULonglong() {
 
  PRUint64 i;
  val=NULL;

  while(PRUint64Vars.size()) {
	i = PRUint64Vars.top();
	PRUint64Vars.pop();
	val=PR_sprintf_append(val,"%lld\n",i);
	serverComponent->TestULonglong(i);        
  } 

  PrintResult("x2j.in.client.ulonglong",val);
  serverComponent->Flush("ulonglong");
 }

/**
 *	This method sets C++ side test results for float and transfers
 *	different float values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestFloat() {

  float i;
  val=NULL;

  while(floatVars.size()) {
	i = floatVars.top();
	floatVars.pop();
	val=PR_sprintf_append(val,"%.3e\n",i);
	serverComponent->TestFloat(i);        
  } 

  PrintResult("x2j.in.client.float",val);
  serverComponent->Flush("float");
 }
 
/**
 *	This method sets C++ side test results for double and transfers
 *	different double values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestDouble() {

  double i;
  val=NULL;

  while(doubleVars.size()) {
	i = doubleVars.top();
	doubleVars.pop();
	val=PR_sprintf_append(val,"%.3e\n",i);
	serverComponent->TestDouble(i);        
  } 

  PrintResult("x2j.in.client.double",val);
  serverComponent->Flush("double");
 }

/**
 *	This method sets C++ side test results for boolean and transfers
 *	different boolean values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestBoolean() {

  PRBool i;
  val=NULL;

  while(PRBoolVars.size()) {
	i = PRBoolVars.top();
	PRBoolVars.pop();
	if (i==PR_TRUE) val=PR_sprintf_append(val,"true\n");
	  else if (i==PR_FALSE) val=PR_sprintf_append(val,"false\n");
		else val=PR_sprintf_append(val,"strange result\n");
	serverComponent->TestBoolean(i);        
  } 

  PrintResult("x2j.in.client.boolean",val);
  serverComponent->Flush("boolean");
 }

/**
 *	This method sets C++ side test results for char and transfers
 *	different char values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestChar() {

  char i;
  val=NULL;

  while(charVars.size()) {
	i = charVars.top();
	charVars.pop();
	val=PR_sprintf_append(val,"%c\n",i);
	serverComponent->TestChar(i);        
  } 

  PrintResult("x2j.in.client.char",val);
  serverComponent->Flush("char");
 }

/**
 *	This method sets C++ side test results for PRUnichar and transfers
 *	different PRUnichar values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestWChar() {

  PRUnichar i;
  val=NULL;

  while(wcharVars.size()) {
	i = wcharVars.top();
	wcharVars.pop();
	val=PR_sprintf_append(val,"%c\n",i);
	serverComponent->TestWChar(i);        
  } 

  PrintResult("x2j.in.client.wchar",val);
  serverComponent->Flush("wchar");
 }

/**
 *	This method sets C++ side test results for char* and transfers
 *	different char* values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestString() {

  char* i;
  val=NULL;

  while(stringVars.size()) {
	i = stringVars.top();
	stringVars.pop();
	val=PR_sprintf_append(val,"%s\n",i);
	serverComponent->TestString(i);        
  } 

  PrintResult("x2j.in.client.string",val);
  serverComponent->Flush("string");
 }

/**
 *	This method sets C++ side test results for PRUnichar* and transfers
 *	different PRUnichar* values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestWString() {
   val=NULL;

   PRUnichar* _wstring;
   char* _string="Test wstring";

   nsCString* cstr=new nsCString(_string);
   _wstring=cstr->ToNewUnicode();

   nsString str = *(new nsString(_wstring));
 
   NS_ALLOC_STR_BUF(aBuf,str,14)
   val=PR_sprintf_append(val,"%s\n",aBuf);
   NS_FREE_STR_BUF(aBuf)

   serverComponent->TestWString(_wstring);

   PrintResult("x2j.in.client.wstring",val);
   serverComponent->Flush("wstring");
 }

/**
 *	This method sets C++ side test results for char* array and transfers
 *	char* array to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestStringArray() {

   PRUint32 count=3;
   const char** _stringArray=(const char**)PR_Malloc(sizeof(const char*)*count);
  _stringArray[0]="ctrl-f";
  _stringArray[1]="iddqd";
  _stringArray[2]="idkfa";

   PrintResultArray("x2j.in.client.stringArray",count,_stringArray);
   serverComponent->TestStringArray(count,_stringArray);
   serverComponent->Flush("stringArray");

 }

/**
 *	This method sets C++ side test results for int array and transfers
 *	int array to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestLongArray() {

   PRUint32 count=3;
   PRInt32* _intArray=(PRInt32*)PR_Malloc(sizeof(PRInt32)*count);
   _intArray[0]=3;
   _intArray[1]=4;
   _intArray[2]=5;

   PrintResultArray("x2j.in.client.longArray",count,_intArray);
   serverComponent->TestLongArray(count,_intArray);
   serverComponent->Flush("longArray");
 }

/**
 *	This method sets C++ side test results for char array and transfers
 *	char array to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestCharArray() {

   PRUint32 count=3;
   char* _charArray=(char*)PR_Malloc(sizeof(char)*count);
   _charArray[0]='x';
   _charArray[1]='y';
   _charArray[2]='z';

   PrintResultArray("x2j.in.client.charArray",count,_charArray);
   serverComponent->TestCharArray(count,_charArray);
   serverComponent->Flush("charArray");
 }

/**
 *	This method sets C++ side test results for misc values and transfers
 *	these values to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestMixed() {
   PRBool _bool=true;
   char _char='s';
   PRUint8 _byte=3;
   PRInt16 _short=3;
   PRUint16 _ushort=3;
   PRInt32 _int=3;
   PRUint32 _uint=3;
   PRInt64 _long=3;
   PRUint64 _ulong=3;
   float _float=7;
   double _double=7;
   char* _string="abc";
   PRUint32 count=3;
   PRInt32* _intArray=(PRInt32*)PR_Malloc(sizeof(PRInt32)*count);
   _intArray[0]=8;
   _intArray[1]=9;
   _intArray[2]=10;

   PrintResultMixed("x2j.in.client.mixed",_bool, _char, _byte, _short,
			_ushort, _int, _uint, _long, _ulong, _float, 
			_double, _string, count, _intArray);

   serverComponent->TestMixed(_bool, _char, _byte, _short, _ushort,
				_int, _uint, _long, _ulong, _float, 
				_double, _string, count, _intArray);
   serverComponent->Flush("mixed");
 }

/**
 *	This method sets C++ side test results for object and transfers
 *	serverComponent object to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestObject() {
   PrintResult("x2j.in.client.object","!!!Right string!!!");
   serverComponent->TestObject(serverComponent);
   serverComponent->Flush("object");
 }

/**
 *	This method sets C++ side test results for IID and transfers
 *	IID object to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestIID() {

   const nsIID& iid=NS_GET_IID(iX2JINServerTestComponent);
   val=NULL;

   val=iid.ToString();

   val=PR_sprintf_append(val,"%s\n",val);
   PrintResult("x2j.in.client.iid",val);
   serverComponent->TestIID(iid);
   serverComponent->Flush("iid");
 }

/**
 *	This method sets C++ side test results for CID and transfers
 *	CID object to java side.
 *	At the end of this method the flush(...) method is invoked to set
 *	results on java side.
 */
 void X2JINClientTestComponentImpl::TestCID() {

   NS_DEFINE_CID(cid,X2JINCLIENTTESTCOMPONENT_CID);
   val=NULL;

   val=cid.ToString();

   val=PR_sprintf_append(val,"%s\n",val);
   PrintResult("x2j.in.client.cid",val);
   serverComponent->TestCID(cid);
   serverComponent->Flush("cid");
 }
