// Copyright (c) 2022 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "colr.h"

#include "cpal.h"
#include "maxp.h"
#include "variations.h"

#include <map>
#include <set>
#include <vector>

// COLR - Color Table
// http://www.microsoft.com/typography/otspec/colr.htm

#define TABLE_NAME "COLR"

namespace {

// Some typedefs so that our local variables will more closely parallel the spec.
typedef int16_t F2DOT14;  // 2.14 fixed-point
typedef int32_t Fixed;    // 16.16 fixed-point
typedef int16_t FWORD;    // A 16-bit integer value in font design units
typedef uint16_t UFWORD;
typedef uint32_t VarIdxBase;

constexpr F2DOT14 F2DOT14_one = 0x4000;

struct colrState
{
  // We track addresses of structs that we have already seen and checked,
  // because fonts may share these among multiple glyph descriptions.
  // (We only do this for color lines, which may be large, depending on the
  // number of color stops, and for paints, which may refer to an extensive
  // sub-graph; for small records like ClipBox and Affine2x3, we just read
  // them directly whenever encountered.)
  std::set<const uint8_t*> colorLines;
  std::set<const uint8_t*> varColorLines;
  std::set<const uint8_t*> paints;

  std::map<uint16_t, std::pair<const uint8_t*, size_t>> baseGlyphMap;
  std::vector<std::pair<const uint8_t*, size_t>> layerList;

  // Set of visited records, for cycle detection.
  // We don't actually need to track every paint table here, as most of the
  // paint-offsets that create the graph can only point forwards;
  // only PaintColrLayers and PaintColrGlyph can cause a backward jump
  // and hence a potential cycle.
  std::set<const uint8_t*> visited;

  uint16_t numGlyphs;  // from maxp
  uint16_t numPaletteEntries;  // from CPAL
};

// std::set<T>::contains isn't available until C++20, so we use this instead
// for better compatibility with old compilers.
template <typename T>
bool setContains(const std::set<T>& set, T item)
{
  return set.find(item) != set.end();
}

enum Extend : uint8_t
{
  EXTEND_PAD     = 0,
  EXTEND_REPEAT  = 1,
  EXTEND_REFLECT = 2,
};

bool ParseColorLine(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state, bool var)
{
  auto& set = var ? state.varColorLines : state.colorLines;
  if (setContains(set, data)) {
    return true;
  }
  set.insert(data);

  ots::Buffer subtable(data, length);

  uint8_t extend;
  uint16_t numColorStops;

  if (!subtable.ReadU8(&extend) ||
      !subtable.ReadU16(&numColorStops)) {
    return OTS_FAILURE_MSG("Failed to read [Var]ColorLine");
  }

  if (extend != EXTEND_PAD && extend != EXTEND_REPEAT && extend != EXTEND_REFLECT) {
    OTS_WARNING("Unknown color-line extend mode %u", extend);
  }

  for (auto i = 0u; i < numColorStops; ++i) {
    F2DOT14 stopOffset;
    uint16_t paletteIndex;
    F2DOT14 alpha;
    VarIdxBase varIndexBase;

    if (!subtable.ReadS16(&stopOffset) ||
        !subtable.ReadU16(&paletteIndex) ||
        !subtable.ReadS16(&alpha) ||
        (var && !subtable.ReadU32(&varIndexBase))) {
      return OTS_FAILURE_MSG("Failed to read [Var]ColorStop");
    }

    if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
      return OTS_FAILURE_MSG("Invalid palette index %u in color stop", paletteIndex);
    }

    if (alpha < 0 || alpha > F2DOT14_one) {
      OTS_WARNING("Alpha value outside valid range 0.0 - 1.0");
    }
  }

  return true;
}

// Composition modes
enum CompositeMode : uint8_t
{
  // Porter-Duff modes
  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators
  COMPOSITE_CLEAR          =  0,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_clear
  COMPOSITE_SRC            =  1,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_src
  COMPOSITE_DEST           =  2,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dst
  COMPOSITE_SRC_OVER       =  3,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcover
  COMPOSITE_DEST_OVER      =  4,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstover
  COMPOSITE_SRC_IN         =  5,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcin
  COMPOSITE_DEST_IN        =  6,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstin
  COMPOSITE_SRC_OUT        =  7,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcout
  COMPOSITE_DEST_OUT       =  8,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstout
  COMPOSITE_SRC_ATOP       =  9,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcatop
  COMPOSITE_DEST_ATOP      = 10,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstatop
  COMPOSITE_XOR            = 11,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_xor
  COMPOSITE_PLUS           = 12,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_plus

