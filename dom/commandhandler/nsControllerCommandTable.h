/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsControllerCommandTable_h_
#define nsControllerCommandTable_h_

#include "nsIControllerCommandTable.h"
#include "nsWeakReference.h"
#include "nsInterfaceHashtable.h"

class nsIControllerCommand;

class nsControllerCommandTable final : public nsIControllerCommandTable,
                                       public nsSupportsWeakReference {
 public:
  nsControllerCommandTable();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMANDTABLE

  static already_AddRefed<nsControllerCommandTable> CreateEditorCommandTable();
  static already_AddRefed<nsControllerCommandTable> CreateEditingCommandTable();
  static already_AddRefed<nsControllerCommandTable>
  CreateHTMLEditorCommandTable();
  static already_AddRefed<nsControllerCommandTable>
  CreateHTMLEditorDocStateCommandTable();
  static already_AddRefed<nsControllerCommandTable> CreateWindowCommandTable();

 protected:
  virtual ~nsControllerCommandTable();

  // Hash table of nsIControllerCommands, keyed by command name.
  nsInterfaceHashtable<nsCStringHashKey, nsIControllerCommand> mCommandsTable;

  // Are we mutable?
  bool mMutable;
};

nsControllerCommandTable*
nsIControllerCommandTable::AsControllerCommandTable() {
  return static_cast<nsControllerCommandTable*>(this);
}

const nsControllerCommandTable*
nsIControllerCommandTable::AsControllerCommandTable() const {
  return static_cast<const nsControllerCommandTable*>(this);
}

#endif  // nsControllerCommandTable_h_
