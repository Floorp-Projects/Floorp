/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
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

#include "CompositorChild.h"
#include "CompositorParent.h"
#include "mozilla/layers/ShadowLayersChild.h"
#include "base/thread.h"

using mozilla::layers::ShadowLayersChild;

namespace mozilla {
namespace layers {

CompositorChild::CompositorChild(Thread *aCompositorThread)
  : mCompositorThread(aCompositorThread)
{

  MOZ_COUNT_CTOR(CompositorChild);
}

CompositorChild::~CompositorChild()
{
  MOZ_COUNT_DTOR(CompositorChild);
}

CompositorChild*
CompositorChild::CreateCompositor()
{
  Thread* compositorThread = new Thread("CompositorThread");
  if (compositorThread->Start()) {
    MessageLoop *parentMessageLoop = MessageLoop::current();
    MessageLoop *childMessageLoop = compositorThread->message_loop();
    CompositorParent *compositorParent = new CompositorParent();
    CompositorChild *compositorChild = new CompositorChild(compositorThread);
    mozilla::ipc::AsyncChannel *parentChannel =
      compositorParent->GetIPCChannel();
    mozilla::ipc::AsyncChannel *childChannel =
      compositorChild->GetIPCChannel();
    mozilla::ipc::AsyncChannel::Side childSide =
      mozilla::ipc::AsyncChannel::Child;

    compositorChild->Open(parentChannel, childMessageLoop, childSide);
    compositorChild->CallInit();
    return compositorChild;
  }

  return NULL;
}

PLayersChild*
CompositorChild::AllocPLayers(const LayersBackend &backend, const WidgetDescriptor &widget)
{
  return new ShadowLayersChild();
}

bool
CompositorChild::DeallocPLayers(PLayersChild* actor)
{
  delete actor;
  return true;
}

} // namespace layers
} // namespace mozilla

