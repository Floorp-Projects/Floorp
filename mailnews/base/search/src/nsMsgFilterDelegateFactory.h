/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

/* creates or retrives filter objects based on their URI */

#include "nsIRDFDelegateFactory.h"

#define MSGFILTER_TAG "#filter"
#define MSGFILTER_TAG_LENGTH 7

class nsIMsgFilterList;
class nsIMsgFilter;

class nsMsgFilterDelegateFactory : public nsIRDFDelegateFactory {
 public:

  nsMsgFilterDelegateFactory();
  virtual ~nsMsgFilterDelegateFactory();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRDFDELEGATEFACTORY

 private:

  static nsresult getFilterListDelegate(nsIRDFResource *aOuter,
                                        nsIMsgFilterList **aResult);
  static nsresult getFilterDelegate(nsIRDFResource *aOuter,
                                    nsIMsgFilter **aResult);
  
  static PRInt32 getFilterNumber(const char *filterTag);

  static nsresult getFilterList(const char *aUri, PRInt32 aTagPosition,
                                nsIMsgFilterList** aResult);
};
