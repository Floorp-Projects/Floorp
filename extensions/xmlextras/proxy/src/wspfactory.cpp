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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
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

#include "wspprivate.h"

// NSPR includes
#include "prprf.h"

WSPFactory::WSPFactory()
{
  NS_INIT_ISUPPORTS();
}

WSPFactory::~WSPFactory()
{
}

NS_IMPL_ISUPPORTS1(WSPFactory, nsIWebServiceProxyFactory)

/* nsIWebServiceProxy createProxy (in AString wsdlURL, in AString portname, in AString qualifier, in boolean isAsync); */
NS_IMETHODIMP 
WSPFactory::CreateProxy(const nsAString & wsdlURL, 
                        const nsAString & portname, 
                        const nsAString & qualifier, 
                        PRBool isAsync, 
                        nsIWebServiceProxy **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void createProxyAsync (in AString wsdlURL, in AString portname, in AString qualifier, in boolean isAsync, in nsIWebServiceProxyListener listener); */
NS_IMETHODIMP 
WSPFactory::CreateProxyAsync(const nsAString & wsdlURL, 
                             const nsAString & portname, 
                             const nsAString & qualifier, 
                             PRBool isAsync, 
                             nsIWebServiceProxyListener *listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


#define P2M_ESCAPE_CHARACTER '_'

nsresult
WSPFactory::MethodToPropertyName(const nsAReadableCString& aMethodName,
                                 nsAWritableString& aPropertyName)
{
  nsReadingIterator<char> current, end;

  aPropertyName.Truncate();
  aMethodName.BeginReading(current);
  aMethodName.EndReading(end);

  while (current != end) {
    char ch = *current++;
    PRUnichar uch;
    if (ch == P2M_ESCAPE_CHARACTER) {
      // Grab the next 4 characters that make up the
      // escape sequence
      char buf[5];
      for (PRUint16 i = 0; (i < 4) && (current != end); i++) {
        buf[i] = *current++;
      }
      // If we didn't get through the entire escape sequence, then
      // it's an error.
      if (i < 4) {
        return NS_ERROR_FAILURE;
      }
      buf[4] = 0;

      PR_sscanf(buf, "%hx", &uch);
    }
    else {
      uch = PRUnichar(ch);
    }
    aPropertyName.Append(uch);
  }

  return NS_OK;
}
 
void
WSPFactory::PropertyToMethodName(const nsAReadableString& aPropertyName,
                                 nsAWritableCString& aMethodName)
{
  nsReadingIterator<PRUnichar> current, end;

  aMethodName.Truncate();
  aPropertyName.BeginReading(current);
  aPropertyName.EndReading(end);

  while (current != end) {
    PRUnichar uch = *current++;
    if (((PRUnichar('a') <= uch) && (uch <= PRUnichar('z'))) ||
        ((PRUnichar('A') <= uch) && (uch <= PRUnichar('Z'))) ||
        ((PRUnichar('0') <= uch) && (uch <= PRUnichar('9')))) {
      // Casting is safe since we know that it's an ASCII character
      aMethodName.Append(char(uch));
    }
    else {
      // Escape the character and append to the string
      char buf[6];
      buf[0] = P2M_ESCAPE_CHARACTER;
      PR_snprintf(buf+1, 5, "%hx", uch);
      aMethodName.Append(buf, 5);
    }
  }
}
