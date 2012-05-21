/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsControllerCommandTable_h_
#define nsControllerCommandTable_h_


#include "nsIControllerCommandTable.h"
#include "nsWeakReference.h"
#include "nsHashtable.h"

class nsIControllerCommand;

class  nsControllerCommandTable : public nsIControllerCommandTable,
                                  public nsSupportsWeakReference
{
public:

                  nsControllerCommandTable();
  virtual         ~nsControllerCommandTable();

  NS_DECL_ISUPPORTS

  NS_DECL_NSICONTROLLERCOMMANDTABLE

protected:

  nsSupportsHashtable   mCommandsTable;   // hash table of nsIControllerCommands, keyed by command name
  bool                  mMutable;         // are we mutable?
};


#endif // nsControllerCommandTable_h_
