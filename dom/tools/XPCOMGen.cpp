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

#ifndef XP_MAC
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <stat.h>
#endif

#if !defined XP_UNIX && !defined XP_MAC
#include <direct.h>
#endif

#include <fstream.h>
#include <ctype.h>

#include "XPCOMGen.h"
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
static const char *kIncludeDefaultsStr = 
"#include \"nsISupports.h\"\n"
"#include \"nsString.h\"\n"
"#include \"nsIScriptContext.h\"\n";
static const char *kIncludeStr = "#include \"nsIDOM%s.h\"\n";
static const char *kIncludeJSStr = "#include \"jsapi.h\"\n";
static const char *kForwardClassStr = "class nsIDOM%s;\n";
static const char *kForwardXPIDLClassStr = "class %s;\n";
static const char *kUuidStr = 
"#define %s \\\n"
"%s\n\n";
static const char *kNoUuidStr = 
"--- IID GOES HERE ---\n\n";
static const char *kClassDeclStr = "class nsIDOM%s : ";
static const char *kBaseClassStr = "public nsIDOM%s";
static const char *kNoBaseClassStr = "public nsISupports";
static const char *kClassPrologStr = " {\npublic:\n";
static const char *kStaticIIDStr = "  static const nsIID& GetIID() { static nsIID iid = %s; return iid; }\n";
static const char *kEnumDeclBeginStr = "  enum {\n";
static const char *kEnumEntryStr = "    %s = %d%s\n";
static const char *kEnumDeclEndStr = "  };\n";
static const char *kGetterMethodDeclStr = "\n  NS_IMETHOD    Get%s(%s%s a%s)=0;\n";
static const char *kGetterMethodDeclNonVirtualStr = "  NS_IMETHOD    Get%s(%s%s a%s);  \\\n";
static const char *kGetterMethodForwardStr = "  NS_IMETHOD    Get%s(%s%s a%s) { return _to Get%s(a%s); } \\\n";
static const char *kSetterMethodDeclStr = "  NS_IMETHOD    Set%s(%s%s a%s)=0;\n";
static const char *kSetterMethodDeclNonVirtualStr = "  NS_IMETHOD    Set%s(%s%s a%s);  \\\n";
static const char *kSetterMethodForwardStr = "  NS_IMETHOD    Set%s(%s%s a%s) { return _to Set%s(a%s); } \\\n";
static const char *kMethodDeclStr = "\n  NS_IMETHOD    %s(%s)=0;\n";
static const char *kMethodDeclNonVirtualStr = "  NS_IMETHOD    %s(%s);  \\\n";
static const char *kMethodForwardStr = "  NS_IMETHOD    %s(%s) { return _to %s(%s); }  \\\n";
static const char *kParamStr = "%s a%s";
static const char *kCallParamStr = "a%s";
static const char *kDelimiterStr = ", ";
static const char *kEllipsisParamStr = "JSContext *cx, jsval *argv, PRUint32 argc";
static const char *kEllipsisCallStr = "cx, argv, argc";
static const char *kReturnStr = "%s%s aReturn";
static const char *kReturnCallStr = "aReturn";
static const char *kClassEpilogStr = "};\n\n";

static const char *kFactoryClassDeclBeginStr =
"class nsIDOM%sFactory : public nsISupports {\n"
"public:\n";
static const char *kConstructorDeclStr = "  NS_IMETHOD   CreateInstance(%snsIDOM%s **aReturn)=0;\n";
static const char *kFactoryClassDeclEndStr = "\n};\n\n";
static const char *kGlobalInitClassStr = "extern nsresult NS_Init%sClass(nsIScriptContext *aContext, nsIScriptGlobalObject *aGlobal);\n\n";
static const char *kInitClassStr = "extern \"C\" NS_DOM nsresult NS_Init%sClass(nsIScriptContext *aContext, void **aPrototype);\n\n";
static const char *kNewObjStr = "extern \"C\" NS_DOM nsresult NS_NewScript%s(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);\n\n";
static const char *kMethodDeclMacroStr = "\n#define NS_DECL_IDOM%s   \\\n";
static const char *kMethodForwardMacroStr = "\n#define NS_FORWARD_IDOM%s(_to)  \\\n";
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
                   IdlSpecification &aSpec,
                   int aIsGlobal)
{
  mIsGlobal = aIsGlobal;

  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    
    if (iface) {
      if (!OpenFile(iface->GetName(), aOutputDirName, 
                    kFilePrefix, kFileSuffix)) {
        throw new CantOpenFileException(aFileName);
      }
  

      GenerateNPL();
      GenerateIfdef(*iface);
      GenerateIncludes(*iface);
      GenerateForwardDecls(*iface);
      
      GenerateGuid(*iface);
      GenerateClassDecl(*iface);
      GenerateStatic(*iface);
      GenerateEnums(*iface);
      GenerateMethods(*iface);
      GenerateEndClassDecl();
      GenerateDeclMacro(*iface);
      GenerateForwardMacro(*iface);
      if (i == 0) {
        GenerateEpilog(*iface, PR_TRUE);
      }
      else {
        GenerateEpilog(*iface, PR_FALSE);
      }
      CloseFile();
    }
  }
}


