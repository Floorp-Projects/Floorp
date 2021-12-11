/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorController_h
#define mozilla_EditorController_h

#include "nscore.h"

class nsControllerCommandTable;

namespace mozilla {

// the editor controller is used for both text widgets, and basic text editing
// commands in composer. The refCon that gets passed to its commands is an
// nsIEditor.

class EditorController final {
 public:
  static nsresult RegisterEditorCommands(
      nsControllerCommandTable* aCommandTable);
  static nsresult RegisterEditingCommands(
      nsControllerCommandTable* aCommandTable);
  static void Shutdown();
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorController_h
