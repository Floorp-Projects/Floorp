/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@netscape.com> (Original Author)
 *
 */

#include "nsString.h"
#include "nsIAbLDAPAutoCompFormatter.h"
#include "nsIConsoleService.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"

class nsAbLDAPAutoCompFormatter : public nsIAbLDAPAutoCompFormatter
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILDAPAUTOCOMPFORMATTER
    NS_DECL_NSIABLDAPAUTOCOMPFORMATTER

    nsAbLDAPAutoCompFormatter();
    virtual ~nsAbLDAPAutoCompFormatter();

  protected:
    nsString mNameFormat;               // how to format these pieces
    nsString mAddressFormat;
    nsString mCommentFormat;

    // parse and process format
    nsresult ProcessFormat(const nsAString & aFormat,
                           nsILDAPMessage *aMessage, 
                           nsACString *aValue,
                           nsCStringArray *aAttrs);

    // process a single attribute while parsing format
    nsresult ParseAttrName(nsReadingIterator<PRUnichar> & aIter,  
                           nsReadingIterator<PRUnichar> & aIterEnd, 
                           PRBool aAttrRequired,
                           nsCOMPtr<nsIConsoleService> & aConsoleSvc,
                           nsACString & aAttrName);

    // append the first value associated with aAttrName in aMessage to aValue
    nsresult AppendFirstAttrValue(const nsACString &aAttrName, 
                                  nsILDAPMessage *aMessage,
                                  PRBool aAttrRequired,
                                  nsACString &aValue);
};

