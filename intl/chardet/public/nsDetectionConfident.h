/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsDetetctionConfident_h__
#define nsDetetctionConfident_h__

/*
  This type is used to indidcate how confident the detection module about
  the return result.
 
  eNoAnswerYet is used to indidcate that the detector have not find out a 
               answer yet based on the data it received.
  eBestAnswer  is used to indidcate that the answer the detector returned
               is the best one within the knowledge of the detector.
               In other words, the test to all other candidcates fail.

               For example, the (Shift_JIS/EUC-JP/ISO-2022-JP) detection
               module may return this with answer "Shift_JIS "if it receive 
               bytes > 0x80 (which make ISO-2022-JP test failed) and byte 
               0x82 (which may EUC-JP test failed)

  eSureAnswer  is used to indidcate that the detector are 100% sure about the 
               answer. 
               Exmaple 1; the Shift_JIS/ISO-2022-JP/EUC-JP detector return
               this w/ ISO-2022-JP when it hit one of the following ESC seq
                  ESC ( J
                  ESC $ @
                  ESC $ B
               Example 2: the detector which can detect UCS2 return w/ UCS2
               when the first 2 byte are BOM mark.
               Example 3: the Korean detector return ISO-2022-KR when it
               hit ESC $ ) C
  
 */
typedef enum {
  eNoAnswerYet = 0,
  eBestAnswer,
  eSureAnswer,
  eNoAnswerMatch
} nsDetectionConfident;

#endif /* nsDetetctionConfident_h__ */
