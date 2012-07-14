/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposerCommands_h_
#define nsComposerCommands_h_

#include "nsIControllerCommand.h"
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED, etc
#include "nscore.h"                     // for nsresult, NS_IMETHOD

class nsIAtom;
class nsICommandParams;
class nsIEditor;
class nsISupports;
class nsString;

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
public:

              nsBaseComposerCommand();
  virtual     ~nsBaseComposerCommand() {}
    
  // nsISupports
  NS_DECL_ISUPPORTS
    
  // nsIControllerCommand. Declared longhand so we can make them pure virtual
  NS_IMETHOD IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *_retval) = 0;
  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandRefCon) = 0;

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
  nsBaseStateUpdatingCommand(nsIAtom* aTagName);
  virtual ~nsBaseStateUpdatingCommand();
    
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSICONTROLLERCOMMAND

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams) = 0;
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditor* aEditor) = 0;

protected:
  nsIAtom* mTagName;
};


// Shared class for the various style updating commands like bold, italics etc.
// Suitable for commands whose state is either 'on' or 'off'.
class nsStyleUpdatingCommand : public nsBaseStateUpdatingCommand
{
public:
  nsStyleUpdatingCommand(nsIAtom* aTagName);
           
protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams);
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditor* aEditor);
};


class nsInsertTagCommand : public nsBaseComposerCommand
{
public:
  explicit nsInsertTagCommand(nsIAtom* aTagName);
  virtual     ~nsInsertTagCommand();
    
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSICONTROLLERCOMMAND

protected:

  nsIAtom* mTagName;
};


class nsListCommand : public nsBaseStateUpdatingCommand
{
public:
  nsListCommand(nsIAtom* aTagName);

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams);
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditor* aEditor);
};

class nsListItemCommand : public nsBaseStateUpdatingCommand
{
public:
  nsListItemCommand(nsIAtom* aTagName);

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams);
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditor* aEditor);
};

// Base class for commands whose state consists of a string (e.g. para format)
class nsMultiStateCommand : public nsBaseComposerCommand
{
public:
  
                   nsMultiStateCommand();
  virtual          ~nsMultiStateCommand();
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTROLLERCOMMAND

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams) =0;
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState) = 0;
  
};


class nsParagraphStateCommand : public nsMultiStateCommand
{
public:
                   nsParagraphStateCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsFontFaceStateCommand : public nsMultiStateCommand
{
public:
                   nsFontFaceStateCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsFontSizeStateCommand : public nsMultiStateCommand
{
public:
                   nsFontSizeStateCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor,
                                   nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsHighlightColorStateCommand : public nsMultiStateCommand
{
public:
                   nsHighlightColorStateCommand();

protected:

  NS_IMETHOD IsCommandEnabled(const char *aCommandName, nsISupports *aCommandRefCon, bool *_retval);
  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);

};

class nsFontColorStateCommand : public nsMultiStateCommand
{
public:
                   nsFontColorStateCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsAlignCommand : public nsMultiStateCommand
{
public:
                   nsAlignCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsBackgroundColorStateCommand : public nsMultiStateCommand
{
public:
                   nsBackgroundColorStateCommand();

protected:

  virtual nsresult GetCurrentState(nsIEditor *aEditor, nsICommandParams* aParams);
  virtual nsresult SetState(nsIEditor *aEditor, nsString& newState);
};

class nsAbsolutePositioningCommand : public nsBaseStateUpdatingCommand
{
public:
  nsAbsolutePositioningCommand();

protected:

  NS_IMETHOD IsCommandEnabled(const char *aCommandName, nsISupports *aCommandRefCon, bool *_retval);
  virtual nsresult  GetCurrentState(nsIEditor* aEditor, nsICommandParams* aParams);
  virtual nsresult  ToggleState(nsIEditor* aEditor);
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
