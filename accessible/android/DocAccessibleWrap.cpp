/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalAccessible-inl.h"
#include "AccAttributes.h"
#include "DocAccessibleChild.h"
#include "DocAccessibleWrap.h"
#include "nsIDocShell.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "Pivot.h"
#include "SessionAccessibility.h"
#include "TraversalRule.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using namespace mozilla::a11y;

#define UNIQUE_ID(acc)                             \
  !acc || (acc->IsDoc() && acc->AsDoc()->IPCDoc()) \
      ? 0                                          \
      : reinterpret_cast<uint64_t>(acc->UniqueID())

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(Document* aDocument, PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell) {
  // We need an nsINode associated with this accessible to register it with the
  // right SessionAccessibility instance. When the base AccessibleWrap
  // constructor is called we don't have one yet because null is passed as the
  // content node. So we do it here after a Document is associated with the
  // accessible.
  if (!IPCAccessibilityActive()) {
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
    SessionAccessibility::RegisterAccessible(this);
  }
}

DocAccessibleWrap::~DocAccessibleWrap() {}

void DocAccessibleWrap::Shutdown() {
  // Unregister here before disconnecting from PresShell.
  if (!IPCAccessibilityActive()) {
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
    if (IsRoot()) {
      SessionAccessibility::UnregisterAll(PresShellPtr());
    } else {
      SessionAccessibility::UnregisterAccessible(this);
    }
  }
  DocAccessible::Shutdown();
}

DocAccessibleWrap* DocAccessibleWrap::GetTopLevelContentDoc(
    AccessibleWrap* aAccessible) {
  DocAccessibleWrap* doc =
      static_cast<DocAccessibleWrap*>(aAccessible->Document());
  while (doc && !doc->IsTopLevelContentDoc()) {
    doc = static_cast<DocAccessibleWrap*>(doc->ParentDocument());
  }

  return doc;
}

bool DocAccessibleWrap::IsTopLevelContentDoc() {
  DocAccessible* parentDoc = ParentDocument();
  return DocumentNode()->IsContentDocument() &&
         (!parentDoc || !parentDoc->DocumentNode()->IsContentDocument());
}

#undef UNIQUE_ID
