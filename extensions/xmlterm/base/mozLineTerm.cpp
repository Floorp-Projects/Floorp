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
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

// mozLineTerm.cpp: class implementing mozILineTerm/mozILineTermAux interfaces,
// providing an XPCOM/XPCONNECT wrapper for the LINETERM module

#include <stdio.h>

#include "nspr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsReadableUtils.h"
#include "prlog.h"
#include "nsMemory.h"

#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrincipal.h"

#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozLineTerm.h"

#define MAXCOL 4096            // Maximum columns in line buffer

/////////////////////////////////////////////////////////////////////////
// mozLineTerm implementaion
/////////////////////////////////////////////////////////////////////////

NS_GENERIC_FACTORY_CONSTRUCTOR(mozLineTerm)

NS_IMPL_THREADSAFE_ISUPPORTS2(mozLineTerm, 
                              mozILineTerm,
                              mozILineTermAux)


PRBool mozLineTerm::mLoggingEnabled = PR_FALSE;

PRBool mozLineTerm::mLoggingInitialized = PR_FALSE;

NS_METHOD
mozLineTerm::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (!mLoggingInitialized) {
    // Initialize all LINETERM operations
    // (This initialization needs to be done at factory creation time;
    //  trying to do it earlier, i.e., at registration time,
    //  does not work ... something to do with loading of static global
    //  variables.)

    int messageLevel = 0;
    char* debugStr = (char*) PR_GetEnv("LTERM_DEBUG");

    if (debugStr && (strlen(debugStr) == 1)) {
      messageLevel = 98;
      debugStr = nsnull;
    }

    int result = lterm_init(0);
    if (result == 0) {
      tlog_set_level(LTERM_TLOG_MODULE, messageLevel, debugStr);
    }
    mLoggingInitialized = PR_TRUE;

    char* logStr = (char*) PR_GetEnv("LTERM_LOG");
    if (logStr && *logStr) {
      // Enable LineTerm logging
      mozLineTerm::mLoggingEnabled = PR_TRUE;
    }
  }

  return mozLineTermConstructor( aOuter,
                                 aIID,
                                 aResult );
}

mozLineTerm::mozLineTerm() :
  mCursorRow(0),
  mCursorColumn(0),
  mSuspended(PR_FALSE),
  mEchoFlag(PR_TRUE),
  mObserver(nsnull),
  mCookie(EmptyString()),
  mLastTime(LL_ZERO)
{
  mLTerm = lterm_new();
}


mozLineTerm::~mozLineTerm()
{
  lterm_delete(mLTerm);
  mObserver = nsnull;
}


/** Checks if preference settings are secure for LineTerm creation and use
 */
NS_IMETHODIMP mozLineTerm::ArePrefsSecure(PRBool *_retval)
{
  nsresult result;

  XMLT_LOG(mozLineTerm::ArePrefsSecure,30,("\n"));

  if (!_retval)
    return NS_ERROR_FAILURE;

  *_retval = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefBranch)
    return NS_ERROR_FAILURE;

  // Check if Components JS object is secure
  PRBool checkXPC;
  result = prefBranch->GetBoolPref("security.checkxpconnect", &checkXPC);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  if (!checkXPC) {
    XMLT_ERROR("mozLineTerm::ArePrefsSecure: Error - Please add the line\n"
    "  pref(\"security.checkxpcconnect\",true);\n"
    "to your preferences file (.mozilla/prefs.js)\n");
    *_retval = PR_FALSE;
#if 0  // Temporarily comented out
    return NS_OK;
#endif
  }

  nsCAutoString secString ("security.policy.");
  /* Get global policy name. */
  nsXPIDLCString policyStr;

  result = prefBranch->GetCharPref("javascript.security_policy",
                                   getter_Copies(policyStr));
  if (NS_SUCCEEDED(result) && !policyStr.IsEmpty()) {
    secString.Append(policyStr);
  } else {
    secString.Append("default");
  }

  secString.Append(".htmldocument.cookie");

  XMLT_LOG(mozLineTerm::ArePrefsSecure,32, ("prefStr=%s\n", secString.get()));

  nsXPIDLCString secLevelString;
  result = prefBranch->GetCharPref(secString.get(), getter_Copies(secLevelString));

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  XMLT_LOG(mozLineTerm::ArePrefsSecure,32,
           ("secLevelString=%s\n", secLevelString.get()));

  *_retval = secLevelString.Equals(NS_LITERAL_CSTRING("sameOrigin"));

  if (!(*_retval)) {
    XMLT_ERROR("mozLineTerm::ArePrefsSecure: Error - Please add the line\n"
    "  pref(\"security.policy.default.htmldocument.cookie\",\"sameOrigin\");\n"
    "to your preferences file (.mozilla/prefs.js)\n");
  }

  return NS_OK;
}


