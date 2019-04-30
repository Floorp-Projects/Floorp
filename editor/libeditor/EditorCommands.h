/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorCommands_h
#define mozilla_EditorCommands_h

#include "mozilla/StaticPtr.h"
#include "nsIControllerCommand.h"
#include "nsISupportsImpl.h"
#include "nsRefPtrHashtable.h"
#include "nsStringFwd.h"

class nsAtom;
class nsCommandParams;
class nsICommandParams;
class nsIEditingSession;

namespace mozilla {

class HTMLEditor;
class TextEditor;

/**
 * This is a base class for commands registered with the editor controller.
 * Note that such commands are designed as singleton classes.  So, MUST be
 * stateless. Any state must be stored via the refCon (an nsIEditor).
 */

class EditorCommand : public nsIControllerCommand {
 public:
  NS_DECL_ISUPPORTS

  // nsIControllerCommand methods.  Use EditorCommand specific methods instead
  // for internal use.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon,
                              bool* aIsEnabled) final;
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandRefCon) final;
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD DoCommandParams(const char* aCommandName,
                             nsICommandParams* aParams,
                             nsISupports* aCommandRefCon) final;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD GetCommandStateParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* aCommandRefCon) final;

  MOZ_CAN_RUN_SCRIPT
  virtual bool IsCommandEnabled(Command aCommand,
                                TextEditor* aTextEditor) const = 0;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult DoCommand(Command aCommand,
                             TextEditor& aTextEditor) const = 0;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult DoCommandParams(Command aCommand, nsCommandParams* aParams,
                                   TextEditor& aTextEditor) const = 0;
  /**
   * @param aTextEditor         If the context is an editor, should be set to
   *                            it.  Otherwise, nullptr.
   * @param aEditingSession     If the context is an editing session, should be
   *                            set to it.  This usually occurs if editor has
   *                            not been created yet during initialization.
   *                            Otherwise, nullptr.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult GetCommandStateParams(
      Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
      nsIEditingSession* aEditingSession) const = 0;

 protected:
  EditorCommand() = default;
  virtual ~EditorCommand() = default;
};

#define NS_DECL_EDITOR_COMMAND_METHODS(_cmd)                                   \
 public:                                                                       \
  MOZ_CAN_RUN_SCRIPT                                                           \
  virtual bool IsCommandEnabled(Command aCommand, TextEditor* aTextEditor)     \
      const final;                                                             \
  using EditorCommand::IsCommandEnabled;                                       \
  MOZ_CAN_RUN_SCRIPT                                                           \
  virtual nsresult DoCommand(Command aCommand, TextEditor& aTextEditor)        \
      const final;                                                             \
  using EditorCommand::DoCommand;                                              \
  MOZ_CAN_RUN_SCRIPT                                                           \
  virtual nsresult DoCommandParams(Command aCommand, nsCommandParams* aParams, \
                                   TextEditor& aTextEditor) const final;       \
  using EditorCommand::DoCommandParams;                                        \
  MOZ_CAN_RUN_SCRIPT                                                           \
  virtual nsresult GetCommandStateParams(                                      \
      Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,     \
      nsIEditingSession* aEditingSession) const final;                         \
  using EditorCommand::GetCommandStateParams;

#define NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(_cmd) \
 public:                                                   \
  static _cmd* GetInstance() {                             \
    if (!sInstance) {                                      \
      sInstance = new _cmd();                              \
    }                                                      \
    return sInstance;                                      \
  }                                                        \
                                                           \
  static void Shutdown() { sInstance = nullptr; }          \
                                                           \
 private:                                                  \
  static StaticRefPtr<_cmd> sInstance;

#define NS_DECL_EDITOR_COMMAND(_cmd)                   \
  class _cmd final : public EditorCommand {            \
    NS_DECL_EDITOR_COMMAND_METHODS(_cmd)               \
    NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(_cmd) \
                                                       \
   protected:                                          \
    _cmd() = default;                                  \
    virtual ~_cmd() = default;                         \
  };

// basic editor commands
NS_DECL_EDITOR_COMMAND(UndoCommand)
NS_DECL_EDITOR_COMMAND(RedoCommand)

NS_DECL_EDITOR_COMMAND(CutCommand)
NS_DECL_EDITOR_COMMAND(CutOrDeleteCommand)
NS_DECL_EDITOR_COMMAND(CopyCommand)
NS_DECL_EDITOR_COMMAND(CopyOrDeleteCommand)
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

/******************************************************************************
 * Commands for HTML editor
 ******************************************************************************/

// virtual base class for commands that need to save and update Boolean state
// (like styles etc)
class StateUpdatingCommandBase : public EditorCommand {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(StateUpdatingCommandBase, EditorCommand)

  NS_DECL_EDITOR_COMMAND_METHODS(StateUpdatingCommandBase)