  // Blend modes
  // https://www.w3.org/TR/compositing-1/#blending
  COMPOSITE_SCREEN         = 13,  // https://www.w3.org/TR/compositing-1/#blendingscreen
  COMPOSITE_OVERLAY        = 14,  // https://www.w3.org/TR/compositing-1/#blendingoverlay
  COMPOSITE_DARKEN         = 15,  // https://www.w3.org/TR/compositing-1/#blendingdarken
  COMPOSITE_LIGHTEN        = 16,  // https://www.w3.org/TR/compositing-1/#blendinglighten
  COMPOSITE_COLOR_DODGE    = 17,  // https://www.w3.org/TR/compositing-1/#blendingcolordodge
  COMPOSITE_COLOR_BURN     = 18,  // https://www.w3.org/TR/compositing-1/#blendingcolorburn
  COMPOSITE_HARD_LIGHT     = 19,  // https://www.w3.org/TR/compositing-1/#blendinghardlight
  COMPOSITE_SOFT_LIGHT     = 20,  // https://www.w3.org/TR/compositing-1/#blendingsoftlight
  COMPOSITE_DIFFERENCE     = 21,  // https://www.w3.org/TR/compositing-1/#blendingdifference
  COMPOSITE_EXCLUSION      = 22,  // https://www.w3.org/TR/compositing-1/#blendingexclusion
  COMPOSITE_MULTIPLY       = 23,  // https://www.w3.org/TR/compositing-1/#blendingmultiply

  // Modes that, uniquely, do not operate on components
  // https://www.w3.org/TR/compositing-1/#blendingnonseparable
  COMPOSITE_HSL_HUE        = 24,  // https://www.w3.org/TR/compositing-1/#blendinghue
  COMPOSITE_HSL_SATURATION = 25,  // https://www.w3.org/TR/compositing-1/#blendingsaturation
  COMPOSITE_HSL_COLOR      = 26,  // https://www.w3.org/TR/compositing-1/#blendingcolor
  COMPOSITE_HSL_LUMINOSITY = 27,  // https://www.w3.org/TR/compositing-1/#blendingluminosity
};

bool ParseAffine(const ots::Font* font,
                 const uint8_t* data, size_t length,
                 bool var)
{
  ots::Buffer subtable(data, length);

  Fixed xx, yx, xy, yy, dx, dy;
  VarIdxBase varIndexBase;

  if (!subtable.ReadS32(&xx) ||
      !subtable.ReadS32(&yx) ||
      !subtable.ReadS32(&xy) ||
      !subtable.ReadS32(&yy) ||
      !subtable.ReadS32(&dx) ||
      !subtable.ReadS32(&dy) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read [Var]Affine2x3");
  }

  return true;
}

// Paint-record dispatch function that reads the format byte and then dispatches
// to one of the record-specific helpers.
bool ParsePaint(const ots::Font* font, const uint8_t* data, size_t length, colrState& state);

// All these paint record parsers start with Skip(1) to ignore the format field,
// which the caller has already read in order to dispatch here.

bool ParsePaintColrLayers(const ots::Font* font,
                          const uint8_t* data, size_t length,
                          colrState& state)
{
  if (setContains(state.visited, data)) {
    return OTS_FAILURE_MSG("Cycle detected in PaintColrLayers");
  }
  state.visited.insert(data);

  ots::Buffer subtable(data, length);

  uint8_t numLayers;
  uint32_t firstLayerIndex;

  if (!subtable.Skip(1) ||
      !subtable.ReadU8(&numLayers) ||
      !subtable.ReadU32(&firstLayerIndex)) {
    return OTS_FAILURE_MSG("Failed to read PaintColrLayers record");
  }

  if (uint64_t(firstLayerIndex) + numLayers > state.layerList.size()) {
    return OTS_FAILURE_MSG("PaintColrLayers exceeds bounds of layer list");
  }

  for (auto i = firstLayerIndex; i < firstLayerIndex + numLayers; ++i) {
    auto layer = state.layerList[i];
    if (!ParsePaint(font, layer.first, layer.second, state)) {
      return OTS_FAILURE_MSG("Failed to parse layer");
    }
  }

  state.visited.erase(data);

  return true;
}

