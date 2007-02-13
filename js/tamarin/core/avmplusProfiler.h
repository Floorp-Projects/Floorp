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

#ifndef __avmplus_AvmProfiler__
#define __avmplus_AvmProfiler__


namespace avmplus
{
#ifdef DEBUGGER
	/**
	 * The ScriptObjectTable is used to track what objects are
	 * in the system, for heap dumps.
	 */
	class ScriptObjectTable : public MMgc::GCObject
	{
	public:
		ScriptObjectTable(AvmCore *core);
		
		/**
		 * This will be called when a ScriptObject is constructed.
		 * The ScriptObject will be added to the scriptObjectTable.
		 *
		 * There will be a matching call to deleteScriptObject
		 * upon ScriptObject destruction.
		 *
		 * @param scriptObject the address to the ScriptObject
		 *                     being constructed.
		 */
		void addScriptObject(ScriptObject *object);

		/**
		 * This will be called when a ScriptObject is destroyed.
		 * The ScriptObject will be removed from the scriptObjectTable.
		 *
		 * There will be a matching call to addScriptObject
		 * upon ScriptObject construction.
		 *
		 * @param scriptObject the address to the ScriptObject
		 *                     being destroyed.
		 */
		void deleteScriptObject(ScriptObject *object);

		/**
		 * Used to iterate over the contents of the table.
		 */
		bool nextObject(int& index,
						ScriptObject*& object,
						StackTrace*& stackTrace);

		/**
		 * Returns # of objects in table
		 */
		int size() const { return table->getSize(); }
		
	private:
		AvmCore *core;
		DWB(Hashtable*) table;

		static Atom mask(ScriptObject *object) {
			return object->atom() ^ 0xFFFFFFF8;
		}

		static ScriptObject* unmask(Atom atom) {
			return AvmCore::atomToScriptObject(atom ^ 0xFFFFFFF8);
		}
	};
	
	/**
	 * The Profiler class is a pure virtual base class.  It specifies
	 * an interface which must be implemented by profilers that
	 * wish to plug in to the AVM+ virtual machine.
	 *
	 * A program embedding AVM+ may set AvmCore::profiler to an
	 * instance of a Profiler subclass.  AvmCore::profiler will be
	 * informed about events of interest to profilers, such as
	 * function entry and exit and execution of each line of code.
	 */
	class Profiler : public MMgc::GCCallback
	{
	public:
		Profiler(MMgc::GC *gc) : MMgc :: GCCallback(gc){}

		virtual ~Profiler() {}

		/**
		 * This will be called to notify the profiler that the
		 * source file for which line number information is reported
		 * has changed.
		 * @param url the URL of the new source file, as encoded
		 *            in the OP_debugfile bytecode
		 */
		virtual void sendDebugFileUrl(UTF8String *url) = 0;

		/**
		 * This will be called to notify the profiler that the
		 * line number has changed.
		 * @param linenumber the line number of the source file, as encoded
		 *                   in the OP_debugline bytecode
		 */
		virtual void sendLineTimestamp(int linenumber) = 0;

		/**
		 * This will be called when ActionScript profile(boolean)
		 * is called. profile(boolean) is used to turn on
		 * and off profiling, so that certain ActionScript
		 * can be profiled.
		 * @param on the setting for profiling. true turns
		 *           profiling on, false turns it off for
		 *           this section of ActionScript.
		 */
		virtual void setEnabled(bool on) = 0;

		/**
		 * This will be called to notify the profiler that a
		 * function has been entered.
		 *
		 * There will be a matching call to sendFunctionExit,
		 * unless an exception is thrown.
		 *
		 * @param method  the method being entered
		 */
		virtual void sendFunctionEnter(AbstractFunction* method) = 0;

		/**
		 * This will be called to notify the profiler that a
		 * function has been exited.
		 *
		 * This is always called to match a previous call to
		 * sendFunctionEnter.
		 */
		virtual void sendFunctionExit() = 0;

		/**
		 * This will be called to notify the profiler that an
		 * exception was thrown and execution will commence in 
		 * the given method's catch handler 
		 *
		 * @param method  the method being entered
		 */
		virtual void sendCatch(AbstractFunction* method) = 0;

		/**
		 * This is called when we want to write out heap profiling data.
		 * ActionScript function heapdump() triggers this call.
		 *
		 * @param heapDumpName enables the user to label this heap dump.
		 *                     This string will be sent down to the
		 *                     profiling output so that the user can
		 *                     refer to it later in the profiling output.
		 *                     heapDumpName should be NULL if the
		 *                     user calls ActionScript heapdump()
		 *                     with no parameter to label the dump.
		 */
		virtual void sendHeapDump(String *heapDumpName) = 0;
		
		/**
		 * This returns the size of the String, including the String type size,
		 * and the length of the String multiplied by the wchar size.
		 *
		 * @param myString the address to the String for which size
		 * should be computed.
		 */
		virtual int computeStringSize(String *myString) = 0;

		/**
		 * Is profiling wanted at all?
		 */
		bool profilingDataWanted;

		/**
		 * Is heap profiling wanted?
		 */
		bool heapProfilingWanted;

		/**
		 * Is profiling on right now, given the usage of AS profile(boolean)?
		 * The main code to turn on and off this value is Profiler::SetProfileSwitch().
		 */
		bool profileSwitch;
	};
#endif
}

#endif /* __avmplus_AvmProfiler__ */
