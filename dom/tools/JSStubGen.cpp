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
#include "nsIPtr.h"
#include "plhash.h"
#include "JSStubGen.h"
#include "Exceptions.h"
#include "IdlSpecification.h"
#include "IdlInterface.h"
#include "IdlVariable.h"
#include "IdlAttribute.h"
#include "IdlFunction.h"
#include "IdlParameter.h"

static const char *kFilePrefix = "nsJS";
static const char *kFileSuffix = "cpp";

JSStubGen::JSStubGen()
{
}

JSStubGen::~JSStubGen()
{
}

void     
JSStubGen::Generate(char *aFileName, 
                    char *aOutputDirName,
                    IdlSpecification &aSpec)
{
  if (!OpenFile(aFileName, aOutputDirName, kFilePrefix, kFileSuffix)) {
      throw new CantOpenFileException(aFileName);
  }

  GenerateNPL();
  GenerateIncludes(aSpec);
  GenerateIIDDefinitions(aSpec);
  GenerateDefPtrs(aSpec);
  GeneratePropertySlots(aSpec);
  GeneratePropertyFunc(aSpec, PR_TRUE);
  GeneratePropertyFunc(aSpec, PR_FALSE);
  GenerateFinalize(aSpec);
  GenerateEnumerate(aSpec);
  GenerateResolve(aSpec);
  GenerateMethods(aSpec);
  GenerateJSClass(aSpec);
  GenerateClassProperties(aSpec);
  GenerateClassFunctions(aSpec);
  GenerateConstructor(aSpec);
  GenerateInitClass(aSpec);
  GenerateNew(aSpec);
  CloseFile();
}

static const char *kIncludeDefaultsStr = "\n"
"#include \"jsapi.h\"\n"
"#include \"nscore.h\"\n"
"#include \"nsIScriptContext.h\"\n"
"#include \"nsIJSScriptObject.h\"\n"
"#include \"nsIScriptObjectOwner.h\"\n"
"#include \"nsIScriptGlobalObject.h\"\n"
"#include \"nsIPtr.h\"\n"
"#include \"nsString.h\"\n";
static const char *kIncludeStr = "#include \"nsIDOM%s.h\"\n";

static PRIntn 
IncludeEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
  char buf[512];

  ofstream *file = (ofstream *)arg;
  sprintf(buf, kIncludeStr, (char *)he->key);
  *file << buf;
  
  return HT_ENUMERATE_NEXT;
}

void     
JSStubGen::GenerateIncludes(IdlSpecification &aSpec)
{
  ofstream *file = GetFile();

  *file << kIncludeDefaultsStr;
  EnumerateAllObjects(aSpec, (PLHashEnumerator)IncludeEnumerator, 
                      file, PR_TRUE);
  *file << "\n";
}

static const char *kIIDDefaultStr = "\n"
"static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);\n"
"static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);\n"
"static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);\n";
static const char *kIIDStr = "static NS_DEFINE_IID(kI%sIID, %s);\n";

PRIntn 
JSStubGen_IIDEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
  char buf[512];
  char iid_buf[256];
  JSStubGen *me = (JSStubGen *)arg;
  ofstream *file = me->GetFile();
  
  me->GetInterfaceIID(iid_buf, (char *)he->key);
  sprintf(buf, kIIDStr, (char *)he->key, iid_buf);
  *file << buf;
  
  return HT_ENUMERATE_NEXT;
}
 
void     
JSStubGen::GenerateIIDDefinitions(IdlSpecification &aSpec)
{
  ofstream *file = GetFile();

  *file << kIIDDefaultStr;
  EnumerateAllObjects(aSpec, (PLHashEnumerator)JSStubGen_IIDEnumerator, 
                      this, PR_FALSE);
  *file << "\n";
}

static const char *kDefPtrStr =
"NS_DEF_PTR(nsIDOM%s);\n";

PRIntn 
JSStubGen_DefPtrEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
  char buf[512];
  JSStubGen *me = (JSStubGen *)arg;
  ofstream *file = me->GetFile();
  
  sprintf(buf, kDefPtrStr, (char *)he->key);
  *file << buf;
  
  return HT_ENUMERATE_NEXT;
}

void     
JSStubGen::GenerateDefPtrs(IdlSpecification &aSpec)
{
  ofstream *file = GetFile();

  EnumerateAllObjects(aSpec, (PLHashEnumerator)JSStubGen_DefPtrEnumerator, 
                      this, PR_FALSE);
  *file << "\n";
}

static const char *kPropEnumStr = 
"//\n"
"// %s property ids\n"
"//\n"
"enum %s_slots {\n";
static const char *kPropSlotStr = "  %s_%s = -%d%d";