bool ParsePaintSolid(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint16_t paletteIndex;
  F2DOT14 alpha;

  if (!subtable.Skip(1) ||
      !subtable.ReadU16(&paletteIndex) ||
      !subtable.ReadS16(&alpha)) {
    return OTS_FAILURE_MSG("Failed to read PaintSolid");
  }

  if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
    return OTS_FAILURE_MSG("Invalid palette index %u PaintSolid", paletteIndex);
  }

  if (alpha < 0 || alpha > F2DOT14_one) {
    OTS_WARNING("Alpha value outside valid range 0.0 - 1.0");
  }

  if (var) {
    VarIdxBase varIndexBase;
    if (!subtable.ReadU32(&varIndexBase)) {
      return OTS_FAILURE_MSG("Failed to read PaintVarSolid");
    }
  }

  return true;
}

bool ParsePaintLinearGradient(const ots::Font* font,
                              const uint8_t* data, size_t length,
                              colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLineOffset;
  FWORD x0, y0, x1, y1, x2, y2;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLineOffset) ||
      !subtable.ReadS16(&x0) ||
      !subtable.ReadS16(&y0) ||
      !subtable.ReadS16(&x1) ||
      !subtable.ReadS16(&y1) ||
      !subtable.ReadS16(&x2) ||
      !subtable.ReadS16(&y2) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]LinearGradient");
  }

  if (colorLineOffset >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLineOffset, length - colorLineOffset, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintRadialGradient(const ots::Font* font,
                              const uint8_t* data, size_t length,
                              colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLineOffset;
  FWORD x0, y0;
  UFWORD radius0;
  FWORD x1, y1;
  UFWORD radius1;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLineOffset) ||
      !subtable.ReadS16(&x0) ||
      !subtable.ReadS16(&y0) ||
      !subtable.ReadU16(&radius0) ||
      !subtable.ReadS16(&x1) ||
      !subtable.ReadS16(&y1) ||
      !subtable.ReadU16(&radius1) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]RadialGradient");
  }

  if (colorLineOffset >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLineOffset, length - colorLineOffset, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintSweepGradient(const ots::Font* font,
                             const uint8_t* data, size_t length,
                             colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t colorLineOffset;
  FWORD centerX, centerY;
  F2DOT14 startAngle, endAngle;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&colorLineOffset) ||
      !subtable.ReadS16(&centerX) ||
      !subtable.ReadS16(&centerY) ||
      !subtable.ReadS16(&startAngle) ||
      !subtable.ReadS16(&endAngle) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]SweepGradient");
  }

  if (colorLineOffset >= length) {
    return OTS_FAILURE_MSG("ColorLine is out of bounds");
  }

  if (!ParseColorLine(font, data + colorLineOffset, length - colorLineOffset, state, var)) {
    return OTS_FAILURE_MSG("Failed to parse [Var]ColorLine");
  }

  return true;
}

bool ParsePaintGlyph(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  uint16_t glyphID;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadU16(&glyphID)) {
    return OTS_FAILURE_MSG("Failed to read PaintGlyph");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in PaintGlyph");
  }

  if (glyphID >= state.numGlyphs) {
    return OTS_FAILURE_MSG("Glyph ID %u out of bounds", glyphID);
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for PaintGlyph");
  }

  return true;
}

