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
nsHtml5Portability::releaseLocal(nsIAtom* local)
{
  NS_RELEASE(local);
}

void
nsHtml5Portability::releaseElement(nsIContent* element)
{
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
