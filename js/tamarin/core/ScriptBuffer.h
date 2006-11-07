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

#ifndef __avmplus_ScriptBuffer__
#define __avmplus_ScriptBuffer__


namespace avmplus
{
	/**
	 * ScriptBufferImpl is the base class for script buffer
	 * implementations.
	 *
	 * This is a pure base class which must be subclassed to be used.
	 * Its methods are not virtual for performance reasons.
	 * For default script buffer behavior, use BasicScriptBufferImpl. 
	 */
	class ScriptBufferImpl
	{
	public:
		operator byte* () const { return buffer; }
		size_t getSize() const { return size; }
		byte* getBuffer() const { return (byte*)buffer; }
		
		byte operator[] (int index) const {
			return buffer[index];
		}
		byte& operator[] (int index) {
			return buffer[index];
		}
	protected:
		byte *buffer;
		size_t size;
	};

	/**
	 * BasicScriptBufferImpl is a ScriptBuffer implementation that
	 * owns its own buffer and has no external dependencies.
	 */
	class BasicScriptBufferImpl : public ScriptBufferImpl
	{
	public:
		BasicScriptBufferImpl(size_t size) {
			this->size = size;
			buffer = (byte*)(this+1);
		}
		// override to skip memset and prevent marking
		static void *operator new(size_t size, MMgc::GC *gc, size_t extra = 0)
		{
			return gc->Alloc(size + extra, 0, 4);
		}
	};

	/**
	 * ReadOnlyScriptBufferImpl is a ScriptBuffer implementation that
	 * points to an external buffer that must stay around longer than
	 * the script buffer itself.
	 */
	class ReadOnlyScriptBufferImpl : public ScriptBufferImpl
	{
	public:
		ReadOnlyScriptBufferImpl(const byte *buf, size_t size) {
			this->size = size;
			this->buffer = (byte*) buf;;
		}
	};
	/**
	 * ScriptBuffer is a "handle" for ScriptBufferImpl which is more convenient
	 * to pass around and use than a ScriptBufferImpl pointer.  It defines
	 * operator[] to make a ScriptBuffer look like an array.
	 */
	class ScriptBuffer
	{
	public:
		ScriptBuffer() {
			m_impl = NULL;
		}
		ScriptBuffer(ScriptBufferImpl* rep) {
			m_impl = rep;
		}
		ScriptBuffer(const ScriptBuffer& toCopy) {
			m_impl = toCopy.m_impl;
		}
		ScriptBuffer& operator= (const ScriptBuffer& toCopy) {
			m_impl = toCopy.m_impl;
			return *this;
		}
		ScriptBufferImpl* getImpl() const {
			return m_impl;
		}
		operator ScriptBufferImpl*() const {
			return m_impl;
		}
		operator byte* () const {
			return m_impl->getBuffer();
		}
		size_t getSize() const {
			return m_impl->getSize();
		}
		byte* getBuffer() const {
			return m_impl->getBuffer();
		}
		byte operator[] (int index) const {
			return (*m_impl)[index];
		}
		byte& operator[] (int index) {
			return (*m_impl)[index];
		}

	private:
		ScriptBufferImpl* m_impl;
	};
}

#endif /* __avmplus_ScriptBuffer__ */
