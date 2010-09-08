/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#include "ImageLayers.h"
#include "Layers.h"
#include "gfxPlatform.h"
 
#ifdef MOZ_LAYERS_HAVE_LOG
FILE*
FILEOrDefault(FILE* aFile)
{
  return aFile ? aFile : stderr;
}
#endif // MOZ_LAYERS_HAVE_LOG

namespace {

// XXX pretty general utilities, could centralize
 
nsACString&
AppendToString(nsACString& s, const gfxPattern::GraphicsFilter& f,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  switch (f) {
  case gfxPattern::FILTER_FAST:      s += "fast"; break;
  case gfxPattern::FILTER_GOOD:      s += "good"; break;
  case gfxPattern::FILTER_BEST:      s += "best"; break;
  case gfxPattern::FILTER_NEAREST:   s += "nearest"; break;
  case gfxPattern::FILTER_BILINEAR:  s += "bilinear"; break;
  case gfxPattern::FILTER_GAUSSIAN:  s += "gaussian"; break;
  default:
    NS_ERROR("unknown filter type");
    s += "???";
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfxRGBA& c,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    128, "rgba(%d, %d, %d, %g)",
    PRUint8(c.r*255.0), PRUint8(c.g*255.0), PRUint8(c.b*255.0), c.a);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfx3DMatrix& m,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  if (m.IsIdentity())
    s += "[ I ]";
  else {
    gfxMatrix matrix;
    if (m.Is2D(&matrix)) {
      s += nsPrintfCString(
        96, "[ %g %g; %g %g; %g %g; ]",
        matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0);
    } else {
      s += nsPrintfCString(
        256, "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
        m._11, m._12, m._13, m._14,
        m._21, m._22, m._23, m._24,
        m._31, m._32, m._33, m._34,
        m._41, m._42, m._43, m._44);
    }
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    256, "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;

  nsIntRegionRectIterator it(r);
  s += "< ";
  while (const nsIntRect* sr = it.Next())
    AppendToString(s, *sr) += "; ";
  s += ">";

  return s += sfx;
}

} // namespace <anon>

namespace mozilla {
namespace layers {

//--------------------------------------------------
// LayerManager
already_AddRefed<gfxASurface>
LayerManager::CreateOptimalSurface(const gfxIntSize &aSize,
                                   gfxASurface::gfxImageFormat aFormat)
{
  return gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, aFormat);
}

//--------------------------------------------------
// Layer

PRBool
Layer::CanUseOpaqueSurface()
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (GetContentFlags() & CONTENT_OPAQUE)
    return PR_TRUE;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  ContainerLayer* parent = GetParent();
  return parent && parent->GetFirstChild() == this &&
    parent->CanUseOpaqueSurface();
}

#ifdef MOZ_LAYERS_HAVE_LOG

void
Layer::Dump(FILE* aFile, const char* aPrefix)
{
  DumpSelf(aFile, aPrefix);

  if (Layer* kid = GetFirstChild()) {
    nsCAutoString pfx(aPrefix);
    pfx += "  ";
    kid->Dump(aFile, pfx.get());
  }

  if (Layer* next = GetNextSibling())
    next->Dump(aFile, aPrefix);
}

void
Layer::DumpSelf(FILE* aFile, const char* aPrefix)
{
  nsCAutoString str;
  PrintInfo(str, aPrefix);
  fprintf(FILEOrDefault(aFile), "%s\n", str.get());
}

void
Layer::Log(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  LogSelf(aPrefix);

  if (Layer* kid = GetFirstChild()) {
    nsCAutoString pfx(aPrefix);
    pfx += "  ";
    kid->Log(pfx.get());
  }

  if (Layer* next = GetNextSibling())
    next->Log(aPrefix);
}

void
Layer::LogSelf(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  nsCAutoString str;
  PrintInfo(str, aPrefix);
  MOZ_LAYERS_LOG(("%s", str.get()));
}

nsACString&
Layer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString(64, "%s%s (0x%p)", mManager->Name(), Name(), this);

