/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsGBKConvUtil_h__
#define nsGBKConvUtil_h__
#include "prtypes.h"
#include "nscore.h"
class nsGBKConvUtil {
public:
  nsGBKConvUtil() {  };
  ~nsGBKConvUtil() { };
  void InitToGBKTable();
  PRUnichar GBKCharToUnicode(char aByte1, char aByte2);
  PRBool UnicodeToGBKChar(PRUnichar aChar, PRBool aToGL, 
                           char* aOutByte1, char* aOutByte2);
  void FillInfo(PRUint32 *aInfo, PRUint8 aStart1, PRUint8 aEnd1,
                                 PRUint8 aStart2, PRUint8 aEnd2);
  void FillGB2312Info(PRUint32 *aInfo);
};
#endif /* nsGBKConvUtil_h__ */
