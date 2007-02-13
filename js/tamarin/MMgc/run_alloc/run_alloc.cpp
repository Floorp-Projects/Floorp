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


#include <stdio.h>
#include <stdlib.h>

#include "avmplus.h"
#include "MMgc.h"

using namespace MMgc;
using namespace avmplus;

int main(int argc, char*argv[])
{
	GCHeap::Init();
	GC *gc = new GC(GCHeap::GetGCHeap());
	int buffer[1024];

	List<void*> allocations(gc, 1024*16, false);

	FILE *file = fopen("alloc.log", "rb");
	int read = 0;

	while((read = fread(buffer, 4, 1024, file)) > 0)
	{
		for(int i=0; i < read; i++) {
			int allocSize = buffer[i];

			// positive # is alloc, negative is index of alloc to free
			if(allocSize > 0)
				allocations.add(gc->Alloc(allocSize&~7,  allocSize&6));
			else {
				int index = -allocSize;
				void *obj = allocations[index];
				allocations.set(index, 0);
				//gc->Free(obj);
			}
		}
	}
	fclose(file);
}
