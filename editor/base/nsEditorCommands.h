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
 */

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
    
  NS_IMETHOD  IsCommandEnabled(const nsAReadableString & aCommandName, nsISupports *aCommandRefCon, PRBool *_retval) = 0;
  NS_IMETHOD  DoCommand(const nsAReadableString & aCommandName, nsISupports *aCommandRefCon) = 0;
  
};


#define NS_DECL_EDITOR_COMMAND(_cmd)                    \
class _cmd : public nsBaseEditorCommand                 \
{                                                       \
public:                                                 \
  NS_IMETHOD IsCommandEnabled(const nsAReadableString & aCommandName, nsISupports *aCommandRefCon, PRBool *_retval); \
  NS_IMETHOD DoCommand(const nsAReadableString & aCommandName, nsISupports *aCommandRefCon); \
};



// basic editor commands
NS_DECL_EDITOR_COMMAND(nsUndoCommand)
NS_DECL_EDITOR_COMMAND(nsRedoCommand)

NS_DECL_EDITOR_COMMAND(nsCutCommand)
NS_DECL_EDITOR_COMMAND(nsCutOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsCopyCommand)
NS_DECL_EDITOR_COMMAND(nsCopyOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsPasteCommand)
NS_DECL_EDITOR_COMMAND(nsDeleteCommand)
NS_DECL_EDITOR_COMMAND(nsSelectAllCommand)

NS_DECL_EDITOR_COMMAND(nsSelectionMoveCommands)



#if 0
// template for new command
NS_IMETHODIMP
nsFooCommand::IsCommandEnabled(const nsAReadableString & aCommandName, nsISupports *aCommandRefCon, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsFooCommand::DoCommand(const nsAReadableString & aCommandName, const nsAReadableString & aCommandParams, nsISupports *aCommandRefCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


#endif

#endif // nsIEditorCommand_h_
