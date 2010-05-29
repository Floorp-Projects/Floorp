#include "Layers.h"
#include "LayerManagerOGL.h"
#include "BasicLayers.h"
#include "ImageLayers.h"
#if defined(XP_WIN) && !defined(WINCE)
#include "LayerManagerD3D9.h"
#endif

using namespace mozilla;
using namespace layers;

void XXXNeverCalled_Layers()
{
  BasicLayerManager(nsnull);
  LayerManagerOGL(nsnull);
#if defined(XP_WIN) && !defined(WINCE)
  LayerManagerD3D9(nsnull);
#endif
}