/** Checks document principal to ensure it has LineTerm creation privileges.
 * Returns the principal string if the principal is secure,
 * and a (zero length) null string if the principal is insecure.
 */
NS_IMETHODIMP mozLineTerm::GetSecurePrincipal(nsIDOMDocument *domDoc,
                                              char** aPrincipalStr)
{
  XMLT_LOG(mozLineTerm::GetSecurePrincipal,30,("\n"));

  nsresult result;

  if (!aPrincipalStr)
    return NS_ERROR_FAILURE;

  *aPrincipalStr = nsnull;

  // Get principal string
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc)
    return NS_ERROR_FAILURE;

  nsIPrincipal *principal = doc->GetPrincipal();
  if (!principal) 
    return NS_ERROR_FAILURE;

#if 0  // Temporarily comented out, because ToString is not immplemented
  result = principal->ToString(aPrincipalStr);
  if (NS_FAILED(result) || !*aPrincipalStr)
    return NS_ERROR_FAILURE;
#else
  const char temStr[] = "unknown";
  PRInt32 temLen = strlen(temStr);
  *aPrincipalStr = strncpy((char*) nsMemory::Alloc(temLen+1),
                           temStr, temLen+1);
#endif

  XMLT_LOG(mozLineTerm::GetSecurePrincipal,32,("aPrincipalStr=%s\n",
                                               *aPrincipalStr));

  // Check if principal is secure
  PRBool insecure = PR_FALSE;
  if (insecure) {
    // Return null string
    XMLT_ERROR("mozLineTerm::GetSecurePrincipal: Error - "
               "Insecure document principal %s\n", *aPrincipalStr);
    nsMemory::Free(*aPrincipalStr);
    *aPrincipalStr = (char*) nsMemory::Alloc(1);
    **aPrincipalStr = '\0';
  }

  return NS_OK;
}


/** Open LineTerm without callback
 */
NS_IMETHODIMP mozLineTerm::Open(const PRUnichar *command,
                                const PRUnichar *initInput,
                                const PRUnichar *promptRegexp,
                                PRInt32 options, PRInt32 processType,
                                nsIDOMDocument *domDoc)
{
  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::Open: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  nsAutoString aCookie;
  return OpenAux(command, initInput, promptRegexp,
                 options, processType,
                 24, 80, 0, 0,
                 domDoc, nsnull, aCookie);
}


/** Open LineTerm, with an Observer for callback to process new input/output
 */
