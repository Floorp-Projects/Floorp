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
#ifndef nsHTMLEntites_h___
#define nsHTMLEntities_h___

#include "nshtmlpars.h"
#include "nsString.h"

class NS_HTMLPARS nsHTMLEntities {
public:

  static void AddRefTable(void);
  static void ReleaseTable(void);

/**
 * Translate an entity string into it's unicode value. This call
 * returns -1 if the entity cannot be mapped. Note that the string
 * passed in must NOT have the leading "&" nor the trailing ";"
 * in it.
 */
  static PRInt32 EntityToUnicode(const nsAReadableString& aEntity);
  static PRInt32 EntityToUnicode(const nsCString& aEntity);

/**
 * Translate an entity string into it's unicode value. This call
 * returns an empty string if the entity cannot be mapped. 
 * Note that the string returned DOES NOT have the leading "&" nor 
 * the trailing ";" in it.
 */
  static const nsCString& UnicodeToEntity(PRInt32 aUnicode);
};


#endif /* nsHTMLEntities_h___ */
