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

#include "nsParserUtils.h"
#include "nsIParser.h" // for kQuote et. al.
#include "jsapi.h"

// This method starts at aOffSet in aStr and tries to find aChar. It keeps 
// skipping whitespace till it finds aChar or some other non-whitespace character.  If
// it finds aChar, it returns aChar's offset.  If it finds some other non-whitespace character
// or runs into the end of the string, it returns -1.
static PRInt32
FindWhileSkippingWhitespace(nsString& aStr, PRUnichar aChar, PRInt32 aOffset)
{
  PRInt32 i = aOffset;
  PRUnichar ch = aStr.CharAt(i);
  PRInt32 index = -1;

  while (ch == '\n' || ch == '\t' || ch == '\r') {
    ch = aStr.CharAt(++i);
  }

  if (ch == aChar)
    index = i;

  return index;
}


nsresult
nsParserUtils::GetQuotedAttributeValue(nsString& aSource,
                                       const nsString& aAttribute,
                                       nsString& aValue)
{  
  PRInt32 startOfAttribute = 0;     // Index into aSource where the attribute name starts
  PRInt32 startOfValue = 0;         // Index into aSource where the attribute value starts
  PRInt32 posnOfValueDelimeter = 0; 
  nsresult result = NS_ERROR_FAILURE;

  // While there are more characters to look at
  while (startOfAttribute != -1) {
    // Find the attribute starting at offset
    startOfAttribute = aSource.Find(aAttribute, PR_FALSE, startOfAttribute);
    // If attribute found
    if (startOfAttribute != -1) { 
      // Find the '=' character while skipping whitespace
      startOfValue = FindWhileSkippingWhitespace(aSource, '=', startOfAttribute + aAttribute.Length());
      // If '=' found
      if (startOfValue != -1) {
        PRUnichar delimeter = kQuote;
        // Find the quote or apostrophe while skipping whitespace
        posnOfValueDelimeter = FindWhileSkippingWhitespace(aSource, kQuote, startOfValue + 1);
        if (posnOfValueDelimeter == -1) {
          posnOfValueDelimeter = FindWhileSkippingWhitespace(aSource, kApostrophe, startOfValue + 1);
          delimeter = kApostrophe;
        }
        // If quote or apostrophe found
        if (posnOfValueDelimeter != -1) {
          startOfValue = posnOfValueDelimeter + 1;
          // Find the ending quote or apostrophe
          posnOfValueDelimeter = aSource.FindChar(delimeter, PR_FALSE, startOfValue);
          // If found
          if (posnOfValueDelimeter != -1) {
            // Set the value of the attibute and exit the loop
            // The attribute value starts at startOfValue and ends at (posnOfValueDelimeter - 1)
            aSource.Mid(aValue, startOfValue, posnOfValueDelimeter - startOfValue);
            result = NS_OK;
            break;
          }
          else {
            // Try to find the attribute in the remainder of the string
            startOfAttribute++;
            continue;
          } // Endif found  
        }
        else {
          // Try to find the attribute in the remainder of the string
          startOfAttribute++;
          continue;
        } // Endif quote or apostrophe found
      } 
      else {
        // Try to find the attribute in the remainder of the string
        startOfAttribute++;
        continue;
      } // Endif '=' found
    } // Endif attribute found
  } // End while
  
  return result;
}


// XXX Stolen from nsHTMLContentSink. Needs to be shared.
// XXXbe share also with nsRDFParserUtils.cpp and nsHTMLContentSink.cpp
// Returns PR_TRUE if the language name is a version of JavaScript and
// PR_FALSE otherwise
PRBool
nsParserUtils::IsJavaScriptLanguage(const nsString& aName, const char* *aVersion)
{
  JSVersion version = JSVERSION_UNKNOWN;

  if (aName.EqualsIgnoreCase("JavaScript") ||
      aName.EqualsIgnoreCase("LiveScript") ||
      aName.EqualsIgnoreCase("Mocha")) {
    version = JSVERSION_DEFAULT;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.0")) {
    version = JSVERSION_1_0;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.1")) {
    version = JSVERSION_1_1;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.2")) {
    version = JSVERSION_1_2;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.3")) {
    version = JSVERSION_1_3;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.4")) {
    version = JSVERSION_1_4;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.5")) {
    version = JSVERSION_1_5;
  }
  if (version == JSVERSION_UNKNOWN)
    return PR_FALSE;
  *aVersion = JS_VersionToString(version);
  return PR_TRUE;
}

