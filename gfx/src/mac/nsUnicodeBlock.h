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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
#ifndef nsUnicodeBlock_h__
#define nsUnicodeBlock_h__


typedef enum {
 // blocks which always use the same script to render, regardless of the order of fonts
 kGreek = 0,
 kCyrillic,
 kArmenian,
 kHebrew,
 kArabic, 
 kDevanagari,
 kBengali,
 kGurmukhi,
 kGujarati, 
 kOriya,
 kTamil,
 kTelugu,
 kKannada,
 kMalayalam,
 kThai,
 kLao,
 kTibetan,
 kGeorgian,
 kHangul,
 kBopomofo,
 
 kUnicodeBlockFixedScriptMax,	
 
 // blocks which may use different script to render, depend on the order of fonts
 kBasicLatin = kUnicodeBlockFixedScriptMax,
 kLatin ,
 kCJKMisc,
 kHiraganaKatakana,
 kCJKIdeographs,
 kOthers, 
 
 kUnicodeBlockSize,
 
 kUnicodeBlockVarScriptMax = kUnicodeBlockSize - kUnicodeBlockFixedScriptMax
} nsUnicodeBlock;

#endif nsUnicodeBlock_h__