void     
JSStubGen::GeneratePropertySlots(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();

  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    char iface_name[128];

    strcpy(iface_name, iface->GetName());
    StrUpr(iface_name);
    int a, acount = iface->AttributeCount();
    for (a = 0; a < acount; a++) {
      IdlAttribute *attr = iface->GetAttributeAt(a);
      char attr_name[128];

      if ((i == 0) && (a == 0)) {
        sprintf(buf, kPropEnumStr, iface->GetName(), iface->GetName());
        *file << buf;
      }

      strcpy(attr_name, attr->GetName());
      StrUpr(attr_name);

      sprintf(buf, kPropSlotStr, iface_name, attr_name, i+1, a+1);
      *file << buf;
      if ((a == acount-1) && (i == icount-1)) {
        *file << "\n};\n";
      }
      else {
        *file << ",\n";
      }
    }
  }
}


static const char *kPropFuncBeginStr = "\n"
"/***********************************************************************/\n"
"//\n"
"// %s Properties %ster\n"
"//\n"
"PR_STATIC_CALLBACK(JSBool)\n"
"%s%sProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)\n"
"{\n"
"  nsIDOM%s *a = (nsIDOM%s*)JS_GetPrivate(cx, obj);\n"
"\n"
"  // If there's no private data, this must be the prototype, so ignore\n"
"  if (nsnull == a) {\n"
"    return JS_TRUE;\n"
"  }\n"
"\n"
"  if (JSVAL_IS_INT(id)) {\n"
"    switch(JSVAL_TO_INT(id)) {\n";

static const char *kPropFuncEndStr = 
"      default:\n"
"      {\n"
"        nsIJSScriptObject *object;\n"
"        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {\n"
"          PRBool rval;\n"
"          rval =  object->%sProperty(cx, id, vp);\n"
"          NS_RELEASE(object);\n"
"          return rval;\n"
"        }\n"
"      }\n"
"    }\n"
"  }\n"
"\n"
"  return PR_TRUE;\n"
"}\n";

#define JSGEN_GENERATE_PROPFUNCBEGIN(buffer, op, className)  \
     sprintf(buffer, kPropFuncBeginStr, className, op, op, className, className, className)

#define JSGEN_GENERATE_PROPFUNCEND(buffer, op)   \
     sprintf(buffer, kPropFuncEndStr, op)

static const char *kPropCaseBeginStr = 
"      case %s_%s:\n"
"      {\n";

static const char *kPropCaseEndStr = 
"        break;\n"
"      }\n";

static const char *kNoAttrStr = "      case 0:\n";


void     
JSStubGen::GeneratePropertyFunc(IdlSpecification &aSpec, PRBool aIsGetter)
{
  char buf[1024];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);
  PRBool any = PR_FALSE;

  JSGEN_GENERATE_PROPFUNCBEGIN(buf,  aIsGetter ? "Get" : "Set", primary_iface->GetName());
  *file << buf;

  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    char iface_name[128];

    strcpy(iface_name, iface->GetName());
    StrUpr(iface_name);

    int a, acount = iface->AttributeCount();
    for (a = 0; a < acount; a++) {
      IdlAttribute *attr = iface->GetAttributeAt(a);
      char attr_name[128];
      
      strcpy(attr_name, attr->GetName());
      StrUpr(attr_name);

      if (!aIsGetter && (attr->GetReadOnly())) {
        continue;
      }

      any = PR_TRUE;
      
      sprintf(buf, kPropCaseBeginStr, iface_name, attr_name);
      *file << buf;

      if (aIsGetter) {
        GeneratePropGetter(file, *iface, *attr, iface == primary_iface);
      }
      else {
        GeneratePropSetter(file, *iface, *attr, iface == primary_iface);
      }

      sprintf(buf, kPropCaseEndStr);
      *file << buf;
   
    }    
  }
  
  if (!any) {
    *file << kNoAttrStr;
  }

  JSGEN_GENERATE_PROPFUNCEND(buf, aIsGetter ? "Get" : "Set");
  *file << buf;
}

static const char *kGetCaseStr = 
"        %s prop;\n"
"        if (NS_OK == a->Get%s(%sprop)) {\n"
"%s"
"        }\n"
"        else {\n"
"          return JS_FALSE;\n"
"        }\n";

static const char *kGetCaseNonPrimaryStr =
"        %s prop;\n"
"        nsIDOM%s* b;\n"
"        if (NS_OK == a->QueryInterface(kI%sIID, (void **)&b)) {\n"
"          if(NS_OK == b->Get%s(%sprop)) {\n"
"%s"
"            NS_RELEASE(b);\n"
"          }\n"
"          else {\n"
"            NS_RELEASE(b);\n"
"            return JS_FALSE;\n"
"          }\n"
"        }\n"
"        else {\n"
"          return JS_FALSE;\n"
"        }\n";

