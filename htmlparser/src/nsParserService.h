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
 */
#ifndef NS_PARSERSERVICE_H__
#define NS_PARSERSERVICE_H__

#include "nsIParserService.h"
#include "nsDTDUtils.h"
#include "nsVoidArray.h"

extern "C" int MOZ_XMLIsValidQName(const char* ptr, const char* end,
                                   int ns_aware, const char** colon);
extern "C" int MOZ_XMLIsLetter(const char* ptr);
extern "C" int MOZ_XMLIsNCNameChar(const char* ptr);

class nsParserService : public nsIParserService {
public:
  nsParserService();
  virtual ~nsParserService();

  NS_DECL_ISUPPORTS

  NS_IMETHOD HTMLAtomTagToId(nsIAtom* aAtom, PRInt32* aId) const;

  NS_IMETHOD HTMLCaseSensitiveAtomTagToId(nsIAtom* aAtom, PRInt32* aId) const;

  NS_IMETHOD HTMLStringTagToId(const nsAString &aTagName,
                               PRInt32* aId) const;

  NS_IMETHOD HTMLIdToStringTag(PRInt32 aId, const PRUnichar **aTagName) const;
  
  NS_IMETHOD HTMLConvertEntityToUnicode(const nsAString& aEntity, 
                                        PRInt32* aUnicode) const;
  NS_IMETHOD HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                        nsCString& aEntity) const;
  NS_IMETHOD IsContainer(PRInt32 aId, PRBool& aIsContainer) const;
  NS_IMETHOD IsBlock(PRInt32 aId, PRBool& aIsBlock) const;

   // Observer mechanism
  NS_IMETHOD RegisterObserver(nsIElementObserver* aObserver,
                              const nsAString& aTopic,
                              const eHTMLTags* aTags = nsnull);

  NS_IMETHOD UnregisterObserver(nsIElementObserver* aObserver,
                                const nsAString& aTopic);
  NS_IMETHOD GetTopicObservers(const nsAString& aTopic,
                               nsIObserverEntry** aEntry);

  PRBool IsValidQName(const nsASingleFragmentString& aQName,
                      PRBool aNamespaceAware,
                      const PRUnichar** aColon)
  {
    const char* colon;
    const PRUnichar *begin, *end;
    aQName.BeginReading(begin);
    aQName.EndReading(end);
    int result = MOZ_XMLIsValidQName(NS_REINTERPRET_CAST(const char*, begin),
                                     NS_REINTERPRET_CAST(const char*, end),
                                     aNamespaceAware, &colon);
    *aColon = NS_REINTERPRET_CAST(const PRUnichar*, colon);
    return !!result;
  }
  PRBool IsXMLLetter(PRUnichar aChar)
  {
    return MOZ_XMLIsLetter(NS_REINTERPRET_CAST(const char*, &aChar));
  }
  PRBool IsXMLNCNameChar(PRUnichar aChar)
  {
    return MOZ_XMLIsNCNameChar(NS_REINTERPRET_CAST(const char*, &aChar));
  }

protected:
  nsObserverEntry* GetEntry(const nsAString& aTopic);
  nsresult CreateEntry(const nsAString& aTopic,
                       nsObserverEntry** aEntry);

  nsDeque  mEntries;  //each topic holds a list of observers per tag.
  PRBool   mHaveNotifiedCategoryObservers;
};

#endif