  if (mUseClipRect) {
    AppendToString(aTo, mClipRect, " [clip=", "]");
  }
  if (!mTransform.IsIdentity()) {
    AppendToString(aTo, mTransform, " [transform=", "]");
  }
  if (!mVisibleRegion.IsEmpty()) {
    AppendToString(aTo, mVisibleRegion, " [visible=", "]");
  }
  if (1.0 != mOpacity) {
    aTo.AppendPrintf(" [opacity=%g]", mOpacity);
  }
  if (GetContentFlags() & CONTENT_OPAQUE) {
    aTo += " [opaqueContent]";
  }
  if (GetContentFlags() & CONTENT_NO_TEXT) {
    aTo += " [noText]";
  }
  if (GetContentFlags() & CONTENT_NO_TEXT_OVER_TRANSPARENT) {
    aTo += " [noTextOverTransparent]";
  }

  return aTo;
}

nsACString&
ThebesLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  return mValidRegion.IsEmpty() ?
    aTo : AppendToString(aTo, mValidRegion, " [valid=", "]");
}

nsACString&
ColorLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  AppendToString(aTo, mColor, " [color=", "]");
  return aTo;
}

nsACString&
CanvasLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (mFilter != gfxPattern::FILTER_GOOD) {
    AppendToString(aTo, mFilter, " [filter=", "]");
  }
  return aTo;
}

nsACString&
ImageLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (mFilter != gfxPattern::FILTER_GOOD) {
    AppendToString(aTo, mFilter, " [filter=", "]");
  }
  return aTo;
}

//--------------------------------------------------
// LayerManager

void
LayerManager::Dump(FILE* aFile, const char* aPrefix)
{
  FILE* file = FILEOrDefault(aFile);

  DumpSelf(file, aPrefix);

  nsCAutoString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    fprintf(file, "%s(null)", pfx.get());
    return;
  }

  GetRoot()->Dump(file, pfx.get());
}

void
LayerManager::DumpSelf(FILE* aFile, const char* aPrefix)
{
  nsCAutoString str;
  PrintInfo(str, aPrefix);
  fprintf(FILEOrDefault(aFile), "%s\n", str.get());
}

void
LayerManager::Log(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  LogSelf(aPrefix);

  nsCAutoString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    MOZ_LAYERS_LOG(("%s(null)", pfx.get()));
    return;
  }

  GetRoot()->Log(pfx.get());
}

void
LayerManager::LogSelf(const char* aPrefix)
{
  nsCAutoString str;
  PrintInfo(str, aPrefix);
  MOZ_LAYERS_LOG(("%s", str.get()));
}

nsACString&
LayerManager::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  return aTo += nsPrintfCString(64, "%sLayerManager (0x%p)", Name(), this);
}

/*static*/ void
LayerManager::InitLog()
{
  if (!sLog)
    sLog = PR_NewLogModule("Layers");
}

/*static*/ bool
LayerManager::IsLogEnabled()
{
  NS_ABORT_IF_FALSE(!!sLog,
                    "layer manager must be created before logging is allowed");
  return PR_LOG_TEST(sLog, PR_LOG_DEBUG);
}

#else  // !MOZ_LAYERS_HAVE_LOG

void Layer::Dump(FILE* aFile, const char* aPrefix) {}
void Layer::DumpSelf(FILE* aFile, const char* aPrefix) {}
void Layer::Log(const char* aPrefix) {}
void Layer::LogSelf(const char* aPrefix) {}
nsACString&
Layer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ThebesLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ColorLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
CanvasLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ImageLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

void LayerManager::Dump(FILE* aFile, const char* aPrefix) {}
void LayerManager::DumpSelf(FILE* aFile, const char* aPrefix) {}
void LayerManager::Log(const char* aPrefix) {}
void LayerManager::LogSelf(const char* aPrefix) {}

nsACString&
LayerManager::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

/*static*/ void LayerManager::InitLog() {}
/*static*/ bool LayerManager::IsLogEnabled() { return false; }

#endif // MOZ_LAYERS_HAVE_LOG

PRLogModuleInfo* LayerManager::sLog;

} // namespace layers 
} // namespace mozilla
