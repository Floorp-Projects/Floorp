/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerSorter.h"
#include <math.h>                       // for fabs
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for fprintf, stderr, FILE
#include <stdlib.h>                     // for getenv
#include "DirectedGraph.h"              // for DirectedGraph
#include "Layers.h"                     // for Layer
#include "gfxEnv.h"                     // for gfxEnv
#include "gfxLineSegment.h"             // for gfxLineSegment
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxQuad.h"                    // for gfxQuad
#include "gfxRect.h"                    // for gfxRect
#include "gfxTypes.h"                   // for gfxFloat
#include "mozilla/gfx/BasePoint3D.h"    // for BasePoint3D
#include "mozilla/Sprintf.h"            // for SprintfLiteral
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, etc
#include "limits.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

enum LayerSortOrder {
  Undefined,
  ABeforeB,
  BBeforeA,
};

/**
 * Recover the z component from a 2d transformed point by finding the intersection
 * of a line through the point in the z direction and the transformed plane.
 *
 * We want to solve:
 *
 * point = normal . (p0 - l0) / normal . l
 */
static gfxFloat RecoverZDepth(const Matrix4x4& aTransform, const gfxPoint& aPoint)
{
    const Point3D l(0, 0, 1);
    Point3D l0 = Point3D(aPoint.x, aPoint.y, 0);
    Point3D p0 = aTransform * Point3D(0, 0, 0);
    Point3D normal = aTransform.GetNormalVector();

    gfxFloat n = normal.DotProduct(p0 - l0); 
    gfxFloat d = normal.DotProduct(l);

    if (!d) {
        return 0;
    }

    return n/d;
}

/**
 * Determine if this transform layer should be drawn before another when they 
 * are both preserve-3d children.
 *
 * We want to find the relative z depths of the 2 layers at points where they
 * intersect when projected onto the 2d screen plane. Intersections are defined
 * as corners that are positioned within the other quad, as well as intersections
 * of the lines.
 *
 * We then choose the intersection point with the greatest difference in Z
 * depths and use this point to determine an ordering for the two layers.
 * For layers that are intersecting in 3d space, this essentially guesses an 
 * order. In a lot of cases we only intersect right at the edge point (3d cubes
 * in particular) and this generates the 'correct' looking ordering. For planes
 * that truely intersect, then there is no correct ordering and this remains
 * unsolved without changing our rendering code.
 */
static LayerSortOrder CompareDepth(Layer* aOne, Layer* aTwo) {
  gfxRect ourRect = ThebesRect(aOne->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());
  gfxRect otherRect = ThebesRect(aTwo->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());

  MOZ_ASSERT(aOne->GetParent() && aOne->GetParent()->Extend3DContext() &&
             aTwo->GetParent() && aTwo->GetParent()->Extend3DContext());
  // Effective transform of leaves may had been projected to 2D.
  Matrix4x4 ourTransform =
    aOne->GetLocalTransform() * aOne->GetParent()->GetEffectiveTransform();
  Matrix4x4 otherTransform =
    aTwo->GetLocalTransform() * aTwo->GetParent()->GetEffectiveTransform();

  // Transform both rectangles and project into 2d space.
  gfxQuad ourTransformedRect = ourRect.TransformToQuad(ourTransform);
  gfxQuad otherTransformedRect = otherRect.TransformToQuad(otherTransform);

  gfxRect ourBounds = ourTransformedRect.GetBounds();
  gfxRect otherBounds = otherTransformedRect.GetBounds();

  if (!ourBounds.Intersects(otherBounds)) {
    return Undefined;
  }

  // Make a list of all points that are within the other rect.
  // Could we just check Contains() on the bounds rects. ie, is it possible
  // for layers to overlap without intersections (in 2d space) and yet still
  // have their bounds rects not completely enclose each other?
  nsTArray<gfxPoint> points;
  for (uint32_t i = 0; i < 4; i++) {
    if (ourTransformedRect.Contains(otherTransformedRect.mPoints[i])) {
      points.AppendElement(otherTransformedRect.mPoints[i]);
    }
    if (otherTransformedRect.Contains(ourTransformedRect.mPoints[i])) {
      points.AppendElement(ourTransformedRect.mPoints[i]);
    }
  }
  
  // Look for intersections between lines (in 2d space) and use these as
  // depth testing points.
  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      gfxPoint intersection;
      gfxLineSegment one(ourTransformedRect.mPoints[i],
                         ourTransformedRect.mPoints[(i + 1) % 4]);
      gfxLineSegment two(otherTransformedRect.mPoints[j],
                         otherTransformedRect.mPoints[(j + 1) % 4]);
      if (one.Intersects(two, intersection)) {
        points.AppendElement(intersection);
      }
    }
  }

  // No intersections, no defined order between these layers.
  if (points.IsEmpty()) {
    return Undefined;
  }

  // Find the relative Z depths of each intersection point and check that the layers are in the same order.
  gfxFloat highest = 0;
  for (uint32_t i = 0; i < points.Length(); i++) {
    gfxFloat ourDepth = RecoverZDepth(ourTransform, points.ElementAt(i));
    gfxFloat otherDepth = RecoverZDepth(otherTransform, points.ElementAt(i));

    gfxFloat difference = otherDepth - ourDepth;

    if (fabs(difference) > fabs(highest)) {
      highest = difference;
    }
  }
  // If layers have the same depth keep the original order
  if (fabs(highest) < 0.1 || highest >= 0) {
    return ABeforeB;
  } else {
    return BBeforeA;
  }
}

