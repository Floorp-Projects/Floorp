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
#include "stdio.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "J2XOUTServerTestComponent.h"
#include "prmem.h"
#include "nsString.h"
#include "nsStringUtil.h"

char* testLocation=NULL;
char* logLocation=NULL;
char* fBuffer=NULL;
PRUint8 end_of_data = 112;
int all=0;

J2XOUTServerTestComponentImpl::J2XOUTServerTestComponentImpl()
{    
    NS_INIT_REFCNT();
    printf("DEbug:avm:J2XOUTServerTestComponentImpl::J2XOUTServerTestComponentImp\n");
    testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
    logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
    if((testLocation == NULL)||(logLocation == NULL)) {
        fprintf(stderr,"ERROR: %s or %s isn't set !", BC_TEST_LOCATION_VAR_NAME, BC_LOG_LOCATION_VAR_NAME);
    }
    InitStackVars();
}

J2XOUTServerTestComponentImpl::~J2XOUTServerTestComponentImpl()
{
}

NS_IMPL_ISUPPORTS1(J2XOUTServerTestComponentImpl, iJ2XOUTServerTestComponent);
IMPL_PROCEED_RESULTS(J2XOUTServerTestComponentImpl)
IMPL_VAR_STACKS(J2XOUTServerTestComponentImpl)

NS_IMETHODIMP J2XOUTServerTestComponentImpl::GetTestLocation(char **tLocation, char **lLocation) {
    *tLocation = testLocation;
    *lLocation = logLocation;
    return NS_OK;
}
/**

 * Method Flush(const char *type) is used to flush content 
 * of fBuffer variable to file with result.
 * Param: used to specify the type of tests. e.g short, string, ...

 */

NS_IMETHODIMP J2XOUTServerTestComponentImpl::Flush(const char *type) {
    char* fileName = NULL;
    fileName = PR_sprintf_append(fileName,"j2x.out.server.%s",type);
    if(fBuffer) {
        PrintResult(fileName,fBuffer);
        PR_smprintf_free(fBuffer);
        fBuffer = NULL;
    }
    PR_smprintf_free(fileName);
    return NS_OK;
}



/**

 * Gets next value from internal stack of PRInt16 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRInt16 variable, that should be used to return value

 */

NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestShort(PRInt16 *i) {
    if(PRInt16Vars.size()) {
        *i = PRInt16Vars.top();
         PRInt16Vars.pop();
         fBuffer = PR_sprintf_append(fBuffer,"%hd\n",*i);
    } else {
        fprintf(stderr,"PRInt16Vars.size() is null\n");
        *i = end_of_data;
    }
    return NS_OK;
}