static const char *kObjectGetCaseStr = 
"          // get the js object\n"
"          if (prop != nsnull) {\n"
"            nsIScriptObjectOwner *owner = nsnull;\n"
"            if (NS_OK == prop->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {\n"
"              JSObject *object = nsnull;\n"
"              nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);\n"
"              if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {\n"
"                // set the return value\n"
"                *vp = OBJECT_TO_JSVAL(object);\n"
"              }\n"
"              NS_RELEASE(owner);\n"
"            }\n"
"            NS_RELEASE(prop);\n"
"          }\n"
"          else {\n"
"            *vp = JSVAL_NULL;\n"
"          }\n";

static const char *kStringGetCaseStr = 
"          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());\n"
"          // set the return value\n"
"          *vp = STRING_TO_JSVAL(jsstring);\n";

static const char *kIntGetCaseStr = 
"          *vp = INT_TO_JSVAL(prop);\n";

static const char *kBoolGetCaseStr =
"          *vp = BOOLEAN_TO_JSVAL(prop);\n";


void
JSStubGen::GeneratePropGetter(ofstream *file,
                              IdlInterface &aInterface,
                              IdlAttribute &aAttribute,
                              PRBool aIsPrimary)
{
  char buf[1024];
  char attr_type[128];
  char attr_name[128];
  const char *case_str;

  GetVariableTypeForLocal(attr_type, aAttribute);
  GetCapitalizedName(attr_name, aAttribute);

  switch (aAttribute.GetType()) {
    case TYPE_BOOLEAN:
      case_str = kBoolGetCaseStr;
      break;
    case TYPE_LONG:
    case TYPE_SHORT:
    case TYPE_ULONG:
    case TYPE_USHORT:
    case TYPE_CHAR:
    case TYPE_INT:
    case TYPE_UINT:
      case_str = kIntGetCaseStr;
      break;
    case TYPE_STRING:
      case_str = kStringGetCaseStr;
      break;
    case TYPE_OBJECT:
      case_str = kObjectGetCaseStr;
      break;
    default:
      // XXX Fail for other cases
      break;
  }

  if (aIsPrimary) {
    sprintf(buf, kGetCaseStr, attr_type, attr_name,
            aAttribute.GetType() == TYPE_STRING ? "" : "&",
            case_str);
  }
  else {
    sprintf(buf, kGetCaseNonPrimaryStr, attr_type,
            aInterface.GetName(), aInterface.GetName(),
            attr_name, 
            aAttribute.GetType() == TYPE_STRING ? "" : "&",
            case_str);
  }

  *file << buf;
}


static const char *kSetCaseStr = 
"        %s prop;\n"
"%s      \n"
"        a->Set%s(prop);\n"
"        %s\n";

static const char *kSetCaseNonPrimaryStr =
"        %s prop;\n"
"%s      \n"
"        nsIDOM%s *b;\n"
"        if (NS_OK == a->QueryInterface(kI%sIID, (void **)&b)) {\n"
"          b->Set%s(prop);\n"
"          NS_RELEASE(b);\n"
"        }\n"
"        else {\n"
"           %s\n"
"           return JS_FALSE;\n"
"        }\n"
"        %s\n";


static const char *kObjectSetCaseStr = 
"        if (JSVAL_IS_OBJECT(*vp)) {\n"
"          JSObject *jsobj = JSVAL_TO_OBJECT(*vp); \n"
"          nsISupports *supports = (nsISupports *)JS_GetPrivate(cx, jsobj);\n"
"          if (NS_OK != supports->QueryInterface(kI%sIID, (void **)&prop)) {\n"
"            return JS_FALSE;\n"
"          }\n"
"        }\n"
"        else if (JSVAL_IS_NULL(*vp)) {\n"
"          prop = nsnull;\n"
"        }\n"
"        else {\n"
"          return JS_FALSE;\n"
"        }\n";

static const char *kObjectSetCaseEndStr = "if (prop) NS_RELEASE(prop);";

static const char *kStringSetCaseStr = 
"        JSString *jsstring;\n"
"        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {\n"
"          prop.SetString(JS_GetStringChars(jsstring));\n"
"        }\n"
"        else {\n"
"          prop.SetString((const char *)nsnull);\n"
"        }\n";

static const char *kIntSetCaseStr = 
"        int32 temp;\n"
"        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {\n"
"          prop = (%s)temp;\n"
"        }\n"
"        else {\n"
"          return JS_FALSE;\n"
"        }\n";

static const char *kBoolSetCaseStr =
"        JSBool temp;\n"
"        if (JSVAL_IS_BOOLEAN(*vp) && JS_ValueToBoolean(cx, *vp, &temp)) {\n"  
"          prop = (%s)temp;\n"
"        }\n"
"        else {\n"
"          return JS_FALSE;\n"
"        }\n";


