/* vim:set ts=4 sw=4 et cindent: */
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
 * The Original Code is the Negotiateauth
 *
 * The Initial Developer of the Original Code is Daniel Kouril.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Kouril <kouril@ics.muni.cz> (original author)
 *   Wyllys Ingersoll <wyllys.ingersoll@sun.com>
 *   Christopher Nebergall <cneberg@sandia.gov>
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsHttpNegotiateAuth_h__
#define nsHttpNegotiateAuth_h__

#include "nsIHttpAuthenticator.h"
#include "nsIURI.h"
#include "nsSubstring.h"

// The nsHttpNegotiateAuth class provides responses for the GSS-API Negotiate method
// as specified by Microsoft in draft-brezak-spnego-http-04.txt

class nsHttpNegotiateAuth : public nsIHttpAuthenticator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

private:
    // returns the value of the given boolean pref
    bool TestBoolPref(const char *pref);

    // returns true if URI is accepted by the list of hosts in the pref
    bool TestPref(nsIURI *, const char *pref);

    bool MatchesBaseURI(const nsCSubstring &scheme,
                          const nsCSubstring &host,
                          PRInt32             port,
                          const char         *baseStart,
                          const char         *baseEnd);
};
#endif /* nsHttpNegotiateAuth_h__ */
