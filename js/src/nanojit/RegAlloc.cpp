/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
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

#include "nanojit.h"

namespace nanojit
{	
	#ifdef FEATURE_NANOJIT

	/**
	 * Generic register allocation routines.
	 */
	void RegAlloc::clear()
	{
		free = 0;
		used = 0;
		memset(active, 0, sizeof(active));
		memset(usepri, 0, sizeof(usepri));
	}

	bool RegAlloc::isFree(Register r) 
	{
		NanoAssert(r != UnknownReg);
		return (free & rmask(r)) != 0;
	}
		
	void RegAlloc::addFree(Register r)
	{
		NanoAssert(!isFree(r));
		free |= rmask(r);
	}

	void RegAlloc::removeFree(Register r)
	{
		NanoAssert(isFree(r));
		free &= ~rmask(r);
	}

	void RegAlloc::addActive(Register r, LIns* v)
	{
		//  Count++;
		NanoAssert(v && r != UnknownReg && active[r] == NULL );
		active[r] = v;
        useActive(r);
	}

    void RegAlloc::useActive(Register r)
    {
        NanoAssert(r != UnknownReg && active[r] != NULL);
        usepri[r] = priority++;
    }

	void RegAlloc::removeActive(Register r)
	{
		//registerReleaseCount++;
		NanoAssert(r != UnknownReg);
		NanoAssert(active[r] != NULL);

		// remove the given register from the active list
		active[r] = NULL;
	}

	void RegAlloc::retire(Register r)
	{
		NanoAssert(r != UnknownReg);
		NanoAssert(active[r] != NULL);
		active[r] = NULL;
		free |= rmask(r);
	}

	// scan table for instruction with the lowest priority, meaning it is used
    // furthest in the future.
	LIns* Assembler::findVictim(RegAlloc &regs, RegisterMask allow)
	{
		NanoAssert(allow != 0);
		LIns *i, *a=0;
        int allow_pri = 0x7fffffff;
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
            if ((allow & rmask(r)) && (i = regs.getActive(r)) != 0)
            {
                int pri = canRemat(i) ? 0 : regs.getPriority(r);
                if (!a || pri < allow_pri) {
                    a = i;
                    allow_pri = pri;
                }
			}
		}
        NanoAssert(a != 0);
        return a;
	}

	#ifdef  NJ_VERBOSE
	/* static */ void RegAlloc::formatRegisters(RegAlloc& regs, char* s, Fragment *frag)
	{
		if (!frag || !frag->lirbuf)
			return;
		LirNameMap *names = frag->lirbuf->names;
		for(int i=FirstReg; i<LastReg; i++)
		{
			LIns* ins = regs.active[i];
			Register r = (Register)i;
			if (ins && regs.isFree(r))
				{ NanoAssertMsg( 0, "Coding error; register is both free and active! " ); }
			//if (!ins && !regs.isFree(r))
			//	{ NanoAssertMsg( 0, "Coding error; register is not in the free list when it should be" ); }
			if (!ins)
				continue;				

			s += strlen(s);
			const char* rname = ins->isQuad() ? fpn(r) : gpn(r);
			sprintf(s, " %s(%s)", rname, names->formatRef(ins));
		}
	}
	#endif /* NJ_VERBOSE */

	#ifdef _DEBUG

	uint32_t RegAlloc::countFree()
	{
		int cnt = 0;
		for(Register i=FirstReg; i <= LastReg; i = nextreg(i))
			cnt += isFree(i) ? 1 : 0;
		return cnt;
	}

	uint32_t RegAlloc::countActive()
	{
		int cnt = 0;
		for(Register i=FirstReg; i <= LastReg; i = nextreg(i))
			cnt += active[i] ? 1 : 0;
		return cnt;
	}

	void RegAlloc::checkCount()
	{
		NanoAssert(count == (countActive() + countFree()));
	}

    bool RegAlloc::isConsistent(Register r, LIns* i)
    {
		NanoAssert(r != UnknownReg);
        return (isFree(r)  && !getActive(r)     && !i) ||
               (!isFree(r) &&  getActive(r)== i && i );
    }

	#endif /*DEBUG*/
	#endif /* FEATURE_NANOJIT */
}
