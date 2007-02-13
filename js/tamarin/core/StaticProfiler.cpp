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
	StaticProfiler::StaticProfiler()
	{
		sprofile = false;
		memset(counts, 0, 256*sizeof(int));
		memset(sizes, 0, 256*sizeof(int));
		totalCount = 0;
		totalSize = 0;
		cpoolSize = 0;
		cpoolIntSize = 0;
		cpoolUIntSize = 0;
		cpoolDoubleSize = 0;
		cpoolStrSize = 0;
		cpoolNsSize = 0;
		cpoolNsSetSize = 0;
		cpoolMnSize = 0;
		bodiesSize = 0;
		methodsSize = 0;
		instancesSize = 0;
		classesSize = 0;
		scriptsSize = 0;
	}

	void StaticProfiler::dump(PrintWriter& console)
    {
        if (sprofile)
        {
			console << "verified instructions "
					<< totalCount
					<< '\n';
			console << "verified code size "
					<< totalSize
					<< '\n';
			console << "cpool size "
					<< cpoolSize
					<< '\n';
			console << "cpool int size "
					<< cpoolIntSize
					<< '\n';
			console << "cpool uint size "
					<< cpoolUIntSize
					<< '\n';
			console << "cpool double size "
					<< cpoolDoubleSize
					<< '\n';
			console << "cpool string size "
					<< cpoolStrSize
					<< '\n';
			console << "cpool namespacesize "
					<< cpoolNsSize
					<< '\n';
			console << "cpool namespace set size "
					<< cpoolNsSetSize
					<< '\n';
			console << "cpool multiname size "
					<< cpoolMnSize
					<< '\n';
			console << "methods size "
					<< methodsSize
					<< '\n';
			console << "instances size "
					<< instancesSize
					<< '\n';
			console << "classes size "
					<< classesSize
					<< '\n';
			console << "scripts size "
					<< scriptsSize
					<< '\n';
			console << "bodies size "
					<< bodiesSize
					<< '\n';
			for (int i=0; i < 256; i++)
			{
				if (counts[i] != 0)
				{
					console << counts[i]
							<< '\t'
							<< (100*counts[i]/totalCount)
							<< " %\t"
							<< sizes[i]
							<< " B\t"
							<< (100*sizes[i]/totalSize)
							<< " %\t "
							<< opNames[i]
							<< '\n';
				}
			}
		}
	}
	
}	
#endif
