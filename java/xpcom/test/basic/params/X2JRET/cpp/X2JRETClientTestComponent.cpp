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
#include "X2JRETClientTestComponent.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringUtil.h"

 iX2JRETServerTestComponent *serverComponent;
 char* testLocation="";
 char* logLocation="";
 char* val=NULL;
 char** hash=NULL;
 PRUint8 hashCount=0;
 PseudoHash* exclusionHash;

X2JRETClientTestComponentImpl::X2JRETClientTestComponentImpl() {    
 NS_INIT_REFCNT();
 testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
 logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
 printf("DEbug:avm:X2JRETClientTestComponentImpl::X2JRETClientTestComponentImp\n");
 hash=(char**)PR_Malloc(sizeof(char*)*50);
 exclusionHash=new PseudoHash();
}

X2JRETClientTestComponentImpl::~X2JRETClientTestComponentImpl() {
}

 NS_IMPL_ISUPPORTS3(X2JRETClientTestComponentImpl,
			iX2JRETClientTestComponent,
			iClientTestComponent, iExclusionSupport);
 IMPL_PROCEED_RESULTS(X2JRETClientTestComponentImpl);
 IMPL_PSEUDOHASH(X2JRETClientTestComponentImpl);

/**
 *	This method creates an instance of server test component and
 *	transfers test and log locations to that instance.
 */
NS_IMETHODIMP X2JRETClientTestComponentImpl::Initialize(const char* serverProgID){

 nsresult rv;
 rv = nsComponentManager::CreateInstance(serverProgID,
					nsnull,
					NS_GET_IID(iX2JRETServerTestComponent),
					(void**)&serverComponent);   
 if (NS_FAILED(rv)) {
  printf("Create instance failed!!!");
  return rv;
 }

 serverComponent->SetTestLocation(testLocation, logLocation);
 return NS_OK;
}

 NS_IMETHODIMP X2JRETClientTestComponentImpl::Exclude(PRUint32 count, const char **exclusionList) {
  printf("Debug:ovk:X2JRETClientTestComponentImpl::exclude\n");
  for(int i=0;i<count;i++) {
    exclusionHash->put((char*)exclusionList[i]);
    hashCount++;
  }
  return NS_OK;
 }

/**
 *	This method executes all tests. See more comments below.
 */
