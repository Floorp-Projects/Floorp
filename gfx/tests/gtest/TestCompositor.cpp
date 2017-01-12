/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"
#include "TestLayers.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
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

  virtual LayoutDeviceIntRect GetClientBounds() override {
    return LayoutDeviceIntRect(0, 0, gCompWidth, gCompHeight);
  }
  virtual LayoutDeviceIntRect GetBounds() override {
    return GetClientBounds();
  }

  void* GetNativeData(uint32_t aDataType) override {
    if (aDataType == NS_NATIVE_OPENGL_CONTEXT) {
      mozilla::gl::SurfaceCaps caps = mozilla::gl::SurfaceCaps::ForRGB();
      caps.preserve = false;
      caps.bpp16 = false;
      nsCString discardFailureId;
      RefPtr<GLContext> context = GLContextProvider::CreateOffscreen(
        IntSize(gCompWidth, gCompHeight), caps,
        CreateContextFlags::REQUIRE_COMPAT_PROFILE,
        &discardFailureId);
      return context.forget().take();
    }
    return nullptr;
  }

  virtual nsresult        Create(nsIWidget* aParent,
                                 nsNativeWidget aNativeParent,
                                 const LayoutDeviceIntRect& aRect,
                                 nsWidgetInitData* aInitData = nullptr) override { return NS_OK; }
  virtual nsresult        Create(nsIWidget* aParent,
                                 nsNativeWidget aNativeParent,
                                 const DesktopIntRect& aRect,
                                 nsWidgetInitData* aInitData = nullptr) override { return NS_OK; }
  virtual void            Show(bool aState) override {}
  virtual bool            IsVisible() const override { return true; }
  virtual void            Move(double aX, double aY) override {}
  virtual void            Resize(double aWidth, double aHeight, bool aRepaint) override {}
  virtual void            Resize(double aX, double aY,
                                 double aWidth, double aHeight, bool aRepaint) override {}

  virtual void            Enable(bool aState) override {}
  virtual bool            IsEnabled() const override { return true; }
  virtual nsresult        SetFocus(bool aRaise) override { return NS_OK; }
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override { return NS_OK; }
  virtual void            Invalidate(const LayoutDeviceIntRect& aRect) override {}
  virtual nsresult        SetTitle(const nsAString& title) override { return NS_OK; }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override { return LayoutDeviceIntPoint(0, 0); }
  virtual nsresult        DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                        nsEventStatus& aStatus) override { return NS_OK; }
  virtual void            SetInputContext(const InputContext& aContext,
                                          const InputContextAction& aAction) override {}
  virtual InputContext    GetInputContext() override { abort(); }

private:
  ~MockWidget() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(MockWidget, nsBaseWidget)

struct LayerManagerData {
  RefPtr<MockWidget> mWidget;
  RefPtr<Compositor> mCompositor;
  RefPtr<widget::CompositorWidget> mCompositorWidget;
  RefPtr<LayerManagerComposite> mLayerManager;

  LayerManagerData(Compositor* compositor,
                   MockWidget* widget,
                   widget::CompositorWidget* aWidget,
                   LayerManagerComposite* layerManager)
    : mWidget(widget)
    , mCompositor(compositor)
    , mCompositorWidget(aWidget)
    , mLayerManager(layerManager)
  {}
};

