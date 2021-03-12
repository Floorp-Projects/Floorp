/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ApplicationAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Components.h"
#include "nsGlobalWindow.h"
#include "nsIStringBundle.h"

using namespace mozilla::a11y;

ApplicationAccessible::ApplicationAccessible()
    : AccessibleWrap(nullptr, nullptr) {
  mType = eApplicationType;
  mAppInfo = do_GetService("@mozilla.org/xre/app-info;1");
  MOZ_ASSERT(mAppInfo, "no application info");
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

ENameValueFlag ApplicationAccessible::Name(nsString& aName) const {
  aName.Truncate();

  nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::components::StringBundle::Service();

  NS_ASSERTION(bundleService, "String bundle service must be present!");
  if (!bundleService) return eNameOK;

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle(
      "chrome://branding/locale/brand.properties", getter_AddRefs(bundle));
  if (NS_FAILED(rv)) return eNameOK;

  nsAutoString appName;
  rv = bundle->GetStringFromName("brandShortName", appName);
  if (NS_FAILED(rv) || appName.IsEmpty()) {
    NS_WARNING("brandShortName not found, using default app name");
    appName.AssignLiteral("Gecko based application");
  }

  aName.Assign(appName);
  return eNameOK;
}

void ApplicationAccessible::Description(nsString& aDescription) {
  aDescription.Truncate();
}

void ApplicationAccessible::Value(nsString& aValue) const { aValue.Truncate(); }

uint64_t ApplicationAccessible::State() {
  return IsDefunct() ? states::DEFUNCT : 0;
}

already_AddRefed<nsIPersistentProperties>
ApplicationAccessible::NativeAttributes() {
  return nullptr;
}

GroupPos ApplicationAccessible::GroupPosition() { return GroupPos(); }

LocalAccessible* ApplicationAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  return nullptr;
}

LocalAccessible* ApplicationAccessible::FocusedChild() {
  LocalAccessible* focus = FocusMgr()->FocusedAccessible();
  if (focus && focus->LocalParent() == this) {
    return focus;
  }

  return nullptr;
}

Relation ApplicationAccessible::RelationByType(
    RelationType aRelationType) const {
  return Relation();
}

nsIntRect ApplicationAccessible::Bounds() const { return nsIntRect(); }

nsRect ApplicationAccessible::BoundsInAppUnits() const { return nsRect(); }

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible public methods

void ApplicationAccessible::Shutdown() { mAppInfo = nullptr; }

void ApplicationAccessible::ApplyARIAState(uint64_t* aState) const {}

role ApplicationAccessible::NativeRole() const { return roles::APP_ROOT; }

uint64_t ApplicationAccessible::NativeState() const { return 0; }

KeyBinding ApplicationAccessible::AccessKey() const { return KeyBinding(); }

void ApplicationAccessible::Init() {
  // Basically children are kept updated by Append/RemoveChild method calls.
  // However if there are open windows before accessibility was started
  // then we need to make sure root accessibles for open windows are created so
  // that all root accessibles are stored in application accessible children
  // array.

  nsGlobalWindowOuter::OuterWindowByIdTable* windowsById =
      nsGlobalWindowOuter::GetWindowsTable();

  if (!windowsById) {
    return;
  }

  for (const auto& entry : *windowsById) {
    nsGlobalWindowOuter* window = entry.GetData();
    if (window->GetDocShell() && window->IsRootOuterWindow()) {
      if (RefPtr<dom::Document> docNode = window->GetExtantDoc()) {
        GetAccService()->GetDocAccessible(docNode);  // ensure creation
      }
    }
  }
}

LocalAccessible* ApplicationAccessible::GetSiblingAtOffset(
    int32_t aOffset, nsresult* aError) const {
  if (aError) *aError = NS_OK;  // fail peacefully

  return nullptr;
}
