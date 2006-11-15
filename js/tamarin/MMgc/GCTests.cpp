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
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
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

 
#include "MMgc.h"
 
#ifdef _MSC_VER
// "behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized"
#pragma warning(disable:4345) // b/c GCObject doesn't have a ctor
#ifdef _DEBUG
// we turn on exceptions in DEBUG builds
#pragma warning(disable:4291) // no matching operator delete found; memory will not be freed if initialization throws an exception
#endif
#endif

#ifdef _DEBUG

namespace MMgc
{
	GC *gc;

	GCWeakRef* createWeakRef(int extra=0)
	{
		return (new (gc, extra) GCObject())->GetWeakRef();
	}

	void weakRefSweepSmall()
	{
		GCWeakRef *ref = createWeakRef();
		gc->Collect();
		gc->CleanStack(true);
		gc->Collect();
		(void)ref;
		GCAssert(ref->get() == NULL);
	}

	void weakRefSweepLarge()
	{
		GCWeakRef *ref = createWeakRef(5000);
		gc->Collect();
		gc->CleanStack(true);
		gc->Collect();
		(void)ref;
		GCAssert(ref->get() == NULL);
	}

	void weakRefFreeSmall()
	{
		GCWeakRef *ref = createWeakRef();
		delete ref->get();
		GCAssert(ref->get() == NULL);
	}

	void weakRefFreeLarge()
	{
		GCWeakRef *ref = createWeakRef(5000);
		delete ref->get();
		GCAssert(ref->get() == NULL);
	}

	class RCObjectAddRefInDtor : public RCObject
	{
	public:
		RCObjectAddRefInDtor(RCObject ** _stackPinners, int _length) : rcs(_stackPinners), length(_length) {}
		~RCObjectAddRefInDtor() 
		{ 
			// whack these, used create freelist
			for(int i=0, n=length; i<n;i++)
			{
				r1 = rcs[i];
			}

			// add/remove myself (this was the apollo bug)
			r1 = this;
			r1 = NULL;
			rcs = NULL;
			length = 0;
		}
		DRCWB(RCObject*) r1;

		// naked pointer so I can kick these pinners out out of the ZCT during reap
		RCObject **rcs;
		int length;
	};

	GCWeakRef* createProblem(RCObject **stackPinners)
	{
		// now create one that causes some removes from the dtor
		return (new (gc) RCObjectAddRefInDtor(stackPinners, 3))->GetWeakRef();
	}

	/* see bug 182420 */
	void drcApolloTest()
	{
		// prime ZCT with some pinners
		RCObject *stackPinners[3];

		GCWeakRef *wr = createProblem(stackPinners);

		stackPinners[0] = new (gc) RCObject();
		stackPinners[1] = new (gc) RCObject();
		stackPinners[2] = new (gc) RCObject();

		// force ZCT
		for(int i=0, n=1000;i<n; i++)
		{
			new (gc) RCObject();
		}

		// it may still be alive if we had a dirty stack
		if(wr->get())
			delete wr->get();
	}

	void RunGCTests(GC *g)
	{
		gc = g;
		weakRefSweepSmall();
		weakRefSweepLarge();
		weakRefFreeSmall();
		weakRefFreeLarge();
		drcApolloTest();
	}
}

#endif // _DEBUG