/**

 * Gets next value from internal stack of PRInt32 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRInt32 variable, that should be used to return value

 */
 
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestLong(PRInt32 *i) {
    if(PRInt32Vars.size()) {
        *i = PRInt32Vars.top();
        PRInt32Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%ld\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**

 * Gets next value from internal stack of PRInt64 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRInt64 variable, that should be used to return value

 */

NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestLonglong(PRInt64 *i) {
    if(PRInt64Vars.size()) {
        *i = PRInt64Vars.top();
        PRInt64Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%lld\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of PRUint8 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRUint8 variable, that should be used to return value

 */
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestByte(PRUint8 *i) {
    if(PRUint8Vars.size()) {
        *i = PRUint8Vars.top();
        PRUint8Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of PRUint16 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRUint16 variable, that should be used to return value

 */
  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestUShort(PRUint16 *i) {
    if(PRUint16Vars.size()) {
        *i = PRUint16Vars.top();
        PRUint16Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%hu\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of PRUint32 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRUint32 variable, that should be used to return value

 */ 
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestULong(PRUint32 *i) {
    if(PRUint32Vars.size()) {
        *i = PRUint32Vars.top();
        PRUint32Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%lu\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of PRUint64 variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to PRUint64 variable, that should be used to return value

 */
  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestULonglong(PRUint64 *i) {
    if(PRUint64Vars.size()) {
        *i = PRUint64Vars.top();
        PRUint64Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%llu\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of float variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to float variable, that should be used to return value

 */
  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestFloat(float *i) {
    if(floatVars.size()) {
        *i = floatVars.top();
        floatVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of double variables,
 * append it fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to double variable, that should be used to return value

 */

NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestDouble(double *i) {
    if(doubleVars.size()) {
        *i = doubleVars.top();
        doubleVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
    } else {
        *i = end_of_data;
    }
    return NS_OK;
}

/**
   
 * Gets next value from internal stack of boolean variables,
 * append it to fBuffer for future saving and return it via out parameter i
 * Parameter i :This is pointer to boolean variable, that should be used to return value

 */
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestBoolean(PRBool *i) {
    if(PRBoolVars.size()) {
        *i = PRBoolVars.top();
        PRBoolVars.pop();
        if(*i == PR_TRUE) {
            fBuffer = PR_sprintf_append(fBuffer,"%s\n","true");
        }else {
            fBuffer = PR_sprintf_append(fBuffer,"%s\n","false");
        }
    }
    return NS_OK;
}



NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestChar(char *i) {
    *i='Z';
    fBuffer = PR_sprintf_append(fBuffer,"%c\n",*i);
    return NS_OK;
}

  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestWChar(PRUnichar *i) {
    *i='Z';
    fBuffer = PR_sprintf_append(fBuffer,"%c\n",*i);
    return NS_OK;
}
  
/**
   
 * Returns some string value via out parameter i
 * Parameter i :This is double pointer to char variable, that should be used to return string value

 */
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestString(char **i) {
    if(stringVars.size()) {
        *i = stringVars.top();
        stringVars.pop();
//fprintf(stderr,"C++==>%s\n",*i);
        fBuffer = PR_sprintf_append(fBuffer,"%s\n",*i);
    } else {
        *i = "112";
    }
    return NS_OK;


//    char* str = "Some string";
//    PrintResult("j2x.out.server.string",fbuffer);
//    *i = PL_strdup(str);
//    return NS_OK;
}

/**
   
 * Returns some unicode string value via out parameter i
 * Parameter i :This is double pointer to PRUnichar variable, that should be used to return string value

 */
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestWString(PRUnichar **i) {
    //Verification code.
    if(all==0) {
    char* str = "Some test string";
    nsCString* cStr = new nsCString(str);
    *i = cStr->ToNewUnicode();
	nsString nsStr = *(new nsString(*i));
	NS_ALLOC_STR_BUF(aBuf,nsStr,100);
        fBuffer = PR_sprintf_append(fBuffer,"%s\n",aBuf);
	NS_FREE_STR_BUF(aBuf);
     }	
    if(all==1) {
    char* str = NULL;
    nsCString* cStr = new nsCString(str);
    *i = cStr->ToNewUnicode();
	nsString nsStr = *(new nsString(*i));
	NS_ALLOC_STR_BUF(aBuf,nsStr,100);
        fBuffer = PR_sprintf_append(fBuffer,"%s\n",aBuf);
	NS_FREE_STR_BUF(aBuf);
     }	

//    if(all==2) Flush("wstring");
	all++;
    return NS_OK;
    //
//    PrintResult("j2x.out.server.wstring",str);
//    return NS_OK;
}
/**
   
 * Fills array of strings and return it.
 * Parameter count : PRUint32 value used to cpecify length of array
 * Parameter valueArray: char*** variable. Used to return array of strings

 */
  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestStringArray(PRUint32 count, char ***valueArray) {
    char* baseString = "This is base string N ";
    *valueArray = (char**)PR_Calloc(sizeof(char*), count);
    for(int i=0;i<count;i++) {
        fprintf(stderr,"I is %d\n",i);
        (*valueArray)[i] = PR_smprintf("%s %d",baseString,i);   
    }
    for(int j=0;j<count;j++) {
      fBuffer = PR_sprintf_append(fBuffer,"%s\n",(*valueArray)[j]);
    }  
    Flush("stringArray");
    return NS_OK;
}
/**
   
 * Fills array of PRInt32 and return it.
 * Parameter count : PRUint32 value used to cpecify length of array
 * Parameter longArray: PRInt32** variable. Used to return array of PRInt32

 */
  
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestLongArray(PRUint32 count, PRInt32 **longArray) {
    *longArray = (PRInt32*)PR_Calloc(sizeof(PRInt32*),count);
    for(int i=0;i<count;i++) {
        (*longArray)[i] = 65600 + i; //Intend, that count is less than 2^16 :)
    }
    PrintResultArray("j2x.out.server.longArray",count,*longArray);
    return NS_OK;
}

/**
   
 * Fills array of PRInt32 and return it.
 * Parameter count : PRUint32 value used to cpecify length of array
 * Parameter valueArray: char** variable. Used to return array of chars

 */ 
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestCharArray(PRUint32 count, char **valueArray) {
    *valueArray = (char*)PR_Calloc(sizeof(char*), count);
    for(int i=0;i<count;i++) {
        (*valueArray)[i] = (65 + i)%96 + 32;
    }
    PrintResultArray("j2x.out.server.charArray",count,*valueArray);
    return NS_OK;
}

/**
   
 * Fills many variables of different types and return them.
 
 */ 

NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestMixed(PRBool *bBool,/* char cChar, */PRUint8 *nByte, PRInt16 *nShort, PRUint16 *nUShort, PRInt32 *nLong, PRUint32 *nULong, PRInt64 *nHyper, PRUint64 *nUHyper, float *fFloat, double *fDouble, char **aString, PRUint32 count, PRInt32 **longArray) {
    *bBool=true;
    *nByte=3;
    *nShort=3;      *nUShort=3;
    *nLong=3;       *nULong=3;
    *nHyper=3;      *nUHyper=3;
    *fFloat=7;
    *fDouble=7;
    *aString=PL_strdup("abc");
    *longArray = (PRInt32*)PR_Calloc(sizeof(PRInt32*),count);
    for(int i=0;i<count;i++) {
        (*longArray)[i] = 65600 + i; //Intend, that count is less than 2^16 :)
    }
    PrintResultMixed("j2x.out.server.mixed",*bBool, (char)'A',*nByte, *nShort,
                   *nUShort, *nLong, *nULong, *nHyper, *nUHyper, *fFloat, 
                   *fDouble, *aString, count, *longArray);
    return NS_OK;
}
/**
   
 * Invoke method GetTestObjectString(char** str) and saves 
 * identification string, as result.Then return "this" as object
 * Parameter obj: iJ2XOUTServerTestComponent ** variable. Used to return object

 */ 
 
NS_IMETHODIMP J2XOUTServerTestComponentImpl::TestObject(iJ2XOUTServerTestComponent **obj) {
    char * str;
    GetTestObjectString(&str);
    *obj = this;
    PrintResult("j2x.out.server.object",str);
    return NS_OK;
}
/**

 * Auxiliary method, that used just for object indentification
 * Invoked from client and from server, then identification strings compared.

 */ 
 
NS_IMETHODIMP J2XOUTServerTestComponentImpl::GetTestObjectString(char** str) {
    *str = "String Ident";
    return NS_OK;
}

