/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EditorCommands_h_
#define EditorCommands_h_

#include "nsIControllerCommand.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsICommandParams;
class nsISupports;

namespace mozilla {

/**
 * This is a virtual base class for commands registered with the editor
 * controller.  Note that such commands can be shared by more than on editor
 * instance, so MUST be stateless. Any state must be stored via the refCon
 * (an nsIEditor).
 */

class EditorCommandBase : public nsIControllerCommand
{
public:
  EditorCommandBase();

  NS_DECL_ISUPPORTS

  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* aIsEnabled) override = 0;
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandRefCon) override = 0;

protected:
  virtual ~EditorCommandBase() {}
};


#define NS_DECL_EDITOR_COMMAND(_cmd)                                           \
class _cmd final : public EditorCommandBase                                    \
{                                                                              \
public:                                                                        \
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,                        \
                              nsISupports* aCommandRefCon,                     \
                              bool* aIsEnabled) override;                      \
  NS_IMETHOD DoCommand(const char* aCommandName,                               \
                       nsISupports* aCommandRefCon) override;                  \
  NS_IMETHOD DoCommandParams(const char* aCommandName,                         \
                             nsICommandParams* aParams,                        \
                             nsISupports* aCommandRefCon) override;            \
  NS_IMETHOD GetCommandStateParams(const char* aCommandName,                   \
                                   nsICommandParams* aParams,                  \
                                   nsISupports* aCommandRefCon) override;      \
};

// basic editor commands
NS_DECL_EDITOR_COMMAND(UndoCommand)
NS_DECL_EDITOR_COMMAND(RedoCommand)
NS_DECL_EDITOR_COMMAND(ClearUndoCommand)

NS_DECL_EDITOR_COMMAND(CutCommand)
NS_DECL_EDITOR_COMMAND(CutOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(CopyCommand)
NS_DECL_EDITOR_COMMAND(CopyOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(CopyAndCollapseToEndCommand)
NS_DECL_EDITOR_COMMAND(PasteCommand)
NS_DECL_EDITOR_COMMAND(PasteTransferableCommand)
NS_DECL_EDITOR_COMMAND(SwitchTextDirectionCommand)
NS_DECL_EDITOR_COMMAND(DeleteCommand)
NS_DECL_EDITOR_COMMAND(SelectAllCommand)

NS_DECL_EDITOR_COMMAND(SelectionMoveCommands)

// Insert content commands
NS_DECL_EDITOR_COMMAND(InsertPlaintextCommand)
NS_DECL_EDITOR_COMMAND(InsertParagraphCommand)
NS_DECL_EDITOR_COMMAND(InsertLineBreakCommand)
NS_DECL_EDITOR_COMMAND(PasteQuotationCommand)


#if 0
// template for new command
NS_IMETHODIMP
FooCommand::IsCommandEnabled(const char* aCommandName,
                             nsISupports* aCommandRefCon,
                             bool* aIsEnabled)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FooCommand::DoCommand(const char* aCommandName,
                      const nsAString& aCommandParams,
                      nsISupports* aCommandRefCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

} // namespace mozilla

#endif // #ifndef EditorCommands_h_
