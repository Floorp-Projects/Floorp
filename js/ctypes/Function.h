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

#ifndef FUNCTION_H
#define FUNCTION_H

#include "Library.h"
#include "nsIXPCScriptable.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "prlink.h"
#include "jsapi.h"
#include "ffi.h"

namespace mozilla {
namespace ctypes {

struct Type
{
  ffi_type mFFIType;
  PRUint16 mType;
};

struct Value
{
  void* mData;
  union {
    PRInt8   mInt8;
    PRInt16  mInt16;
    PRInt32  mInt32;
    PRInt64  mInt64;
    PRUint8  mUint8;
    PRUint16 mUint16;
    PRUint32 mUint32;
    PRUint64 mUint64;
    float    mFloat;
    double   mDouble;
    void*    mPointer;
  } mValue;
};

class Function : public nsIXPCScriptable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE

  Function();

  nsresult Init(JSContext* aContext, Library* aLibrary, PRFuncPtr aFunc, PRUint16 aCallType, jsval aResultType, const nsTArray<jsval>& aArgTypes);

private:
  ~Function();

  PRBool Execute(JSContext* aContext, PRUint32 aArgc, jsval* aArgv, jsval* aValue);

protected:
  // reference to the library our function is in
  nsRefPtr<Library> mLibrary;

  PRFuncPtr mFunc;

  ffi_abi mCallType;
  Type mResultType;
  nsAutoTArray<Type, 16> mArgTypes;
  nsAutoTArray<ffi_type*, 16> mFFITypes;

  ffi_cif mCIF;
};

}
}

#endif
