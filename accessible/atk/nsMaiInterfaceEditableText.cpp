/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "nsMai.h"
#include "RemoteAccessible.h"
#include "nsString.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {
static void setTextContentsCB(AtkEditableText* aText, const gchar* aString) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      NS_ConvertUTF8toUTF16 strContent(aString);
      text->ReplaceText(strContent);
    }
  }
}

static void insertTextCB(AtkEditableText* aText, const gchar* aString,
                         gint aLength, gint* aPosition) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      NS_ConvertUTF8toUTF16 strContent(aString);
      text->InsertText(strContent, *aPosition);
    }
  }
}

static void copyTextCB(AtkEditableText* aText, gint aStartPos, gint aEndPos) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      text->CopyText(aStartPos, aEndPos);
    }
  }
}

static void cutTextCB(AtkEditableText* aText, gint aStartPos, gint aEndPos) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      text->CutText(aStartPos, aEndPos);
    }
  }
}

static void deleteTextCB(AtkEditableText* aText, gint aStartPos, gint aEndPos) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      text->DeleteText(aStartPos, aEndPos);
    }
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY
static void pasteTextCB(AtkEditableText* aText, gint aPosition) {
  if (Accessible* acc = GetInternalObj(ATK_OBJECT(aText))) {
    if (acc->IsTextRole()) {
      return;
    }
    if (HyperTextAccessibleBase* text = acc->AsHyperTextBase()) {
      text->PasteText(aPosition);
    }
  }
}
}

void editableTextInterfaceInitCB(AtkEditableTextIface* aIface) {
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->set_text_contents = setTextContentsCB;
  aIface->insert_text = insertTextCB;
  aIface->copy_text = copyTextCB;
  aIface->cut_text = cutTextCB;
  aIface->delete_text = deleteTextCB;
  aIface->paste_text = pasteTextCB;
}
