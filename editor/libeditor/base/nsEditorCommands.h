/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIEditorCommand_h_
#define nsIEditorCommand_h_

#include "nsCOMPtr.h"
#include "nsIControllerCommand.h"
#include "nsIAtom.h"

// This is a virtual base class for commands registered with the editor controller.
// Note that such commands can be shared by more than on editor instance, so
// MUST be stateless. Any state must be stored via the refCon (an nsIEditor).
class nsBaseEditorCommand : public nsIControllerCommand
{
public:

              nsBaseEditorCommand();
  virtual     ~nsBaseEditorCommand() {}
    
  NS_DECL_ISUPPORTS
    
  NS_IMETHOD  IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *_retval) = 0;
  NS_IMETHOD  DoCommand(const char *aCommandName, nsISupports *aCommandRefCon) = 0;
  
};


#define NS_DECL_EDITOR_COMMAND(_cmd)                    \
class _cmd : public nsBaseEditorCommand                 \
{                                                       \
public:                                                 \
  NS_IMETHOD IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *_retval); \
  NS_IMETHOD DoCommand(const char *aCommandName, nsISupports *aCommandRefCon); \
  NS_IMETHOD DoCommandParams(const char *aCommandName,nsICommandParams *aParams, nsISupports *aCommandRefCon); \
  NS_IMETHOD GetCommandStateParams(const char *aCommandName,nsICommandParams *aParams, nsISupports *aCommandRefCon); \
};



// basic editor commands
NS_DECL_EDITOR_COMMAND(nsUndoCommand)
NS_DECL_EDITOR_COMMAND(nsRedoCommand)
NS_DECL_EDITOR_COMMAND(nsClearUndoCommand)

NS_DECL_EDITOR_COMMAND(nsCutCommand)
NS_DECL_EDITOR_COMMAND(nsCutOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsCopyCommand)
NS_DECL_EDITOR_COMMAND(nsCopyOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsPasteCommand)
NS_DECL_EDITOR_COMMAND(nsPasteTransferableCommand)
NS_DECL_EDITOR_COMMAND(nsSwitchTextDirectionCommand)
NS_DECL_EDITOR_COMMAND(nsDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsSelectAllCommand)

NS_DECL_EDITOR_COMMAND(nsSelectionMoveCommands)

// Insert content commands
NS_DECL_EDITOR_COMMAND(nsInsertPlaintextCommand)
NS_DECL_EDITOR_COMMAND(nsPasteQuotationCommand)


#if 0
// template for new command
NS_IMETHODIMP
nsFooCommand::IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsFooCommand::DoCommand(const char *aCommandName, const nsAString & aCommandParams, nsISupports *aCommandRefCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


#endif

#endif // nsIEditorCommand_h_
