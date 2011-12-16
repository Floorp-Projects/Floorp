/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsStructuredCloneContainer_h__
#define nsStructuredCloneContainer_h__

#include "nsIStructuredCloneContainer.h"
#include "jsapi.h"

#define NS_STRUCTUREDCLONECONTAINER_CLASSNAME "nsStructuredCloneContainer"
#define NS_STRUCTUREDCLONECONTAINER_CONTRACTID \
  "@mozilla.org/docshell/structured-clone-container;1"
#define NS_STRUCTUREDCLONECONTAINER_CID \
{ /* 38bd0634-0fd4-46f0-b85f-13ced889eeec */       \
  0x38bd0634,                                      \
  0x0fd4,                                          \
  0x46f0,                                          \
  {0xb8, 0x5f, 0x13, 0xce, 0xd8, 0x89, 0xee, 0xec} \
}

class nsStructuredCloneContainer : public nsIStructuredCloneContainer
{
  public:
    nsStructuredCloneContainer();
    ~nsStructuredCloneContainer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTRUCTUREDCLONECONTAINER

  private:
    uint64_t* mData;

    // This needs to be size_t rather than a PR-type so it matches the JS API.
    size_t mSize;
    uint32_t mVersion;
};

#endif
