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
#include "java_lang_Float.h"
#include <stdio.h>

extern "C" {
/*
 * Class : java/lang/Float
 * Method : floatToIntBits
 * Signature : (F)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_Float_floatToIntBits(float f)
{
    return *reinterpret_cast<Int32*>(&f);
}


/*
 * Class : java/lang/Float
 * Method : intBitsToFloat
 * Signature : (I)F
 */
NS_EXPORT NS_NATIVECALL(float)
Netscape_Java_java_lang_Float_intBitsToFloat(int32 i)
{
      return *reinterpret_cast<Flt32*>(&i);
}

} /* extern "C" */

