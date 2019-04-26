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
class nsICommandParams;

namespace mozilla {

class HTMLEditor;

/**
 * This is a base class for commands registered with the editor controller.
 * Note that such commands are designed as singleton classes.  So, MUST be
 * stateless. Any state must be stored via the refCon (an nsIEditor).
 */

class EditorCommand : public nsIControllerCommand {
 public:
  NS_DECL_ISUPPORTS

 protected:
  EditorCommand() = default;
  virtual ~EditorCommand() = default;
};

#define NS_DECL_EDITOR_COMMAND_METHODS(_cmd)                                  \
 public:                                                                      \
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,                       \
                              nsISupports* aCommandRefCon, bool* aIsEnabled)  \
      override;                                                               \
  MOZ_CAN_RUN_SCRIPT                                                          \
  NS_IMETHOD DoCommand(const char* aCommandName, nsISupports* aCommandRefCon) \
      override;                                                               \
  MOZ_CAN_RUN_SCRIPT                                                          \
  NS_IMETHOD DoCommandParams(const char* aCommandName,                        \
                             nsICommandParams* aParams,                       \
                             nsISupports* aCommandRefCon) override;           \
  NS_IMETHOD GetCommandStateParams(const char* aCommandName,                  \
                                   nsICommandParams* aParams,                 \
                                   nsISupports* aCommandRefCon) override;

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
  virtual ~StateUpdatingCommandBase() { sTagNameTable.Clear(); }

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      virtual nsresult
      GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                      nsICommandParams* aParams) = 0;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) = 0;

  static already_AddRefed<nsAtom> TagName(const char* aCommandName) {
    MOZ_DIAGNOSTIC_ASSERT(aCommandName);
    if (NS_WARN_IF(!aCommandName)) {
      return nullptr;
    }
    if (!sTagNameTable.Count()) {
      sTagNameTable.Put("cmd_bold", nsGkAtoms::b);
      sTagNameTable.Put("cmd_italic", nsGkAtoms::i);
      sTagNameTable.Put("cmd_underline", nsGkAtoms::u);
      sTagNameTable.Put("cmd_tt", nsGkAtoms::tt);
      sTagNameTable.Put("cmd_strikethrough", nsGkAtoms::strike);
      sTagNameTable.Put("cmd_superscript", nsGkAtoms::sup);
      sTagNameTable.Put("cmd_subscript", nsGkAtoms::sub);
      sTagNameTable.Put("cmd_nobreak", nsGkAtoms::nobr);
      sTagNameTable.Put("cmd_em", nsGkAtoms::em);
      sTagNameTable.Put("cmd_strong", nsGkAtoms::strong);
      sTagNameTable.Put("cmd_cite", nsGkAtoms::cite);
      sTagNameTable.Put("cmd_abbr", nsGkAtoms::abbr);
      sTagNameTable.Put("cmd_acronym", nsGkAtoms::acronym);
      sTagNameTable.Put("cmd_code", nsGkAtoms::code);
      sTagNameTable.Put("cmd_samp", nsGkAtoms::samp);
      sTagNameTable.Put("cmd_var", nsGkAtoms::var);
      sTagNameTable.Put("cmd_removeLinks", nsGkAtoms::href);
      sTagNameTable.Put("cmd_ol", nsGkAtoms::ol);
      sTagNameTable.Put("cmd_ul", nsGkAtoms::ul);
      sTagNameTable.Put("cmd_dt", nsGkAtoms::dt);
      sTagNameTable.Put("cmd_dd", nsGkAtoms::dd);
      sTagNameTable.Put("cmd_absPos", nsGkAtoms::_empty);
    }
    RefPtr<nsAtom> tagName = sTagNameTable.Get(aCommandName);
    MOZ_DIAGNOSTIC_ASSERT(tagName);
    return tagName.forget();
  }

  static nsRefPtrHashtable<nsCharPtrHashKey, nsAtom> sTagNameTable;
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
  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                      nsICommandParams* aParams) final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) final;
};

