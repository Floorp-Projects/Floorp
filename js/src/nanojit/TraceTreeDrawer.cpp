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

#include <stdio.h>
#include "nanojit.h"

#define verbose_draw_only(x) 

namespace nanojit {

#ifdef AVMPLUS_VERBOSE
	using namespace avmplus;

	TraceTreeDrawer::TraceTreeDrawer(Fragmento *frago, AvmCore *core, char *fileName) {
		this->_frago = frago;
		this->_core = core;
		this->_labels = frago->labels;
		this->_fileName = fileName;
	}
	
	TraceTreeDrawer::~TraceTreeDrawer() {
	}
	
	void TraceTreeDrawer::addNode(Fragment *fragment, const char *color) {
        fprintf(_fstream, "<node id=\"%d\">\n"
        		"<data key=\"nodeGraphicsID\">\n"
            	"<y:ShapeNode>\n"
            	"<y:Shape type=\"roundrectangle\"/>\n"
            	"<y:NodeLabel alignment=\"center\">%s"
            	" %s"	//fragment->ip
            	"</y:NodeLabel>\n"
            	"<y:Fill color=\"#%s\" transparent=\"false\"/>\n"
            	"</y:ShapeNode>\n"
            	"</data>\n"
            	"</node>\n", 
				(int)fragment,
				_labels->format(fragment), 
				_labels->format(fragment->ip), color);
	}
	
	void TraceTreeDrawer::addNode(Fragment *fragment) {
		if (!fragment->isAnchor())
    		addNode(fragment, "FFCC99");	// standard color
		else
			addNode(fragment, "CCFFFF");	// Root node
	}
	
    void TraceTreeDrawer::addEdge(Fragment *from, Fragment *to) {
    	this->addNode(from);
    	this->addNode(to);
    	
    	// Create the edge
    	fprintf(_fstream, "<edge directed=\"true\" source=\"%d\" target=\"%d\">\n", (int)from, (int)to); 
    	drawDirectedEdge();
    	fprintf(_fstream, "</edge>\n");
    }
    
    void TraceTreeDrawer::recursiveDraw(Fragment *root) {
    	if (!isCompiled(root)) {
    		return;
    	}
    	
    	addBackEdges(root);
    	
        Fragment *lastDrawnBranch = root;
    	for (Fragment *treeBranch = root->branches; treeBranch != 0; treeBranch = treeBranch->nextbranch) {
			if (!isMergeFragment(treeBranch)) {
				struct SideExit* exit = treeBranch->spawnedFrom->exit();
				if (isValidSideExit(exit) && isCompiled(treeBranch)) {
					verbose_draw_only(nj_dprintf("Adding edge between %s and %s\n", _labels->format(lastDrawnBranch), _labels->format(treeBranch)));
					
					this->addEdge(lastDrawnBranch, treeBranch);
					lastDrawnBranch = treeBranch;
				}
				
                recursiveDraw(treeBranch);
			}
			else {
				addMergeNode(treeBranch);
			} // end ifelse
    	}	// end for loop
    }
    
    void TraceTreeDrawer::addBackEdges(Fragment *root) {
    	// At the end of a tree, find out where it goes
    	if (isCrossFragment(root)) {
			if (root->eot_target) {
    			verbose_draw_only(nj_dprintf("Found a cross fragment %s TO %s \n", _labels->format(root), _labels->format(root->eot_target)));
    			this->addEdge(root, root->eot_target);
			}
    	}
    	else if (isBackEdgeSideExit(root)) {
			verbose_draw_only(nj_dprintf("Adding anchor branch edge from %s TO %s\n", _labels->format(root), _labels->format(root->anchor)));
			this->addEdge(root, root->anchor);
    	}
    	else if (isSingleTrace(root)) {
    		verbose_draw_only(nj_dprintf("Found a single trace %s\n", _labels->format(root)));
    		this->addEdge(root, root);
    	}
    	else if (isSpawnedTrace(root)) {
    		struct SideExit *exit = root->spawnedFrom->exit();
			if (isValidSideExit(exit) && isCompiled(root)) {
				verbose_draw_only(nj_dprintf("Found a spawned side exit from %s that is a spawn and compiled %s\n", _labels->format(root), _labels->format(exit->from)));
				this->addEdge(root, root->parent);
			}
    	}
    	else if (hasEndOfTraceFrag(root)) {
    		verbose_draw_only(nj_dprintf("%s has an EOT to %s\n", _labels->format(root), _labels->format(root->eot_target)));
    		addEdge(root, root->eot_target);
    	}
    }
    
	void TraceTreeDrawer::addMergeNode(Fragment *mergeRoot) {
        verbose_draw_only(nj_dprintf("Found a merge fragment %s and anchor %s\n", _labels->format(mergeRoot), _labels->format(mergeRoot->anchor)));
        
		if (hasCompiledBranch(mergeRoot)) {
			verbose_draw_only(nj_dprintf("Found a branch to %s\n", _labels->format(mergeRoot->branches)));
			addEdge(mergeRoot, mergeRoot->branches);
			recursiveDraw(mergeRoot->branches);
		}
		
		if (hasEndOfTraceFrag(mergeRoot)) {
            verbose_draw_only(nj_dprintf("Merge with an EOT to %s\n", _labels->format(mergeRoot->eot_target)));
			addEdge(mergeRoot, mergeRoot->eot_target);
		}
		else {
            verbose_draw_only(nj_dprintf("Merge to anchor %s\n", _labels->format(mergeRoot->anchor)));
			addEdge(mergeRoot, mergeRoot->anchor);
		}
	}
					    