void
JSStubGen::GeneratePropSetter(ofstream *file,
                              IdlInterface &aInterface,
                              IdlAttribute &aAttribute,
                              PRBool aIsPrimary)
{
  char buf[1024];
  char attr_type[128];
  char attr_name[128];
  char case_buf[1024];
  const char *end_str;

  GetVariableTypeForLocal(attr_type, aAttribute);
  GetCapitalizedName(attr_name, aAttribute);

  switch (aAttribute.GetType()) {
    case TYPE_BOOLEAN:
      sprintf(case_buf, kBoolSetCaseStr, attr_type);
      break;
    case TYPE_LONG:
    case TYPE_SHORT:
    case TYPE_ULONG:
    case TYPE_USHORT:
    case TYPE_CHAR:
    case TYPE_INT:
    case TYPE_UINT:
      sprintf(case_buf, kIntSetCaseStr, attr_type);
      break;
    case TYPE_STRING:
      strcpy(case_buf, kStringSetCaseStr);
      break;
    case TYPE_OBJECT:
      sprintf(case_buf, kObjectSetCaseStr, aAttribute.GetTypeName());
      break;
    default:
      // XXX Fail for other cases
      break;
  }

  end_str = aAttribute.GetType() == TYPE_OBJECT ? kObjectSetCaseEndStr : "";
  if (aIsPrimary) {
    sprintf(buf, kSetCaseStr, attr_type, case_buf, attr_name, end_str);
  }
  else {
    sprintf(buf, kSetCaseNonPrimaryStr, attr_type, case_buf,
            aInterface.GetName(), aInterface.GetName(), attr_name, 
            end_str, end_str);
  }

  *file << buf;
}

static const char *kFinalizeStr = 
"\n\n//\n"
"// %s finalizer\n"
"//\n"
"PR_STATIC_CALLBACK(void)\n"
"Finalize%s(JSContext *cx, JSObject *obj)\n"
"{\n"
"  nsIDOM%s *a = (nsIDOM%s*)JS_GetPrivate(cx, obj);\n"
"  \n"
"  if (nsnull != a) {\n"
"    // get the js object\n"
"    nsIScriptObjectOwner *owner = nsnull;\n"
"    if (NS_OK == a->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {\n"
"      owner->ResetScriptObject();\n"
"      NS_RELEASE(owner);\n"
"    }\n"
"\n"
"    NS_RELEASE(a);\n"
"  }\n"
"}\n";

void     
JSStubGen::GenerateFinalize(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *iface = aSpec.GetInterfaceAt(0);

  sprintf(buf, kFinalizeStr, iface->GetName(), iface->GetName(), 
          iface->GetName(), iface->GetName());
  *file << buf;
}


static const char *kEnumerateStr =
"\n\n//\n"
"// %s enumerate\n"
"//\n"
"PR_STATIC_CALLBACK(JSBool)\n"
"Enumerate%s(JSContext *cx, JSObject *obj)\n"
"{\n"
"  nsIDOM%s *a = (nsIDOM%s*)JS_GetPrivate(cx, obj);\n"
"  \n"
"  if (nsnull != a) {\n"
"    // get the js object\n"
"    nsIJSScriptObject *object;\n"
"    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {\n"
"      object->EnumerateProperty(cx);\n"
"      NS_RELEASE(object);\n"
"    }\n"
"  }\n"
"  return JS_TRUE;\n"
"}\n";

#define JSGEN_GENERATE_ENUMERATE(buf, className)                           \
  sprintf(buf, kEnumerateStr, className, className, className, className);

void     
JSStubGen::GenerateEnumerate(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *iface = aSpec.GetInterfaceAt(0);
  char *name = iface->GetName();

  JSGEN_GENERATE_ENUMERATE(buf, name);
  *file << buf;
}


static const char *kResolveStr =
"\n\n//\n"
"// %s resolve\n"
"//\n"
"PR_STATIC_CALLBACK(JSBool)\n"
"Resolve%s(JSContext *cx, JSObject *obj, jsval id)\n"
"{\n"
"  nsIDOM%s *a = (nsIDOM%s*)JS_GetPrivate(cx, obj);\n"
"  \n"
"  if (nsnull != a) {\n"
"    // get the js object\n"
"    nsIJSScriptObject *object;\n"
"    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {\n"
"      object->Resolve(cx, id);\n"
"      NS_RELEASE(object);\n"
"    }\n"
"  }\n"
"  return JS_TRUE;\n"
"}\n";

#define JSGEN_GENERATE_RESOLVE(buf, className)                           \
  sprintf(buf, kResolveStr, className, className, className, className);

void     
JSStubGen::GenerateResolve(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *iface = aSpec.GetInterfaceAt(0);
  char *name = iface->GetName();

  JSGEN_GENERATE_RESOLVE(buf, name);
  *file << buf;
}

