/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMai.h"
#include "DocAccessibleWrap.h"
#include "mozilla/PresShell.h"
#include "nsWindow.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(dom::Document* aDocument,
                                     PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell) {}

DocAccessibleWrap::~DocAccessibleWrap() {}

bool DocAccessibleWrap::IsActivated() {
  if (nsWindow* window = nsWindow::GetFocusedWindow()) {
    if (nsIWidgetListener* listener = window->GetWidgetListener()) {
      if (PresShell* presShell = listener->GetPresShell()) {
        return presShell == PresShellPtr();
      }
    }
  }

  return false;
}
