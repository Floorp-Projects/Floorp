/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsWalletPreview.h"
#include "nsSignonViewer.h"
#include "nsWalletEditor.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(WalletPreviewImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(SignonViewerImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(WalletEditorImpl)

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "WalletPreview World Component", NS_WALLETPREVIEW_CID,
      "@mozilla.org/walletpreview/walletpreview-world;1", WalletPreviewImplConstructor },
    { "SignonViewer World Component", NS_SIGNONVIEWER_CID,
      "@mozilla.org/signonviewer/signonviewer-world;1", SignonViewerImplConstructor },
    { "WalletEditor World Component", NS_WALLETEDITOR_CID,
      "@mozilla.org/walleteditor/walleteditor-world;1", WalletEditorImplConstructor },
};

NS_IMPL_NSGETMODULE(nsWalletViewerModule, components)

