/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAOLCiter.h"

#include "nsWrapUtils.h"
#include "nsReadableUtils.h"

/** Mail citations using the AOL style >> This is a citation <<
  */

nsAOLCiter::nsAOLCiter()
{
  NS_INIT_REFCNT();
}

nsAOLCiter::~nsAOLCiter()
{
}

NS_IMPL_ADDREF(nsAOLCiter)

NS_IMPL_RELEASE(nsAOLCiter)

NS_IMETHODIMP
nsAOLCiter::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports)))
  {
    *aInstancePtr = (void*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsICiter))) {
    *aInstancePtr = (void*)(nsICiter*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsAOLCiter::GetCiteString(const nsAReadableString& aInString, nsAWritableString& aOutString)
{
  aOutString = NS_LITERAL_STRING("\n\n>> ");
  aOutString += aInString;

  // See if the last char is a newline, and replace it if so
  PRUnichar newline ('\n');
  if (aOutString.Last() == newline)
  {
    aOutString.Append(PRUnichar(' '));
    aOutString.Append(NS_LITERAL_STRING("<<\n"));
  }
  else
  {
    aOutString.Append(NS_LITERAL_STRING(" <<\n"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAOLCiter::StripCites(const nsAReadableString& aInString, nsAWritableString& aOutString)
{
  // Remove the beginning cites, if any:
  nsAutoString tOutputString;
  nsReadingIterator <PRUnichar> iter,enditer;
  aInString.BeginReading(iter);
  aInString.EndReading(enditer);
  if (!Compare(Substring(aInString,0,2),NS_LITERAL_STRING(">>")))
  {
    iter.advance(2);
    while (nsCRT::IsAsciiSpace(*iter))
      ++iter;
    AppendUnicodeTo(iter,enditer,tOutputString);
  }
  else
    CopyUnicodeTo(iter,enditer,tOutputString);

  // Remove the end cites, if any:
  tOutputString.Trim("<", PR_FALSE, PR_TRUE, PR_FALSE);
  aOutString.Assign(tOutputString);
  return NS_OK;
}

NS_IMETHODIMP
nsAOLCiter::Rewrap(const nsAReadableString& aInString,
                   PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                   PRBool aRespectNewlines,
                   nsAWritableString& aOutString)
{
  nsString citeString;
  return nsWrapUtils::Rewrap(aInString, aWrapCol, aFirstLineOffset,
                             aRespectNewlines, citeString,
                             aOutString);
}

