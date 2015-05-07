/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gfxUtils.h"
#include "gtest/gtest.h"
#include "TestLayers.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "nsBaseWidget.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include <vector>

const int gCompWidth = 256;
const int gCompHeight = 256;

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gl;

class MockWidget : public nsBaseWidget
{
public:
  MockWidget() {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD              GetClientBounds(IntRect &aRect) override {
    aRect = IntRect(0, 0, gCompWidth, gCompHeight);
    return NS_OK;
  }
  NS_IMETHOD              GetBounds(IntRect &aRect) override { return GetClientBounds(aRect); }

  void* GetNativeData(uint32_t aDataType) override {
    if (aDataType == NS_NATIVE_OPENGL_CONTEXT) {
      mozilla::gl::SurfaceCaps caps = mozilla::gl::SurfaceCaps::ForRGB();
      caps.preserve = false;
      caps.bpp16 = false;
      nsRefPtr<GLContext> context = GLContextProvider::CreateOffscreen(
        gfxIntSize(gCompWidth, gCompHeight), caps, true);
      return context.forget().take();
    }
    return nullptr;
  }

  NS_IMETHOD              Create(nsIWidget *aParent,
                                 nsNativeWidget aNativeParent,
                                 const IntRect &aRect,
                                 nsWidgetInitData *aInitData = nullptr) override { return NS_OK; }
  NS_IMETHOD              Show(bool aState) override { return NS_OK; }
  virtual bool            IsVisible() const override { return true; }
  NS_IMETHOD              ConstrainPosition(bool aAllowSlop,
                                            int32_t *aX, int32_t *aY) override { return NS_OK; }
  NS_IMETHOD              Move(double aX, double aY) override { return NS_OK; }
  NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint) override { return NS_OK; }
  NS_IMETHOD              Resize(double aX, double aY,
                                 double aWidth, double aHeight, bool aRepaint) override { return NS_OK; }

  NS_IMETHOD              Enable(bool aState) override { return NS_OK; }
  virtual bool            IsEnabled() const override { return true; }
  NS_IMETHOD              SetFocus(bool aRaise) override { return NS_OK; }
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override { return NS_OK; }
  NS_IMETHOD              Invalidate(const IntRect &aRect) override { return NS_OK; }
  NS_IMETHOD              SetTitle(const nsAString& title) override { return NS_OK; }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override { return LayoutDeviceIntPoint(0, 0); }
  NS_IMETHOD              DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                        nsEventStatus& aStatus) override { return NS_OK; }
  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener * aListener, bool aDoCapture) override { return NS_OK; }
  NS_IMETHOD_(void)       SetInputContext(const InputContext& aContext,
                                          const InputContextAction& aAction) override {}
  NS_IMETHOD_(InputContext) GetInputContext() override { abort(); }
  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent) override { return NS_OK; }
private:
  ~MockWidget() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(MockWidget, nsBaseWidget)

struct LayerManagerData {
  RefPtr<MockWidget> mWidget;
  RefPtr<Compositor> mCompositor;
  nsRefPtr<LayerManagerComposite> mLayerManager;

  LayerManagerData(Compositor* compositor, MockWidget* widget, LayerManagerComposite* layerManager)
    : mWidget(widget)
    , mCompositor(compositor)
    , mLayerManager(layerManager)
  {}
};

static TemporaryRef<Compositor> CreateTestCompositor(LayersBackend backend, MockWidget* widget)
{
  gfxPrefs::GetSingleton();

  RefPtr<Compositor> compositor;

  if (backend == LayersBackend::LAYERS_OPENGL) {
    compositor = new CompositorOGL(widget,
                                   gCompWidth,
                                   gCompHeight,
                                   true);
    compositor->SetDestinationSurfaceSize(IntSize(gCompWidth, gCompHeight));
  } else if (backend == LayersBackend::LAYERS_BASIC) {
    compositor = new BasicCompositor(widget);
#ifdef XP_WIN
  } else if (backend == LayersBackend::LAYERS_D3D11) {
    //compositor = new CompositorD3D11();
    MOZ_CRASH(); // No support yet
  } else if (backend == LayersBackend::LAYERS_D3D9) {
    //compositor = new CompositorD3D9(this, mWidget);
    MOZ_CRASH(); // No support yet
#endif
  }

  if (!compositor) {
    printf_stderr("Failed to construct layer manager for the requested backend\n");
    abort();
  }

  return compositor;
}

/**
 * Get a list of layers managers for the platform to run the test on.
 */
static std::vector<LayerManagerData> GetLayerManagers(std::vector<LayersBackend> aBackends)
{
  std::vector<LayerManagerData> managers;

  for (size_t i = 0; i < aBackends.size(); i++) {
    auto backend = aBackends[i];

    RefPtr<MockWidget> widget = new MockWidget();
    RefPtr<Compositor> compositor = CreateTestCompositor(backend, widget);

    nsRefPtr<LayerManagerComposite> layerManager = new LayerManagerComposite(compositor);

    layerManager->Initialize();

    managers.push_back(LayerManagerData(compositor, widget, layerManager));
  }

  return managers;
}