bool ParsePaintColrGlyph(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state)
{
  if (setContains(state.visited, data)) {
    return OTS_FAILURE_MSG("Cycle detected in PaintColrGlyph");
  }
  state.visited.insert(data);

  ots::Buffer subtable(data, length);

  uint16_t glyphID;

  if (!subtable.Skip(1) ||
      !subtable.ReadU16(&glyphID)) {
    return OTS_FAILURE_MSG("Failed to read PaintColrGlyph");
  }

  auto baseGlyph = state.baseGlyphMap.find(glyphID);
  if (baseGlyph == state.baseGlyphMap.end()) {
    return OTS_FAILURE_MSG("Glyph ID %u not found in BaseGlyphList", glyphID);
  }

  if (!ParsePaint(font, baseGlyph->second.first, baseGlyph->second.second, state)) {
    return OTS_FAILURE_MSG("Failed to parse referenced color glyph %u", glyphID);
  }

  state.visited.erase(data);

  return true;
}

bool ParsePaintTransform(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  uint32_t transformOffset;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadU24(&transformOffset)) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Transform");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in Paint[Var]Transform");
  }
  if (transformOffset >= length) {
    return OTS_FAILURE_MSG("Transform offset out of bounds");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Transform");
  }

  if (!ParseAffine(font, data + transformOffset, length - transformOffset, var)) {
    return OTS_FAILURE_MSG("Failed to parse affine transform");
  }

  return true;
}

bool ParsePaintTranslate(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state, bool var)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  FWORD dx, dy;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&dx) ||
      !subtable.ReadS16(&dy) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Translate");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in Paint[Var]Translate");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Translate");
  }

  return true;
}

bool ParsePaintScale(const ots::Font* font,
                     const uint8_t* data, size_t length,
                     colrState& state,
                     bool var, bool aroundCenter, bool uniform)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 scaleX, scaleY;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&scaleX) ||
      (!uniform && !subtable.ReadS16(&scaleY)) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Scale[...]");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in Paint[Var]Scale[...]");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Scale[...]");
  }

  return true;
}

bool ParsePaintRotate(const ots::Font* font,
                      const uint8_t* data, size_t length,
                      colrState& state,
                      bool var, bool aroundCenter)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 angle;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&angle) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Rotate[...]");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in Paint[Var]Rotate[...]");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Rotate[...]");
  }

  return true;
}

bool ParsePaintSkew(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state,
                    bool var, bool aroundCenter)
{
  ots::Buffer subtable(data, length);

  uint32_t paintOffset;
  F2DOT14 xSkewAngle, ySkewAngle;
  FWORD centerX, centerY;
  VarIdxBase varIndexBase;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&paintOffset) ||
      !subtable.ReadS16(&xSkewAngle) ||
      !subtable.ReadS16(&ySkewAngle) ||
      (aroundCenter && (!subtable.ReadS16(&centerX) ||
                        !subtable.ReadS16(&centerY))) ||
      (var && !subtable.ReadU32(&varIndexBase))) {
    return OTS_FAILURE_MSG("Failed to read Paint[Var]Skew[...]");
  }

  if (!paintOffset || paintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid paint offset in Paint[Var]Skew[...]");
  }

  if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse paint for Paint[Var]Skew[...]");
  }

  return true;
}

