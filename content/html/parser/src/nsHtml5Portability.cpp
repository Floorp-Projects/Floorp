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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "jArray.h"

#include "nsHtml5Portability.h"


nsIAtom*
nsHtml5Portability::newLocalNameFromBuffer(PRUnichar* buf, PRInt32 offset, PRInt32 length)
{
  // Optimization opportunity: make buf itself null-terminated
  PRUnichar* nullTerminated = new PRUnichar[length + 1];
  memcpy(nullTerminated,buf, length * sizeof(PRUnichar));
  nullTerminated[length] = 0;
  nsIAtom* rv = NS_NewAtom(nullTerminated);
  delete[] nullTerminated;
  return rv;
}

nsString*
nsHtml5Portability::newStringFromBuffer(PRUnichar* buf, PRInt32 offset, PRInt32 length)
{
  return new nsString(buf + offset, length);
}

nsString*
nsHtml5Portability::newEmptyString()
{
  return new nsString();
}

jArray<PRUnichar,PRInt32>
nsHtml5Portability::newCharArrayFromLocal(nsIAtom* local)
{
  nsAutoString temp;
  local->ToString(temp);
  PRInt32 len = temp.Length();
  jArray<PRUnichar,PRInt32> rv = jArray<PRUnichar,PRInt32>(len);
  memcpy(rv, temp.BeginReading(), len * sizeof(PRUnichar));
  return rv;  
}

jArray<PRUnichar,PRInt32>
nsHtml5Portability::newCharArrayFromString(nsString* string)
{
  PRInt32 len = string->Length();
  jArray<PRUnichar,PRInt32> rv = jArray<PRUnichar,PRInt32>(len);
  memcpy(rv, string->BeginReading(), len * sizeof(PRUnichar));
  return rv;
}

void
nsHtml5Portability::releaseString(nsString* str)
{
  delete str;
}

void
nsHtml5Portability::retainLocal(nsIAtom* local)
{
  NS_IF_ADDREF(local);
}

void
nsHtml5Portability::releaseLocal(nsIAtom* local)
{
  NS_IF_RELEASE(local);
}

void
nsHtml5Portability::retainElement(nsIContent* element)
{
  NS_IF_ADDREF(element);
}

void
nsHtml5Portability::releaseElement(nsIContent* element)
{
  NS_IF_RELEASE(element);
}

PRBool
nsHtml5Portability::localEqualsBuffer(nsIAtom* local, PRUnichar* buf, PRInt32 offset, PRInt32 length)
{
  nsAutoString temp = nsAutoString(buf + offset, length);
  return local->Equals(temp);
}

PRBool
nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(nsString* lowerCaseLiteral, nsString* string)
{
  if (!string) {
    return PR_FALSE;
  }
  if (lowerCaseLiteral->Length() > string->Length()) {
    return PR_FALSE;
  }
  const PRUnichar* litPtr = lowerCaseLiteral->BeginReading();
  const PRUnichar* end = lowerCaseLiteral->EndReading();
  const PRUnichar* strPtr = string->BeginReading();
  while (litPtr < end) {
    PRUnichar litChar = *litPtr;
    PRUnichar strChar = *strPtr;
    if (strChar >= 'A' && strChar <= 'Z') {
      strChar += 0x20;
    }
    if (litChar != strChar) {
      return PR_FALSE;
    }
    ++litPtr;
    ++strPtr;
  }
  return PR_TRUE;
}

PRBool
nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(nsString* lowerCaseLiteral, nsString* string)
{
  if (!string) {
    return PR_FALSE;
  }
  if (lowerCaseLiteral->Length() != string->Length()) {
    return PR_FALSE;
  }
  const PRUnichar* litPtr = lowerCaseLiteral->BeginReading();
  const PRUnichar* end = lowerCaseLiteral->EndReading();
  const PRUnichar* strPtr = string->BeginReading();
  while (litPtr < end) {
    PRUnichar litChar = *litPtr;
    PRUnichar strChar = *strPtr;
    if (strChar >= 'A' && strChar <= 'Z') {
      strChar += 0x20;
    }
    if (litChar != strChar) {
      return PR_FALSE;
    }
    ++litPtr;
    ++strPtr;
  }
  return PR_TRUE;
}

PRBool
nsHtml5Portability::literalEqualsString(nsString* literal, nsString* string)
{
  return literal->Equals(*string);
}

jArray<PRUnichar,PRInt32>
nsHtml5Portability::isIndexPrompt()
{
  // Yeah, this whole method is uncool
  char* literal = "This is a searchable index. Insert your search keywords here: ";
  jArray<PRUnichar,PRInt32> rv = jArray<PRUnichar,PRInt32>(62);
  for (PRInt32 i = 0; i < 62; ++i) {
    rv[i] = literal[i];
  }
  return rv;
}

PRBool
nsHtml5Portability::localEqualsHtmlIgnoreAsciiCase(nsIAtom* name)
{
  const char* reference = "html";
  const char* buf;
  name->GetUTF8String(&buf);
  for(;;) {
    char refChar = *reference;
    char bufChar = *buf;
    if (bufChar >= 'A' && bufChar <= 'Z') {
      bufChar += 0x20;
    }
    if (refChar != bufChar) {
      return PR_FALSE;
    }
    if (!refChar) {
      return PR_TRUE;
    }
    ++reference;
    ++buf;
  }
  return PR_TRUE; // unreachable but keep compiler happy
}

void
nsHtml5Portability::initializeStatics()
{
}

void
nsHtml5Portability::releaseStatics()
{
}
