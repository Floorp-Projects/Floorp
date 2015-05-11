/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCommandGroup_h__
#define nsCommandGroup_h__

#include "nsIController.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

// {ecd55a01-2780-11d5-a73c-ca641a6813bc}
#define NS_CONTROLLER_COMMAND_GROUP_CID \
  { 0xecd55a01, 0x2780, 0x11d5, { 0xa7, 0x3c, 0xca, 0x64, 0x1a, 0x68, 0x13, 0xbc } }

#define NS_CONTROLLER_COMMAND_GROUP_CONTRACTID \
  "@mozilla.org/embedcomp/controller-command-group;1"

class nsControllerCommandGroup : public nsIControllerCommandGroup
{
public:
  nsControllerCommandGroup();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMANDGROUP

public:
  typedef nsClassHashtable<nsCStringHashKey, nsTArray<nsCString>>
    GroupsHashtable;

protected:
  virtual ~nsControllerCommandGroup();

  void ClearGroupsHash();

protected:
  // Hash keyed on command group. This could be made more space-efficient,
  // maybe with atoms.
  GroupsHashtable mGroupsHash;
};

#endif // nsCommandGroup_h__
