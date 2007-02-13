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

#include <stddef.h>

#include "avmplus.h"

namespace avmplus
{
	/**
	 * call a native method using powerpc calling conventions.
	 * we use the traits of the function to figure out what native types
	 * to convert to, as follows:
	 *
	 *  AS type         C++ type
     *  -------         --------
	 *  Void            void
     *  Object          Atom
	 *  Boolean         bool      (unsigned 1bit)
	 *  int             int       (signed 32bit)
	 *  uint            unsigned  (unsigned 32bit)
	 *  Number          double    (float 64bit)
	 *  String          String*
	 *  Class           ClassClosure*
	 *  MovieClip       MovieClipObject*   (similar for any other class)
	 *
	 * The PowerPC uses a standard calling convention with registers
	 * r3-r10 used for the first 8 non-floating point arguments,
	 * from left to right.  Floating point registers f1-f13 are used
	 * for the first 13 floating point arguments.
	 * The return value is placed in r3 (non-FP) or f1 (FP).
	 *
	 * All arguments are pushed on the stack.  
	 *
	 */
}
