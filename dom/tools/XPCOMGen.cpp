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

#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <fstream.h>
#include <ctype.h>
#include "XPComGen.h"
#include "Exceptions.h"
#include "plhash.h"
#include "IdlSpecification.h"
#include "IdlInterface.h"
#include "IdlVariable.h"
#include "IdlAttribute.h"
#include "IdlFunction.h"
#include "IdlParameter.h"

static const char *kFilePrefix = "nsIDOM";
static const char *kFileSuffix = "h";
static const char *kIfdefStr = "\n"
"#ifndef nsIDOM%s_h__\n"
"#define nsIDOM%s_h__\n\n";
static const char *kIncludeISupportsStr = "#include \"nsISupports.h\"\n";
static const char *kIncludeStr = "#include \"nsIDOM%s.h\"\n";
static const char *kForwardClassStr = "class nsIDOM%s;\n";
static const char *kUuidStr = 
"#define %s \\\n"
" { 0x6f7652e0,  0xee43, 0x11d1,\\\n"
" { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }\n\n";
static const char *kClassDeclStr = "class nsIDOM%s : ";
static const char *kBaseClassStr = "public nsIDOM%s";
static const char *kNoBaseClassStr = "public nsISupports";
static const char *kClassPrologStr = " {\npublic:\n";
static const char *kConstDeclStr = "  const %s %s = %l;\n";
static const char *kGetterMethodDeclStr = "\n  NS_IMETHOD    Get%s(%s%s a%s)=0;\n";
static const char *kSetterMethodDeclStr = "  NS_IMETHOD    Set%s(%s a%s)=0;\n";
static const char *kMethodDeclStr = "\n  NS_IMETHOD    %s(%s)=0;\n";
static const char *kParamStr = "%s a%s, ";
static const char *kReturnStr = "%s%s aReturn";
static const char *kClassEpilogStr = "};\n\n";
static const char *kInitClassStr = "extern nsresult NS_Init%sClass(JSContext *aContext, JSObject **aPrototype);\n\n";
static const char *kNewObjStr = "extern nsresult NS_NewScript%s(JSContext *aContext, nsIDOM%s *aSupports, JSObject *aParent, JSObject **aReturn);\n\n";
static const char *kEndifStr = "#endif // nsIDOM%s_h__\n";


XPCOMGen::XPCOMGen()
{
}

XPCOMGen::~XPCOMGen()
{
}

void     
XPCOMGen::Generate(char *aFileName, 
                   char *aOutputDirName,
                   IdlSpecification &aSpec)
{
  if (!OpenFile(aFileName, aOutputDirName, kFilePrefix, kFileSuffix)) {
      throw new CantOpenFileException(aFileName);
  }
  
  GenerateNPL();
  GenerateIfdef(aSpec);
  GenerateIncludes(aSpec);
  GenerateForwardDecls(aSpec);
  
  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    
    if (iface) {
      GenerateGuid(*iface);
      GenerateClassDecl(*iface);
      GenerateMethods(*iface);
      GenerateEndClassDecl();
    }
  }

  GenerateEpilog(aSpec);
  CloseFile();
}


void     
XPCOMGen::GenerateIfdef(IdlSpecification &aSpec)
{
  char buf[512];
  IdlInterface *iface = aSpec.GetInterfaceAt(0);
  ofstream *file = GetFile();

  if (iface) {
    sprintf(buf, kIfdefStr, iface->GetName(), iface->GetName());
    *file << buf;
  } 
}

void     
XPCOMGen::GenerateIncludes(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();

  *file << kIncludeISupportsStr;
  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);

    if (iface) {
      int b, bcount = iface->BaseClassCount();
      for (b = 0; b < bcount; b++) {
        sprintf(buf, kIncludeStr, iface->GetBaseClassAt(b));
        *file << buf;
      }
    }
  }
  
  *file << "\n";
}

static PRIntn 
ForwardDeclEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
  char buf[512];

  ofstream *file = (ofstream *)arg;
  sprintf(buf, kForwardClassStr, (char *)he->key);
  *file << buf;
  
  return HT_ENUMERATE_NEXT;
}
 
