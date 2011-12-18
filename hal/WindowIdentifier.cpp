/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/dom/ContentChild.h"
#include "WindowIdentifier.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace hal {

WindowIdentifier::WindowIdentifier()
  : mWindow(nsnull)
  , mIsEmpty(true)
{
}

WindowIdentifier::WindowIdentifier(nsIDOMWindow *window)
  : mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const nsTArray<uint64> &id, nsIDOMWindow *window)
  : mID(id)
  , mWindow(window)
  , mIsEmpty(false)
{
  mID.AppendElement(GetWindowID());
}

WindowIdentifier::WindowIdentifier(const WindowIdentifier &other)
  : mID(other.mID)
  , mWindow(other.mWindow)
  , mIsEmpty(other.mIsEmpty)
{
}

const InfallibleTArray<uint64>&
WindowIdentifier::AsArray() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID;
}

bool
WindowIdentifier::HasTraveledThroughIPC() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mID.Length() >= 2;
}

void
WindowIdentifier::AppendProcessID()
{
  MOZ_ASSERT(!mIsEmpty);
  mID.AppendElement(dom::ContentChild::GetSingleton()->GetID());
}

uint64
WindowIdentifier::GetWindowID() const
{
  MOZ_ASSERT(!mIsEmpty);
  nsCOMPtr<nsPIDOMWindow> pidomWindow = do_QueryInterface(mWindow);
  if (!pidomWindow) {
    return uint64(-1);
  }
  return pidomWindow->WindowID();
}

nsIDOMWindow*
WindowIdentifier::GetWindow() const
{
  MOZ_ASSERT(!mIsEmpty);
  return mWindow;
}

} // namespace hal
} // namespace mozilla
