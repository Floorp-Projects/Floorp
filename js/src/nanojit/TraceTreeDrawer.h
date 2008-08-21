/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#ifndef __nanojit_TraceTreeDrawer__
#define __nanojit_TraceTreeDrawer__
#include <stdio.h>

namespace nanojit {
#ifdef AVMPLUS_VERBOSE
    using namespace avmplus;
    
    class TraceTreeDrawer : public GCFinalizedObject {
    public:
    	TraceTreeDrawer(Fragmento *frago, AvmCore *core, char *fileName);
    	~TraceTreeDrawer();
    	
    	void createGraphHeader();
    	void createGraphFooter();
    	
        void addEdge(Fragment *from, Fragment *to);
        void addNode(Fragment *fragment);
        void addNode(Fragment *fragment, const char *color);
	
        void draw(Fragment *rootFragment);
        void recursiveDraw(Fragment *root);
    	
    private:
    	FILE*				_fstream;
    	DWB(AvmCore *)		_core;
    	DWB(Fragmento *)	_frago;
    	DWB(LabelMap *)		_labels;
    	char *				_fileName;
    	
    	void addBackEdges(Fragment *f);
    	void addMergeNode(Fragment *f);
    	
    	bool isValidSideExit(struct SideExit *exit);
    	bool isCompiled(Fragment *f);
    	bool isLoopFragment(Fragment *f);
    	bool isCrossFragment(Fragment *f);
    	bool isMergeFragment(Fragment *f);
    	bool isSingleTrace(Fragment *f);
    	bool isSpawnedTrace(Fragment *f);
    	bool isBackEdgeSideExit(Fragment *f);
    	bool hasEndOfTraceFrag(Fragment *f);
    	bool hasCompiledBranch(Fragment *f);
    	
    	void printTreeStatus(Fragment *root);
    	void drawDirectedEdge();
    };
#endif
}

#endif /*TRACETREEDRAWER_H_*/
