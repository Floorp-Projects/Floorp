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
 
namespace MMgc
{
	GCWeakRef* GCObject::GetWeakRef() const
	{
		return GC::GetWeakRef(this);
	}

	GCWeakRef* GCFinalizedObject::GetWeakRef() const
	{
		return GC::GetWeakRef(this);
	}
  
	void* GCFinalizedObject::operator new(size_t size, GC *gc, size_t extra)
	{
		return gc->Alloc(size + extra, GC::kFinalize|GC::kContainsPointers|GC::kZero, 4);
	}

	void GCFinalizedObject::operator delete (void *gcObject)
	{
		GC::GetGC(gcObject)->Free(gcObject);
	}		

	void* GCFinalizedObjectOptIn::operator new(size_t size, GC *gc, size_t extra)
	{
		return gc->Alloc(size + extra, GC::kContainsPointers|GC::kZero, 4);
	}

	void GCFinalizedObjectOptIn::operator delete (void *gcObject)
	{
		GC::GetGC(gcObject)->Free(gcObject);
	}		

#if defined(MMGC_DRC) && defined(_DEBUG)
	void RCObject::DumpHistory()
	{			
		GCDebugMsg(false, "Ref count modification history for object 0x%x:\n", this);
		int *traces = history.GetData();
		for(int i=0, n=history.Count(); i<n; i++)
		{
			PrintStackTraceByIndex(traces[i]);
		}
	}
#endif
}
