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
#ifndef _JAVA_STRING_H_
#define _JAVA_STRING_H_

#include "JavaObject.h"

class ClassCentral;


/* A String is a Java Object that holds an array of characters This is the C++
 * structure corresponding to java/lang/string.
 */
struct NS_EXTERN JavaString: JavaObject {
private:
    JavaArray *value;   /* Array of Java char's */
    Int32 offset;       /* First index of the storage that is used */
    Int32 count;        /* Number of characters in the string */
    Int64 serialVersionId;
    
    static Type *strType; /* Type: must be set to Java/lang/String::type */
    
public:
    
    static JavaString* make(const char* str) {return new JavaString(str); /* To change when we have a GC. */}

    JavaString(const char *str);
    const int16 *getStr() { return (int16 *) ((char *) value+arrayEltsOffset(tkChar)); }
    
    /* Must be called before an instance of string is created */
    static void staticInit();  
    
    char *convertUtf();
    static void freeUtf(char *);
    
    // Return the size of the string
    Int32 getLength() { return count; }
    void dump();
};

inline JavaString &asJavaString(JavaObject &obj)
{
    assert(&obj.getType() == &asType(Standard::get(cString))); 
    return *(JavaString*)&obj;
}


#endif /* _JAVA_STRING_H_ */