static const char *kMethodBeginStr = "\n\n"
"//\n"
"// Native method %s\n"
"//\n"
"PR_STATIC_CALLBACK(JSBool)\n"
"%s%s(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)\n"
"{\n"
"  nsIDOM%s *nativeThis = (nsIDOM%s*)JS_GetPrivate(cx, obj);\n"
"  JSBool rBool = JS_FALSE;\n";

static const char *kMethodReturnStr = 
"  %s nativeRet;\n";

static const char *kMethodParamStr =  "  %s b%d;\n";

static const char *kMethodBodyBeginStr = "\n"
"  *rval = JSVAL_NULL;\n"
"\n"
"  // If there's no private data, this must be the prototype, so ignore\n"
"  if (nsnull == nativeThis) {\n"
"    return JS_TRUE;\n"
"  }\n"
"\n"
"  if (argc >= %d) {\n";

static const char *kMethodObjectParamStr = "\n"
"    if (JSVAL_IS_OBJECT(argv[%d])) {\n"
"      nsISupports *supports%d = (nsISupports *)JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[%d]));\n"
"      NS_ASSERTION(nsnull != supports%d, \"null pointer\");\n"
"\n"
"      if ((nsnull == supports%d) ||\n"
"          (NS_OK != supports%d->QueryInterface(kI%sIID, (void **)(b%d.Query())))) {\n"
"        return JS_FALSE;\n"
"      }\n"
"    }\n"
"    else if (JSVAL_IS_NULL(argv[%d])){\n"
"      b%d = nsnull;\n"
"    }\n"
"    else {\n"
"      return JS_FALSE;\n"
"    }\n";

#define JSGEN_GENERATE_OBJECTPARAM(buffer, paramNum, paramType) \
    sprintf(buffer, kMethodObjectParamStr, paramNum, paramNum,  \
            paramNum, paramNum, paramNum, paramNum, paramType,  \
            paramNum,  paramNum, paramNum)


static const char *kMethodStringParamStr = "\n"
"    JSString *jsstring%d = JS_ValueToString(cx, argv[%d]);\n"
"    if (nsnull != jsstring%d) {\n"
"      b%d.SetString(JS_GetStringChars(jsstring%d));\n"
"    }\n"
"    else {\n"
"      b%d.SetString(\"\");   // Should this really be null?? \n"
"    }\n";

#define JSGEN_GENERATE_STRINGPARAM(buffer, paramNum) \
    sprintf(buffer, kMethodStringParamStr, paramNum, paramNum, \
            paramNum, paramNum, paramNum, paramNum)

static const char *kMethodBoolParamStr = "\n"
"    if (!JS_ValueToBoolean(cx, argv[%d], &b%d)) {\n"
"      return JS_FALSE;\n"
"    }\n";

#define JSGEN_GENERATE_BOOLPARAM(buffer, paramNum) \
    sprintf(buffer, kMethodBoolParamStr, paramNum, paramNum)

static const char *kMethodIntParamStr = "\n"
"    if (!JS_ValueToInt32(cx, argv[%d], (int32 *)&b%d)) {\n"
"      return JS_FALSE;\n"
"    }\n";

#define JSGEN_GENERATE_INTPARAM(buffer, paramNum) \
    sprintf(buffer, kMethodIntParamStr, paramNum, paramNum)

static const char *kMethodParamListStr = "b%d";
static const char *kMethodParamListDelimiterStr = ", ";

static const char *kMethodBodyMiddleStr =
"\n"
"    if (NS_OK != nativeThis->%s(%s%snativeRet)) {\n"
"      return JS_FALSE;\n"
"    }\n"
"\n";

static const char *kMethodBodyMiddleNoReturnStr =
"\n"
"    if (NS_OK != nativeThis->%s(%s)) {\n"
"      return JS_FALSE;\n"
"    }\n"
"\n";

static const char *kMethodObjectRetStr = 
"    if (nativeRet != nsnull) {\n"
"      nsIScriptObjectOwner *owner = nsnull;\n"
"      if (NS_OK == nativeRet->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {\n"
"        JSObject *object = nsnull;\n"
"        nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);\n"
"        if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {\n"
"          // set the return value\n"
"          *rval = OBJECT_TO_JSVAL(object);\n"
"        }\n"
"        NS_RELEASE(owner);\n"
"      }\n"
"      NS_RELEASE(nativeRet);\n"
"    }\n"
"    else {\n"
"      *rval = JSVAL_NULL;\n"
"    }\n";

static const char *kMethodStringRetStr = 
"    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());\n"
"    // set the return value\n"
"    *rval = STRING_TO_JSVAL(jsstring);\n";

static const char *kMethodIntRetStr = 
"    *rval = INT_TO_JSVAL(nativeRet);\n";

static const char *kMethodBoolRetStr =
"    *rval = BOOLEAN_TO_JSVAL(nativeRet);\n";