void     
XPCOMGen::GenerateIfdef(IdlInterface& aInterface)
{
  char buf[512];
  ofstream *file = GetFile();

  sprintf(buf, kIfdefStr, aInterface.GetName(), aInterface.GetName());
  *file << buf;
}

void     
XPCOMGen::GenerateIncludes(IdlInterface &aInterface)
{
  char buf[512];
  ofstream *file = GetFile();

  *file << kIncludeDefaultsStr;

  int b, bcount = aInterface.BaseClassCount();
  for (b = 0; b < bcount; b++) {
    sprintf(buf, kIncludeStr, aInterface.GetBaseClassAt(b));
    *file << buf;
  }

  int m, mcount = aInterface.FunctionCount();
  for (m = 0; m < mcount; m++) {
    IdlFunction *func = aInterface.GetFunctionAt(m);
      
    if (func->GetHasEllipsis()) {
      *file << kIncludeJSStr;
      break;
    }
  }
  
  *file << "\n";
}

static PRIntn 
ForwardDeclEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
  char buf[512];

  ofstream *file = (ofstream *)arg;
  switch ((Type)(int)(he->value)) {
  case TYPE_OBJECT:
  case TYPE_FUNC:
    sprintf(buf, kForwardClassStr, (char *)he->key);
    break;

  case TYPE_XPIDL_OBJECT:
    sprintf(buf, kForwardXPIDLClassStr, (char *)he->key);
    break;

  default:
    // uh oh...
    break;
  }

  *file << buf;
  
  return HT_ENUMERATE_NEXT;
}
 
void     
XPCOMGen::GenerateForwardDecls(IdlInterface &aInterface)
{
  ofstream *file = GetFile();
  EnumerateAllObjects(aInterface, (PLHashEnumerator)ForwardDeclEnumerator, 
                      file);
  *file << "\n";
}
 
void     
XPCOMGen::GenerateGuid(IdlInterface &aInterface)
{
  char buf[512];
  char uuid_buf[256];
  ofstream *file = GetFile();
  char *uuid;
  
  uuid = aInterface.GetIIDAt(0);
  GetInterfaceIID(uuid_buf, aInterface);
  if (NULL != uuid) {
    sprintf(buf, kUuidStr, uuid_buf, uuid);
  }
  else {
    sprintf(buf, kUuidStr, uuid_buf, kNoUuidStr);
  }
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
        *file << kDelimiterStr;
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
XPCOMGen::GenerateStatic(IdlInterface &aInterface)
{
  char buf[512];
  char uuid_buf[256];
  ofstream *file = GetFile();

  GetInterfaceIID(uuid_buf, aInterface);
  sprintf(buf, kStaticIIDStr, uuid_buf);
  *file << buf;
}

void     
XPCOMGen::GenerateEnums(IdlInterface &aInterface)
{
  char buf[512];
  ofstream *file = GetFile();

  if (aInterface.ConstCount() > 0) {
    int c, ccount = aInterface.ConstCount();

    *file << kEnumDeclBeginStr;
    
    for (c = 0; c < ccount; c++) {
      IdlVariable *var = aInterface.GetConstAt(c);
      
      if (NULL != var) {
        sprintf(buf, kEnumEntryStr, var->GetName(), var->GetLongValue(), 
                ((c < ccount-1) ? "," : ""));
        *file << buf;
      }
    }
    
    *file << kEnumDeclEndStr;
  }
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
      sprintf(buf, kSetterMethodDeclStr, name_buf, 
              attr->GetType() == TYPE_STRING ? "const " : "", type_buf, 
              name_buf);
      *file << buf;
    }
  }
  
  int m, mcount = aInterface.FunctionCount();
  for (m = 0; m < mcount; m++) {
    char param_buf[256];
    char *cur_param = param_buf;
    IdlFunction *func = aInterface.GetFunctionAt(m);

    // Don't generate a constructor method
    if (strcmp(func->GetName(), aInterface.GetName()) == 0) {
      continue;
    }

    int p, pcount = func->ParameterCount();
    for (p = 0; p < pcount; p++) {
      IdlParameter *param = func->GetParameterAt(p);

      if (p > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }

      GetParameterType(type_buf, *param);
      GetCapitalizedName(name_buf, *param);
      sprintf(cur_param, kParamStr, type_buf, name_buf);
      cur_param += strlen(cur_param);
    }

    if (func->GetHasEllipsis()) {
      if (pcount > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }
      sprintf(cur_param, kEllipsisParamStr);
      cur_param += strlen(cur_param);
    }

    IdlVariable *rval = func->GetReturnValue();
    if (rval->GetType() != TYPE_VOID) {
      if ((pcount > 0) || func->GetHasEllipsis()) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }
      GetVariableTypeForParameter(type_buf, *rval);
      sprintf(cur_param, kReturnStr, type_buf,
              rval->GetType() == TYPE_STRING ? "" : "*");
    }
    else {
      *cur_param++ = '\0';
    }
 
    GetCapitalizedName(name_buf, *func);
    sprintf(buf, kMethodDeclStr, name_buf, param_buf);
    *file << buf;
  }
}

