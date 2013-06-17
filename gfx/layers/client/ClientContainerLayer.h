#include "ClientLayerManager.h"
#include "gfxPlatform.h"

using namespace mozilla::layers;

template<class Container> void
ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == aContainer->Manager() &&
                aAfter->GetParent() == aContainer),
               "aAfter is not our child");

  aChild->SetParent(aContainer);
  if (aAfter == aContainer->mLastChild) {
    aContainer->mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(aContainer->mFirstChild);
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(aChild);
    }
    aContainer->mFirstChild = aChild;
    NS_ADDREF(aChild);
    aContainer->DidInsertChild(aChild);
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  aContainer->DidInsertChild(aChild);
}

template<class Container> void
ContainerRemoveChild(Layer* aChild, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == aContainer,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    aContainer->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    aContainer->mLastChild = prev;
  }

  aChild->SetNextSibling(nullptr);
  aChild->SetPrevSibling(nullptr);
  aChild->SetParent(nullptr);

  aContainer->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
}

template<class Container> void
ContainerRepositionChild(Layer* aChild, Layer* aAfter, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == aContainer,
               "aChild not our child");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == aContainer->Manager() &&
                aAfter->GetParent() == aContainer),
               "aAfter is not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev == aAfter) {
    // aChild is already in the correct position, nothing to do.
    return;
  }
  if (prev) {
    prev->SetNextSibling(next);
  }
  if (next) {
    next->SetPrevSibling(prev);
  }
  if (!aAfter) {
    aChild->SetPrevSibling(nullptr);
    aChild->SetNextSibling(aContainer->mFirstChild);
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(aChild);
    }
    aContainer->mFirstChild = aChild;
    return;
  }

  Layer* afterNext = aAfter->GetNextSibling();
  if (afterNext) {
    afterNext->SetPrevSibling(aChild);
  } else {
    aContainer->mLastChild = aChild;
  }
  aAfter->SetNextSibling(aChild);
  aChild->SetPrevSibling(aAfter);
  aChild->SetNextSibling(afterNext);
}

static bool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
  }
  return false;
}

class ClientContainerLayer : public ContainerLayer,
                             public ClientLayer
{
  template<class Container>
  friend void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
  template<class Container>
  friend void ContainerRemoveChild(Layer* aChild, Container* aContainer);
  template<class Container>
  friend void ContainerRepositionChild(Layer* aChild, Layer* aAfter, Container* aContainer);

public:
  ClientContainerLayer(ClientLayerManager* aManager) :
    ContainerLayer(aManager, static_cast<ClientLayer*>(this))
  {
    MOZ_COUNT_CTOR(ClientContainerLayer);
    mSupportsComponentAlphaChildren = true;
  }
  virtual ~ClientContainerLayer()
  {
    while (mFirstChild) {
      ContainerRemoveChild(mFirstChild, this);
    }

    MOZ_COUNT_DTOR(ClientContainerLayer);
  }

  virtual void RenderLayer()
  {
    if (GetMaskLayer()) {
      ToClientLayer(GetMaskLayer())->RenderLayer();
    }
    
    // Setup mSupportsComponentAlphaChildren in the same way 
    // that ContainerLayerComposite will do.
    if (UseIntermediateSurface()) {
      if (GetEffectiveVisibleRegion().GetNumRects() != 1 ||
          !(GetContentFlags() & Layer::CONTENT_OPAQUE))
      {
        const gfx3DMatrix& transform3D = GetEffectiveTransform();
        gfxMatrix transform;
        if (HasOpaqueAncestorLayer(this) &&
            transform3D.Is2D(&transform) && 
            !transform.HasNonIntegerTranslation()) {
          SetSupportsComponentAlphaChildren(
            gfxPlatform::GetPlatform()->UsesSubpixelAATextRendering());
        }
      }
    } else {
      SetSupportsComponentAlphaChildren(
        (GetContentFlags() & Layer::CONTENT_OPAQUE) ||
        (GetParent() && GetParent()->SupportsComponentAlphaChildren()));
    }

    nsAutoTArray<Layer*, 12> children;
    SortChildrenBy3DZOrder(children);

    for (uint32_t i = 0; i < children.Length(); i++) {
      if (children.ElementAt(i)->GetEffectiveVisibleRegion().IsEmpty()) {
        continue;
      }

      ToClientLayer(children.ElementAt(i))->RenderLayer();
    }
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerLayer::SetVisibleRegion(aRegion);
  }
  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->InsertAfter(ClientManager()->Hold(this),
                                 ClientManager()->Hold(aChild),
                                 aAfter ? ClientManager()->Hold(aAfter) : nullptr);
    ContainerInsertAfter(aChild, aAfter, this);
  }

  virtual void RemoveChild(Layer* aChild)
  { 
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->RemoveChild(ClientManager()->Hold(this),
                                 ClientManager()->Hold(aChild));
    ContainerRemoveChild(aChild, this);
  }

  virtual void RepositionChild(Layer* aChild, Layer* aAfter)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->RepositionChild(ClientManager()->Hold(this),
                                     ClientManager()->Hold(aChild),
                                     aAfter ? ClientManager()->Hold(aAfter) : nullptr);
    ContainerRepositionChild(aChild, aAfter, this);
  }
  
  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  void ForceIntermediateSurface() { mUseIntermediateSurface = true; }

  void SetSupportsComponentAlphaChildren(bool aSupports) { mSupportsComponentAlphaChildren = aSupports; }

protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
};

class ClientRefLayer : public RefLayer,
                       public ClientLayer {
public:
  ClientRefLayer(ClientLayerManager* aManager) :
    RefLayer(aManager, static_cast<ClientLayer*>(this))
  {
    MOZ_COUNT_CTOR(ClientRefLayer);
  }
  virtual ~ClientRefLayer()
  {
    MOZ_COUNT_DTOR(ClientRefLayer);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    ClientLayer::Disconnect();
  }

  virtual void RenderLayer() { }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

private:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
};
