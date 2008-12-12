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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef jArray_h__
#define jArray_h__

#define J_ARRAY_STATIC(T, L, arr) \
  jArray<T,L>( (arr), (sizeof(arr)/sizeof(arr[0])) ) 

template<class T, class L> 
class jArray {
  private:
    T* arr;
  public:
    L length;
    jArray(T* const a, L const len);
    jArray(L const len);
    jArray(const jArray<T,L>& other);
    jArray();
    operator T*() { return arr; }
    T& operator[] (L const index) { return arr[index]; }
    void release() { delete[] arr; }
    L binarySearch(T const elem);
};

template<class T, class L>
jArray<T,L>::jArray(T* const a, L const len)
       : arr(a), length(len)
{
}

template<class T, class L>
jArray<T,L>::jArray(L const len)
       : arr(new T[len]), length(len)
{
}

template<class T, class L>
jArray<T,L>::jArray(const jArray<T,L>& other)
       : arr(other.arr), length(other.length)
{
}

template<class T, class L>
jArray<T,L>::jArray()
       : arr(0), length(0)
{
}

template<class T, class L>
L
jArray<T,L>::binarySearch(T const elem)
{
  L lo = 0;
  L hi = length - 1;
  while (lo <= hi) {
    L mid = (lo + hi) / 2;
    if (arr[mid] > elem) {
      hi = mid - 1;
    } else if (arr[mid] < elem) {
      lo = mid + 1;
    } else {
      return mid;
    }
  }
  return -1;
}

#endif // jArray_h__