#ifdef DEBUG
// #define USE_XTERM_COLORING
#ifdef USE_XTERM_COLORING
// List of color values, which can be added to the xterm foreground offset or
// background offset to generate a xterm color code.
// NOTE: The colors that we don't explicitly use (by name) are commented out,
// to avoid triggering Wunused-const-variable build warnings.
static const int XTERM_FOREGROUND_COLOR_OFFSET = 30;
static const int XTERM_BACKGROUND_COLOR_OFFSET = 40;
static const int BLACK = 0;
//static const int RED = 1;
static const int GREEN = 2;
//static const int YELLOW = 3;
//static const int BLUE = 4;
//static const int MAGENTA = 5;
//static const int CYAN = 6;
//static const int WHITE = 7;

static const int RESET = 0;
// static const int BRIGHT = 1;
// static const int DIM = 2;
// static const int UNDERLINE = 3;
// static const int BLINK = 4;
// static const int REVERSE = 7;
// static const int HIDDEN = 8;

static void SetTextColor(uint32_t aColor)
{
  char command[13];

  /* Command is the control command to the terminal */
  SprintfLiteral(command, "%c[%d;%d;%dm", 0x1B, RESET,
                 aColor + XTERM_FOREGROUND_COLOR_OFFSET,
                 BLACK + XTERM_BACKGROUND_COLOR_OFFSET);
  printf("%s", command);
}

static void print_layer_internal(FILE* aFile, Layer* aLayer, uint32_t aColor)
{
  SetTextColor(aColor);
  fprintf(aFile, "%p", aLayer);
  SetTextColor(GREEN);
}
#else

const char *colors[] = { "Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White" };

static void print_layer_internal(FILE* aFile, Layer* aLayer, uint32_t aColor)
{
  fprintf(aFile, "%p(%s)", aLayer, colors[aColor]);
}
#endif

static void print_layer(FILE* aFile, Layer* aLayer)
{
  print_layer_internal(aFile, aLayer, aLayer->GetDebugColorIndex());
}