bool ParsePaintComposite(const ots::Font* font,
                         const uint8_t* data, size_t length,
                         colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t sourcePaintOffset;
  uint8_t compositeMode;
  uint32_t backdropPaintOffset;

  if (!subtable.Skip(1) ||
      !subtable.ReadU24(&sourcePaintOffset) ||
      !subtable.ReadU8(&compositeMode) ||
      !subtable.ReadU24(&backdropPaintOffset)) {
    return OTS_FAILURE_MSG("Failed to read PaintComposite");
  }
  if (compositeMode > COMPOSITE_HSL_LUMINOSITY) {
    OTS_WARNING("Unknown composite mode %u\n", compositeMode);
  }

  if (!sourcePaintOffset || sourcePaintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid source paint offset");
  }
  if (!ParsePaint(font, data + sourcePaintOffset, length - sourcePaintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse source paint");
  }

  if (!backdropPaintOffset || backdropPaintOffset >= length) {
    return OTS_FAILURE_MSG("Invalid backdrop paint offset");
  }
  if (!ParsePaint(font, data + backdropPaintOffset, length - backdropPaintOffset, state)) {
    return OTS_FAILURE_MSG("Failed to parse backdrop paint");
  }

  return true;
}

bool ParsePaint(const ots::Font* font,
                const uint8_t* data, size_t length,
                colrState& state)
{
  if (setContains(state.paints, data)) {
    return true;
  }

  ots::Buffer subtable(data, length);

  uint8_t format;

  if (!subtable.ReadU8(&format)) {
    return OTS_FAILURE_MSG("Failed to read paint record format");
  }

  bool ok = true;
  switch (format) {
    case 1: ok = ParsePaintColrLayers(font, data, length, state); break;
    case 2: ok = ParsePaintSolid(font, data, length, state, false); break;
    case 3: ok = ParsePaintSolid(font, data, length, state, true); break;
    case 4: ok = ParsePaintLinearGradient(font, data, length, state, false); break;
    case 5: ok = ParsePaintLinearGradient(font, data, length, state, true); break;
    case 6: ok = ParsePaintRadialGradient(font, data, length, state, false); break;
    case 7: ok = ParsePaintRadialGradient(font, data, length, state, true); break;
    case 8: ok = ParsePaintSweepGradient(font, data, length, state, false); break;
    case 9: ok = ParsePaintSweepGradient(font, data, length, state, true); break;
    case 10: ok = ParsePaintGlyph(font, data, length, state); break;
    case 11: ok = ParsePaintColrGlyph(font, data, length, state); break;
    case 12: ok = ParsePaintTransform(font, data, length, state, false); break;
    case 13: ok = ParsePaintTransform(font, data, length, state, true); break;
    case 14: ok = ParsePaintTranslate(font, data, length, state, false); break;
    case 15: ok = ParsePaintTranslate(font, data, length, state, true); break;
    case 16: ok = ParsePaintScale(font, data, length, state, false, false, false); break; // Scale
    case 17: ok = ParsePaintScale(font, data, length, state, true, false, false); break; // VarScale
    case 18: ok = ParsePaintScale(font, data, length, state, false, true, false); break; // ScaleAroundCenter
    case 19: ok = ParsePaintScale(font, data, length, state, true, true, false); break; // VarScaleAroundCenter
    case 20: ok = ParsePaintScale(font, data, length, state, false, false, true); break; // ScaleUniform
    case 21: ok = ParsePaintScale(font, data, length, state, true, false, true); break; // VarScaleUniform
    case 22: ok = ParsePaintScale(font, data, length, state, false, true, true); break; // ScaleUniformAroundCenter
    case 23: ok = ParsePaintScale(font, data, length, state, true, true, true); break; // VarScaleUniformAroundCenter
    case 24: ok = ParsePaintRotate(font, data, length, state, false, false); break; // Rotate
    case 25: ok = ParsePaintRotate(font, data, length, state, true, false); break; // VarRotate
    case 26: ok = ParsePaintRotate(font, data, length, state, false, true); break; // RotateAroundCenter
    case 27: ok = ParsePaintRotate(font, data, length, state, true, true); break; // VarRotateAroundCenter
    case 28: ok = ParsePaintSkew(font, data, length, state, false, false); break; // Skew
    case 29: ok = ParsePaintSkew(font, data, length, state, true, false); break; // VarSkew
    case 30: ok = ParsePaintSkew(font, data, length, state, false, true); break; // SkewAroundCenter
    case 31: ok = ParsePaintSkew(font, data, length, state, true, true); break; // VarSkewAroundCenter
    case 32: ok = ParsePaintComposite(font, data, length, state); break;
    default:
      // Clients are supposed to ignore unknown paint types.
      OTS_WARNING("Unknown paint type %u", format);
      break;
  }

  state.paints.insert(data);

  return ok;
}

#pragma pack(1)
struct COLRv1
{
  // Version-0 fields
  uint16_t version;
  uint16_t numBaseGlyphRecords;
  uint32_t baseGlyphRecordsOffset;
  uint32_t layerRecordsOffset;
  uint16_t numLayerRecords;
  // Version-1 additions
  uint32_t baseGlyphListOffset;
  uint32_t layerListOffset;
  uint32_t clipListOffset;
  uint32_t varIdxMapOffset;
  uint32_t varStoreOffset;
};
#pragma pack()

bool ParseBaseGlyphRecords(const ots::Font* font,
                           const uint8_t* data, size_t length,
                           uint32_t numBaseGlyphRecords,
                           uint32_t numLayerRecords,
                           colrState& state)
{
  ots::Buffer subtable(data, length);

  int32_t prevGlyphID = -1;
  for (auto i = 0u; i < numBaseGlyphRecords; ++i) {
    uint16_t glyphID,
             firstLayerIndex,
             numLayers;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU16(&firstLayerIndex) ||
        !subtable.ReadU16(&numLayers)) {
      return OTS_FAILURE_MSG("Failed to read base glyph record");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Base glyph record glyph ID %u out of bounds", glyphID);
    }

    if (int32_t(glyphID) <= prevGlyphID) {
      return OTS_FAILURE_MSG("Base glyph record for glyph ID %u out of order", glyphID);
    }

    if (uint32_t(firstLayerIndex) + uint32_t(numLayers) > numLayerRecords) {
      return OTS_FAILURE_MSG("Layer index out of bounds");
    }

    prevGlyphID = glyphID;
  }

  return true;
}