void     
XPCOMGen::GenerateForwardDecls(IdlSpecification &aSpec)
{
  ofstream *file = GetFile();
  EnumerateAllObjects(aSpec, (PLHashEnumerator)ForwardDeclEnumerator, 
                      file, PR_FALSE);
  *file << "\n";
}
 
void     
XPCOMGen::GenerateGuid(IdlInterface &aInterface)
{
  char buf[512];
  char uuid_buf[256];
  ofstream *file = GetFile();

  // XXX Need to generate unique guids
  GetInterfaceIID(uuid_buf, aInterface);
  sprintf(buf, kUuidStr, uuid_buf);
  *file << buf;
}
 
void     
XPCOMGen::GenerateClassDecl(IdlInterface &aInterface)
{
  char buf[512];
  ofstream *file = GetFile();
  
  sprintf(buf, kClassDeclStr, aInterface.GetName());
  *file << buf;

  if (aInterface.BaseClassCount() > 0) {
    int b, bcount = aInterface.BaseClassCount();
    for (b = 0; b < bcount; b++) {
      if (b > 0) {
        *file << ", ";
      }
      sprintf(buf, kBaseClassStr, aInterface.GetBaseClassAt(b));
      *file << buf;
    }
  }
  else {
    *file << kNoBaseClassStr;
  }
  
  *file << kClassPrologStr;
}

void     
XPCOMGen::GenerateMethods(IdlInterface &aInterface)
{
  char buf[512];
  char name_buf[128];
  char type_buf[128];
  ofstream *file = GetFile();

  int a, acount = aInterface.AttributeCount();
  for (a = 0; a < acount; a++) {
    IdlAttribute *attr = aInterface.GetAttributeAt(a);

    GetVariableTypeForParameter(type_buf, *attr);
    GetCapitalizedName(name_buf, *attr);
    sprintf(buf, kGetterMethodDeclStr, name_buf, type_buf,
            attr->GetType() == TYPE_STRING ? "" : "*", name_buf);
    *file << buf;

    if (!attr->GetReadOnly()) {
      sprintf(buf, kSetterMethodDeclStr, name_buf, type_buf, 
              name_buf);
      *file << buf;
    }
  }
  
  int m, mcount = aInterface.FunctionCount();
  for (m = 0; m < mcount; m++) {
    char param_buf[256];
    char *cur_param = param_buf;
    IdlFunction *func = aInterface.GetFunctionAt(m);

    int p, pcount = func->ParameterCount();
    for (p = 0; p < pcount; p++) {
      IdlParameter *param = func->GetParameterAt(p);

      GetParameterType(type_buf, *param);
      GetCapitalizedName(name_buf, *param);
      sprintf(cur_param, kParamStr, type_buf, name_buf);
      cur_param += strlen(cur_param);
    }

    IdlVariable *rval = func->GetReturnValue();
    GetVariableTypeForParameter(type_buf, *rval);
    sprintf(cur_param, kReturnStr, type_buf,
            rval->GetType() == TYPE_STRING ? "" : "*");
    
    GetCapitalizedName(name_buf, *func);
    sprintf(buf, kMethodDeclStr, name_buf, param_buf);
    *file << buf;
  }
}

void
XPCOMGen::GenerateEndClassDecl()
{
  ofstream *file = GetFile();
  
  *file << kClassEpilogStr;
}

void     
XPCOMGen::GenerateEpilog(IdlSpecification &aSpec)
{
  char buf[512];
  IdlInterface *iface = aSpec.GetInterfaceAt(0);
  ofstream *file = GetFile();
  char *iface_name = iface->GetName();

  sprintf(buf, kInitClassStr, iface_name);
  *file << buf;
  
  sprintf(buf, kNewObjStr, iface_name, iface_name);
  *file << buf;

  if (iface) {
    sprintf(buf, kEndifStr, iface_name);
    *file << buf;
  } 
}