static already_AddRefed<Compositor> CreateTestCompositor(LayersBackend backend, widget::CompositorWidget* widget)
{
  gfxPrefs::GetSingleton();

  RefPtr<Compositor> compositor;

  if (backend == LayersBackend::LAYERS_OPENGL) {
    compositor = new CompositorOGL(nullptr,
                                   widget,
                                   gCompWidth,
                                   gCompHeight,
                                   true);
    compositor->SetDestinationSurfaceSize(IntSize(gCompWidth, gCompHeight));
  } else if (backend == LayersBackend::LAYERS_BASIC) {
    compositor = new BasicCompositor(nullptr, widget);
#ifdef XP_WIN
  } else if (backend == LayersBackend::LAYERS_D3D11) {
    //compositor = new CompositorD3D11();
    MOZ_CRASH(); // No support yet
  } else if (backend == LayersBackend::LAYERS_D3D9) {
    //compositor = new CompositorD3D9(this, mWidget);
    MOZ_CRASH(); // No support yet
#endif
  }
  nsCString failureReason;
  if (!compositor || !compositor->Initialize(&failureReason)) {
    printf_stderr("Failed to construct layer manager for the requested backend\n");
    abort();
  }

  return compositor.forget();
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
    CompositorOptions options;
    RefPtr<widget::CompositorWidget> proxy = new widget::InProcessCompositorWidget(options, widget);
    RefPtr<Compositor> compositor = CreateTestCompositor(backend, proxy);

    RefPtr<LayerManagerComposite> layerManager = new LayerManagerComposite(compositor);

    managers.push_back(LayerManagerData(compositor, widget, proxy, layerManager));
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

static already_AddRefed<DrawTarget> CreateDT()
{
  return gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
    IntSize(gCompWidth, gCompHeight), SurfaceFormat::B8G8R8A8);
}

static bool CompositeAndCompare(RefPtr<LayerManagerComposite> layerManager, DrawTarget* refDT)
{
  RefPtr<DrawTarget> drawTarget = CreateDT();

  layerManager->BeginTransactionWithDrawTarget(drawTarget, IntRect(0, 0, gCompWidth, gCompHeight));
  layerManager->EndTransaction(TimeStamp::Now());

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

static void CompositorSimpleTree() {
  const int benchmarkRepeatCount = 30;

  RefPtr<DrawTarget> refDT = CreateDT();
  refDT->FillRect(Rect(0, 0, gCompWidth, gCompHeight), ColorPattern(Color(1.f, 0.f, 1.f, 1.f)));
  refDT->FillRect(Rect(0, 0, 100, 100), ColorPattern(Color(1.f, 0.f, 0.f, 1.f)));
  refDT->FillRect(Rect(0, 50, 100, 100), ColorPattern(Color(0.f, 0.f, 1.f, 1.f)));

  auto layerManagers = GetLayerManagers(GetPlatformBackends());
  for (size_t i = 0; i < layerManagers.size(); i++) {
    // Benchmark n composites
    for (size_t n = 0; n < benchmarkRepeatCount; n++) {
      RefPtr<LayerManagerComposite> layerManager = layerManagers[i].mLayerManager;
      RefPtr<LayerManager> lmBase = layerManager.get();
      nsTArray<RefPtr<Layer>> layers;
      nsIntRegion layerVisibleRegion[] = {
        nsIntRegion(IntRect(0, 0, gCompWidth, gCompHeight)),
        nsIntRegion(IntRect(0, 0, gCompWidth, gCompHeight)),
        nsIntRegion(IntRect(0, 0, 100, 100)),
        nsIntRegion(IntRect(0, 50, 100, 100)),
      };
      RefPtr<Layer> root = CreateLayerTree("c(ooo)", layerVisibleRegion, nullptr, lmBase, layers);

      { // background
        ColorLayer* colorLayer = layers[1]->AsColorLayer();
        colorLayer->SetColor(Color(1.f, 0.f, 1.f, 1.f));
        colorLayer->SetBounds(colorLayer->GetVisibleRegion().ToUnknownRegion().GetBounds());
      }

      {
        ColorLayer* colorLayer = layers[2]->AsColorLayer();
        colorLayer->SetColor(Color(1.f, 0.f, 0.f, 1.f));
        colorLayer->SetBounds(colorLayer->GetVisibleRegion().ToUnknownRegion().GetBounds());
      }

      {
        ColorLayer* colorLayer = layers[3]->AsColorLayer();
        colorLayer->SetColor(Color(0.f, 0.f, 1.f, 1.f));
        colorLayer->SetBounds(colorLayer->GetVisibleRegion().ToUnknownRegion().GetBounds());
      }

      EXPECT_TRUE(CompositeAndCompare(layerManager, refDT));
    }
  }
};

MOZ_GTEST_BENCH(GfxBench, CompositorSimpleTree, &CompositorSimpleTree);

