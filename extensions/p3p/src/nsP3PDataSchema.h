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

#ifndef nsP3PDataSchema_h__
#define nsP3PDataSchema_h__

#include "nsIP3PDataSchema.h"
#include "nsIP3PXMLListener.h"
#include "nsIP3PDataStruct.h"

#include <nsHashtable.h>
#include <nsVoidArray.h>


class nsP3PDataSchema : public nsIP3PDataSchema,
                        public nsIP3PXMLListener {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PDataSchema
  NS_DECL_NSIP3PDATASCHEMA

  // nsIP3PXMLListener
  NS_DECL_NSIP3PXMLLISTENER

  // nsP3PXMLProcessor method overrides
  NS_IMETHOD            PostInit( nsString&  aURISpec );

  NS_IMETHOD            PreRead( PRInt32   aReadType,
                                 nsIURI   *aReadForURI );

  NS_IMETHOD            ProcessContent( PRInt32   aReadType );

  NS_IMETHOD_( void )   CheckForComplete( );

  // nsP3PDataSchema methods
  nsP3PDataSchema( );
  virtual ~nsP3PDataSchema( );

protected:
  NS_METHOD_( void )    ProcessDataSchema( PRInt32   aReadType );

  NS_METHOD             ProcessDataSchemas( PRInt32   aReadType );

  NS_METHOD             ProcessDataStructTagDataSchema( nsIP3PTag  *aTag,
                                                        PRInt32     aReadType );

  NS_METHOD             ProcessDataDefTagDataSchema( nsIP3PTag  *aTag,
                                                     PRInt32     aReadType );

  NS_METHOD             AddDataSchema( nsString&  aDataSchema,
                                       PRInt32    aReadType );

  NS_METHOD_( void )    SetNotValid( );

  NS_METHOD_( void )    BuildDataModel( );

  NS_METHOD             BuildDataStructTagDataStruct( nsIP3PTag  *aTag );

  NS_METHOD             BuildDataDefTagDataStruct( nsIP3PTag  *aTag );

  NS_METHOD             CreateDataStruct( nsString&   aName,
                                          nsString&   aStructRef,
                                          nsString&   aStructRefDataSchemaURISpec,
                                          nsString&   aStructRefDataStructName,
                                          nsString&   aShortDescription,
                                          nsIP3PTag  *aCategoriesTag,
                                          nsIP3PTag  *aLongDescriptionTag );

  NS_METHOD             FindChildDataStruct( nsString&          aName,
                                             nsIP3PDataStruct  *aParent,
                                             nsIP3PDataStruct **aChild );

  NS_METHOD             UpdateDataStruct( nsISupportsArray  *aCategoryValueTags,
                                          nsIP3PDataStruct  *aDataStruct );

  NS_METHOD             UpdateDataStructBelow( nsISupportsArray  *aCategoryValueTags,
                                               nsIP3PDataStruct  *aDataStruct );

  NS_METHOD             ReplaceDataStruct( nsString&          aName,
                                           nsString&          aStructRef,
                                           nsString&          aStructRefDataSchemaURISpec,
                                           nsString&          aStructRefDataStructName,
                                           nsString&          aShortDescription,
                                           nsIP3PTag         *aLongDescriptionTag,
                                           nsIP3PDataStruct  *aParent,
                                           nsIP3PDataStruct  *aOldChild,
                                           nsIP3PDataStruct **aNewChild );

  NS_METHOD             NewDataStruct( PRBool             aExplicitCreate,
                                       nsString&          aName,
                                       nsString&          aStructRef,
                                       nsString&          aStructRefDataSchemaURISpec,
                                       nsString&          aStructRefDataStructName,
                                       nsString&          aShortDescription,
                                       nsISupportsArray  *aCategoryValueTags,
                                       nsIP3PTag         *aLongDescriptionTag,
                                       nsIP3PDataStruct  *aParent,
                                       nsIP3PDataStruct **aChild );

  NS_METHOD             ResolveReferences( nsIP3PDataStruct  *aDataStruct );


  nsStringArray              mDataSchemasToRead;      // The list of DataSchemas required for this DataSchema object

  nsSupportsHashtable        mDataSchemas;            // The collection of DataSchema objects created for this DataSchema object

  nsCOMPtr<nsIP3PDataStruct> mDataStruct;             // The head of the DataStruct model
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PDataSchema( nsString&          aDataSchemaURISpec,
                                             nsIP3PDataSchema **aDataSchema );

#endif                                           /* nsP3PDataSchema_h__       */