bool ParseLayerRecords(const ots::Font* font,
                       const uint8_t* data, size_t length,
                       uint32_t numLayerRecords,
                       colrState& state)
{
  ots::Buffer subtable(data, length);

  for (auto i = 0u; i < numLayerRecords; ++i) {
    uint16_t glyphID,
             paletteIndex;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU16(&paletteIndex)) {
      return OTS_FAILURE_MSG("Failed to read layer record");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Layer record glyph ID %u out of bounds", glyphID);
    }

    if (paletteIndex >= state.numPaletteEntries && paletteIndex != 0xffffu) {
      return OTS_FAILURE_MSG("Invalid palette index %u in layer record", paletteIndex);
    }
  }

  return true;
}

bool ParseBaseGlyphList(const ots::Font* font,
                        const uint8_t* data, size_t length,
                        colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t numBaseGlyphPaintRecords;

  if (!subtable.ReadU32(&numBaseGlyphPaintRecords)) {
    return OTS_FAILURE_MSG("Failed to read base glyph list");
  }

  int32_t prevGlyphID = -1;
  // We loop over the list twice, first to collect all the glyph IDs present,
  // and then to check they can be parsed.
  size_t saveOffset = subtable.offset();
  for (auto i = 0u; i < numBaseGlyphPaintRecords; ++i) {
    uint16_t glyphID;
    uint32_t paintOffset;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU32(&paintOffset)) {
      return OTS_FAILURE_MSG("Failed to read base glyph list");
    }

    if (glyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Base glyph list glyph ID %u out of bounds", glyphID);
    }

    if (int32_t(glyphID) <= prevGlyphID) {
      return OTS_FAILURE_MSG("Base glyph list record for glyph ID %u out of order", glyphID);
    }

    if (!paintOffset || paintOffset >= length) {
      return OTS_FAILURE_MSG("Invalid paint offset for base glyph ID %u", glyphID);
    }

    // Record the base glyph list records so that we can follow them when processing
    // PaintColrGlyph records.
    state.baseGlyphMap[glyphID] = std::pair<const uint8_t*, size_t>(data + paintOffset, length - paintOffset);
    prevGlyphID = glyphID;
  }

  subtable.set_offset(saveOffset);
  for (auto i = 0u; i < numBaseGlyphPaintRecords; ++i) {
    uint16_t glyphID;
    uint32_t paintOffset;

    if (!subtable.ReadU16(&glyphID) ||
        !subtable.ReadU32(&paintOffset)) {
      return OTS_FAILURE_MSG("Failed to read base glyph list");
    }

    if (!ParsePaint(font, data + paintOffset, length - paintOffset, state)) {
      return OTS_FAILURE_MSG("Failed to parse paint for base glyph ID %u", glyphID);
    }

    // After each base glyph record is fully processed, the visited set should be clear;
    // otherwise, we have a bug in the cycle-detection logic.
    assert(state.visited.empty());
  }

  return true;
}

