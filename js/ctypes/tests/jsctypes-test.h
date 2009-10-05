/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Fredrik Larsson <nossralf@gmail.com>
 *  Mark Finkle <mark.finkle@gmail.com>, <mfinkle@mozilla.com>
 *  Dan Witte <dwitte@mozilla.com>
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

#include "nscore.h"
#include "prtypes.h"

NS_EXTERN_C
{
  NS_EXPORT void test_v();

  NS_EXPORT PRInt8 test_i8();
  NS_EXPORT PRInt8 test_i8_i8(PRInt8);
  NS_EXPORT PRInt8 test_i8_i8_sum(PRInt8, PRInt8);

  NS_EXPORT PRInt16 test_i16();
  NS_EXPORT PRInt16 test_i16_i16(PRInt16);
  NS_EXPORT PRInt16 test_i16_i16_sum(PRInt16, PRInt16);

  NS_EXPORT PRInt32 test_i32();
  NS_EXPORT PRInt32 test_i32_i32(PRInt32);
  NS_EXPORT PRInt32 test_i32_i32_sum(PRInt32, PRInt32);

  NS_EXPORT PRInt64 test_i64();
  NS_EXPORT PRInt64 test_i64_i64(PRInt64);
  NS_EXPORT PRInt64 test_i64_i64_sum(PRInt64, PRInt64);

  NS_EXPORT float test_f();
  NS_EXPORT float test_f_f(float);
  NS_EXPORT float test_f_f_sum(float, float);

  NS_EXPORT double test_d();
  NS_EXPORT double test_d_d(double);
  NS_EXPORT double test_d_d_sum(double, double);

  NS_EXPORT PRInt32 test_ansi_len(const char*);
  NS_EXPORT PRInt32 test_wide_len(const PRUnichar*);
  NS_EXPORT const char* test_ansi_ret();
  NS_EXPORT const PRUnichar* test_wide_ret();
  NS_EXPORT char* test_ansi_echo(const char*);

  NS_EXPORT PRInt32 test_floor(PRInt32, float);
}

