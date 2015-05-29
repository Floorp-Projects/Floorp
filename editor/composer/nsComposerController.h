/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsComposerController_h__
#define nsComposerController_h__


#include "nscore.h"                     // for nsresult

class nsIControllerCommandTable;


// The plaintext editor controller is used for basic text editing and html editing
// commands in composer
// The refCon that gets passed to its commands is initially nsIEditingSession,
//   and after successfule editor creation it is changed to nsIEditor.
#define NS_EDITORDOCSTATECONTROLLER_CID \
 { 0x50e95301, 0x17a8, 0x11d4, { 0x9f, 0x7e, 0xdd, 0x53, 0x0d, 0x5f, 0x05, 0x7c } }

// The HTMLEditor controller is used only for HTML editors and takes nsIEditor as refCon
#define NS_HTMLEDITORCONTROLLER_CID \
 { 0x62db0002, 0xdbb6, 0x43f4, { 0x8f, 0xb7, 0x9d, 0x25, 0x38, 0xbc, 0x57, 0x47 } }


class nsComposerController
{
public:
  static nsresult RegisterEditorDocStateCommands(nsIControllerCommandTable* inCommandTable);
  static nsresult RegisterHTMLEditorCommands(nsIControllerCommandTable* inCommandTable);
};

#endif /* nsComposerController_h__ */
