/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PDataStruct_h__
#define nsP3PDataStruct_h__

#include "nsIP3PDataStruct.h"

#include <nsVoidArray.h>


class nsP3PDataStruct : public nsIP3PDataStruct {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PDataStruct methods
  NS_DECL_NSIP3PDATASTRUCT

  // nsP3PDataStruct methods
  nsP3PDataStruct( );
  virtual ~nsP3PDataStruct( );

  NS_METHOD             Init( PRBool             aExplicitlyCreated,
                              nsString&          aName,
                              nsString&          aStructRef,
                              nsString&          aStructRefDataSchemaURISpec,
                              nsString&          aStructRefDataStructName,
                              nsString&          aShortDescription,
                              nsISupportsArray  *aCategoryValueTags,
                              nsIP3PTag         *aLongDescriptionTag );

protected:
  NS_METHOD_( PRBool )  IsBasicStruct( );


  PRBool                     mExplicitlyCreated,     // Indicates that the DataStruct object was explicitly created
                             mReferenceResolved;     // Indicates that the DataStruct reference has been resolved

  nsString                   mName;                  // DataStruct name

  nsString                   mStructRef;             // "structref" attribute value

  nsString                   mDataSchemaURISpec,     // DataSchema URI spec portion of the "structref" attribute value
                             mDataStructName;        // DataStruct name portion of the "structref" attribute value

  nsString                   mShortDescription;      // "short-description" attribute value

  nsCOMPtr<nsISupportsArray> mCategoryValueTags;     // Category value Tag objects

  nsCOMPtr<nsIP3PTag>        mLongDescriptionTag;    // <LONG-DESCRIPTION> Tag object

  nsCOMPtr<nsIP3PDataStruct> mParent;                // Parent DataStruct object

  nsCOMPtr<nsISupportsArray> mChildren;              // Children DataStruct objects
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PDataStruct( PRBool             aExplicitlyCreated,
                                             nsString&          aName,
                                             nsString&          aStructRef,
                                             nsString&          aStructRefDataSchemaURISpec,
                                             nsString&          aStructRefDataStructName,
                                             nsString&          aShortDescription,
                                             nsISupportsArray  *aCategoryValueTags,
                                             nsIP3PTag         *aLongDescriptionTag,
                                             nsIP3PDataStruct **aDataStruct );

#endif                                           /* nsP3PDataStruct_h___      */