 protected:
  StateUpdatingCommandBase() = default;
  virtual ~StateUpdatingCommandBase() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                                   nsCommandParams& aParams) const = 0;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult ToggleState(nsAtom* aTagName,
                               HTMLEditor* aHTMLEditor) const = 0;

  static nsAtom* GetTagName(Command aCommand) {
    switch (aCommand) {
      case Command::FormatBold:
        return nsGkAtoms::b;
      case Command::FormatItalic:
        return nsGkAtoms::i;
      case Command::FormatUnderline:
        return nsGkAtoms::u;
      case Command::FormatTeletypeText:
        return nsGkAtoms::tt;
      case Command::FormatStrikeThrough:
        return nsGkAtoms::strike;
      case Command::FormatSuperscript:
        return nsGkAtoms::sup;
      case Command::FormatSubscript:
        return nsGkAtoms::sub;
      case Command::FormatNoBreak:
        return nsGkAtoms::nobr;
      case Command::FormatEmphasis:
        return nsGkAtoms::em;
      case Command::FormatStrong:
        return nsGkAtoms::strong;
      case Command::FormatCitation:
        return nsGkAtoms::cite;
      case Command::FormatAbbreviation:
        return nsGkAtoms::abbr;
      case Command::FormatAcronym:
        return nsGkAtoms::acronym;
      case Command::FormatCode:
        return nsGkAtoms::code;
      case Command::FormatSample:
        return nsGkAtoms::samp;
      case Command::FormatVariable:
        return nsGkAtoms::var;
      case Command::FormatRemoveLink:
        return nsGkAtoms::href;
      case Command::InsertOrderedList:
        return nsGkAtoms::ol;
      case Command::InsertUnorderedList:
        return nsGkAtoms::ul;
      case Command::InsertDefinitionTerm:
        return nsGkAtoms::dt;
      case Command::InsertDefinitionDetails:
        return nsGkAtoms::dd;
      case Command::FormatAbsolutePosition:
        return nsGkAtoms::_empty;
      default:
        return nullptr;
    }
  }
  friend class InsertTagCommand;  // for allowing it to access GetTagName()
};

// Shared class for the various style updating commands like bold, italics etc.
// Suitable for commands whose state is either 'on' or 'off'.
class StyleUpdatingCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(StyleUpdatingCommand)

 protected:
  StyleUpdatingCommand() = default;
  virtual ~StyleUpdatingCommand() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) const final;
};

class InsertTagCommand final : public EditorCommand {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(InsertTagCommand, EditorCommand)

  NS_DECL_EDITOR_COMMAND_METHODS(InsertTagCommand)
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(InsertTagCommand)

 protected:
  InsertTagCommand() = default;
  virtual ~InsertTagCommand() = default;

  static nsAtom* GetTagName(Command aCommand) {
    switch (aCommand) {
      case Command::InsertLink:
        return nsGkAtoms::a;
      case Command::InsertImage:
        return nsGkAtoms::img;
      case Command::InsertHorizontalRule:
        return nsGkAtoms::hr;
      default:
        return StateUpdatingCommandBase::GetTagName(aCommand);
    }
  }
};

class ListCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ListCommand)

 protected:
  ListCommand() = default;
  virtual ~ListCommand() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) const final;
};

class ListItemCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ListItemCommand)

 protected:
  ListItemCommand() = default;
  virtual ~ListItemCommand() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) const final;
};

// Base class for commands whose state consists of a string (e.g. para format)
class MultiStateCommandBase : public EditorCommand {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultiStateCommandBase, EditorCommand)
  NS_DECL_EDITOR_COMMAND_METHODS(MultiStateCommandBase)

 protected:
  MultiStateCommandBase() = default;
  virtual ~MultiStateCommandBase() = default;

  MOZ_CAN_RUN_SCRIPT
  virtual nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                                   nsCommandParams& aParams) const = 0;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult SetState(HTMLEditor* aHTMLEditor,
                            const nsString& newState) const = 0;
};

class ParagraphStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ParagraphStateCommand)

 protected:
  ParagraphStateCommand() = default;
  virtual ~ParagraphStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class FontFaceStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontFaceStateCommand)

 protected:
  FontFaceStateCommand() = default;
  virtual ~FontFaceStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class FontSizeStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontSizeStateCommand)

 protected:
  FontSizeStateCommand() = default;
  virtual ~FontSizeStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class HighlightColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(HighlightColorStateCommand)

 protected:
  HighlightColorStateCommand() = default;
  virtual ~HighlightColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class FontColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontColorStateCommand)

 protected:
  FontColorStateCommand() = default;
  virtual ~FontColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class AlignCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(AlignCommand)

 protected:
  AlignCommand() = default;
  virtual ~AlignCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class BackgroundColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(BackgroundColorStateCommand)

 protected:
  BackgroundColorStateCommand() = default;
  virtual ~BackgroundColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor,
                    const nsString& newState) const final;
};

class AbsolutePositioningCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(AbsolutePositioningCommand)

 protected:
  AbsolutePositioningCommand() = default;
  virtual ~AbsolutePositioningCommand() = default;

  MOZ_CAN_RUN_SCRIPT
  nsresult GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                           nsCommandParams& aParams) const final;
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) const final;
};

// composer commands

NS_DECL_EDITOR_COMMAND(DocumentStateCommand)
NS_DECL_EDITOR_COMMAND(SetDocumentStateCommand)

NS_DECL_EDITOR_COMMAND(DecreaseZIndexCommand)
NS_DECL_EDITOR_COMMAND(IncreaseZIndexCommand)

// Generic commands

// Edit menu
NS_DECL_EDITOR_COMMAND(PasteNoFormattingCommand)

// Block transformations
NS_DECL_EDITOR_COMMAND(IndentCommand)
NS_DECL_EDITOR_COMMAND(OutdentCommand)

NS_DECL_EDITOR_COMMAND(RemoveListCommand)
NS_DECL_EDITOR_COMMAND(RemoveStylesCommand)
NS_DECL_EDITOR_COMMAND(IncreaseFontSizeCommand)
NS_DECL_EDITOR_COMMAND(DecreaseFontSizeCommand)

// Insert content commands
NS_DECL_EDITOR_COMMAND(InsertHTMLCommand)

#undef NS_DECL_EDITOR_COMMAND
#undef NS_DECL_EDITOR_COMMAND_METHODS
#undef NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorCommands_h
