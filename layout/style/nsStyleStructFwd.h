/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * Forward declarations to avoid including all of nsStyleStruct.h.
 */

#ifndef nsStyleStructFwd_h_
#define nsStyleStructFwd_h_

enum nsStyleStructID {

/*
 * Define the constants eStyleStruct_Font, etc.
 *
 * The C++ standard, section 7.2, guarantees that enums begin with 0 and
 * increase by 1.
 *
 * We separate the IDs of Reset and Inherited structs so that we can use
 * the IDs as indices (offset by nsStyleStructID_*_Start) into arrays of
 * one type or the other.
 */

nsStyleStructID_Inherited_Start = 0,
// a dummy value so the value after it is the same as ..._Inherited_Start
nsStyleStructID_DUMMY1 = nsStyleStructID_Inherited_Start - 1,

#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
  eStyleStruct_##name,
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

nsStyleStructID_Reset_Start,
// a dummy value so the value after it is the same as ..._Reset_Start
nsStyleStructID_DUMMY2 = nsStyleStructID_Reset_Start - 1,

#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
  eStyleStruct_##name,
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args)
#include "nsStyleStructList.h"
#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

// one past the end; length of 0-based list
nsStyleStructID_Length,

nsStyleStructID_Inherited_Count =
  nsStyleStructID_Reset_Start - nsStyleStructID_Inherited_Start,
nsStyleStructID_Reset_Count =
  nsStyleStructID_Length - nsStyleStructID_Reset_Start,

// An ID used for properties that are not in style structs.  This is
// used only in some users of nsStyleStructID, such as
// nsCSSProps::kSIDTable, including some that store SIDs in a bitfield,
// such as nsCSSCompressedDataBlock::mStyleBits.
eStyleStruct_BackendOnly = nsStyleStructID_Length

};

// A bit corresponding to each struct ID
#define NS_STYLE_INHERIT_BIT(sid_)        (1 << PRInt32(eStyleStruct_##sid_))

#endif /* nsStyleStructFwd_h_ */
