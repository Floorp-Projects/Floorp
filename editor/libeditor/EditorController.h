/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorController_h
#define mozilla_EditorController_h

#include "nscore.h"

#define NS_EDITORCONTROLLER_CID \
{ 0x26fb965c, 0x9de6, 0x11d3, \
  { 0xbc, 0xcc, 0x0, 0x60, 0xb0, 0xfc, 0x76, 0xbd } }

#define NS_EDITINGCONTROLLER_CID \
{ 0x2c5a5cdd, 0xe742, 0x4dfe, \
  { 0x86, 0xb8, 0x06, 0x93, 0x09, 0xbf, 0x6c, 0x91 } }

class nsIControllerCommandTable;

namespace mozilla {

// the editor controller is used for both text widgets, and basic text editing
// commands in composer. The refCon that gets passed to its commands is an nsIEditor.

class EditorController final
{
public:
  static nsresult RegisterEditorCommands(
                    nsIControllerCommandTable* aCommandTable);
  static nsresult RegisterEditingCommands(
                    nsIControllerCommandTable* aCommandTable);
};

} // namespace mozilla

#endif // #ifndef mozilla_EditorController_h
