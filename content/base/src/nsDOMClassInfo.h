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
#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "nsIClassInfo.h"
#include "nsVoidArray.h"

class nsDOMClassInfo : public nsIClassInfo
{
public:
  nsDOMClassInfo();
  virtual ~nsDOMClassInfo();

  NS_DECL_ISUPPORTS

  NS_DECL_NSICLASSINFO

  virtual void GetIIDs(nsVoidArray& aArray) = 0;
};


/**
 * nsIClassInfo helper macros
 */
 
#define __NS_DECL_DOM_CLASSINFO_BODY(_class)                                  \
  {                                                                           \
  public:                                                                     \
    void GetIIDs(nsVoidArray& aArray);                                        \
                                                                              \
    static _class##_dci *sClassInfo;                                          \
  };


#define NS_DECL_DOM_CLASSINFO(_class, _baseclass)                             \
  class _class##_dci : public _baseclass##_dci                                \
  __NS_DECL_DOM_CLASSINFO_BODY(_class)


#define NS_DECL_DOM_CLASSINFO_NO_BASE(_class)                                 \
  class _class##_dci : public nsDOMClassInfo                                  \
  __NS_DECL_DOM_CLASSINFO_BODY(_class)


#define NS_BEGIN_DOM_CLASSINFO_IMPL(_class)                                   \
  _class##_dci *_class##_dci::sClassInfo = nsnull;                            \
                                                                              \
  void _class##_dci::GetIIDs(nsVoidArray& aArray)                             \
  {


#define NS_DOM_CLASSINFO_IID_ENTRY(_interface)                                \
      aArray.AppendElement((void *)&NS_GET_IID(_interface));


#define NS_DOM_CLASSINFO_BASECLASS_IIDS(_base)                                \
      _base##_dci::GetIIDs(aArray);


#define NS_END_DOM_CLASSINFO_DECL_NO_STATIC                                   \
  }


#define NS_END_DOM_CLASSINFO_IMPL                                             \
  }


#define NS_DEFINE_DOM_CLASSINFO_STATIC(_class)                                \
  _class##_dci *_class##_dci::sClassInfo = nsnull;

#define NS_DOM_INTERFACE_MAP_CLASSINFO(_class)                                \
    if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                              \
      if (!_class##_dci::sClassInfo) {                                        \
        _class##_dci::sClassInfo = new _class##_dci;                          \
        NS_ENSURE_TRUE(_class##_dci::sClassInfo, NS_ERROR_OUT_OF_MEMORY);     \
        NS_ADDREF(_class##_dci::sClassInfo);                                  \
      }                                                                       \
                                                                              \
      inst = _class##_dci::sClassInfo;                                        \
    } else



#endif /* nsDOMClassInfo_h___ */