// We call this twice: first with parsePaints = false, to just get the number of layers,
// and then with parsePaints = true to actually descend the paint graphs.
bool ParseLayerList(const ots::Font* font,
                    const uint8_t* data, size_t length,
                    colrState& state)
{
  ots::Buffer subtable(data, length);

  uint32_t numLayers;
  if (!subtable.ReadU32(&numLayers)) {
    return OTS_FAILURE_MSG("Failed to read layer list");
  }

  for (auto i = 0u; i < numLayers; ++i) {
    uint32_t paintOffset;

    if (!subtable.ReadU32(&paintOffset)) {
      return OTS_FAILURE_MSG("Failed to read layer list");
    }

    if (!paintOffset || paintOffset >= length) {
      return OTS_FAILURE_MSG("Invalid paint offset in layer list");
    }

    state.layerList.push_back(std::pair<const uint8_t*, size_t>(data + paintOffset, length - paintOffset));
  }

  return true;
}

bool ParseClipBox(const ots::Font* font,
                  const uint8_t* data, size_t length)
{
  ots::Buffer subtable(data, length);

  uint8_t format;
  FWORD xMin, yMin, xMax, yMax;

  if (!subtable.ReadU8(&format) ||
      !subtable.ReadS16(&xMin) ||
      !subtable.ReadS16(&yMin) ||
      !subtable.ReadS16(&xMax) ||
      !subtable.ReadS16(&yMax)) {
    return OTS_FAILURE_MSG("Failed to read clip box");
  }

  switch (format) {
    case 1:
      break;
    case 2: {
      uint32_t varIndexBase;
      if (!subtable.ReadU32(&varIndexBase)) {
        return OTS_FAILURE_MSG("Failed to read clip box");
      }
      break;
    }
    default:
      return OTS_FAILURE_MSG("Invalid clip box format: %u", format);
  }

  if (xMin > xMax || yMin > yMax) {
    return OTS_FAILURE_MSG("Invalid clip box bounds");
  }

  return true;
}