    void TraceTreeDrawer::draw(Fragment *root) {
		this->recursiveDraw(root);
		
		verbose_draw_only(nj_dprintf("\nFinished drawing, printing status\n"));
		verbose_draw_only(this->printTreeStatus(root));
    }
    
    void TraceTreeDrawer::createGraphHeader() {
    	char outputFileName[128];
    	const char *graphMLExtension = ".graphml";
    	
    	int fileNameLength = strlen(this->_fileName);
    	memset(outputFileName, 0, sizeof(outputFileName));
    	strncat(outputFileName, this->_fileName, 128);
    	strncat(outputFileName + fileNameLength - 1, graphMLExtension, 128);	// -1 to overwrite the \0
    	
    	verbose_draw_only(nj_dprintf("output file name is %s\n", outputFileName));
    	this->_fstream = fopen(outputFileName, "w");
    	
		fprintf(_fstream, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
    			"<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns/graphml\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:y=\"http://www.yworks.com/xml/graphml\" xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns/graphml http://www.yworks.com/xml/schema/graphml/1.0/ygraphml.xsd\">\n"
    			"<key for=\"node\" id=\"nodeGraphicsID\" yfiles.type=\"nodegraphics\"/>\n"
    			"<key attr.name=\"description\" attr.type=\"string\" for=\"node\" id=\"nodeDescID\"/>\n"
    			"<key for=\"edge\" id=\"edgeGraphicsID\" yfiles.type=\"edgegraphics\"/>\n"
    			"<key attr.name=\"description\" attr.type=\"string\" for=\"edge\" id=\"edgeDescID\"/>\n"
    			"<graph edgedefault=\"directed\" id=\"rootGraph\">\n");
    	
    }
    
	void TraceTreeDrawer::createGraphFooter() {
    	fprintf(_fstream, "  </graph></graphml>");
    	fclose(this->_fstream);
	}
	
	bool TraceTreeDrawer::isValidSideExit(struct SideExit *exit) {
		return exit != 0;
	}
	
	bool TraceTreeDrawer::isCompiled(Fragment *f) {
		return f->compileNbr != 0;
	}
	
	bool TraceTreeDrawer::isLoopFragment(Fragment *f) {
		return f->kind == LoopTrace;
	}
	
	bool TraceTreeDrawer::isCrossFragment(Fragment *f) {
		return f->kind == BranchTrace;
	}
	
	bool TraceTreeDrawer::isMergeFragment(Fragment *f) {
		return f->kind == MergeTrace;
	}
	
	bool TraceTreeDrawer::isSingleTrace(Fragment *f) {
        return f->isAnchor() && !hasCompiledBranch(f);
	}
	
	bool TraceTreeDrawer::hasCompiledBranch(Fragment *f) {
		for (Fragment *current = f->branches; current != 0; current = current->nextbranch) {
			if (isCompiled(current)) {
				return true;
			}
		}
		
		return false;
	}
	
	bool TraceTreeDrawer::isSpawnedTrace(Fragment *f) {
		return f->spawnedFrom != 0;
	}
	
	bool TraceTreeDrawer::isBackEdgeSideExit(Fragment *f) {
        return !f->branches && !f->isAnchor(); 
    }
	
    bool TraceTreeDrawer::hasEndOfTraceFrag(Fragment *f) {
    	return (f->eot_target) && (f != f->eot_target);
    }
    
    void TraceTreeDrawer::drawDirectedEdge() {
    	// Make it directed
        fprintf(_fstream, "<data key=\"edgeGraphicsID\">\n"
				"<y:PolyLineEdge>\n"
				"<y:Arrows source=\"none\" target=\"standard\"/>\n"
				"</y:PolyLineEdge>\n"
				"</data>\n");
    }
	
	void TraceTreeDrawer::printTreeStatus(Fragment *root) {
		if (!isCompiled(root)) {
			return;
		}
		
    	nj_dprintf("\nRoot is %s\n", _labels->format(root));
    	if (root->spawnedFrom) {
			if (root->compileNbr) {
					nj_dprintf("Found a root that is a spawn and compiled %s\n", _labels->format(root->parent));
			}
    	}
    	
    	for (Fragment *x = root->branches; x != 0; x = x->nextbranch) {
    			if (x->kind != MergeTrace) {
    				struct SideExit* exit = x->spawnedFrom->exit();
    				if (exit && x->compileNbr) {
    					nj_dprintf("Found one with an SID and compiled %s\n", _labels->format(x));
    				}
    				
    				printTreeStatus(x);
    			}
    	}
    	nj_dprintf("\n");
    }
#endif
}
	

void drawTraceTrees(nanojit::Fragmento *frago, nanojit::FragmentMap * _frags, avmplus::AvmCore *core, char *fileName) {
#ifdef AVMPLUS_VERBOSE
	nanojit::TraceTreeDrawer *traceDrawer = new (core->gc) nanojit::TraceTreeDrawer(frago, core, fileName);
	traceDrawer->createGraphHeader();
	
	int32_t count = _frags->size();
	for (int32_t i=0; i<count; i++)
    {
        Fragment *frag = _frags->at(i);
        // Count only fragments which have something compiled. Needs the -Dverbose flag
        if (frag->compileNbr) {
        	traceDrawer->draw(frag);
        }
    }
	
	traceDrawer->createGraphFooter();
#else
	(void)frago;
	(void)_frags;
	(void)core;
	(void)fileName;
#endif
}
    
