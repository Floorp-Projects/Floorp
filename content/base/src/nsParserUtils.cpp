/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsParserUtils.h"
#include "nsIParser.h" // for kQuote et. al.
#include "jsapi.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#define SKIP_WHITESPACE(iter, end_iter)                          \
  while ((iter) != (end_iter) && nsCRT::IsAsciiSpace(*(iter))) { \
    ++(iter);                                                    \
  }                                                              \
  if ((iter) == (end_iter))                                      \
    break

#define SKIP_ATTR_NAME(iter, end_iter)                            \
  while ((iter) != (end_iter) && !nsCRT::IsAsciiSpace(*(iter)) && \
         *(iter) != '=') {                                        \
    ++(iter);                                                     \
  }                                                               \
  if ((iter) == (end_iter))                                       \
    break

PRBool
nsParserUtils::GetQuotedAttributeValue(const nsAString& aSource,
                                       const nsAString& aAttribute,
                                       nsAString& aValue)
{
  NS_ASSERTION(!aAttribute.IsEmpty(), "Empty attribute name cannot be searched for usefully");
  aValue.Truncate();
  nsAString::const_iterator start, end;
  aSource.BeginReading(start);
  aSource.EndReading(end);
  nsAString::const_iterator iter;
  
  while (start != end) {
    SKIP_WHITESPACE(start, end);
    iter = start;
    SKIP_ATTR_NAME(iter, end);

    // Remember the attr name.
    const nsAString & attrName = Substring(start, iter);

    // Now check whether this is a valid name="value" pair.
    start = iter;
    SKIP_WHITESPACE(start, end);
    if (*start != '=') {
      // No '=', so this is not a name="value" pair.  We don't know
      // what it is, and we have no way to handle it.
      break;
    }
    
    // Have to skip the value.
    ++start;
    SKIP_WHITESPACE(start, end);
    PRUnichar q = *start;
    if (q != kQuote && q != kApostrophe) {
      // Not a valid quoted value, so bail.
      break;
    }
    
    ++start;  // Point to the first char of the value.
    iter = start;
    if (!FindCharInReadable(q, iter, end)) {
      // Oops, unterminated quoted string.
      break;
    }
    
    // At this point attrName holds the name of the "attribute" and
    // the value is between start and iter.
    
    if (!attrName.Equals(aAttribute)) {
      // Resume scanning after the end of the attribute value.
      start = iter;
      ++start;  // To move past the quote char.
      continue;
    }

    aValue = Substring(start, iter);
    return PR_TRUE;
  }

  return PR_FALSE;
}


// XXX Stolen from nsHTMLContentSink. Needs to be shared.
// XXXbe share also with nsRDFParserUtils.cpp and nsHTMLContentSink.cpp
// Returns PR_TRUE if the language name is a version of JavaScript and
// PR_FALSE otherwise
PRBool
nsParserUtils::IsJavaScriptLanguage(const nsString& aName, const char* *aVersion)
{
  JSVersion version = JSVERSION_UNKNOWN;

  if (aName.LowerCaseEqualsLiteral("javascript") ||
      aName.LowerCaseEqualsLiteral("livescript") ||
      aName.LowerCaseEqualsLiteral("mocha")) {
    version = JSVERSION_DEFAULT;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.0")) {
    version = JSVERSION_1_0;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.1")) {
    version = JSVERSION_1_1;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.2")) {
    version = JSVERSION_1_2;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.3")) {
    version = JSVERSION_1_3;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.4")) {
    version = JSVERSION_1_4;
  }
  else if (aName.LowerCaseEqualsLiteral("javascript1.5")) {
    version = JSVERSION_1_5;
  }
  if (version == JSVERSION_UNKNOWN)
    return PR_FALSE;
  *aVersion = JS_VersionToString(version);
  return PR_TRUE;
}

void
nsParserUtils::SplitMimeType(const nsAString& aValue, nsString& aType,
                             nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aType = Substring(aValue, 0, semiIndex);
    aParams = Substring(aValue, semiIndex + 1,
                       aValue.Length() - (semiIndex + 1));
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
}
