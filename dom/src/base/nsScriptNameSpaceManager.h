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
 * Contributor(s): 
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#ifndef nsScriptNameSpaceManager_h__
#define nsScriptNameSpaceManager_h__

#include "nsIScriptNameSpaceManager.h"
#include "nsString.h"
#include "nsID.h"
#include "pldhash.h"

#include "nsDOMClassInfo.h"

struct nsGlobalNameStruct
{
  struct ConstructorAlias
  {
    nsCID mCID;
    nsString mProtoName;
    nsGlobalNameStruct* mProto;    
  };

  enum nametype {
    eTypeNotInitialized,
    eTypeInterface,
    eTypeProperty,
    eTypeExternalConstructor,
    eTypeStaticNameSet,
    eTypeDynamicNameSet,
    eTypeClassConstructor,
    eTypeClassProto,
    eTypeExternalClassInfoCreator,
    eTypeExternalClassInfo,
    eTypeExternalConstructorAlias
  } mType;

  union {
    PRInt32 mDOMClassInfoID; // eTypeClassConstructor
    nsIID mIID; // eTypeInterface, eTypeClassProto
    nsExternalDOMClassInfoData* mData; // eTypeExternalClassInfo
    ConstructorAlias* mAlias; // eTypeExternalConstructorAlias
    nsCID mCID; // All other types...
  };

private:

  // copy constructor
};


class nsIScriptContext;
class nsICategoryManager;


class nsScriptNameSpaceManager
{
public:
  nsScriptNameSpaceManager();
  virtual ~nsScriptNameSpaceManager();

  nsresult Init();
  nsresult InitForContext(nsIScriptContext *aContext);

  // Returns a nsGlobalNameStruct for aName, or null if one is not
  // found. The returned nsGlobalNameStruct is only guaranteed to be
  // valid until the next call to any of the methods in this class.
  nsresult LookupName(const nsAString& aName,
                      const nsGlobalNameStruct **aNameStruct);

  nsresult RegisterClassName(const char *aClassName,
                             PRInt32 aDOMClassInfoID);

  nsresult RegisterClassProto(const char *aClassName,
                              const nsIID *aConstructorProtoIID,
                              PRBool *aFoundOld);

  nsresult RegisterExternalInterfaces(PRBool aAsProto);

  nsresult RegisterExternalClassName(const char *aClassName,
                                     nsCID& aCID);

  // Register the info for an external class. aName must be static
  // data, it will not be deleted by the DOM code.
  nsresult RegisterDOMCIData(const char *aName,
                             nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                             const nsIID *aProtoChainInterface,
                             const nsIID **aInterfaces,
                             PRUint32 aScriptableFlags,
                             PRBool aHasClassInterface,
                             const nsCID *aConstructorCID);

  nsGlobalNameStruct* GetConstructorProto(const nsGlobalNameStruct* aStruct);

protected:
  // Adds a new entry to the hash and returns the nsGlobalNameStruct
  // that aKey will be mapped to. If mType in the returned
  // nsGlobalNameStruct is != eTypeNotInitialized, an entry for aKey
  // already existed.
  nsGlobalNameStruct *AddToHash(const nsAString& aKey);

  nsresult FillHash(nsICategoryManager *aCategoryManager,
                    const char *aCategory,
                    nsGlobalNameStruct::nametype aType);
  nsresult FillHashWithDOMInterfaces();
  nsresult RegisterInterface(const char* aIfName,
                             const nsIID *aIfIID,
                             PRBool* aFoundOld);

  // Inline PLDHashTable, init with PL_DHashTableInit() and delete
  // with PL_DHashTableFinish().
  PLDHashTable mGlobalNames;

  PRPackedBool mIsInitialized;
};

#endif /* nsScriptNameSpaceManager_h__ */
