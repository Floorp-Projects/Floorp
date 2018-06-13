/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HTMLEditorCommands_h_
#define mozilla_HTMLEditorCommands_h_

#include "nsIControllerCommand.h"
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED, etc
#include "nsStringFwd.h"
#include "nscore.h"                     // for nsresult, NS_IMETHOD

class nsAtom;
class nsICommandParams;
class nsISupports;

namespace mozilla {
class HTMLEditor;

// This is a virtual base class for commands registered with the composer controller.
// Note that such commands are instantiated once per composer, so can store state.
// Also note that IsCommandEnabled can be called with an editor that may not
// have an editor yet (because the document is loading). Most commands will want
// to return false in this case.
// Don't hold on to any references to the editor or document from
// your command. This will cause leaks. Also, be aware that the document the
// editor is editing can change under you (if the user Reverts the file, for
// instance).
class HTMLEditorCommandBase : public nsIControllerCommand
{
protected:
  virtual ~HTMLEditorCommandBase() {}

public:
  HTMLEditorCommandBase();

  // nsISupports
  NS_DECL_ISUPPORTS
};


#define NS_DECL_COMPOSER_COMMAND(_cmd)                  \
class _cmd final : public HTMLEditorCommandBase         \
{                                                       \
public:                                                 \
  NS_DECL_NSICONTROLLERCOMMAND                          \
};

// virtual base class for commands that need to save and update Boolean state (like styles etc)
class StateUpdatingCommandBase : public HTMLEditorCommandBase
{
public:
  explicit StateUpdatingCommandBase(nsAtom* aTagName);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(StateUpdatingCommandBase,
                                       HTMLEditorCommandBase)

  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~StateUpdatingCommandBase();

  // get the current state (on or off) for this style or block format
  virtual nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) = 0;

  // add/remove the style
  virtual nsresult ToggleState(HTMLEditor* aHTMLEditor) = 0;

protected:
  nsAtom* mTagName;
};


// Shared class for the various style updating commands like bold, italics etc.
// Suitable for commands whose state is either 'on' or 'off'.
class StyleUpdatingCommand final : public StateUpdatingCommandBase
{
public:
  explicit StyleUpdatingCommand(nsAtom* aTagName);

protected:
  // get the current state (on or off) for this style or block format
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;

  // add/remove the style
  nsresult ToggleState(HTMLEditor* aHTMLEditor) final;
};


class InsertTagCommand final : public HTMLEditorCommandBase
{
public:
  explicit InsertTagCommand(nsAtom* aTagName);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(InsertTagCommand, HTMLEditorCommandBase)

  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~InsertTagCommand();

  nsAtom* mTagName;
};


class ListCommand final : public StateUpdatingCommandBase
{
public:
  explicit ListCommand(nsAtom* aTagName);

protected:
  // get the current state (on or off) for this style or block format
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;

  // add/remove the style
  nsresult ToggleState(HTMLEditor* aHTMLEditor) final;
};

class ListItemCommand final : public StateUpdatingCommandBase
{
public:
  explicit ListItemCommand(nsAtom* aTagName);

protected:
  // get the current state (on or off) for this style or block format
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;

  // add/remove the style
  nsresult ToggleState(HTMLEditor* aHTMLEditor) final;
};

// Base class for commands whose state consists of a string (e.g. para format)
class MultiStateCommandBase : public HTMLEditorCommandBase
{
public:
  MultiStateCommandBase();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultiStateCommandBase,
                                       HTMLEditorCommandBase)
  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~MultiStateCommandBase();

  virtual nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) = 0;
  virtual nsresult SetState(HTMLEditor* aHTMLEditor,
                            const nsString& newState) = 0;

};


class ParagraphStateCommand final : public MultiStateCommandBase
{
public:
  ParagraphStateCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class FontFaceStateCommand final : public MultiStateCommandBase
{
public:
  FontFaceStateCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class FontSizeStateCommand final : public MultiStateCommandBase
{
public:
  FontSizeStateCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class HighlightColorStateCommand final : public MultiStateCommandBase
{
public:
  HighlightColorStateCommand();

protected:
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* _retval) final;
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class FontColorStateCommand final : public MultiStateCommandBase
{
public:
  FontColorStateCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class AlignCommand final : public MultiStateCommandBase
{
public:
  AlignCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class BackgroundColorStateCommand final : public MultiStateCommandBase
{
public:
  BackgroundColorStateCommand();

protected:
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) final;
};

class AbsolutePositioningCommand final : public StateUpdatingCommandBase
{
public:
  AbsolutePositioningCommand();

protected:
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* _retval) final;
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsICommandParams* aParams) final;
  nsresult ToggleState(HTMLEditor* aHTMLEditor) final;
};

// composer commands

NS_DECL_COMPOSER_COMMAND(DocumentStateCommand)
NS_DECL_COMPOSER_COMMAND(SetDocumentStateCommand)
NS_DECL_COMPOSER_COMMAND(SetDocumentOptionsCommand)

NS_DECL_COMPOSER_COMMAND(DecreaseZIndexCommand)
NS_DECL_COMPOSER_COMMAND(IncreaseZIndexCommand)

// Generic commands

// Edit menu
NS_DECL_COMPOSER_COMMAND(PasteNoFormattingCommand)

// Block transformations
NS_DECL_COMPOSER_COMMAND(IndentCommand)
NS_DECL_COMPOSER_COMMAND(OutdentCommand)

NS_DECL_COMPOSER_COMMAND(RemoveListCommand)
NS_DECL_COMPOSER_COMMAND(RemoveStylesCommand)
NS_DECL_COMPOSER_COMMAND(IncreaseFontSizeCommand)
NS_DECL_COMPOSER_COMMAND(DecreaseFontSizeCommand)

// Insert content commands
NS_DECL_COMPOSER_COMMAND(InsertHTMLCommand)

} // namespace mozilla

#endif // mozilla_HTMLEditorCommands_h_