void     
XPCOMGen::GenerateDeclMacro(IdlInterface &aInterface)
{
  char buf[512];
  char name_buf[128];
  char type_buf[128];
  ofstream *file = GetFile();
  
  strcpy(name_buf, aInterface.GetName());
  StrUpr(name_buf);
  sprintf(buf, kMethodDeclMacroStr, name_buf);
  *file << buf;

  int a, acount = aInterface.AttributeCount();
  for (a = 0; a < acount; a++) {
    IdlAttribute *attr = aInterface.GetAttributeAt(a);

    GetVariableTypeForParameter(type_buf, *attr);
    GetCapitalizedName(name_buf, *attr);
    sprintf(buf, kGetterMethodDeclNonVirtualStr, name_buf, type_buf,
            attr->GetType() == TYPE_STRING ? "" : "*", name_buf);
    *file << buf;

    if (!attr->GetReadOnly()) {
      sprintf(buf, kSetterMethodDeclNonVirtualStr, name_buf, 
              attr->GetType() == TYPE_STRING ? "const " : "", type_buf, 
              name_buf);
      *file << buf;
    }
  }
  
  int m, mcount = aInterface.FunctionCount();
  for (m = 0; m < mcount; m++) {
    char param_buf[256];
    char *cur_param = param_buf;
    IdlFunction *func = aInterface.GetFunctionAt(m);

    // Don't generate a constructor method
    if (strcmp(func->GetName(), aInterface.GetName()) == 0) {
      continue;
    }

    int p, pcount = func->ParameterCount();
    for (p = 0; p < pcount; p++) {
      IdlParameter *param = func->GetParameterAt(p);

      if (p > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }

      GetParameterType(type_buf, *param);
      GetCapitalizedName(name_buf, *param);
      sprintf(cur_param, kParamStr, type_buf, name_buf);
      cur_param += strlen(cur_param);
    }

    if (func->GetHasEllipsis()) {
      if (pcount > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }
      sprintf(cur_param, kEllipsisParamStr);
      cur_param += strlen(cur_param);
    }

    IdlVariable *rval = func->GetReturnValue();
    if (rval->GetType() != TYPE_VOID) {
      if ((pcount > 0) || func->GetHasEllipsis()) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }
      GetVariableTypeForParameter(type_buf, *rval);
      sprintf(cur_param, kReturnStr, type_buf,
              rval->GetType() == TYPE_STRING ? "" : "*");
    }
    else {
      *cur_param++ = '\0';
    }
 
    GetCapitalizedName(name_buf, *func);
    sprintf(buf, kMethodDeclNonVirtualStr, name_buf, param_buf);
    *file << buf;
  }

  *file << "\n\n";
}