static const char *kMethodVoidRetStr = 
"    *rval = JSVAL_VOID;\n";

static const char *kMethodEndStr =
"  }\n"
"  else {\n"
"    return JS_FALSE;\n"
"  }\n"
"\n"
"  return JS_TRUE;\n"
"}\n";

void     
JSStubGen::GenerateMethods(IdlSpecification &aSpec)
{
  char buf[1024];
  ofstream *file = GetFile();

  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);

    int m, mcount = iface->FunctionCount();
    for (m = 0; m < mcount; m++) {
      IdlFunction *func = iface->GetFunctionAt(m);
      IdlVariable *rval = func->GetReturnValue();
      char method_name[128];
      char return_type[128];
      int p, pcount = func->ParameterCount();

      GetCapitalizedName(method_name, *func);
      GetVariableTypeForLocal(return_type, *rval);
      sprintf(buf, kMethodBeginStr, method_name, iface->GetName(),
              method_name, iface->GetName(), iface->GetName());
      *file << buf;
      if (rval->GetType() != TYPE_VOID) {
        sprintf(buf, kMethodReturnStr, return_type);
        *file << buf;
      }

      for (p = 0; p < pcount; p++) {
        IdlParameter *param = func->GetParameterAt(p);
        
        GetVariableTypeForMethodLocal(return_type, *param);
        sprintf(buf, kMethodParamStr, return_type, p);

        *file << buf;
      }

      sprintf(buf, kMethodBodyBeginStr, func->ParameterCount());
      *file << buf;

      for (p = 0; p < pcount; p++) {
        IdlParameter *param = func->GetParameterAt(p);
        
        switch(param->GetType()) {
          case TYPE_BOOLEAN:
            JSGEN_GENERATE_BOOLPARAM(buf, p);
            break;
          case TYPE_LONG:
          case TYPE_SHORT:
          case TYPE_ULONG:
          case TYPE_USHORT:
          case TYPE_CHAR:
          case TYPE_INT:
          case TYPE_UINT:
            JSGEN_GENERATE_INTPARAM(buf, p);
            break;
          case TYPE_STRING:
            JSGEN_GENERATE_STRINGPARAM(buf, p);
            break;
          case TYPE_OBJECT:
            JSGEN_GENERATE_OBJECTPARAM(buf, p, param->GetTypeName());
            break;
          default:
            // XXX Fail for other cases
            break;
        }
        *file << buf;
      }

      char param_buf[512];
      char *param_ptr = param_buf;
      param_buf[0] = '\0';
      for (p = 0; p < pcount; p++) {
        if (p > 0) {
          strcpy(param_ptr, kMethodParamListDelimiterStr);
          param_ptr += strlen(param_ptr);
        }
        sprintf(param_ptr, kMethodParamListStr, p);
        param_ptr += strlen(param_ptr);
      }

      if (rval->GetType() != TYPE_VOID) {
        if (pcount > 0) {
          strcpy(param_ptr, kMethodParamListDelimiterStr);
        }
        sprintf(buf, kMethodBodyMiddleStr, method_name, param_buf,
                rval->GetType() == TYPE_STRING ? "" : "&");
      }
      else {
        sprintf(buf, kMethodBodyMiddleNoReturnStr, method_name, param_buf);
      }
      *file << buf;

      switch(rval->GetType()) {
          case TYPE_BOOLEAN:
            *file << kMethodBoolRetStr;
            break;
          case TYPE_LONG:
          case TYPE_SHORT:
          case TYPE_ULONG:
          case TYPE_USHORT:
          case TYPE_CHAR:
          case TYPE_INT:
          case TYPE_UINT:
            *file << kMethodIntRetStr;
            break;
          case TYPE_STRING:
            *file << kMethodStringRetStr;
            break;
          case TYPE_OBJECT:
            *file << kMethodObjectRetStr;
            break;
          case TYPE_VOID:
            *file << kMethodVoidRetStr;
            break;
          default:
            // XXX Fail for other cases
            break;
      }

      *file << kMethodEndStr;
    }
  }
}


static const char *kJSClassStr = 
"\n\n/***********************************************************************/\n"
"//\n"
"// class for %s\n"
"//\n"
"JSClass %sClass = {\n"
"  \"%s\", \n"
"  JSCLASS_HAS_PRIVATE,\n"
"  JS_PropertyStub,\n"
"  JS_PropertyStub,\n"
"  Get%sProperty,\n"
"  Set%sProperty,\n"
"  Enumerate%s,\n"
"  Resolve%s,\n"
"  JS_ConvertStub,\n"
"  Finalize%s\n"
"};\n";

#define JSGEN_GENERATE_JSCLASS(buf, className)                              \
     sprintf(buf, kJSClassStr, className, className, className, className,  \
             className, className, className, className);

