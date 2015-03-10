/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "nsMai.h"
#include "ProxyAccessible.h"
#include "nsString.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {
static void
setTextContentsCB(AtkEditableText *aText, const gchar *aString)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    NS_ConvertUTF8toUTF16 strContent(aString);
    text->ReplaceText(strContent);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    NS_ConvertUTF8toUTF16 strContent(aString);
    proxy->ReplaceText(strContent);
  }
}

static void
insertTextCB(AtkEditableText *aText,
             const gchar *aString, gint aLength, gint *aPosition)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    NS_ConvertUTF8toUTF16 strContent(aString);
    text->InsertText(strContent, *aPosition);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    NS_ConvertUTF8toUTF16 strContent(aString);
    proxy->InsertText(strContent, *aPosition);
  }
}

static void
copyTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    text->CopyText(aStartPos, aEndPos);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    proxy->CopyText(aStartPos, aEndPos);
  }
}

static void
cutTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    text->CutText(aStartPos, aEndPos);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    proxy->CutText(aStartPos, aEndPos);
  }
}

static void
deleteTextCB(AtkEditableText *aText, gint aStartPos, gint aEndPos)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    text->DeleteText(aStartPos, aEndPos);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    proxy->DeleteText(aStartPos, aEndPos);
  }
}

static void
pasteTextCB(AtkEditableText *aText, gint aPosition)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* text = accWrap->AsHyperText();
    if (!text || !text->IsTextRole()) {
      return;
    }

    text->PasteText(aPosition);
  } else if (ProxyAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    proxy->PasteText(aPosition);
  }
}
}

void
editableTextInterfaceInitCB(AtkEditableTextIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->set_text_contents = setTextContentsCB;
  aIface->insert_text = insertTextCB;
  aIface->copy_text = copyTextCB;
  aIface->cut_text = cutTextCB;
  aIface->delete_text = deleteTextCB;
  aIface->paste_text = pasteTextCB;
}
