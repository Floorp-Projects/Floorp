/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DIRECTEDGRAPH_H
#define GFX_DIRECTEDGRAPH_H

#include "gfxTypes.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

template <typename T>
class DirectedGraph {
public:

  class Edge {
    public:
    Edge(T aFrom, T aTo) : mFrom(aFrom), mTo(aTo) {}

    bool operator==(const Edge& aOther) const
    {
      return mFrom == aOther.mFrom && mTo == aOther.mTo;
    }

    T mFrom;
    T mTo;
  };

  class RemoveEdgesToComparator 
  {
  public:
    bool Equals(const Edge& a, T const& b) const { return a.mTo == b; }
  };

  /**
   * Add a new edge to the graph.
   */
  void AddEdge(Edge aEdge)
  {
    NS_ASSERTION(!mEdges.Contains(aEdge), "Adding a duplicate edge!");
    mEdges.AppendElement(aEdge);
  }

  void AddEdge(T aFrom, T aTo)
  {
    AddEdge(Edge(aFrom, aTo));
  }

  /**
   * Get the list of edges.
   */
  const nsTArray<Edge>& GetEdgeList() const
  {
    return mEdges; 
  }

  /**
   * Remove the given edge from the graph.
   */
  void RemoveEdge(Edge aEdge)
  {
    mEdges.RemoveElement(aEdge);
  }

  /**
   * Remove all edges going into aNode.
   */
  void RemoveEdgesTo(T aNode)
  {
    RemoveEdgesToComparator c;
    while (mEdges.RemoveElement(aNode, c)) {}
  }
  
  /**
   * Get the number of edges going into aNode.
   */
  unsigned int NumEdgesTo(T aNode)
  {
    unsigned int count = 0;
    for (unsigned int i = 0; i < mEdges.Length(); i++) {
      if (mEdges.ElementAt(i).mTo == aNode) {
        count++;
      }
    }
    return count;
  }

  /**
   * Get the list of all edges going from aNode
   */
  void GetEdgesFrom(T aNode, nsTArray<Edge>& aResult)
  {
    for (unsigned int i = 0; i < mEdges.Length(); i++) {
      if (mEdges.ElementAt(i).mFrom == aNode) {
        aResult.AppendElement(mEdges.ElementAt(i));
      }
    }
  }

  /**
   * Get the total number of edges.
   */
  unsigned int GetEdgeCount() { return mEdges.Length(); }

private:

  nsTArray<Edge> mEdges;
};

}
}

#endif // GFX_DIRECTEDGRAPH_H