NS_IMETHODIMP X2JRETClientTestComponentImpl::Execute(){

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
  if (!exclusionHash->containsKey("object")) TestObject();
//  if (!exclusionHash->containsKey("iid")) TestIID();
//  if (!exclusionHash->containsKey("cid")) TestCID();

 return NS_OK;
}
/**
 *	This method gets different short values from java side and
 *	sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestShort(){

  val=NULL;
  PRInt16  _short=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestShort(&_short);        
   val=PR_sprintf_append(val,"%d\n",_short);
  }
    
  PrintResult("x2j.ret.client.short",val);
}

/**
 *	This method gets different byte values from java side and
 *	sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestByte(){

  val=NULL;
  PRUint8  _byte=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestByte(&_byte);        
   val=PR_sprintf_append(val,"%d\n",_byte);
  }
    
  PrintResult("x2j.ret.client.octet",val);
}

/**
 *	This method gets different int values from java side and
 *	sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestLong(){

  val=NULL;
  PRInt32  _int=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestLong(&_int);        
   val=PR_sprintf_append(val,"%d\n",_int);
  }
    
  PrintResult("x2j.ret.client.long",val);
}

/**
 *	This method gets different long values from java side and
 *	sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestLonglong(){

  val=NULL;
  PRInt64  _long=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestLonglong(&_long);
   val=PR_sprintf_append(val,"%lld\n",_long);
  }
    
  PrintResult("x2j.ret.client.longlong",val);
}

/**
 *	This method gets different unsigned short values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestUShort(){

  val=NULL;
  PRUint16  _ushort=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestUShort(&_ushort);
   val=PR_sprintf_append(val,"%d\n",_ushort);
  }
    
  PrintResult("x2j.ret.client.ushort",val);
}

/**
 *	This method gets different unsigned int values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestULong(){

  val=NULL;
  PRUint32  _uint=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestULong(&_uint);
   val=PR_sprintf_append(val,"%d\n",_uint);
  }
    
  PrintResult("x2j.ret.client.ulong",val);
}

/**
 *	This method gets different unsigned long values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestULonglong(){

  val=NULL;
  PRUint64  _ulong=0;

  for(int i=0;i<3;i++) {
   serverComponent->TestULonglong(&_ulong);
   val=PR_sprintf_append(val,"%lld\n",_ulong);
  }
    
  PrintResult("x2j.ret.client.ulonglong",val);
}

/**
 *	This method gets different float values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestFloat(){

  val=NULL;
  float  _float=0;

  for(int i=0;i<4;i++) {
   serverComponent->TestFloat(&_float);
   val=PR_sprintf_append(val,"%.3e\n",_float);
  }
    
  PrintResult("x2j.ret.client.float",val);
}

/**
 *	This method gets different double values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestDouble(){

  val=NULL;
  double  _double=0;

  for(int i=0;i<4;i++) {
   serverComponent->TestDouble(&_double);
   val=PR_sprintf_append(val,"%.3e\n",_double);
  }
    
  PrintResult("x2j.ret.client.double",val);
}

/**
 *	This method gets different boolean values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestBoolean(){

  val=NULL;
  PRBool _bool=false;

  for(int i=0;i<2;i++) {
   serverComponent->TestBoolean(&_bool);
   if (_bool==PR_TRUE) {
	val=PR_sprintf_append(val,"%s\n","true");
   } else if (_bool==PR_FALSE) {
	val=PR_sprintf_append(val,"%s\n","false");
   } else val=PR_sprintf_append(val,"%s\n","strange result");
  }
    
  PrintResult("x2j.ret.client.boolean",val);
}
 
/**
 *	This method gets different char values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestChar(){

  val=NULL;
  char _char='f';  

  for(int i=0;i<2;i++) {
   serverComponent->TestChar(&_char);
   val=PR_sprintf_append(val,"%c\n",_char);
  }
    
  PrintResult("x2j.ret.client.char",val);
}

/**
 *	This method gets different PRUnichar values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestWChar(){

  val=NULL;
  PRUnichar _wchar=0;  

  for(int i=0;i<2;i++) {
   serverComponent->TestWChar(&_wchar);
   val=PR_sprintf_append(val,"%c\n",_wchar);
  }
    
  PrintResult("x2j.ret.client.wchar",val);
}

/**
 *	This method gets different char* values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestString(){

  val=NULL;
  char* _string="0";  

  for(int i=0;i<3;i++) {
   serverComponent->TestString(&_string);
   val=PR_sprintf_append(val,"%s\n",_string);
  }
    
  PrintResult("x2j.ret.client.string",val);
}

/**
 *	This method gets different PRUnichar* values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestWString(){

  val=NULL;
  PRUnichar* _wstring;

  for(int i=0;i<3;i++) {
   serverComponent->TestWString(&_wstring);
   {
    nsString str = *(new nsString(_wstring));
    NS_ALLOC_STR_BUF(aBuf,str,100)
    val=PR_sprintf_append(val,"%s\n",aBuf);
    NS_FREE_STR_BUF(aBuf)
   }
  }
    
  PrintResult("x2j.ret.client.wstring",val);
}

/**
 *	This method gets char* array values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestStringArray(){

  PRUint32 count=3;
  char** _stringArray=(char**)PR_Malloc(sizeof(char*)*count);
  const char** sA=(const char**)PR_Malloc(sizeof(const char*)*count);
  serverComponent->TestStringArray(count,&_stringArray);
  sA=(const char**) _stringArray;
  PrintResultArray("x2j.ret.client.stringArray",count,sA);
}

/**
 *	This method gets int array values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestLongArray(){

  PRUint32 count=3;
  PRInt32* _intArray=(PRInt32*)PR_Malloc(sizeof(PRInt32)*count);
  serverComponent->TestLongArray(count,&_intArray);
  PrintResultArray("x2j.ret.client.longArray",count,_intArray);
}

/**
 *	This method gets char array values from java side
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestCharArray(){

  PRUint32 count=3;
  char* _charArray=(char*)PR_Malloc(sizeof(char)*count); 
  serverComponent->TestCharArray(count,&_charArray);
  PrintResultArray("x2j.ret.client.charArray",count,_charArray);
}

/**
 *	This method gets iX2JRETServerTestComponent object from java
 *	side, tries to invoke TestObj() on java side, using this object,
 *	and sets test results for C++ side.
 */
void X2JRETClientTestComponentImpl::TestObject(){

  val=NULL;
  iX2JRETServerTestComponent* obj=NULL;

  serverComponent->TestObject(&obj);
  obj->TestObj();
  PrintResult("x2j.ret.client.object","!!!Right string!!!");
}
/*
 void X2JRETClientTestComponentImpl::TestIID() {

   const nsIID& iid=NS_GET_IID(iX2JRETServerTestComponent);
   val=NULL;

   val=iid.ToString();

   val=PR_sprintf_append(val,"%s\n",val);
   PrintResult("x2j.ret.client.iid",val);
   serverComponent->TestIID(&iid);

 }

 void X2JRETClientTestComponentImpl::TestCID() {

   NS_DEFINE_CID(cid,X2JRETCLIENTTESTCOMPONENT_CID);
   val=NULL;

   val=cid.ToString();

   val=PR_sprintf_append(val,"%s\n",val);
   PrintResult("x2j.ret.client.cid",val);
   serverComponent->TestCID(&cid);

 }
*/
