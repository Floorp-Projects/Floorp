/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditRules_h
#define mozilla_TextEditRules_h

#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/HTMLEditor.h"  // for nsIEditor::AsHTMLEditor()
#include "mozilla/TextEditor.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class HTMLEditor;
class HTMLEditRules;
namespace dom {
class HTMLBRElement;
class Selection;
}  // namespace dom

/**
 * Object that encapsulates HTML text-specific editing rules.
 *
 * To be a good citizen, edit rules must live by these restrictions:
 * 1. All data manipulation is through the editor.
 *    Content nodes in the document tree must <B>not</B> be manipulated
 *    directly.  Content nodes in document fragments that are not part of the
 *    document itself may be manipulated at will.  Operations on document
 *    fragments must <B>not</B> go through the editor.
 * 2. Selection must not be explicitly set by the rule method.
 *    Any manipulation of Selection must be done by the editor.
 * 3. Stop handling edit action if method returns NS_ERROR_EDITOR_DESTROYED
 *    since if mutation event lister or selectionchange event listener disables
 *    the editor, we should not modify the DOM tree anymore.
 * 4. Any method callers have to check nsresult return value (both directly or
 *    with simple class like EditActionResult) whether the value is
 *    NS_ERROR_EDITOR_DESTROYED at least.
 * 5. Callers of methods of other classes such as TextEditor, have to check
 *    CanHandleEditAction() before checking its result and if the result is
 *    false, the method have to return NS_ERROR_EDITOR_DESTROYED.  In other
 *    words, any methods which may change Selection or the DOM tree have to
 *    return nsresult directly or with simple class like EditActionResult.
 *    And such methods should be marked as MOZ_MUST_USE.
 */
class TextEditRules {
 protected:
  typedef EditorBase::AutoSelectionRestorer AutoSelectionRestorer;
  typedef EditorBase::AutoEditSubActionNotifier AutoEditSubActionNotifier;
  typedef EditorBase::AutoTransactionsConserveSelection
      AutoTransactionsConserveSelection;

 public:
  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;
  template <typename T>
  using OwningNonNull = OwningNonNull<T>;

  NS_INLINE_DECL_REFCOUNTING(TextEditRules)

  TextEditRules();

  HTMLEditRules* AsHTMLEditRules();
  const HTMLEditRules* AsHTMLEditRules() const;

  MOZ_CAN_RUN_SCRIPT
  virtual nsresult Init(TextEditor* aTextEditor);
  virtual nsresult DetachEditor();
  virtual nsresult BeforeEdit();
  MOZ_CAN_RUN_SCRIPT virtual nsresult AfterEdit();

 protected:
  virtual ~TextEditRules() = default;

  void InitFields();

  // TextEditRules implementation methods

  bool IsSingleLineEditor() const;
  bool IsPlaintextEditor() const;

 private:
  TextEditor* MOZ_NON_OWNING_REF mTextEditor;

 protected:
  /**
   * AutoSafeEditorData grabs editor instance and related instances during
   * handling an edit action.  When this is created, its pointer is set to
   * the mSafeData, and this guarantees the lifetime of grabbing objects
   * until it's destroyed.
   */
  class MOZ_STACK_CLASS AutoSafeEditorData {
   public:
    AutoSafeEditorData(TextEditRules& aTextEditRules, TextEditor& aTextEditor)
        : mTextEditRules(aTextEditRules), mHTMLEditor(nullptr) {
      // mTextEditRules may have AutoSafeEditorData instance since in some
      // cases. E.g., while public methods of *EditRules are called, it
      // calls attaching editor's method, then, editor will call public
      // methods of *EditRules again.
      if (mTextEditRules.mData) {
        return;
      }
      mTextEditor = &aTextEditor;
      mHTMLEditor = aTextEditor.AsHTMLEditor();
      mTextEditRules.mData = this;
    }

    ~AutoSafeEditorData() {
      if (mTextEditRules.mData != this) {
        return;
      }
      mTextEditRules.mData = nullptr;
    }

    TextEditor& TextEditorRef() const { return *mTextEditor; }
    HTMLEditor& HTMLEditorRef() const {
      MOZ_ASSERT(mHTMLEditor);
      return *mHTMLEditor;
    }

   private:
    // This class should be created by public methods TextEditRules and
    // HTMLEditRules and in the stack.  So, the lifetime of this should
    // be guaranteed the callers of the public methods.
    TextEditRules& MOZ_NON_OWNING_REF mTextEditRules;
    RefPtr<TextEditor> mTextEditor;
    // Shortcut for HTMLEditorRef().  So, not necessary to use RefPtr.
    HTMLEditor* MOZ_NON_OWNING_REF mHTMLEditor;
  };
  AutoSafeEditorData* mData;

  TextEditor& TextEditorRef() const {
    MOZ_ASSERT(mData);
    return mData->TextEditorRef();
  }
  // SelectionRefPtr() won't return nullptr unless editor instance accidentally
  // ignored result of AutoEditActionDataSetter::CanHandle() and keep handling
  // edit action.
  const RefPtr<Selection>& SelectionRefPtr() const {
    MOZ_ASSERT(mData);
    return TextEditorRef().SelectionRefPtr();
  }
  bool CanHandleEditAction() const {
    if (!mTextEditor) {
      return false;
    }
    if (mTextEditor->Destroyed()) {
      return false;
    }
    MOZ_ASSERT(mTextEditor->IsInitialized());
    return true;
  }

#ifdef DEBUG
  bool IsEditorDataAvailable() const { return !!mData; }
#endif  // #ifdef DEBUG

#ifdef DEBUG
  bool mIsHandling;
#endif  // #ifdef DEBUG

  bool mIsHTMLEditRules;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TextEditRules_h
