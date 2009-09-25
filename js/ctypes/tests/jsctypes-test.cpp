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

#include "jsctypes-test.h"
#include "nsCRTGlue.h"
#include <string.h>
#include <math.h>

void
test_v()
{
  // do nothing
  return;
}

PRInt8
test_i8()
{
  return 123;
}

PRInt8
test_i8_i8(PRInt8 number)
{
  return number;
}

PRInt8
test_i8_i8_sum(PRInt8 number1, PRInt8 number2)
{
  return number1 + number2;
}

PRInt16
test_i16()
{
  return 12345;
}

PRInt16
test_i16_i16(PRInt16 number)
{
  return number;
}

PRInt16
test_i16_i16_sum(PRInt16 number1, PRInt16 number2)
{
  return number1 + number2;
}

PRInt32
test_i32()
{
  return 123456789;
}

PRInt32
test_i32_i32(PRInt32 number)
{
  return number;
}

PRInt32
test_i32_i32_sum(PRInt32 number1, PRInt32 number2)
{
  return number1 + number2;
}

PRInt64
test_i64()
{
#if defined(WIN32) && !defined(__GNUC__)
  return 0x28590a1c921de000i64;
#else
  return 0x28590a1c921de000LL;
#endif
}

PRInt64
test_i64_i64(PRInt64 number)
{
  return number;
}

PRInt64
test_i64_i64_sum(PRInt64 number1, PRInt64 number2)
{
  return number1 + number2;
}

float
test_f()
{
  return 123456.5f;
}

float
test_f_f(float number)
{
  return number;
}

float
test_f_f_sum(float number1, float number2)
{
  return (number1 + number2);
}

double
test_d()
{
  return 1234567890123456789.5;
}

double
test_d_d(double number)
{
  return number;
}

double
test_d_d_sum(double number1, double number2)
{
  return (number1 + number2);
}

PRInt32
test_ansi_len(const char* string)
{
  return PRInt32(strlen(string));
}

PRInt32
test_wide_len(const PRUnichar* string)
{
  return PRInt32(NS_strlen(string));
}

const char *
test_ansi_ret()
{
  return "success";
}

const PRUnichar *
test_wide_ret()
{
  static const PRUnichar kSuccess[] = {'s', 'u', 'c', 'c', 'e', 's', 's', '\0'};
  return kSuccess;
}

char *
test_ansi_echo(const char* string)
{
  return (char*)string;
}

PRInt32
test_floor(PRInt32 number1, float number2)
{
  return PRInt32(floor(float(number1) + number2));
}

