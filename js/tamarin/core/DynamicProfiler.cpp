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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

#include "avmplus.h"


#ifdef AVMPLUS_PROFILE
namespace avmplus
{
	DynamicProfiler::DynamicProfiler()
	{
		dprofile = false;
		sprofile = false;
		memset(counts2, 0, 256*sizeof(int));
		memset(times, 0, 256*sizeof(uint64));
		totalCount = 0;
		lastOp = (AbcOpcode)0;
		lastTime = 0;
	}

	void DynamicProfiler::dump(PrintWriter& console)
	{
		if (dprofile) 
		{
			uint64 totalTime = 0;
			for (int i=1; i < 256; i++) {
				totalTime += times[i];
			}
			console << "total count=" << totalCount << " cycles=" << (double)totalTime;
			if (totalCount != 0)
				console	<< " avg CPI=" << (int)(totalTime/totalCount);
			console << '\n';

			uint64 gcoverhead = times[OP_sweep] + times[OP_mark] + times[OP_alloc] + times[OP_wb];
			uint64 overhead = gcoverhead + times[OP_decode] + times[OP_verifypass] + times[OP_codegenop];
			console << "user     " << percent(100.0*(totalTime-overhead)/totalTime) << "%\n";
			console << "gc       " << percent(100.0*gcoverhead/totalTime) << "%\n";
			console << "decoder  " << percent(100.0*times[OP_decode]/totalTime) << "%\n";
			console << "verifier " << percent(100.0*times[OP_verifyop]/totalTime) << "%\n";
			if (times[OP_codegenop] != 0)
				console << "codegen  " << percent(100.0*times[OP_codegenop]/totalTime) << "%\n";

			console << "count"
					<< tabstop(11)
					<< "cycles"
					<< tabstop(25)
					<< "%count"
					<< tabstop(35)
					<< "%time"
					<< tabstop(45)
					<< "CPI"
					<< tabstop(55)
					<< "opcode\n";
			console << "-----"
					<< tabstop(11)
					<< "--------"
					<< tabstop(25)
					<< "-------"
					<< tabstop(35)
					<< "-------"
					<< tabstop(45)
					<< "---"
					<< tabstop(55)				
					<< "------\n";

			for (int i=1; i < 256; i++)
			{
				uint64 max = 0;
				int k = 0;
				// find highest time
				for (int j=1; j < 256; j++)
					if (times[j] > max)
						max = times[k=j];
				if (max != 0)
				{
					// print highest time then clear it
					uint64 ticks = times[k];//(int)(times[i]*1000000/frequency); 
					console << counts2[k]
							<< tabstop(11)
							<< (double)ticks
							<< tabstop(25)
							<< percent(100.0*counts2[k]/totalCount)
							<< tabstop(35)
							<< percent(100.0*times[k]/totalTime)
							<< tabstop(45)
							<< (int)(ticks/counts2[k])
							<< tabstop(55)
							<< opNames[k]
							<< '\n';
					times[k] = 0;
					counts2[k] = 0;
				}
			}

			// TODO include MD instruction counts, and compute ratios
//			console << "Register Allocator Ops      Count\n";
//			console << "--------------------------  -----\n";
//			console << "add active            O(1)  " << CodegenMIR::addActiveCount << "\n";
//			console << "register release      O(1)  " << CodegenMIR::registerReleaseCount << "\n";
//			console << "expire                O(1)  " << CodegenMIR::expireCount << "\n";
//			console << "remove free reg       O(1)  " << CodegenMIR::removeRegCount << "\n";
//			console << "remove first free     O(N)  " << CodegenMIR::removeFreeCount << "\n";
//			console << "remove last active    O(N)  " << CodegenMIR::removeLastActiveCount << "\n";
//			console << "spill caller regs     O(N)  " << CodegenMIR::spillCallerRegistersCount << "\n";
//			console << "flush caller actives  O(N)  " << CodegenMIR::flushCallerActivesCount << "\n";
		}

	}
}
#endif