class InsertTagCommand final : public EditorCommand {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(InsertTagCommand, EditorCommand)

  NS_DECL_EDITOR_COMMAND_METHODS(InsertTagCommand)
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(InsertTagCommand)

 protected:
  InsertTagCommand() = default;
  virtual ~InsertTagCommand() { sTagNameTable.Clear(); }

  static already_AddRefed<nsAtom> TagName(const char* aCommandName) {
    MOZ_DIAGNOSTIC_ASSERT(aCommandName);
    if (NS_WARN_IF(!aCommandName)) {
      return nullptr;
    }
    if (!sTagNameTable.Count()) {
      sTagNameTable.Put("cmd_insertLinkNoUI", nsGkAtoms::a);
      sTagNameTable.Put("cmd_insertImageNoUI", nsGkAtoms::img);
      sTagNameTable.Put("cmd_insertHR", nsGkAtoms::hr);
    }
    RefPtr<nsAtom> tagName = sTagNameTable.Get(aCommandName);
    MOZ_DIAGNOSTIC_ASSERT(tagName);
    return tagName.forget();
  }

  static nsRefPtrHashtable<nsCharPtrHashKey, nsAtom> sTagNameTable;
};

class ListCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ListCommand)

 protected:
  ListCommand() = default;
  virtual ~ListCommand() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                      nsICommandParams* aParams) final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) final;
};

class ListItemCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ListItemCommand)

 protected:
  ListItemCommand() = default;
  virtual ~ListItemCommand() = default;

  // get the current state (on or off) for this style or block format
  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                      nsICommandParams* aParams) final;

  // add/remove the style
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) final;
};

// Base class for commands whose state consists of a string (e.g. para format)
class MultiStateCommandBase : public EditorCommand {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultiStateCommandBase, EditorCommand)
  NS_DECL_EDITOR_COMMAND_METHODS(MultiStateCommandBase)

 protected:
  MultiStateCommandBase() = default;
  virtual ~MultiStateCommandBase() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      virtual nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) = 0;
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult SetState(HTMLEditor* aHTMLEditor,
                            const nsString& newState) = 0;
};

class ParagraphStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(ParagraphStateCommand)

 protected:
  ParagraphStateCommand() = default;
  virtual ~ParagraphStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class FontFaceStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontFaceStateCommand)

 protected:
  FontFaceStateCommand() = default;
  virtual ~FontFaceStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class FontSizeStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontSizeStateCommand)

 protected:
  FontSizeStateCommand() = default;
  virtual ~FontSizeStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class HighlightColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(HighlightColorStateCommand)

 protected:
  HighlightColorStateCommand() = default;
  virtual ~HighlightColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class FontColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(FontColorStateCommand)

 protected:
  FontColorStateCommand() = default;
  virtual ~FontColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class AlignCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(AlignCommand)

 protected:
  AlignCommand() = default;
  virtual ~AlignCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class BackgroundColorStateCommand final : public MultiStateCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(BackgroundColorStateCommand)

 protected:
  BackgroundColorStateCommand() = default;
  virtual ~BackgroundColorStateCommand() = default;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(HTMLEditor* aHTMLEditor, nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult SetState(HTMLEditor* aHTMLEditor, const nsString& newState) final;
};

class AbsolutePositioningCommand final : public StateUpdatingCommandBase {
 public:
  NS_INLINE_DECL_EDITOR_COMMAND_MAKE_SINGLETON(AbsolutePositioningCommand)

 protected:
  AbsolutePositioningCommand() = default;
  virtual ~AbsolutePositioningCommand() = default;

  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandRefCon, bool* _retval) final;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY  // XXX Needs to change nsIControllerCommand.idl
      nsresult
      GetCurrentState(nsAtom* aTagName, HTMLEditor* aHTMLEditor,
                      nsICommandParams* aParams) final;
  MOZ_CAN_RUN_SCRIPT
  nsresult ToggleState(nsAtom* aTagName, HTMLEditor* aHTMLEditor) final;
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
