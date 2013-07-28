/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_EdgeCaseAnalysis_h
#define ion_EdgeCaseAnalysis_h

#include "ion/MIRGenerator.h"

namespace js {
namespace ion {

class MIRGraph;

class EdgeCaseAnalysis
{
    MIRGenerator *mir;
    MIRGraph &graph;

  public:
    EdgeCaseAnalysis(MIRGenerator *mir, MIRGraph &graph);
    bool analyzeLate();
};


} // namespace ion
} // namespace js

#endif /* ion_EdgeCaseAnalysis_h */
