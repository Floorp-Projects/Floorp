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

NS_EXTERN_C
{
  NS_EXPORT void test_v();

  NS_EXPORT short test_s();
  NS_EXPORT short test_s_s(short);
  NS_EXPORT short test_s_ss(short, short);

  NS_EXPORT int test_i();
  NS_EXPORT int test_i_i(int);
  NS_EXPORT int test_i_ii(int, int);

  NS_EXPORT float test_f();
  NS_EXPORT float test_f_f(float);
  NS_EXPORT float test_f_ff(float, float);

  NS_EXPORT double test_d();
  NS_EXPORT double test_d_d(double);
  NS_EXPORT double test_d_dd(double, double);

  NS_EXPORT int test_ansi_len(const char*);
  NS_EXPORT int test_wide_len(const PRUnichar*);
  NS_EXPORT const char* test_ansi_ret();
  NS_EXPORT const PRUnichar* test_wide_ret();
  NS_EXPORT char* test_ansi_echo(const char*);

  NS_EXPORT int test_i_if_floor(int, float);

  struct POINT {
    int x;
    int y;
  };

  struct RECT {
    int top;
    int left;
    int bottom;
    int right;
  };

  struct INNER {
    unsigned char i1;
    long long int i2;
    char i3;
  };

  struct NESTED {
    int n1;
    short n2;
    INNER inner;
    long long int n3;
    int n4;
  };

  NS_EXPORT int test_pt_in_rect(RECT, POINT);
  NS_EXPORT int test_nested_struct(NESTED);
  NS_EXPORT POINT test_struct_return(RECT);
}
