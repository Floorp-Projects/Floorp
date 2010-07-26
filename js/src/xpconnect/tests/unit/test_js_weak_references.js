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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jwatt@jwatt.org> (original author)
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

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=317304 */

function run_test()
{
  var obj = { num: 5, str: 'foo' };
  var weak = Components.utils.getWeakReference(obj);

  do_check_true(weak.get() === obj);
  do_check_true(weak.get().num == 5);
  do_check_true(weak.get().str == 'foo');

  // Force garbage collection
  Components.utils.forceGC();

  // obj still references the object, so it should still be accessible via weak
  do_check_true(weak.get() === obj);
  do_check_true(weak.get().num == 5);
  do_check_true(weak.get().str == 'foo');

  // Clear obj's reference to the object and force garbage collection. To make
  // sure that there are no instances of obj stored in the registers or on the
  // native stack and the conservative GC would not find it we force the same
  // code paths that we used for the initial allocation.
  obj = { num: 6, str: 'foo2' };
  var weak2 = Components.utils.getWeakReference(obj);
  do_check_true(weak2.get() === obj);

  Components.utils.forceGC();

  // The object should have been garbage collected and so should no longer be
  // accessible via weak
  do_check_true(weak.get() === null);
}
