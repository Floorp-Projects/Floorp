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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
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

#pragma once

#include "DotNETEmbed.h"

// Forward declarations, Mozilla classes/interfaces.
class nsIURI;

namespace Mozilla
{
  namespace Embedding
  {
    namespace Networking
    {
      public __gc class URI : public ICloneable,
                              public IDisposable
      {
      public:
        URI(String *aSpec);
        URI(nsIURI *aURI);
        ~URI();

        // ICloneable
        Object *Clone();

        // IDisposable
        void Dispose();

        __property String* get_Spec();
        __property void set_Spec(String *aSpec);
        __property String* get_PrePath();
        __property String* get_Scheme();
        __property void set_Scheme(String *aScheme);
        __property String* get_UserPass();
        __property void set_UserPass(String *aUserPass);
        __property String* get_Username();
        __property void set_Username(String *aUsername);
        __property String* get_Password();
        __property void set_Password(String *aPassword);
        __property String* get_HostPort();
        __property void set_HostPort(String *aHostPort);
        __property String* get_Host();
        __property void set_Host(String *aHost);
        __property Int32 get_Port();
        __property void set_Port(Int32 aPort);
        __property String* get_Path();
        __property void set_Path(String *aPath);
        bool Equals(URI *aOther); 
        bool SchemeIs(String *aScheme); 
        // No need for URI *Clone() since we implement ICloneable
        String* Resolve(String *aRelativePath); 
        __property String* get_AsciiSpec();
        __property String* get_AsciiHost();
        __property String* get_OriginCharset();

      private:
        nsIURI *mURI; // [OWNER]
      }; // class URI
    }; // namespace Networking
  } // namespace Embedding
}
