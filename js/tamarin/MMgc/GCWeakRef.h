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

//
// GCWeakRef.h
// GC weak references (aka safe handles) as template classes
//

#ifndef _GC_WEAK_REF_H_
#define _GC_WEAK_REF_H_

namespace MMgc
{
	// new improved weak ref
	class GCWeakRef : public GCFinalizedObject
	{
		friend class GC;
	public:
		GCObject *get() { return (GCObject*)m_obj; }
		~GCWeakRef() 
		{ 
			if(m_obj) {
				GC::GetGC(this)->ClearWeakRef(m_obj); 
			}
		}
	private:
		// override new so we can be tell the GC we don't contain pointers
		static void *operator new(size_t size, GC *gc)
		{
			return gc->Alloc(size, GC::kFinalize, 4);
		}
		// private, only GC can access
		GCWeakRef(const void *obj) : m_obj(obj) 
		{
#ifdef _DEBUG
			obj_creation = *((int*)GetRealPointer(obj)+1);
#endif
		}
		const void *m_obj;
#ifdef _DEBUG
		int obj_creation;
#endif
	};

#if 0
	// something like this would be nice
	template<class T> class GCWeakRefPtr 
	{
		
	public:
		GCWeakRefPtr() {}
		GCWeakRefPtr(T t) {  set(t);}
		~GCWeakRefPtr() { t = NULL;	}

		T operator=(const GCWeakRefPtr<T>& wb)
		{
			return set(wb.t);	
		}

		T operator=(T tNew)
		{
			return set(tNew);
		}

		operator T() const { return (T) t->get(); }

		bool operator!=(T other) const { return other != t; }

		T operator->() const
		{
			return (T) t->get();
		}
	private:		
		T set(const T tNew)
		{
			t = tNew->GetWeakRef();
		}
		GCWeakRef* t;
	};
#endif
}

#endif // _GC_WEAK_REF_H_