void     
JSStubGen::GenerateJSClass(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *iface = aSpec.GetInterfaceAt(0);
  char *name = iface->GetName();

  JSGEN_GENERATE_JSCLASS(buf, name);
  *file << buf;
}


static const char *kPropSpecBeginStr = 
"\n\n//\n"
"// %s class properties\n"
"//\n"
"static JSPropertySpec %sProperties[] =\n"
"{\n";

static const char *kPropSpecEntryStr = 
"  {\"%s\",    %s_%s,    JSPROP_ENUMERATE%s},\n";

static const char *kPropSpecReadOnlyStr = " | JSPROP_READONLY";

static const char *kPropSpecEndStr = 
"  {0}\n"
"};\n";

void     
JSStubGen::GenerateClassProperties(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);

  sprintf(buf, kPropSpecBeginStr, primary_iface->GetName(),
          primary_iface->GetName());
  *file << buf;
  
  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    char iface_name[128];

    strcpy(iface_name, iface->GetName());
    StrUpr(iface_name);
    int a, acount = iface->AttributeCount();
    for (a = 0; a < acount; a++) {
      IdlAttribute *attr = iface->GetAttributeAt(a);
      char attr_name[128];

      strcpy(attr_name, attr->GetName());
      StrUpr(attr_name);

      sprintf(buf, kPropSpecEntryStr, attr->GetName(),
              iface_name, attr_name, 
              attr->GetReadOnly() ? kPropSpecReadOnlyStr : "");
      *file << buf;
    }
  }

  *file << kPropSpecEndStr;
}


static const char *kFuncSpecBeginStr =
"\n\n//\n"
"// %s class methods\n"
"//\n"
"static JSFunctionSpec %sMethods[] = \n"
"{\n";

static const char *kFuncSpecEntryStr =
"  {\"%s\",          %s%s,     %d},\n";

static const char *kFuncSpecEndStr = 
"  {0}\n"
"};\n";

void     
JSStubGen::GenerateClassFunctions(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);

  sprintf(buf, kFuncSpecBeginStr, primary_iface->GetName(),
          primary_iface->GetName());
  *file << buf;
  
  int i, icount = aSpec.InterfaceCount();
  for (i = 0; i < icount; i++) {
    IdlInterface *iface = aSpec.GetInterfaceAt(i);
    
    int m, mcount = iface->FunctionCount();
    for (m = 0; m < mcount; m++) {
      char method_name[128];
      IdlFunction *func = iface->GetFunctionAt(m);

      GetCapitalizedName(method_name, *func);
      sprintf(buf, kFuncSpecEntryStr, func->GetName(),
              iface->GetName(), method_name,
              func->ParameterCount());
      *file << buf;
    }
  }

  *file << kFuncSpecEndStr;  
}

static const char *kConstructorStr = 
"\n\n//\n"
"// %s constructor\n"
"//\n"
"PR_STATIC_CALLBACK(JSBool)\n"
"%s(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)\n"
"{\n"
"  return JS_TRUE;\n"
"}\n";

void     
JSStubGen::GenerateConstructor(IdlSpecification &aSpec)
{
  char buf[512];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);

  sprintf(buf, kConstructorStr, primary_iface->GetName(),
          primary_iface->GetName());
  *file << buf;
}

static const char *kInitClassBeginStr =
"\n\n//\n"
"// %s class initialization\n"
"//\n"
"nsresult NS_Init%sClass(nsIScriptContext *aContext, void **aPrototype)\n"
"{\n"
"  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();\n"
"  JSObject *proto = nsnull;\n"
"  JSObject *constructor = nsnull;\n"
"  JSObject *parent_proto = nsnull;\n"
"  JSObject *global = JS_GetGlobalObject(jscontext);\n"
"  jsval vp;\n"
"\n"
"  if ((PR_TRUE != JS_LookupProperty(jscontext, global, \"%s\", &vp)) ||\n"
"      !JSVAL_IS_OBJECT(vp) ||\n"
"      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||\n"
"      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), \"prototype\", &vp)) || \n"
"      !JSVAL_IS_OBJECT(vp)) {\n\n";

#define JSGEN_GENERATE_INITCLASSBEGIN(buffer, className)         \
   sprintf(buffer, kInitClassBeginStr, className, className, className)

static const char *kGetParentProtoStr =
"    if (NS_OK != NS_Init%sClass(aContext, (void **)&parent_proto)) {\n"
"      return NS_ERROR_FAILURE;\n"
"    }\n";

