/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include <stdio.h>
#include <fstream.h>
#include <ctype.h>
#include "plhash.h"
#include "FileGen.h"
#include "IdlSpecification.h"
#include "IdlObject.h"
#include "IdlVariable.h"
#include "IdlParameter.h"
#include "IdlAttribute.h"
#include "IdlFunction.h"
#include "IdlInterface.h"

static const char *kNPLStr =  \
"/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-\n"
" *\n"
" * The contents of this file are subject to the Netscape Public License\n"
" * Version 1.0 (the \"NPL\"); you may not use this file except in\n" 
" * compliance with the NPL.  You may obtain a copy of the NPL at\n"
" * http://www.mozilla.org/NPL/\n"
" *\n"
" * Software distributed under the NPL is distributed on an \"AS IS\" basis,\n"
" * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL\n"
" * for the specific language governing rights and limitations under the\n"
" * NPL.\n"
" *\n"
" * The Initial Developer of this code under the NPL is Netscape\n"
" * Communications Corporation.  Portions created by Netscape are\n"
" * Copyright (C) 1998 Netscape Communications Corporation.  All Rights\n"
" * Reserved.\n"
" */\n";
static const char *kDisclaimerStr = "/* AUTO-GENERATED. DO NOT EDIT!!! */\n";
static const char *kObjTypeStr = "nsIDOM%s*";
static const char *kObjTypePtrStr = "nsIDOM%sPtr";
static const char *kUuidStr = "NS_IDOM%s_IID";

FileGen::FileGen()
{
    mOutputFile = new ofstream();
}

FileGen::~FileGen()
{
    delete mOutputFile;
}

int
FileGen::OpenFile(char *aFileName, char *aOutputDirName,
                  const char *aFilePrefix, const char *aFileSuffix)
{
  char file_buf[512];

  if (aOutputDirName) {
    strcpy(file_buf, aOutputDirName);
    strcat(file_buf, "/");
  }
  else {
    file_buf[0] = '\0';
  }
  strcat(file_buf, aFilePrefix);
  strcat(file_buf, strtok(aFileName, "."));
  strcat(file_buf, ".");
  strcat(file_buf, aFileSuffix);

  mOutputFile->open(file_buf);
  return mOutputFile->is_open();
}

void
FileGen::CloseFile()
{
  mOutputFile->close();
  mOutputFile->clear();
}

void            
FileGen::GenerateNPL()
{
  *mOutputFile << kNPLStr;
  *mOutputFile << kDisclaimerStr;
}

void
FileGen::GetVariableTypeForMethodLocal(char *aBuffer, IdlVariable &aVariable)
{
    switch (aVariable.GetType()) {
      case TYPE_BOOLEAN:
        strcpy(aBuffer, "PRBool");
        break;
      case TYPE_LONG:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_SHORT:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_ULONG:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_USHORT:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_CHAR:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_INT:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_UINT:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_STRING:
        strcpy(aBuffer, "nsAutoString");
        break;
      case TYPE_OBJECT:
        sprintf(aBuffer, kObjTypePtrStr, aVariable.GetTypeName());
        break;
      default:
        // XXX Fail for other cases
        break;
    }
}

void
FileGen::GetVariableTypeForLocal(char *aBuffer, IdlVariable &aVariable)
{
    switch (aVariable.GetType()) {
      case TYPE_BOOLEAN:
        strcpy(aBuffer, "PRBool");
        break;
      case TYPE_LONG:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_SHORT:
        strcpy(aBuffer, "PRInt16");
        break;
      case TYPE_ULONG:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_USHORT:
        strcpy(aBuffer, "PRUint16");
        break;
      case TYPE_CHAR:
        strcpy(aBuffer, "PRUint8");
        break;
      case TYPE_INT:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_UINT:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_STRING:
        strcpy(aBuffer, "nsAutoString");
        break;
      case TYPE_OBJECT:
        sprintf(aBuffer, kObjTypeStr, aVariable.GetTypeName());
        break;
      default:
        // XXX Fail for other cases
        break;
    }
}

