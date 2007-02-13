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

namespace avmplus
{
	using namespace MMgc;

	StringOutputStream::StringOutputStream(GC *gc)
	{
		GCAssert(!gc->IsPointerToGCPage(this));
		m_buffer = (char*) gc->Alloc(kInitialCapacity);
		m_buffer[0] = 0;
		m_length = 0;
	}

	StringOutputStream::~StringOutputStream()
	{
		if (m_buffer) {
			GC* gc = MMgc::GC::GetGC(m_buffer);
			gc->Free(m_buffer);
		}
	}

	int StringOutputStream::write(const void *buffer,
								  int count)
	{
		if (m_length + count >= (int)GC::Size(m_buffer))
		{
			GC* gc = MMgc::GC::GetGC(m_buffer);
			int newCapacity = (m_length+count+1)*2;
			char *newBuffer = (char*) gc->Alloc(newCapacity);
			if (!newBuffer) {
				return 0;
			}
			memcpy(newBuffer, m_buffer, m_length);
			gc->Free(m_buffer);
			m_buffer = newBuffer;
		}
		memcpy(m_buffer+m_length, buffer, count);
		m_length += count;
		m_buffer[m_length] = 0;
		return count;
	}
}
