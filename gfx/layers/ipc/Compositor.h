/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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

#ifndef mozilla_layers_Compositor_h
#define mozilla_layers_Compositor_h

// Note we can't include IPDL generated headers here

#include "Layers.h"
#include "nsDebug.h"

namespace mozilla {
namespace layers {
namespace compositor {

// Layer user data to store the native content
// on the shadow layer manager since cocoa widgets
// expects to find the native context from the layer manager.
extern int sShadowNativeContext;

class ShadowNativeContextUserData : public LayerUserData
{
public:
  ShadowNativeContextUserData(void *aNativeContext)
    : mNativeContext(aNativeContext)
  {
    MOZ_COUNT_CTOR(ShadowNativeContextUserData);
  }
  ~ShadowNativeContextUserData()
  {
    MOZ_COUNT_DTOR(ShadowNativeContextUserData);
  }

  // This native context is shared with the compositor.
  // The user is responsible for locking usage of this context
  // with the compositor. Note that using this context blocks
  // the compositor and must be avoided.
  void* GetNativeContext() {
    return mNativeContext;
  }
private:
  void *mNativeContext;
};


}
}
}
#endif