void
FileGen::GetVariableTypeForParameter(char *aBuffer, IdlVariable &aVariable)
{
    switch (aVariable.GetType()) {
      case TYPE_BOOLEAN:
        strcpy(aBuffer, "PRBool");
        break;
      case TYPE_LONG:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_SHORT:
        strcpy(aBuffer, "PRInt16");
        break;
      case TYPE_ULONG:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_USHORT:
        strcpy(aBuffer, "PRUint16");
        break;
      case TYPE_CHAR:
        strcpy(aBuffer, "PRUint8");
        break;
      case TYPE_INT:
        strcpy(aBuffer, "PRInt32");
        break;
      case TYPE_UINT:
        strcpy(aBuffer, "PRUint32");
        break;
      case TYPE_STRING:
        strcpy(aBuffer, "nsString&");
        break;
      case TYPE_OBJECT:
        sprintf(aBuffer, kObjTypeStr, aVariable.GetTypeName());
        break;
      default:
        // XXX Fail for other cases
        break;
    }
}

void
FileGen::GetParameterType(char *aBuffer, IdlParameter &aParameter)
{
  GetVariableTypeForParameter(aBuffer, aParameter);
  
  switch (aParameter.GetAttribute()) {
    case IdlParameter::OUTPUT:
    case IdlParameter::INOUT:
      if (aParameter.GetType() != TYPE_STRING) {
        strcat (aBuffer, "*");
      }
      break;
  }
}

void
FileGen::GetInterfaceIID(char *aBuffer, char *aInterfaceName)
{
  char buf[256];

  strcpy(buf, aInterfaceName);
  StrUpr(buf);

  sprintf(aBuffer, kUuidStr, buf);
}

void
FileGen::GetInterfaceIID(char *aBuffer, IdlInterface &aInterface)
{
  GetInterfaceIID(aBuffer, aInterface.GetName());
}

void
FileGen::GetCapitalizedName(char *aBuffer, IdlObject &aObject)
{
  strcpy(aBuffer, aObject.GetName());
  if ((aBuffer[0] >= 'a') && (aBuffer[0] <= 'z')) {
    aBuffer[0] += ('A' - 'a');
  }
}

void 
FileGen::StrUpr(char *aBuffer)
{
  char *cur_ptr = aBuffer;

  while (*cur_ptr != 0) {
    if ((*cur_ptr >= 'a') && (*cur_ptr <= 'z')) {
      *cur_ptr += ('A' - 'a');
    }
    cur_ptr++;
  }
}

void
FileGen::EnumerateAllObjects(IdlSpecification &aSpec, 
                             PLHashEnumerator aEnumerator,
                             void *aArg,
                             PRBool aOnlyPrimary)
{
  PLHashTable *htable = PL_NewHashTable(10, PL_HashString,
                                        PL_CompareStrings, 
                                        PL_CompareValues,
                                        (PLHashAllocOps *)NULL, NULL);
  
  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);

    if (((i == 0) || !aOnlyPrimary) &&
        !PL_HashTableLookup(htable, iface->GetName())) {
      PL_HashTableAdd(htable, iface->GetName(), (void *)1);
    }

    int a, acount = iface->AttributeCount();
    for (a = 0; a < acount; a++) {
      IdlAttribute *attr = iface->GetAttributeAt(a);
      
      if ((attr->GetType() == TYPE_OBJECT) &&
          !PL_HashTableLookup(htable, attr->GetTypeName())) {
        PL_HashTableAdd(htable, attr->GetTypeName(), (void *)1);
      }
    }

    int m, mcount = iface->FunctionCount();
    for (m = 0; m < mcount; m++) {
      IdlFunction *func = iface->GetFunctionAt(m);
      IdlVariable *rval = func->GetReturnValue();

      if ((rval->GetType() == TYPE_OBJECT) &&
          !PL_HashTableLookup(htable, rval->GetTypeName())) {
        PL_HashTableAdd(htable, rval->GetTypeName(), (void *)1);
      }

      int p, pcount = func->ParameterCount();
      for (p = 0; p < pcount; p++) {
        IdlParameter *param = func->GetParameterAt(p);
        
        if ((param->GetType() == TYPE_OBJECT) &&
            !PL_HashTableLookup(htable, param->GetTypeName())) {
          PL_HashTableAdd(htable, param->GetTypeName(), (void *)1);
        }
      }
    }
  }

  PL_HashTableEnumerateEntries(htable, aEnumerator, aArg);
  PL_HashTableDestroy(htable);
}
