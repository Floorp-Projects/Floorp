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
#include "java_lang_Double.h"
#include "java_lang_String.h"
#include <stdio.h>

extern "C" {

/*
 * Class : java/lang/Double
 * Method : doubleToLongBits
 * Signature : (D)J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_lang_Double_doubleToLongBits(float64 d)
{
    return *reinterpret_cast<Int64*>(&d);
}


/*
 * Class : java/lang/Double
 * Method : longBitsToDouble
 * Signature : (J)D
 */
NS_EXPORT NS_NATIVECALL(float64)
Netscape_Java_java_lang_Double_longBitsToDouble(int64 i)
{
    return *reinterpret_cast<Flt64*>(&i);
}


/*
 * Class : java/lang/Double
 * Method : valueOf0
 * Signature : (Ljava/lang/String;)D
 */
NS_EXPORT NS_NATIVECALL(float64)
Netscape_Java_java_lang_Double_valueOf0(Java_java_lang_String *)
{
  printf("Netscape_Java_java_lang_Double_valueOf0() not implemented");
  return 0;
}

} /* extern "C" */

