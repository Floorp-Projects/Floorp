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
 * The Original Code is XPConnect code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsCSSPropertiesQS_h__
#define nsCSSPropertiesQS_h__

#include "nsICSSDeclaration.h"

#define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_, type_,     \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)        \
static const nsCSSProperty QS_CSS_PROP_##method_ = eCSSProperty_##id_;

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) \
  CSS_PROP(name_, id_, method_, flags_, X, X, X, X, X, X, X)
#include "nsCSSPropList.h"

// Aliases
CSS_PROP(X, opacity, MozOpacity, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline, MozOutline, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_color, MozOutlineColor, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_style, MozOutlineStyle, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_width, MozOutlineWidth, 0, X, X, X, X, X, X, X)
CSS_PROP(X, outline_offset, MozOutlineOffset, 0, X, X, X, X, X, X, X)

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP

#endif /* nsCSSPropertiesQS_h__ */
