/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_edge_case_analysis_h__
#define jsion_ion_edge_case_analysis_h__

namespace js {
namespace ion {

class MIRGraph;

class EdgeCaseAnalysis
{
    MIRGraph &graph;

  public:
    EdgeCaseAnalysis(MIRGraph &graph);
    bool analyzeEarly();
    bool analyzeLate();
    static bool AllUsesTruncate(MInstruction *m);
};


} // namespace ion
} // namespace js

#endif // jsion_ion_edge_case_analysis_h__

