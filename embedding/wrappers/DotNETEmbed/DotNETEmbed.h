/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"

#using <mscorlib.dll>
#using <System.Windows.Forms.dll>
#using <System.dll>

using namespace System;
using namespace System::Windows::Forms;

class nsIWebBrowserChrome;

namespace Mozilla
{
  namespace Embedding
  {
    // .NET UI control class that can be placed in a Windows Form
    // using DevStudio.NET's GUI.

    public __gc class Gecko : public System::Windows::Forms::UserControl
    {
    public:
      Gecko();

      static void TermEmbedding();

      void OpenURL(String *url);

    protected:
      // Overriden System::Windows::Forms::Control methods
      void OnResize(EventArgs *e);

    private:
      void CreateBrowserWindow(PRUint32 aChromeFlags,
                               nsIWebBrowserChrome *aParent);

      nsIWebBrowserChrome *mChrome;

      static bool sIsInitialized = false;
    }; // class Gecko

    // Throw an exception if NS_FAILED(rv)
    void ThrowIfFailed(nsresult rv);

    // Helper for copying an UCS2 Mozilla nsAFlatString to a managed
    // String.
    inline String * CopyString(const nsAFlatString& aStr)
    {
      return new String(aStr.get(), 0, aStr.Length());
    }

    // Helper for copying an UTF8 Mozilla nsAFlatCString to a managed
    // String.
    String * CopyString(const nsAFlatCString& aStr);

    // Helper for copying a managed String into a Mozilla UCS2
    // nsAFlatString.
    nsAFlatString& CopyString(String *aSrc, nsAFlatString& aDest);

    // Helper for copying a managed String into a Mozilla UTF8
    // nsAFlatCString.
    nsAFlatCString& CopyString(String *aSrc, nsAFlatCString& aDest);

  } // namespace Embedding
}