static void DumpLayerList(nsTArray<Layer*>& aLayers)
{
  for (uint32_t i = 0; i < aLayers.Length(); i++) {
    print_layer(stderr, aLayers.ElementAt(i));
    fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");
}

static void DumpEdgeList(DirectedGraph<Layer*>& aGraph)
{
  const nsTArray<DirectedGraph<Layer*>::Edge>& edges = aGraph.GetEdgeList();
  
  for (uint32_t i = 0; i < edges.Length(); i++) {
    fprintf(stderr, "From: ");
    print_layer(stderr, edges.ElementAt(i).mFrom);
    fprintf(stderr, ", To: ");
    print_layer(stderr, edges.ElementAt(i).mTo);
    fprintf(stderr, "\n");
  }
}
#endif

// The maximum number of layers that we will attempt to sort. Anything
// greater than this will be left unsorted. We should consider enabling
// depth buffering for the scene in this case.
#define MAX_SORTABLE_LAYERS 100


uint32_t gColorIndex = 1;

void SortLayersBy3DZOrder(nsTArray<Layer*>& aLayers)
{
  uint32_t nodeCount = aLayers.Length();
  if (nodeCount > MAX_SORTABLE_LAYERS) {
    return;
  }
  DirectedGraph<Layer*> graph;

#ifdef DEBUG
  if (gfxEnv::DumpLayerSortList()) {
    for (uint32_t i = 0; i < nodeCount; i++) {
      if (aLayers.ElementAt(i)->GetDebugColorIndex() == 0) {
        aLayers.ElementAt(i)->SetDebugColorIndex(gColorIndex++);
        if (gColorIndex > 7) {
          gColorIndex = 1;
        }
      }
    }
    fprintf(stderr, " --- Layers before sorting: --- \n");
    DumpLayerList(aLayers);
  }
#endif

  // Iterate layers and determine edges.
  for (uint32_t i = 0; i < nodeCount; i++) {
    for (uint32_t j = i + 1; j < nodeCount; j++) {
      Layer* a = aLayers.ElementAt(i);
      Layer* b = aLayers.ElementAt(j);
      LayerSortOrder order = CompareDepth(a, b);
      if (order == ABeforeB) {
        graph.AddEdge(a, b);
      } else if (order == BBeforeA) {
        graph.AddEdge(b, a);
      }
    }
  }

#ifdef DEBUG
  if (gfxEnv::DumpLayerSortList()) {
    fprintf(stderr, " --- Edge List: --- \n");
    DumpEdgeList(graph);
  }
#endif

  // Build a new array using the graph.
  nsTArray<Layer*> noIncoming;
  nsTArray<Layer*> sortedList;

  // Make a list of all layers with no incoming edges.
  noIncoming.AppendElements(aLayers);
  const nsTArray<DirectedGraph<Layer*>::Edge>& edges = graph.GetEdgeList();
  for (uint32_t i = 0; i < edges.Length(); i++) {
    noIncoming.RemoveElement(edges.ElementAt(i).mTo);
  }

  // Move each item without incoming edges into the sorted list,
  // and remove edges from it.
  do {
    if (!noIncoming.IsEmpty()) {
      uint32_t last = noIncoming.Length() - 1;

      Layer* layer = noIncoming.ElementAt(last);
      MOZ_ASSERT(layer); // don't let null layer pointers sneak into sortedList

      noIncoming.RemoveElementAt(last);
      sortedList.AppendElement(layer);

      nsTArray<DirectedGraph<Layer*>::Edge> outgoing;
      graph.GetEdgesFrom(layer, outgoing);
      for (uint32_t i = 0; i < outgoing.Length(); i++) {
        DirectedGraph<Layer*>::Edge edge = outgoing.ElementAt(i);
        graph.RemoveEdge(edge);
        if (!graph.NumEdgesTo(edge.mTo)) {
          // If this node also has no edges now, add it to the list
          noIncoming.AppendElement(edge.mTo);
        }
      }
    }

    // If there are no nodes without incoming edges, but there
    // are still edges, then we have a cycle.
    if (noIncoming.IsEmpty() && graph.GetEdgeCount()) {
      // Find the node with the least incoming edges.
      uint32_t minEdges = UINT_MAX;
      Layer* minNode = nullptr;
      for (uint32_t i = 0; i < aLayers.Length(); i++) {
        uint32_t edgeCount = graph.NumEdgesTo(aLayers.ElementAt(i));
        if (edgeCount && edgeCount < minEdges) {
          minEdges = edgeCount;
          minNode = aLayers.ElementAt(i);
          if (minEdges == 1) {
            break;
          }
        }
      }

      if (minNode) {
        // Remove all of them!
        graph.RemoveEdgesTo(minNode);
        noIncoming.AppendElement(minNode);
      }
    }
  } while (!noIncoming.IsEmpty());
  NS_ASSERTION(!graph.GetEdgeCount(), "Cycles detected!");
#ifdef DEBUG
  if (gfxEnv::DumpLayerSortList()) {
    fprintf(stderr, " --- Layers after sorting: --- \n");
    DumpLayerList(sortedList);
  }
#endif

  aLayers.Clear();
  aLayers.AppendElements(sortedList);
}

} // namespace layers
} // namespace mozilla
