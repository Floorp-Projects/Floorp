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
#ifdef DEBUGGER
	void CallStackNode::initialize(MethodEnv *			env,
								   AbstractFunction *	info,
								   Atom*				framep,
								   Traits**				traits,
								   int					argc,
								   uint32 *				ap,
								   sintptr volatile *	eip)
	{
		AvmCore *core = info->core();
		this->env       = env;
		this->info      = info;
		this->ap        = ap;
		this->argc      = argc;
		this->framep	= framep;  // pointer to top of AS registers
		this->traits    = traits;  // pointer to traits of top of AS registers
		this->eip		= eip;     // ptr to where the current instruction pointer is stored
		filename        = NULL;
		linenum         = 0;

		// scopechain stuff
		#ifdef AVMPLUS_INTERP
		this->scopeDepth = NULL;
		#endif

		// link into callstack
		next            = core->callStack;
		core->callStack = this;

		depth           = next ? (next->depth+1) : 1;
		AvmAssert(info != NULL);
	}

	void CallStackNode::exit()
	{
		info->core()->callStack = next;
		next = NULL;
	}

	void** CallStackNode::scopeBase()
	{
		// If we were given a real frame, calculate the scope base; otherwise return NULL
		if (framep && info)
			return (void**) (framep + ((MethodInfo*)info)->local_count);
		else
			return NULL;
	}

	// Dump a filename.  The incoming filename is of the form
	// "C:\path\to\package\root;package/package;filename".  The path format
	// will depend on the platform on which the movie was originally
	// compiled, NOT the platform the the player is running in.
	//
	// We want to replace the semicolons with path separators.  We'll take
	// a guess at the appropriate path separator of the compilation
	// platform by looking for any backslashes in the path.  If there are
	// any, then we'll assume backslash is the path separator.  If not,
	// we'll use forward slash.
	void StackTrace::dumpFilename(Stringp filename, PrintWriter& out) const
	{
		wchar semicolonReplacement = '/';
		int length = filename->length();
		wchar ch;
		int i;

		// look for backslashes; if there are any, then semicolons will be
		// replaced with backslashes, not forward slashes
		for (i=0; i<length; ++i) {
			ch = (*filename)[i];
			if (ch == '\\') {
				semicolonReplacement = '\\';
				break;
			}
		}

		// output the entire path
		bool previousWasSlash = false;
		for (i=0; i<length; ++i) {
			ch = (*filename)[i];
			if (ch == ';') {
				if (previousWasSlash)
					continue;
				ch = semicolonReplacement;
				previousWasSlash = true;
			} else if (ch == '/' || ch == '\\') {
				previousWasSlash = true;
			} else {
				previousWasSlash = false;
			}
			out << ch;
		}
	}

	bool StackTrace::equals(StackTrace::Element *e, int depth)
	{
		if(this->depth != depth)
			return false;

		Element *me = elements;
		while(depth--)
		{
			if(me->info != e->info || me->filename != e->filename || me->linenum != e->linenum)
				return false;
			me++;
			e++;
		}
		return true;
	}

	/*static*/
	uintptr StackTrace::hashCode(StackTrace::Element *e, int depth)
	{
		uintptr hashCode = 0;
		while(depth--)
		{
			hashCode ^= uintptr(e->info)>>3;
			hashCode ^= uintptr(e->filename)>>3;
			hashCode ^= (uintptr)e->linenum;
			e++;
		}
		return hashCode;
	}

	Stringp StackTrace::format(AvmCore* core)
	{
		if(!stringRep)
		{
			Stringp s = core->kEmptyString;
			int displayDepth = depth;
			if (displayDepth > kMaxDisplayDepth) {
				displayDepth = kMaxDisplayDepth;
			}
			const Element *e = elements;
			for (int i=0; i<displayDepth; i++, e++)
			{
				if(i != 0)
					s = core->concatStrings(s, core->newString("\n"));
				s = core->concatStrings(s, core->newString("\tat "));
				s = core->concatStrings(s, e->info->format(core));
				if(e->filename)
				{
					StringBuffer out(core);
					out << '[';
					dumpFilename(e->filename, out);
					out << ':'
						<< (e->linenum)
						<< ']';
					s = core->concatStrings(s, core->newString(out.c_str()));
				}
			}
            stringRep = s;
		}
		return stringRep;
	}

	
	FakeCallStackNode::FakeCallStackNode(AvmCore *core, const char *name)
	{
		this->core = core;
		if(core && core->sampling && core->fakeMethodInfos && core->builtinPool)
		{
			this->core = core;
			// have to turn sampling off during allocations to avoid recursion
			core->sampling = false;
			Stringp s = core->internAllocAscii(name);
			Atom a = core->fakeMethodInfos->get(s->atom());
			AbstractFunction *af = (AbstractFunction*)AvmCore::atomToGCObject(a);
			if(!af)
			{
				af = new (core->GetGC()) FakeAbstractFunction();
				a = AvmCore::gcObjectToAtom(af);
				core->fakeMethodInfos->add(s->atom(), a);
				af->name = s;
				af->pool = core->builtinPool;
			}
			initialize(NULL, af, NULL, NULL, 0, NULL, NULL);
			core->sampling = true;
			core->sampleCheck();
		}
		else
		{
			memset(this, 0, sizeof(FakeCallStackNode));
		}
	}

	FakeCallStackNode::~FakeCallStackNode()
	{
		if(core)
		{
			exit();
			core->sampleCheck();
		}
	}
#endif /* DEBUGGER */
}	
