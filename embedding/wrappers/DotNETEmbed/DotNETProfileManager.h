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

#pragma once

using namespace System;

namespace Mozilla 
{
  namespace Embedding
  {
    namespace ProfileManager
    {
      public __gc class Profile
      {
        private:
          unmanagedProfile __nogc * mProfile;
          bool CreateUnmanagedProfile();

        public:
          Profile();
          ~Profile();
          void Dispose(bool disposing);
          void Dispose();

          static UInt32 SHUTDOWN_PERSIST = 1;
          static UInt32 SHUTDOWN_CLEANSE = 2;

          __property Int32 get_ProfileCount();
          void GetProfileList(UInt32* length, String** profileNames);
          bool ProfileExists(String *aProfileName);
          __property String* get_CurrentProfile();
          __property void set_CurrentProfile(String* aCurrentProfile);
          void ShutDownCurrentProfile(UInt32 shutDownType);
          bool CreateNewProfile(String* aProfileName, String *aNativeProfileDir, String* aLangcode, bool useExistingDir);
          void RenameProfile(String* aOldName, String* aNewName);
          void DeleteProfile(String* aName, bool aCanDeleteFiles);
          void CloneProfile(String* aProfileName);
      }; // class Profile 

    } // namespace ProfileManager

  } // namespace Embedding
}