NS_IMETHODIMP mozLineTerm::OpenAux(const PRUnichar *command,
                                   const PRUnichar *initInput,
                                   const PRUnichar *promptRegexp,
                                   PRInt32 options, PRInt32 processType,
                                   PRInt32 nRows, PRInt32 nCols,
                                   PRInt32 xPixels, PRInt32 yPixels,
                                   nsIDOMDocument *domDoc,
                                   nsIObserver* anObserver,
                                   nsString& aCookie)
{
  nsresult result;

  XMLT_LOG(mozLineTerm::Open,20,("\n"));

  // Ensure that preferences are secure for LineTerm creation and use
  PRBool arePrefsSecure;
  result = ArePrefsSecure(&arePrefsSecure);
#if 0  // Temporarily comented out
  if (NS_FAILED(result) || !arePrefsSecure)
    return NS_ERROR_FAILURE;
#endif

  // Ensure that document principal is secure for LineTerm creation
  char* securePrincipal;
  result = GetSecurePrincipal(domDoc, &securePrincipal);
  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  if (!*securePrincipal) {
    nsMemory::Free(securePrincipal);
    XMLT_ERROR("mozLineTerm::OpenAux: Error - "
               "Failed to create LineTerm for insecure document principal\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMHTMLDocument> domHTMLDoc = do_QueryInterface(domDoc);
  if (!domHTMLDoc)
      return NS_ERROR_FAILURE;

  // Ensure that cookie attribute of document is defined
  NS_NAMED_LITERAL_STRING(cookiePrefix, "xmlterm=");
  nsAutoString cookieStr;
  result = domHTMLDoc->GetCookie(cookieStr);

  if (NS_SUCCEEDED(result) &&
      (cookieStr.Length() > cookiePrefix.Length()) &&
      StringBeginsWith(cookieStr, cookiePrefix)) {
    // Cookie value already defined for document; simply copy it
    mCookie = cookieStr;

  } else {
    // Create random session cookie
    nsAutoString cookieValue;
    result = mozXMLTermUtils::RandomCookie(cookieValue);
    if (NS_FAILED(result))
      return result;

    mCookie = cookiePrefix;
    mCookie += cookieValue;

    // Set new cookie value
    result = domHTMLDoc->SetCookie(mCookie);
    if (NS_FAILED(result))
      return result;
  }

  // Return copy of cookie to caller
  aCookie = mCookie;

  mObserver = anObserver;  // non-owning reference

  // Convert cookie to CString
  char* cookieCStr = ToNewCString(mCookie);
  XMLT_LOG(mozLineTerm::Open,22, ("mCookie=%s\n", cookieCStr));

  // Convert initInput to CString
  nsCAutoString initCStr;
  initCStr.AssignWithConversion(initInput);
  XMLT_LOG(mozLineTerm::Open,22, ("initInput=%s\n", initCStr.get()));

  // List of prompt delimiters
  static const PRInt32 PROMPT_DELIMS = 5;
  UNICHAR prompt_regexp[PROMPT_DELIMS+1];
  ucscopy(prompt_regexp, "#$%>?", PROMPT_DELIMS+1);
  PR_ASSERT(ucslen(prompt_regexp) == PROMPT_DELIMS);

  if (anObserver != nsnull) {
    result = lterm_open(mLTerm, NULL, cookieCStr, initCStr.get(),
                        prompt_regexp, options, processType,
                        nRows, nCols, xPixels, yPixels,
                        mozLineTerm::Callback, (void *) this);
  } else {
    result = lterm_open(mLTerm, NULL, cookieCStr, initCStr.get(),
                        prompt_regexp, options, processType,
                        nRows, nCols, xPixels, yPixels,
                        NULL, NULL);
  }

  // Free cookie CString
  nsMemory::Free(cookieCStr);

  if (mLoggingEnabled) {
    // Log time stamp for LineTerm open operation
    nsAutoString timeStamp;
    result = mozXMLTermUtils::TimeStamp(0, mLastTime, timeStamp);
    if (NS_SUCCEEDED(result)) {
      char* temStr = ToNewCString(timeStamp);
      PR_LogPrint("<TS %s> LineTerm %d opened by principal %s\n",
                  temStr, mLTerm, securePrincipal);
      nsMemory::Free(temStr);
    }
  }

  if (result == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
}


/** GTK-style callback funtion for mozLineTerm object
 */
void mozLineTerm::Callback(gpointer data,
                           gint source,
                           GdkInputCondition condition)
{
  mozLineTerm* lineTerm = (mozLineTerm*) data;

  //XMLT_LOG(mozLineTerm::Callback,50,("\n"));

  PR_ASSERT(lineTerm != nsnull);
  PR_ASSERT(lineTerm->mObserver != nsnull);

  lineTerm->mObserver->Observe((nsISupports*) lineTerm, nsnull, nsnull);
  return;
}

/** Suspends (or restores) LineTerm activity depending upon aSuspend
 */
NS_IMETHODIMP mozLineTerm::SuspendAux(PRBool aSuspend)
{
  mSuspended = aSuspend;
  return NS_OK;
}


/** Close LineTerm (a Finalize method)
 */
NS_IMETHODIMP mozLineTerm::Close(const PRUnichar* aCookie)
{
  XMLT_LOG(mozLineTerm::Close,20, ("\n"));

  if (!mCookie.Equals(aCookie)) {
    XMLT_ERROR("mozLineTerm::Close: Error - Cookie mismatch\n");
    return NS_ERROR_FAILURE;
  }

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::Close: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  if (lterm_close(mLTerm) == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }

  mObserver = nsnull;
}


/** Close LineTerm (a Finalize method)
 */
NS_IMETHODIMP mozLineTerm::CloseAux(void)
{
  XMLT_LOG(mozLineTerm::CloseAux,20, ("\n"));

  if (lterm_close(mLTerm) == 0) {
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }

  mObserver = nsnull;
}


/** Close all LineTerm instances
 */
NS_IMETHODIMP mozLineTerm::CloseAllAux(void)
{
  lterm_close_all();
  return NS_OK;
}


/** Resizes XMLterm to match a resized window.
 * @param nRows number of rows
 * @param nCols number of columns
 */
NS_IMETHODIMP mozLineTerm::ResizeAux(PRInt32 nRows, PRInt32 nCols)
{
  int retCode;

  XMLT_LOG(mozLineTerm::ResizeAux,30,("nRows=%d, nCols=%d\n", nRows, nCols));

  // Resize LTERM
  retCode = lterm_resize(mLTerm, (int) nRows, (int) nCols);
  if (retCode < 0)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Writes a string to LTERM
 */
NS_IMETHODIMP mozLineTerm::Write(const PRUnichar *buf,
                                 const PRUnichar* aCookie)
{
  if (!mCookie.Equals(aCookie)) {
    XMLT_ERROR("mozLineTerm::Write: Error - Cookie mismatch\n");
    return NS_ERROR_FAILURE;
  }

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::Write: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  XMLT_LOG(mozLineTerm::Write,30,("\n"));

  nsresult result;
  UNICHAR ubuf[MAXCOL];
  int jLen, retCode;
  PRBool newline = PR_FALSE;

  jLen = 0;
  while ((jLen < MAXCOL-1) && (buf[jLen] != 0)) {
    if (buf[jLen] == U_LINEFEED)
      newline = PR_TRUE;

    ubuf[jLen] = (UNICHAR) buf[jLen];

    if (ubuf[jLen] == U_PRIVATE0) {
      // Hack to handle input of NUL characters in NUL-terminated strings
      // See also: mozXMLTermKeyListener::KeyPress
      ubuf[jLen] = U_NUL;
    }

    jLen++;
  }

  if (jLen >= MAXCOL-1) {
    XMLT_ERROR("mozLineTerm::Write: Error - Buffer overflow\n");
    return NS_ERROR_FAILURE;
  }

  if (mLoggingEnabled && (jLen > 0)) {
    /* Log all input to STDERR */
    ucsprint(stderr, ubuf, jLen);

    nsAutoString timeStamp;
    result = mozXMLTermUtils::TimeStamp(60, mLastTime, timeStamp);

    if (NS_SUCCEEDED(result) && !timeStamp.IsEmpty()) {
      char* temStr = ToNewCString(timeStamp);
      PR_LogPrint("<TS %s>\n", temStr);
      nsMemory::Free(temStr);

    } else if (newline) {
      PR_LogPrint("\n");
    }
  }

  retCode = lterm_write(mLTerm, ubuf, jLen, LTERM_WRITE_PLAIN_INPUT);
  if (retCode < 0)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::Read(PRInt32 *opcodes, PRInt32 *opvals,
                                PRInt32 *buf_row, PRInt32 *buf_col,
                                const PRUnichar* aCookie,
                                PRUnichar **_retval)
{
  if (!mCookie.Equals(aCookie)) {
    XMLT_ERROR("mozLineTerm::Read: Error - Cookie mismatch\n");
    return NS_ERROR_FAILURE;
  }

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::Read: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  return ReadAux(opcodes, opvals, buf_row, buf_col, _retval, nsnull);
}


/** Reads a line from LTERM and returns it as a string (may be null string)
 */
NS_IMETHODIMP mozLineTerm::ReadAux(PRInt32 *opcodes, PRInt32 *opvals,
                                   PRInt32 *buf_row, PRInt32 *buf_col,
                                   PRUnichar **_retval, PRUnichar **retstyle)
{
  UNICHAR ubuf[MAXCOL];
  UNISTYLE ustyle[MAXCOL];
  int cursor_row, cursor_col;
  int retCode, j;

  XMLT_LOG(mozLineTerm::ReadAux,30,("\n"));

  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  retCode = lterm_read(mLTerm, 0, ubuf, MAXCOL-1,
                       ustyle, opcodes, opvals,
                       buf_row, buf_col, &cursor_row, &cursor_col);
  if (retCode < 0)
    return NS_ERROR_FAILURE;

  if (*opcodes == 0) {
    // Return null pointer(s)
    *_retval = nsnull;

    if (retstyle)
      *retstyle = nsnull;

  } else {
    // Return output string
    mCursorRow = cursor_row;
    mCursorColumn = cursor_col;

    XMLT_LOG(mozLineTerm::ReadAux,72,("cursor_col=%d\n", cursor_col));

    int allocBytes = sizeof(PRUnichar)*(retCode + 1);
    *_retval = (PRUnichar*) nsMemory::Alloc(allocBytes);

    for (j=0; j<retCode; j++)
      (*_retval)[j] = (PRUnichar) ubuf[j];

    // Insert null string terminator
    (*_retval)[retCode] = 0;

    if (retstyle != nsnull) {
      // Return style array as well
      *retstyle = (PRUnichar*) nsMemory::Alloc(allocBytes);

      for (j=0; j<retCode; j++)
        (*retstyle)[j] = (PRUnichar) ustyle[j];

      // Insert null string terminator
      (*retstyle)[retCode] = 0;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::GetCookie(nsString& aCookie)
{
  aCookie = mCookie;
  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::GetCursorRow(PRInt32 *aCursorRow)
{
  *aCursorRow = mCursorRow;
  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::SetCursorRow(PRInt32 aCursorRow)
{
  int retCode;

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::SetCursorRow: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  retCode = lterm_setcursor(mLTerm, aCursorRow, mCursorColumn);
  if (retCode < 0)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::GetCursorColumn(PRInt32 *aCursorColumn)
{
  *aCursorColumn = mCursorColumn;
  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::SetCursorColumn(PRInt32 aCursorColumn)
{
  int retCode;

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::SetCursorColumn: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  retCode = lterm_setcursor(mLTerm, mCursorRow, aCursorColumn);
  if (retCode < 0)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::GetEchoFlag(PRBool *aEchoFlag)
{
  *aEchoFlag = mEchoFlag;
  return NS_OK;
}


NS_IMETHODIMP mozLineTerm::SetEchoFlag(PRBool aEchoFlag)
{
  int result;

  if (mSuspended) {
    XMLT_ERROR("mozLineTerm::SetEchoFlag: Error - LineTerm %d is suspended\n",
               mLTerm);
    return NS_ERROR_FAILURE;
  }

  XMLT_LOG(mozLineTerm::SetEchoFlag,70,("aEchoFlag=0x%x\n", aEchoFlag));

  if (aEchoFlag) {
    result = lterm_setecho(mLTerm, 1);
  } else {
    result = lterm_setecho(mLTerm, 0);
  }

  if (result != 0)
    return NS_ERROR_FAILURE;

  mEchoFlag = aEchoFlag;
  return NS_OK;
}
