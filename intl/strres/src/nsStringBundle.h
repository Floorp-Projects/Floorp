/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
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

#ifndef nsStringBundle_h__
#define nsStringBundle_h__

#include "mozilla/ReentrantMonitor.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"
#include "nsIPersistentProperties2.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsIStringBundleOverride.h"

class nsStringBundle : public nsIStringBundle
{
public:
    // init version
    nsStringBundle(const char* aURLSpec, nsIStringBundleOverride*);
    nsresult LoadProperties();
    virtual ~nsStringBundle();
  
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTRINGBUNDLE

    nsCOMPtr<nsIPersistentProperties> mProps;

protected:
    //
    // functional decomposition of the funitions repeatively called 
    //
    nsresult GetStringFromID(PRInt32 aID, nsAString& aResult);
    nsresult GetStringFromName(const nsAString& aName, nsAString& aResult);

    nsresult GetCombinedEnumeration(nsIStringBundleOverride* aOverrideString,
                                    nsISimpleEnumerator** aResult);
private:
    nsCString              mPropertiesURL;
    nsCOMPtr<nsIStringBundleOverride> mOverrideStrings;
    mozilla::ReentrantMonitor    mReentrantMonitor;
    bool                         mAttemptedLoad;
    bool                         mLoaded;
    
public:
    static nsresult FormatString(const PRUnichar *formatStr,
                                 const PRUnichar **aParams, PRUint32 aLength,
                                 PRUnichar **aResult);
};

/**
 * An extesible implementation of the StringBudle interface.
 *
 * @created         28/Dec/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsExtensibleStringBundle : public nsIStringBundle
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLE

  nsresult Init(const char * aCategory, nsIStringBundleService *);
private:
  
  nsCOMArray<nsIStringBundle> mBundles;
  bool               mLoaded;

public:

  nsExtensibleStringBundle();
  virtual ~nsExtensibleStringBundle();
};



#endif
