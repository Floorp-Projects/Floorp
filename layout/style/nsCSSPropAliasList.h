/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsCSSPropAliasList.h.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

/*
 * a list of all CSS property aliases with data about them, for preprocessing
 */

/******

  This file contains the list of all CSS properties that are just
  aliases for other properties (e.g., for when we temporarily continue
  to support a prefixed property after adding support for its unprefixed
  form).  It is designed to be used as inline input through the magic of
  C preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_ALIAS macro which will have cruel and unusual things done to
  it.

  The arguments to CSS_PROP_ALIAS are:

  -. 'aliasname' entries represent a CSS property name and *must* use
  only lowercase characters.

  -. 'id' should be the same as the 'id' field in nsCSSPropList.h for
  the property that 'aliasname' is being aliased to.

  -. 'method' is the CSS2Properties property name.  Unlike
  nsCSSPropList.h, prefixes should just be included in this file (rather
  than needing the CSS_PROP_DOMPROP_PREFIXED(prop) macro).

 ******/

