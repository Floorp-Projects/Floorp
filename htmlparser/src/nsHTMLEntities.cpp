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
 */

#include "nsHTMLEntities.h"



#include "nsString.h"
#include "nsAVLTree.h"

struct EntityNode {
  EntityNode(void)
    : mStr(),
      mUnicode(-1)
  {}

  EntityNode(const nsCString& aStringValue)
    : mStr(),
      mUnicode(-1)
  { // point to the incomming buffer
    // note that the incomming buffer may really be 2 byte
    nsStr::Initialize(mStr, aStringValue.mStr, aStringValue.mCapacity, 
                      aStringValue.mLength, aStringValue.mCharSize, PR_FALSE);
  }

  EntityNode(PRInt32 aUnicode)
    : mStr(),
      mUnicode(aUnicode)
  {}

  nsCAutoString mStr;
  PRInt32       mUnicode;
};

class EntityNameComparitor: public nsAVLNodeComparitor {
public:
  virtual ~EntityNameComparitor(void) {}
  virtual PRInt32 operator()(void* anItem1,void* anItem2) {
    EntityNode* one = (EntityNode*)anItem1;
    EntityNode* two = (EntityNode*)anItem2;
    return one->mStr.CompareWithConversion(two->mStr, PR_FALSE);
  }
}; 

class EntityCodeComparitor: public nsAVLNodeComparitor {
public:
  virtual ~EntityCodeComparitor(void) {}
  virtual PRInt32 operator()(void* anItem1,void* anItem2) {
    EntityNode* one = (EntityNode*)anItem1;
    EntityNode* two = (EntityNode*)anItem2;
    return (one->mUnicode - two->mUnicode);
  }
}; 


static PRInt32        gTableRefCount;
static EntityNode*    gEntityArray;
static nsAVLTree*     gEntityToCodeTree;
static nsAVLTree*     gCodeToEntityTree;
static EntityNameComparitor* gNameComparitor;
static EntityCodeComparitor* gCodeComparitor;

// define array of entity names
#define HTML_ENTITY(_name, _value) #_name,
static char* gEntityNames[] = {
#include "nsHTMLEntityList.h"
};
#undef HTML_ENTITY

#define HTML_ENTITY(_name, _value) _value,
static PRInt32 gEntityCodes[] = {
#include "nsHTMLEntityList.h"
};
#undef HTML_ENTITY

#define NS_HTML_ENTITY_COUNT ((PRInt32)(sizeof(gEntityCodes) / sizeof(PRInt32)))

void
nsHTMLEntities::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    if (! gEntityArray) {
      gEntityArray = new EntityNode[NS_HTML_ENTITY_COUNT];
      gNameComparitor = new EntityNameComparitor();
      gCodeComparitor = new EntityCodeComparitor();
      if (gEntityArray && gNameComparitor && gCodeComparitor) {
        gEntityToCodeTree = new nsAVLTree(*gNameComparitor, nsnull);
        gCodeToEntityTree = new nsAVLTree(*gCodeComparitor, nsnull);
      }
      if (gEntityToCodeTree && gCodeToEntityTree) {
        PRInt32 index = -1;
        while (++index < NS_HTML_ENTITY_COUNT) {
          gEntityArray[index].mStr = gEntityNames[index];
          gEntityArray[index].mUnicode = gEntityCodes[index];
          gEntityToCodeTree->AddItem(&(gEntityArray[index]));
          gCodeToEntityTree->AddItem(&(gEntityArray[index]));
        }
      }
    }
  }
}

void
nsHTMLEntities::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gEntityArray) {
      delete[] gEntityArray;
      gEntityArray = nsnull;
    }
    if (gEntityToCodeTree) {
      delete gEntityToCodeTree;
      gEntityToCodeTree = nsnull;
    }
    if (gCodeToEntityTree) {
      delete gCodeToEntityTree;
      gCodeToEntityTree = nsnull;
    }
    if (gNameComparitor) {
      delete gNameComparitor;
      gNameComparitor = nsnull;
    }
    if (gCodeComparitor) {
      delete gCodeComparitor;
      gCodeComparitor = nsnull;
    }
  }
}

PRInt32 
nsHTMLEntities::EntityToUnicode(const nsCString& aEntity)
{
  NS_ASSERTION(gEntityToCodeTree, "no lookup table, needs addref");
  if (gEntityToCodeTree) {

    //this little piece of code exists because entities may or may not have the terminating ';'.
    //if we see it, strip if off for this test...

    if(';'==aEntity.Last()) {
      nsCAutoString temp(aEntity);
      temp.Truncate(aEntity.mLength-1);
      return EntityToUnicode(temp);
    }
      

    EntityNode node(aEntity);
    EntityNode*  found = (EntityNode*)gEntityToCodeTree->FindItem(&node);
    if (found) {
      NS_ASSERTION(found->mStr.Equals(aEntity), "bad tree");
      return found->mUnicode;
    }
  }
  return -1;
}


PRInt32 
nsHTMLEntities::EntityToUnicode(const nsAReadableString& aEntity) {
  nsCAutoString theEntity; theEntity.AssignWithConversion(aEntity);
  if(';'==theEntity.Last()) {
    theEntity.Truncate(theEntity.Length()-1);
  }

  return EntityToUnicode(theEntity);
}


const nsCString& 
nsHTMLEntities::UnicodeToEntity(PRInt32 aUnicode)
{
  NS_ASSERTION(gCodeToEntityTree, "no lookup table, needs addref");
  if (gCodeToEntityTree) {
    EntityNode node(aUnicode);
    EntityNode*  found = (EntityNode*)gCodeToEntityTree->FindItem(&node);
    if (found) {
      NS_ASSERTION(found->mUnicode == aUnicode, "bad tree");
      return found->mStr;
    }
  }
  static const nsCString* kNullStr=0;
  if(!kNullStr) {
    kNullStr=new nsCString("");
  }
  return *kNullStr;

}

#ifdef NS_DEBUG
#include <stdio.h>

class nsTestEntityTable {
public:
   nsTestEntityTable() {
     PRInt32 value;
     nsHTMLEntities::AddRefTable();

     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_ENTITY_COUNT; i++) {
       nsAutoString entity; entity.AssignWithConversion(gEntityArray[i].mStr);

       value = nsHTMLEntities::EntityToUnicode(entity);
       NS_ASSERTION(value != -1, "can't find entity");
       NS_ASSERTION(value == gEntityArray[i].mUnicode, "bad unicode value");

       entity.AssignWithConversion(nsHTMLEntities::UnicodeToEntity(value));
       NS_ASSERTION(entity.EqualsWithConversion(gEntityArray[i].mStr.mStr), "bad entity name");
     }

     // Make sure we don't find things that aren't there
     value = nsHTMLEntities::EntityToUnicode(NS_ConvertToString("@"));
     NS_ASSERTION(value == -1, "found @");
     value = nsHTMLEntities::EntityToUnicode(nsCAutoString("zzzzz"));
     NS_ASSERTION(value == -1, "found zzzzz");
     nsHTMLEntities::ReleaseTable();
   }
};
//nsTestEntityTable validateEntityTable;
#endif