static const char *kInitClassBodyStr =
"    proto = JS_InitClass(jscontext,     // context\n"
"                         global,        // global object\n"
"                         parent_proto,  // parent proto \n"
"                         &%sClass,      // JSClass\n"
"                         %s,            // JSNative ctor\n"
"                         0,             // ctor args\n"
"                         %sProperties,  // proto props\n"
"                         %sMethods,     // proto funcs\n"
"                         nsnull,        // ctor props (static)\n"
"                         nsnull);       // ctor funcs (static)\n"
"    if (nsnull == proto) {\n"
"      return NS_ERROR_FAILURE;\n"
"    }\n"
"\n";

#define JSGEN_GENERATE_INITCLASSBODY(buffer, className)         \
   sprintf(buffer, kInitClassBodyStr, className, className,     \
           className, className) 

static const char *kInitStaticBeginStr =
"    if ((PR_TRUE == JS_LookupProperty(jscontext, global, \"%s\", &vp)) &&\n"
"        JSVAL_IS_OBJECT(vp) &&\n"
"        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {\n";

static const char *kInitStaticEntryStr =
"      vp = INT_TO_JSVAL(nsIDOM%s::%s);\n"
"      JS_SetProperty(jscontext, constructor, \"%s\", &vp);\n"
"\n";

static const char *kInitStaticEndStr =
"    }\n"
"\n";

static const char *kInitClassEndStr =
"  }\n"
"  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {\n"
"    proto = JSVAL_TO_OBJECT(vp);\n"
"  }\n"
"  else {\n"
"    return NS_ERROR_FAILURE;\n"
"  }\n"
"\n"
"  if (aPrototype) {\n"
"    *aPrototype = proto;\n"
"  }\n"
"  return NS_OK;\n"
"}\n";

void     
JSStubGen::GenerateInitClass(IdlSpecification &aSpec)
{
  char buf[2048];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);
  char *primary_class = primary_iface->GetName();

  JSGEN_GENERATE_INITCLASSBEGIN(buf, primary_class);
  *file << buf;

  if (primary_iface->BaseClassCount() > 0) {
    char *base_class = primary_iface->GetBaseClassAt(0);
    
    sprintf(buf, kGetParentProtoStr, base_class);
    *file << buf;
  }

  JSGEN_GENERATE_INITCLASSBODY(buf, primary_class);
  *file << buf;

  int c, ccount = primary_iface->ConstCount();
  
  if (ccount > 0) {
    sprintf(buf, kInitStaticBeginStr, primary_iface->GetName());
    *file << buf;
    
    for (c = 0; c < ccount; c++) {
      IdlVariable *var = primary_iface->GetConstAt(c);
      
      if (NULL != var) {
        sprintf(buf, kInitStaticEntryStr, primary_iface->GetName(), 
              var->GetName(), var->GetName());
        *file << buf;
      }
    }
    
    *file << kInitStaticEndStr;
  }

  *file << kInitClassEndStr;
}


static const char *kNewJSObjectStr =
"\n\n//\n"
"// Method for creating a new %s JavaScript object\n"
"//\n"
"extern \"C\" NS_DOM NS_NewScript%s(nsIScriptContext *aContext, nsIDOM%s *aSupports, nsISupports *aParent, void **aReturn)\n"
"{\n"
"  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, \"null argument to NS_NewScript%s\");\n"
"  JSObject *proto;\n"
"  JSObject *parent;\n"
"  nsIScriptObjectOwner *owner;\n"
"  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();\n"
"\n"
"  if (nsnull == aParent) {\n"
"    parent = nsnull;\n"
"  }\n"
"  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {\n"
"    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {\n"
"      NS_RELEASE(owner);\n"
"      return NS_ERROR_FAILURE;\n"
"    }\n"
"    NS_RELEASE(owner);\n"
"  }\n"
"  else {\n"
"    return NS_ERROR_FAILURE;\n"
"  }\n"
"\n"
"  if (NS_OK != NS_Init%sClass(aContext, (void **)&proto)) {\n"
"    return NS_ERROR_FAILURE;\n"
"  }\n"
"\n"
"  // create a js object for this class\n"
"  *aReturn = JS_NewObject(jscontext, &%sClass, proto, parent);\n"
"  if (nsnull != *aReturn) {\n"
"    // connect the native object to the js object\n"
"    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSupports);\n"
"    NS_ADDREF(aSupports);\n"
"  }\n"
"  else {\n"
"    return NS_ERROR_FAILURE; \n"
"  }\n"
"\n"
"  return NS_OK;\n"
"}\n";

#define JSGEN_GENERATE_NEWJSOBJECT(buffer, className)      \
    sprintf(buffer, kNewJSObjectStr, className, className, \
            className, className, className, className)

void     
JSStubGen::GenerateNew(IdlSpecification &aSpec)
{
  char buf[2048];
  ofstream *file = GetFile();
  IdlInterface *primary_iface = aSpec.GetInterfaceAt(0);
  char *primary_class = primary_iface->GetName();

  JSGEN_GENERATE_NEWJSOBJECT(buf, primary_class);
  *file << buf;
}

