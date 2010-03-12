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
test_void_t_cdecl()
{
  // do nothing
  return;
}

#define DEFINE_TYPE(name, type, ffiType)                                       \
type                                                                           \
get_##name##_cdecl()                                                           \
{                                                                              \
  return 109.25;                                                               \
}                                                                              \
                                                                               \
type                                                                           \
set_##name##_cdecl(type x)                                                     \
{                                                                              \
  return x;                                                                    \
}                                                                              \
                                                                               \
type                                                                           \
sum_##name##_cdecl(type x, type y)                                             \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type                                                                           \
sum_alignb_##name##_cdecl(char a, type x, char b, type y, char c)              \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type                                                                           \
sum_alignf_##name##_cdecl(float a, type x, float b, type y, float c)           \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type                                                                           \
sum_many_##name##_cdecl(                                                       \
  type a, type b, type c, type d, type e, type f, type g, type h, type i,      \
  type j, type k, type l, type m, type n, type o, type p, type q, type r)      \
{                                                                              \
  return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r;\
}                                                                              \
                                                                               \
struct align_##name {                                                          \
  char x;                                                                      \
  type y;                                                                      \
};                                                                             \
struct nested_##name {                                                         \
  char a;                                                                      \
  align_##name b;                                                              \
  char c;                                                                      \
};                                                                             \
                                                                               \
void                                                                           \
get_##name##_stats(size_t* align, size_t* size, size_t* nalign, size_t* nsize, \
                   size_t offsets[])                                           \
{                                                                              \
  *align = offsetof(align_##name, y);                                          \
  *size = sizeof(align_##name);                                                \
  *nalign = offsetof(nested_##name, b);                                        \
  *nsize = sizeof(nested_##name);                                              \
  offsets[0] = offsetof(align_##name, y);                                      \
  offsets[1] = offsetof(nested_##name, b);                                     \
  offsets[2] = offsetof(nested_##name, c);                                     \
}

#include "../typedefs.h"

#if defined(_WIN32) && !defined(__WIN64)

#define DEFINE_TYPE(name, type, ffiType)                                       \
type NS_STDCALL                                                                \
get_##name##_stdcall()                                                         \
{                                                                              \
  return 109.25;                                                               \
}                                                                              \
                                                                               \
type NS_STDCALL                                                                \
set_##name##_stdcall(type x)                                                   \
{                                                                              \
  return x;                                                                    \
}                                                                              \
                                                                               \
type NS_STDCALL                                                                \
sum_##name##_stdcall(type x, type y)                                           \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type NS_STDCALL                                                                \
sum_alignb_##name##_stdcall(char a, type x, char b, type y, char c)            \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type NS_STDCALL                                                                \
sum_alignf_##name##_stdcall(float a, type x, float b, type y, float c)         \
{                                                                              \
  return x + y;                                                                \
}                                                                              \
                                                                               \
type NS_STDCALL                                                                \
sum_many_##name##_stdcall(                                                     \
  type a, type b, type c, type d, type e, type f, type g, type h, type i,      \
  type j, type k, type l, type m, type n, type o, type p, type q, type r)      \
{                                                                              \
  return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r;\
}

#include "../typedefs.h"

void NS_STDCALL
test_void_t_stdcall()
{
  // do nothing
  return;
}

#endif /* defined(_WIN32) && !defined(__WIN64) */

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
test_pt_in_rect(RECT rc, POINT pt)
{
  if (pt.x < rc.left || pt.x > rc.right)
    return 0;
  if (pt.y < rc.bottom || pt.y > rc.top)
    return 0;
  return 1;
}

void
test_init_pt(POINT* pt, PRInt32 x, PRInt32 y)
{
  pt->x = x;
  pt->y = y;
}

PRInt32
test_nested_struct(NESTED n)
{
  return PRInt32(n.n1 + n.n2 + n.inner.i1 + n.inner.i2 + n.inner.i3 + n.n3 + n.n4);
}

POINT
test_struct_return(RECT r)
{
  POINT p;
  p.x = r.left; p.y = r.top;
  return p;
}

RECT
test_large_struct_return(RECT a, RECT b)
{
  RECT r;
  r.left = a.left; r.right = a.right;
  r.top = b.top; r.bottom = b.bottom;
  return r;
}

ONE_BYTE
test_1_byte_struct_return(RECT r)
{
  ONE_BYTE s;
  s.a = r.top;
  return s;
}

TWO_BYTE
test_2_byte_struct_return(RECT r)
{
  TWO_BYTE s;
  s.a = r.top;
  s.b = r.left;
  return s;
}

THREE_BYTE
test_3_byte_struct_return(RECT r)
{
  THREE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  return s;
}

FOUR_BYTE
test_4_byte_struct_return(RECT r)
{
  FOUR_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  return s;
}

FIVE_BYTE
test_5_byte_struct_return(RECT r)
{
  FIVE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  return s;
}

SIX_BYTE
test_6_byte_struct_return(RECT r)
{
  SIX_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  s.f = r.left;
  return s;
}

SEVEN_BYTE
test_7_byte_struct_return(RECT r)
{
  SEVEN_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  s.f = r.left;
  s.g = r.bottom;
  return s;
}
