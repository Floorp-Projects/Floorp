/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "JNIHeaderGenerator.h"
#include "prprf.h"
#include "plstr.h"
#include "CUtils.h"
#include "StringUtils.h"
#include "JavaObject.h"
#include "FieldOrMethod.h"
#include "JNIManglers.h"

#ifdef USE_PR_IO
#define PR_fprintf fprintf
#define PR_Write fwrite
#define PRFileDesc FILE
#endif

bool JNIHeaderGenerator::genHeaderFile(ClassFileSummary &summ,
				       PRFileDesc *fp)
{
  const ClassFileReader &reader = *summ.getReader();
  const char *className = reader.getThisClass()->getUtf()->getUtfString();

  PR_fprintf(fp, "/* This file is machine-generated. Do not edit!! */\n");

  PR_fprintf(fp, "\n/* Header for class %s */\n\n", className);

  PR_fprintf(fp, "#include <jni.h>\n\n");

  /* Go through each native method and generate a declaration for it */
  const Method **methods = summ.getMethods();
  Uint32 methodCount = summ.getMethodCount();
  
  JNIShortMangler mangler;
  
  /* extern "C" the generated method headers */
  PR_fprintf(fp, "#ifdef __cplusplus\n");
  PR_fprintf(fp, "extern \"C\" {\n");
  PR_fprintf(fp, "#endif\n");

  for (Uint32 i = 0; i < methodCount; i++) {
    Method *m = (Method *) methods[i];
    const Signature &sig = m->getSignature();
    
    if ((m->getModifiers() & CR_METHOD_NATIVE)) {
      /* Generate the return type */
      char *retString = getArgString(*sig.resultType);
      const char *methodName = m->getName();
      const char *signature = const_cast<Method *>(m)->getSignatureString();

      if (!retString)
	return false;
      
      int32 len = PL_strlen(methodName) + PL_strlen(className)*2 +
	PL_strlen(signature)*2 + sizeof("Java___");
  
      TemporaryBuffer copy(len);
      char *mangledMethodName = copy;
      
      if (!mangler.mangle(className, methodName, signature, 
			  mangledMethodName, len)) 
	continue;

      PR_fprintf(fp, "\n/*\n");
      PR_fprintf(fp, " * Class : %s\n", className);
      PR_fprintf(fp, " * Method : %s\n", methodName);
      PR_fprintf(fp, " * Signature : %s\n", signature);
      PR_fprintf(fp, " */\n");

      PR_fprintf(fp, "JNIEXPORT JNICALL(%s)\n%s(", 
		 retString, mangledMethodName);

      // Kills the release build since NSPR needs to give us a clean way to 
      // free strings that it allocates
#ifdef DEBUG
      //free(retString);
#endif

      /* The first argument is always a (JNIEnv *) */
      PR_fprintf(fp, "JNIEnv *");
      
      /* For static methods, the second argument is a jclass */
      if (m->getModifiers() & CR_METHOD_STATIC)
	PR_fprintf(fp, ", jclass");

      for (uint32 j = 0; j < sig.nArguments; j++) {
	char *typeString = getArgString(*sig.argumentTypes[j]);
	
	if (!typeString)
	  return false;
	
	PR_fprintf(fp, ", %s", typeString);
	
	// Currently does not work with the release build since NSPR needs to
	// give us a clean way of freeing strings that it allocates
#ifdef DEBUG
	//free(typeString);
#endif
	
      }       	
      
      PR_fprintf(fp, ");\n\n");
    }
  }
  
  PR_fprintf(fp, "#ifdef __cplusplus\n");
  PR_fprintf(fp, "}\n");    /* Matches the extern "C" declaration generated earlier */
  PR_fprintf(fp, "#endif \n\n");  /* Matches the #ifdef __cplusplus */
  
  return true;
}


char *JNIHeaderGenerator::getArgString(const Type &type)
{
  switch (type.typeKind) {
  case tkChar:    
    return PL_strdup("jchar");
    break;
      
  case tkShort:
    return PL_strdup("jshort");
    break;
    
  case tkInt:
    return PL_strdup("jint");
    break;
    
  case tkByte:
    return PL_strdup("jbyte");
    break;
    
  case tkBoolean:
    return PL_strdup("jbool");
    break;

  case tkLong:
    return PL_strdup("jlong");
    break;
      
  case tkFloat:
    return PL_strdup("jfloat");
    break;
    
  case tkDouble:
    return PL_strdup("jdouble");
    break;

  case tkVoid:
    return PL_strdup("void");
    break;
  
  case tkObject: {
    const Class &clazz = *static_cast<const Class *>(&type);

    /* Special-case certain objects XXX Need to do throwable here */
    if (!PL_strcmp(clazz.getName(), "java.lang.String"))
      return PL_strdup("jstring");
    else
      return PL_strdup("jobject");
  }
								 
  case tkInterface:
    return PL_strdup("jobject");
    
  case tkArray: {
    const Array &atype = *static_cast<const Array *>(&type);
    
    switch (atype.componentType.typeKind) {
    case tkChar:    
      return PL_strdup("jcharArray");
      
    case tkShort:
      return PL_strdup("jshortArray");

    case tkInt:
      return PL_strdup("jintArray");
    
    case tkByte:
      return PL_strdup("jbyteArray");
    
    case tkBoolean:
      return PL_strdup("jboolArray");

    case tkLong:
      return PL_strdup("jlongArray");
      
    case tkFloat:
      return PL_strdup("jfloatArray");
    
    case tkDouble:
      return PL_strdup("jdoubleArray");

    case tkObject:  
    case tkInterface:
    case tkArray: 
      return PL_strdup("jobjectArray");

    default:
      return 0;
    }
    break;
  }

  default:
    return 0;
  }
    
}