void     
XPCOMGen::GenerateForwardMacro(IdlInterface &aInterface)
{
  char buf[512];
  char name_buf[128];
  char type_buf[128];
  ofstream *file = GetFile();
  
  strcpy(name_buf, aInterface.GetName());
  StrUpr(name_buf);
  sprintf(buf, kMethodForwardMacroStr, name_buf);
  *file << buf;

  int a, acount = aInterface.AttributeCount();
  for (a = 0; a < acount; a++) {
    IdlAttribute *attr = aInterface.GetAttributeAt(a);

    GetVariableTypeForParameter(type_buf, *attr);
    GetCapitalizedName(name_buf, *attr);
    sprintf(buf, kGetterMethodForwardStr, name_buf, type_buf,
            attr->GetType() == TYPE_STRING ? "" : "*", name_buf,
            name_buf, name_buf);
    *file << buf;

    if (!attr->GetReadOnly()) {
      sprintf(buf, kSetterMethodForwardStr, name_buf, 
              attr->GetType() == TYPE_STRING ? "const " : "", type_buf, 
              name_buf, name_buf, name_buf);
      *file << buf;
    }
  }
  
  int m, mcount = aInterface.FunctionCount();
  for (m = 0; m < mcount; m++) {
    char param_buf[256];
    char *cur_param = param_buf;
    char param_buf2[256];
    char *cur_param2 = param_buf2;
    IdlFunction *func = aInterface.GetFunctionAt(m);

    // Don't generate a constructor method
    if (strcmp(func->GetName(), aInterface.GetName()) == 0) {
      continue;
    }

    int p, pcount = func->ParameterCount();
    for (p = 0; p < pcount; p++) {
      IdlParameter *param = func->GetParameterAt(p);

      if (p > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
        strcpy(cur_param2, kDelimiterStr);
        cur_param2 += strlen(kDelimiterStr);
      }

      GetParameterType(type_buf, *param);
      GetCapitalizedName(name_buf, *param);
      sprintf(cur_param, kParamStr, type_buf, name_buf);
      cur_param += strlen(cur_param);
      sprintf(cur_param2, kCallParamStr, name_buf);
      cur_param2 += strlen(cur_param2);
    }

    if (func->GetHasEllipsis()) {
      if (pcount > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
        strcpy(cur_param2, kDelimiterStr);
        cur_param2 += strlen(kDelimiterStr);
      }
      sprintf(cur_param, kEllipsisParamStr);
      cur_param += strlen(cur_param);
      sprintf(cur_param2, kEllipsisCallStr);
      cur_param2 += strlen(cur_param2);
    }

    IdlVariable *rval = func->GetReturnValue();
    if (rval->GetType() != TYPE_VOID) {
      if ((pcount > 0) || func->GetHasEllipsis()) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
        strcpy(cur_param2, kDelimiterStr);
        cur_param2 += strlen(kDelimiterStr);
      }
      GetVariableTypeForParameter(type_buf, *rval);
      sprintf(cur_param, kReturnStr, type_buf,
              rval->GetType() == TYPE_STRING ? "" : "*");
      strcpy(cur_param2, kReturnCallStr);
    }
    else {
      *cur_param++ = '\0';
      *cur_param2++ = '\0';
    }
 
    GetCapitalizedName(name_buf, *func);
    sprintf(buf, kMethodForwardStr, name_buf, param_buf, name_buf, param_buf2);
    *file << buf;
  }

  *file << "\n\n";
}

void
XPCOMGen::GenerateEndClassDecl()
{
  ofstream *file = GetFile();
  
  *file << kClassEpilogStr;
}

void 
XPCOMGen::GenerateFactory(IdlInterface &aInterface)
{
  char buf[512];
  ofstream *file = GetFile();
  char *iface_name = aInterface.GetName();
  char name_buf[128];
  char type_buf[128];
  char uuid_buf[128];
  char param_buf[256];
  char *cur_param = param_buf;
  IdlFunction *constructor;

  *cur_param = '\0';
  
  if (HasConstructor(aInterface, &constructor)) {
    strcpy(name_buf, iface_name);
    strcat(name_buf, "Factory");
    char *iid = aInterface.GetIIDAt(1);
    GetInterfaceIID(uuid_buf, name_buf);
    if (NULL != iid) {
      sprintf(buf, kUuidStr, uuid_buf, iid);
    }
    else {
      sprintf(buf, kUuidStr, uuid_buf, kNoUuidStr);
    }
    *file << buf;
    
    sprintf(buf, kFactoryClassDeclBeginStr, iface_name);
    *file << buf;
    
    int p, pcount = constructor->ParameterCount();
    for (p = 0; p < pcount; p++) {
      IdlParameter *param = constructor->GetParameterAt(p);
      
      if (p > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }

      GetParameterType(type_buf, *param);
      GetCapitalizedName(name_buf, *param);
      sprintf(cur_param, kParamStr, type_buf, name_buf);
      cur_param += strlen(cur_param);
    }
    
    if (constructor->GetHasEllipsis()) {
      if (pcount > 0) {
        strcpy(cur_param, kDelimiterStr);
        cur_param += strlen(kDelimiterStr);
      }
      sprintf(cur_param, kEllipsisParamStr);
      cur_param += strlen(cur_param);
    }
    
    GetCapitalizedName(name_buf, *constructor);
    sprintf(buf, kConstructorDeclStr, param_buf, name_buf);
    *file << buf;
    
    *file << kFactoryClassDeclEndStr;
  }
}

void     
XPCOMGen::GenerateEpilog(IdlInterface &aInterface, PRBool aIsPrimary)
{
  char buf[512];
  ofstream *file = GetFile();
  char *iface_name = aInterface.GetName();

  if (aIsPrimary) {
    if (mIsGlobal) {
      sprintf(buf, kGlobalInitClassStr, iface_name);
    }
    else {
      sprintf(buf, kInitClassStr, iface_name);
    }
    *file << buf;
  
    sprintf(buf, kNewObjStr, iface_name);
    *file << buf;
  }

  sprintf(buf, kEndifStr, iface_name);
  *file << buf;
}