/**
 * This will return the default list of backends that
 * units test should run against.
 */
static std::vector<LayersBackend> GetPlatformBackends()
{
  std::vector<LayersBackend> backends;

  // For now we only support Basic for gtest
  backends.push_back(LayersBackend::LAYERS_BASIC);

#ifdef XP_MACOSX
  backends.push_back(LayersBackend::LAYERS_OPENGL);
#endif

  // TODO Support OGL/D3D backends with unit test
  return backends;
}

static TemporaryRef<DrawTarget> CreateDT()
{
  RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
    IntSize(gCompWidth, gCompHeight), SurfaceFormat::B8G8R8A8);

  return dt;
}

static bool CompositeAndCompare(nsRefPtr<LayerManagerComposite> layerManager, DrawTarget* refDT)
{
  RefPtr<DrawTarget> drawTarget = CreateDT();

  layerManager->BeginTransactionWithDrawTarget(drawTarget, IntRect(0, 0, gCompWidth, gCompHeight));
  layerManager->EndEmptyTransaction();

  RefPtr<SourceSurface> ss = drawTarget->Snapshot();
  RefPtr<DataSourceSurface> dss = ss->GetDataSurface();
  uint8_t* bitmap = dss->GetData();

  RefPtr<SourceSurface> ssRef = refDT->Snapshot();
  RefPtr<DataSourceSurface> dssRef = ssRef->GetDataSurface();
  uint8_t* bitmapRef = dssRef->GetData();

  for (int y = 0; y < gCompHeight; y++) {
    for (int x = 0; x < gCompWidth; x++) {
      for (size_t channel = 0; channel < 4; channel++) {
        uint8_t bit = bitmap[y * dss->Stride() + x * 4 + channel];
        uint8_t bitRef = bitmapRef[y * dss->Stride() + x * 4 + channel];
        if (bit != bitRef) {
          printf("Layer Tree:\n");
          layerManager->Dump();
          printf("Original:\n");
          gfxUtils::DumpAsDataURI(drawTarget);
          printf("\n\n");

          printf("Reference:\n");
          gfxUtils::DumpAsDataURI(refDT);
          printf("\n\n");

          return false;
        }
      }
    }
  }

  return true;
}

TEST(Gfx, CompositorConstruct)
{
  auto layerManagers = GetLayerManagers(GetPlatformBackends());
}

TEST(Gfx, CompositorSimpleTree)
{
  auto layerManagers = GetLayerManagers(GetPlatformBackends());
  for (size_t i = 0; i < layerManagers.size(); i++) {
    nsRefPtr<LayerManagerComposite> layerManager = layerManagers[i].mLayerManager;
    nsRefPtr<LayerManager> lmBase = layerManager.get();
    nsTArray<nsRefPtr<Layer>> layers;
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(IntRect(0, 0, gCompWidth, gCompHeight)),
      nsIntRegion(IntRect(0, 0, gCompWidth, gCompHeight)),
      nsIntRegion(IntRect(0, 0, 100, 100)),
      nsIntRegion(IntRect(0, 50, 100, 100)),
    };
    nsRefPtr<Layer> root = CreateLayerTree("c(ooo)", layerVisibleRegion, nullptr, lmBase, layers);

    { // background
      ColorLayer* colorLayer = layers[1]->AsColorLayer();
      colorLayer->SetColor(gfxRGBA(1.f, 0.f, 1.f, 1.f));
      colorLayer->SetBounds(colorLayer->GetVisibleRegion().GetBounds());
    }

    {
      ColorLayer* colorLayer = layers[2]->AsColorLayer();
      colorLayer->SetColor(gfxRGBA(1.f, 0.f, 0.f, 1.f));
      colorLayer->SetBounds(colorLayer->GetVisibleRegion().GetBounds());
    }

    {
      ColorLayer* colorLayer = layers[3]->AsColorLayer();
      colorLayer->SetColor(gfxRGBA(0.f, 0.f, 1.f, 1.f));
      colorLayer->SetBounds(colorLayer->GetVisibleRegion().GetBounds());
    }

    RefPtr<DrawTarget> refDT = CreateDT();
    refDT->FillRect(Rect(0, 0, gCompWidth, gCompHeight), ColorPattern(Color(1.f, 0.f, 1.f, 1.f)));
    refDT->FillRect(Rect(0, 0, 100, 100), ColorPattern(Color(1.f, 0.f, 0.f, 1.f)));
    refDT->FillRect(Rect(0, 50, 100, 100), ColorPattern(Color(0.f, 0.f, 1.f, 1.f)));
    EXPECT_TRUE(CompositeAndCompare(layerManager, refDT));
  }
}

