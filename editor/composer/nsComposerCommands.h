/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposerCommands_h_
#define nsComposerCommands_h_

#include "nsIControllerCommand.h"
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED, etc
#include "nsStringFwd.h"
#include "nscore.h"                     // for nsresult, NS_IMETHOD

class nsAtom;
class nsICommandParams;
class nsISupports;

namespace mozilla {
class HTMLEditor;
} // namespace mozilla

// This is a virtual base class for commands registered with the composer controller.
// Note that such commands are instantiated once per composer, so can store state.
// Also note that IsCommandEnabled can be called with an editor that may not
// have an editor yet (because the document is loading). Most commands will want
// to return false in this case.
// Don't hold on to any references to the editor or document from
// your command. This will cause leaks. Also, be aware that the document the
// editor is editing can change under you (if the user Reverts the file, for
// instance).
class nsBaseComposerCommand : public nsIControllerCommand
{
protected:
  virtual ~nsBaseComposerCommand() {}

public:

  nsBaseComposerCommand();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIControllerCommand. Declared longhand so we can make them pure virtual
  NS_IMETHOD IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *_retval) override = 0;
  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandRefCon) override = 0;

};


#define NS_DECL_COMPOSER_COMMAND(_cmd)                  \
class _cmd : public nsBaseComposerCommand               \
{                                                       \
public:                                                 \
  NS_DECL_NSICONTROLLERCOMMAND                          \
};

// virtual base class for commands that need to save and update Boolean state (like styles etc)
class nsBaseStateUpdatingCommand : public nsBaseComposerCommand
{
public:
  explicit nsBaseStateUpdatingCommand(nsAtom* aTagName);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~nsBaseStateUpdatingCommand();

  // get the current state (on or off) for this style or block format
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) = 0;

  // add/remove the style
  virtual nsresult ToggleState(mozilla::HTMLEditor* aHTMLEditor) = 0;

protected:
  nsAtom* mTagName;
};


// Shared class for the various style updating commands like bold, italics etc.
// Suitable for commands whose state is either 'on' or 'off'.
class nsStyleUpdatingCommand final : public nsBaseStateUpdatingCommand
{
public:
  explicit nsStyleUpdatingCommand(nsAtom* aTagName);

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;

  // add/remove the style
  virtual nsresult ToggleState(mozilla::HTMLEditor* aHTMLEditor) override final;
};


class nsInsertTagCommand : public nsBaseComposerCommand
{
public:
  explicit nsInsertTagCommand(nsAtom* aTagName);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~nsInsertTagCommand();

  nsAtom* mTagName;
};


class nsListCommand final : public nsBaseStateUpdatingCommand
{
public:
  explicit nsListCommand(nsAtom* aTagName);

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;

  // add/remove the style
  virtual nsresult ToggleState(mozilla::HTMLEditor* aHTMLEditor) override final;
};

class nsListItemCommand final : public nsBaseStateUpdatingCommand
{
public:
  explicit nsListItemCommand(nsAtom* aTagName);

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;

  // add/remove the style
  virtual nsresult ToggleState(mozilla::HTMLEditor* aHTMLEditor) override final;
};

// Base class for commands whose state consists of a string (e.g. para format)
class nsMultiStateCommand : public nsBaseComposerCommand
{
public:

  nsMultiStateCommand();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTROLLERCOMMAND

protected:
  virtual ~nsMultiStateCommand();

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) = 0;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) = 0;

};


class nsParagraphStateCommand final : public nsMultiStateCommand
{
public:
                   nsParagraphStateCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsFontFaceStateCommand final : public nsMultiStateCommand
{
public:
                   nsFontFaceStateCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsFontSizeStateCommand final : public nsMultiStateCommand
{
public:
                   nsFontSizeStateCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsHighlightColorStateCommand final : public nsMultiStateCommand
{
public:
                   nsHighlightColorStateCommand();

protected:

  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* _retval) override final;
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;

};

class nsFontColorStateCommand final : public nsMultiStateCommand
{
public:
                   nsFontColorStateCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsAlignCommand final : public nsMultiStateCommand
{
public:
                   nsAlignCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsBackgroundColorStateCommand final : public nsMultiStateCommand
{
public:
                   nsBackgroundColorStateCommand();

protected:

  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult SetState(mozilla::HTMLEditor* aHTMLEditor,
                            nsString& newState) override final;
};

class nsAbsolutePositioningCommand final : public nsBaseStateUpdatingCommand
{
public:
  nsAbsolutePositioningCommand();

protected:

  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* _retval) override final;
  virtual nsresult GetCurrentState(mozilla::HTMLEditor* aHTMLEditor,
                                   nsICommandParams* aParams) override final;
  virtual nsresult ToggleState(mozilla::HTMLEditor* aHTMLEditor) override final;
};

// composer commands

NS_DECL_COMPOSER_COMMAND(nsCloseCommand)
NS_DECL_COMPOSER_COMMAND(nsDocumentStateCommand)
NS_DECL_COMPOSER_COMMAND(nsSetDocumentStateCommand)
NS_DECL_COMPOSER_COMMAND(nsSetDocumentOptionsCommand)
//NS_DECL_COMPOSER_COMMAND(nsPrintingCommands)

NS_DECL_COMPOSER_COMMAND(nsDecreaseZIndexCommand)
NS_DECL_COMPOSER_COMMAND(nsIncreaseZIndexCommand)

// Generic commands

// File menu
NS_DECL_COMPOSER_COMMAND(nsNewCommands)   // handles 'new' anything

// Edit menu
NS_DECL_COMPOSER_COMMAND(nsPasteNoFormattingCommand)

// Block transformations
NS_DECL_COMPOSER_COMMAND(nsIndentCommand)
NS_DECL_COMPOSER_COMMAND(nsOutdentCommand)

NS_DECL_COMPOSER_COMMAND(nsRemoveListCommand)
NS_DECL_COMPOSER_COMMAND(nsRemoveStylesCommand)
NS_DECL_COMPOSER_COMMAND(nsIncreaseFontSizeCommand)
NS_DECL_COMPOSER_COMMAND(nsDecreaseFontSizeCommand)

// Insert content commands
NS_DECL_COMPOSER_COMMAND(nsInsertHTMLCommand)

#endif // nsComposerCommands_h_
