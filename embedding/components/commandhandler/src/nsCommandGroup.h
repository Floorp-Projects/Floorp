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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */



#ifndef nsCommandGroup_h__
#define nsCommandGroup_h__

#include "nsIController.h"

#include "nsHashtable.h"


// {ecd55a01-2780-11d5-a73c-ca641a6813bc}
#define NS_CONTROLLER_COMMAND_GROUP_CID \
{ 0xecd55a01, 0x2780, 0x11d5, { 0xa7, 0x3c, 0xca, 0x64, 0x1a, 0x68, 0x13, 0xbc } }

#define NS_CONTROLLER_COMMAND_GROUP_CONTRACTID \
 "@mozilla.org/embedcomp/controller-command-group;1"


class nsControllerCommandGroup : public nsIControllerCommandGroup
{
public:

                    nsControllerCommandGroup();
  virtual           ~nsControllerCommandGroup();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMANDGROUP


protected:

  void              ClearGroupsHash();

protected:

  static PRBool     ClearEnumerator(nsHashKey *aKey, void *aData, void* closure);

protected:

  nsHashtable       mGroupsHash;    // hash keyed on command group.
                                    // Entries are nsVoidArrays of pointers to PRUnichar*
                                    // This could be made more space-efficient, maybe with atoms
  
};


#endif // nsCommandGroup_h__

