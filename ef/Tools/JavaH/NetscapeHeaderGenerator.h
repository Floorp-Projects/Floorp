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
#ifndef _NETSCAPE_HEADER_GENERATOR_H_
#define _NETSCAPE_HEADER_GENERATOR_H_

#include "HeaderGenerator.h"

/* Header generator for Electrical-fire style internal
 * native methods. Internal native methods are of the form
 * NS_<mangledMethodName> where the mangledMethodName is 
 * the method name of the Java method mangled exactly as
 * specified by the JNI. Internal structures corresponding to
 * Java objects are all sub-classes of JavaObject; the name of
 * the struct corresponding to a class whose fully qualified name
 * <foo> is Java_<foo>, with underscore characters ('_') replacing the
 * slashes ('/') in the fully-qualified name.
 * The signature of the method is exactly as in the Java class, with the 
 * following differences: 
 * - All Class types are replaced by a sub-class of JavaObject.
 * - All array types are replaced by a sub-class of JavaArray.
 *
 * For example, consider the Java Class foo:
 *
 * class Foo extends String {
 *  String str;
 *  int i;
 *  char j;
 *  long k;
 * 
 *  public native void bar(String s, int i, int j);
 * };
 * 
 * will generate:
 *
 * #include "java_lang_String.h"
 * struct Java_Foo : Java_java_lang_String {
 *   Java_java_lang_String *str;
 *   int32 i;
 *   int32 j;
 *   int64 k;
 * };
 * 
 * void NS_Java_Foo_bar(Java_Foo *obj, Java_java_lang_string *str,
 *                      int32 i, int32 j);
 */
class NetscapeHeaderGenerator : public HeaderGenerator {
public:
  NetscapeHeaderGenerator(ClassCentral &central) :
    HeaderGenerator(central) { } 

  virtual ~NetscapeHeaderGenerator() { } 

protected:
  virtual bool needParents() { return true; }
#ifndef USE_PR_IO
  virtual bool genHeaderFile(ClassFileSummary &summ, PRFileDesc *fp);
#else
  virtual bool genHeaderFile(ClassFileSummary &summ, FILE *fp);
#endif
private:
};

#endif /* _NETSCAPE_HEADER_GENERATOR_H_ */