bool ParseClipList(const ots::Font* font,
                   const uint8_t* data, size_t length,
                   colrState& state)
{
  ots::Buffer subtable(data, length);

  uint8_t format;
  uint32_t numClipRecords;

  if (!subtable.ReadU8(&format) ||
      !subtable.ReadU32(&numClipRecords)) {
    return OTS_FAILURE_MSG("Failed to read clip list");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Unknown clip list format: %u", format);
  }

  int32_t prevEndGlyphID = -1;
  for (auto i = 0u; i < numClipRecords; ++i) {
    uint16_t startGlyphID,
             endGlyphID;
    uint32_t clipBoxOffset;

    if (!subtable.ReadU16(&startGlyphID) ||
        !subtable.ReadU16(&endGlyphID) ||
        !subtable.ReadU24(&clipBoxOffset)) {
      return OTS_FAILURE_MSG("Failed to read clip list");
    }

    if (int32_t(startGlyphID) <= prevEndGlyphID ||
        endGlyphID < startGlyphID ||
        endGlyphID >= int32_t(state.numGlyphs)) {
      return OTS_FAILURE_MSG("Bad or out-of-order glyph ID range %u-%u in clip list", startGlyphID, endGlyphID);
    }

    if (clipBoxOffset >= length) {
      return OTS_FAILURE_MSG("Clip box offset out of bounds for glyphs %u-%u", startGlyphID, endGlyphID);
    }

    if (!ParseClipBox(font, data + clipBoxOffset, length - clipBoxOffset)) {
      return OTS_FAILURE_MSG("Failed to parse clip box for glyphs %u-%u", startGlyphID, endGlyphID);
    }

    prevEndGlyphID = endGlyphID;
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeCOLR::Parse(const uint8_t *data, size_t length) {
  // Parsing COLR table requires |maxp->num_glyphs| and |cpal->num_palette_entries|.
  Font *font = GetFont();
  Buffer table(data, length);

  uint32_t headerSize = offsetof(COLRv1, baseGlyphListOffset);

  // Version 0 header fields.
  uint16_t version = 0;
  uint16_t numBaseGlyphRecords = 0;
  uint32_t offsetBaseGlyphRecords = 0;
  uint32_t offsetLayerRecords = 0;
  uint16_t numLayerRecords = 0;
  if (!table.ReadU16(&version) ||
      !table.ReadU16(&numBaseGlyphRecords) ||
      !table.ReadU32(&offsetBaseGlyphRecords) ||
      !table.ReadU32(&offsetLayerRecords) ||
      !table.ReadU16(&numLayerRecords)) {
    return Error("Incomplete table");
  }

  if (version > 1) {
    return Error("Unknown COLR table version %u", version);
  }

  // Additional header fields for Version 1.
  uint32_t offsetBaseGlyphList = 0;
  uint32_t offsetLayerList = 0;
  uint32_t offsetClipList = 0;
  uint32_t offsetVarIdxMap = 0;
  uint32_t offsetItemVariationStore = 0;

  if (version == 1) {
    if (!table.ReadU32(&offsetBaseGlyphList) ||
        !table.ReadU32(&offsetLayerList) ||
        !table.ReadU32(&offsetClipList) ||
        !table.ReadU32(&offsetVarIdxMap) ||
        !table.ReadU32(&offsetItemVariationStore)) {
      return Error("Incomplete v.1 table");
    }
    headerSize = sizeof(COLRv1);
  }

  colrState state;

  auto* maxp = static_cast<ots::OpenTypeMAXP*>(font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  state.numGlyphs = maxp->num_glyphs;

  auto *cpal = static_cast<ots::OpenTypeCPAL*>(font->GetTypedTable(OTS_TAG_CPAL));
  if (!cpal) {
    return OTS_FAILURE_MSG("Required cpal table missing");
  }
  state.numPaletteEntries = cpal->num_palette_entries;

  if (numBaseGlyphRecords) {
    if (offsetBaseGlyphRecords < headerSize || offsetBaseGlyphRecords >= length) {
      return Error("Bad base glyph records offset in table header");
    }
    if (!ParseBaseGlyphRecords(font, data + offsetBaseGlyphRecords, length - offsetBaseGlyphRecords,
                               numBaseGlyphRecords, numLayerRecords, state)) {
      return Error("Failed to parse base glyph records");
    }
  }

  if (numLayerRecords) {
    if (offsetLayerRecords < headerSize || offsetLayerRecords >= length) {
      return Error("Bad layer records offset in table header");
    }
    if (!ParseLayerRecords(font, data + offsetLayerRecords, length - offsetLayerRecords, numLayerRecords,
                           state)) {
      return Error("Failed to parse layer records");
    }
  }

  if (offsetLayerList) {
    if (offsetLayerList < headerSize || offsetLayerList >= length) {
      return Error("Bad layer list offset in table header");
    }
    // This reads the layer list into our state.layerList vector, but does not parse the
    // paint graphs within it; these will be checked when visited via PaintColrLayers.
    if (!ParseLayerList(font, data + offsetLayerList, length - offsetLayerList, state)) {
      return Error("Failed to parse layer list");
    }
  }

  if (offsetBaseGlyphList) {
    if (offsetBaseGlyphList < headerSize || offsetBaseGlyphList >= length) {
      return Error("Bad base glyph list offset in table header");
    }
    // Here, we recursively check the paint graph starting at each base glyph record.
    if (!ParseBaseGlyphList(font, data + offsetBaseGlyphList, length - offsetBaseGlyphList,
                            state)) {
      return Error("Failed to parse base glyph list");
    }
  }

  if (offsetClipList) {
    if (offsetClipList < headerSize || offsetClipList >= length) {
      return Error("Bad clip list offset in table header");
    }
    if (!ParseClipList(font, data + offsetClipList, length - offsetClipList, state)) {
      return Error("Failed to parse clip list");
    }
  }

  if (offsetVarIdxMap) {
    if (offsetVarIdxMap < headerSize || offsetVarIdxMap >= length) {
      return Error("Bad delta set index offset in table header");
    }
    if (!ParseDeltaSetIndexMap(font, data + offsetVarIdxMap, length - offsetVarIdxMap)) {
      return Error("Failed to parse delta set index map");
    }
  }

  if (offsetItemVariationStore) {
    if (offsetItemVariationStore < headerSize || offsetItemVariationStore >= length) {
      return Error("Bad item variation store offset in table header");
    }
    if (!ParseItemVariationStore(font, data + offsetItemVariationStore, length - offsetItemVariationStore)) {
      return Error("Failed to parse item variation store");
    }
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeCOLR::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write COLR table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
