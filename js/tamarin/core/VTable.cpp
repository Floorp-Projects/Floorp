/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#include "avmplus.h"

namespace avmplus
{
	
	VTable::VTable(Traits* traits, VTable* base, ScopeChain* scope, AbcEnv* abcEnv, Toplevel* toplevel)
	{
		AvmAssert(traits != NULL);
		this->traits = traits;
		this->base = base;
		this->scope = scope;
		this->abcEnv = abcEnv;
		this->toplevel = toplevel;
		this->linked = false;
		this->init = NULL;
	}

	void VTable::resolveSignatures()
	{
		if( this->linked )
			return;
		linked = true;
		if (!traits->linked)
			traits->resolveSignatures(toplevel);

		// make sure the traits of the base vtable matches the base traits
		AvmAssert(base == NULL && traits->base == NULL || base != NULL && traits->base == base->traits);

		AvmCore* core = traits->core;

		if (traits->init && !this->init)
		{
			this->init = new (core->GetGC()) MethodEnv(traits->init, this);
		}

		// populate method table

		if (base)
		{
			// inheriting
			int baseMethodCount = base->traits->methodCount;
			for (int i=0, n=traits->methodCount; i < n; i++)
			{
				AbstractFunction* method = traits->getMethod(i);

				if (i < baseMethodCount && method == traits->base->getMethod(i))
				{
					// inherited method
					//this->methods[i] = base->methods[i];
					WB(core->GetGC(), this, &methods[i], base->methods[i]);
				}
				else
				{
					// new definition
					if (method != NULL)
					{
						//this->methods[i] = new (core->GetGC()) MethodEnv(method, this);
						WB(core->GetGC(), this, &methods[i], new (core->GetGC()) MethodEnv(method, this));
					}
					#ifdef AVMPLUS_VERBOSE
					else if (traits->pool->verbose)
					{
						// why would the compiler assign sparse disp_id's?
						traits->core->console << "WARNING: empty disp_id " << i << " on " << traits << "\n";
					}
					#endif
				}
			}

			// this is done here b/c this property of the traits isn't set until the
			// Dictionary's ClassClosure is called
			traits->isDictionary = base->traits->isDictionary;
		}
		else
		{
			// not inheriting
			for (int i=0, n=traits->methodCount; i < n; i++)
			{
				AbstractFunction* method = traits->getMethod(i);
				if (method != NULL)
				{
					MethodEnv *env = new (core->GetGC()) MethodEnv(method, this);
					//this->methods[i] = env;
					WB(core->GetGC(), this, &methods[i], env);
				}
				#ifdef AVMPLUS_VERBOSE
				else if (traits->pool->verbose)
				{
					// why would the compiler assign sparse disp_id's?
					traits->core->console << "WARNING: empty disp_id " << i << " on " << traits << "\n";
				}
				#endif
			}
		}

		if(traits->hasInterfaces)
		{
			for (int i=0; i < Traits::IMT_SIZE; i++)
			{
				Binding b = traits->getIMT()[i];
				if (AvmCore::isMethodBinding(b))
				{
					//imt[i] = methods[AvmCore::bindingToMethodId(b)];
					WB(core->GetGC(), this, &imt[i], methods[AvmCore::bindingToMethodId(b)]);
				}
				else if ((b&7) == BIND_ITRAMP)
				{
					if (base && traits->base->hasInterfaces && b == traits->base->getIMT()[i])
					{
						// copy down imt stub from base class
						//imt[i] = base->imt[i];
						WB(core->GetGC(), this, &imt[i], base->imt[i]);
					}
					else
					{
						// create new imt stub
						//imt[i] = new (core->GetGC()) MethodEnv((void*)(b&~7));
						WB(core->GetGC(), this, &imt[i], new (core->GetGC()) MethodEnv((void*)(b&~7), this));
					}
				}
			}
		}
	}

#ifdef DEBUGGER
	uint32 VTable::size() const
	{
		uint32 size = sizeof(VTable);

		if(ivtable != NULL)
			size += ivtable->size();

		size += traits->numTriplets * 3;

		size += traits->methodCount*sizeof(AbstractFunction*);

		int baseMethodCount = base ? base->traits->methodCount : 0;
		for (int i=0, n=traits->methodCount; i < n; i++)
		{
			AbstractFunction* method = traits->getMethod(i);
			
			if (i < baseMethodCount && traits->base && method == traits->base->getMethod(i))
			{
				continue;
			}
			else if(method != NULL)
			{
				size += method->size();
			}
		}
		return size;
	}
#endif
}
