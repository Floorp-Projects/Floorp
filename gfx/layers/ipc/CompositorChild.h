/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorChild_h
#define mozilla_layers_CompositorChild_h

#include "mozilla/layers/PCompositorChild.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layers {

class LayerManager;
class CompositorParent;
struct TextureFactoryIdentifier;

class CompositorChild : public PCompositorChild
{
  NS_INLINE_DECL_REFCOUNTING(CompositorChild)
public:
  CompositorChild(LayerManager *aLayerManager);
  virtual ~CompositorChild();

  void Destroy();

  /**
   * We're asked to create a new Compositor in response to an Opens()
   * or Bridge() request from our parent process.  The Transport is to
   * the compositor's context.
   */
  static PCompositorChild*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  static PCompositorChild* Get();

  static bool ChildProcessHasCompositor() { return sCompositor != nullptr; }
protected:
  virtual PLayerTransactionChild*
    AllocPLayerTransaction(const LayersBackend& aBackendHint,
                           const uint64_t& aId,
                           TextureFactoryIdentifier* aTextureFactoryIdentifier) MOZ_OVERRIDE;

  virtual bool DeallocPLayerTransaction(PLayerTransactionChild *aChild) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  nsRefPtr<LayerManager> mLayerManager;
  nsCOMPtr<nsIObserver> mMemoryPressureObserver;

  // When we're in a child process, this is the process-global
  // compositor that we use to forward transactions directly to the
  // compositor context in another process.
  static CompositorChild* sCompositor;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorChild);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorChild_h
