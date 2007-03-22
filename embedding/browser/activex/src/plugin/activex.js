/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Adam Lock <adamlock@netscape.com>
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


///////////////////////////////////////////////////////////////////////////////
// This is the default preferences file defining the behavior for hosting
// ActiveX controls in Gecko embedded applications. Embedders should override
// this file to set their own policy.
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// General hosting flags settings. Read nsIActiveXSecurityPolicy.idl in
// http://lxr.mozilla.org/seamonkey/find?string=nsIActiveXSecurityPolicy.idl
// for more combinations.
//
// Briefly,
//
//   0 means no hosting of activex controls whatsoever
//   13 means medium settings (safe for scripting controls and download / install)
//   31 means host anything (extremely dangerous!)
//

pref("security.xpconnect.activex.global.hosting_flags", 13);


///////////////////////////////////////////////////////////////////////////////
// Whitelist / Blacklist capabilities
//
// The whitelist and blacklist settings define what controls Gecko will host
// and the default allow / deny behavior.
//
//   Note 1:
//
//   The hosting flags pref value above takes priority over settings below.
//   Therefore if the hosting flags are set to 0 (i.e. host nothing) then
//   no control will be hosted no matter what controls are enabled. Likewise, 
//   If safe for scripting checks are (wisely) enabled, no unsafe control
//   will be hosted even if it is explicitly enabled below.
//
//
//   Note 2:
//
//   Gecko always reads the IE browser's control blacklist if one is defined
//   in the registry. This is to ensure any control identified by Microsoft
//   or others as unsafe is not hosted without requiring it to be explicitly
//   listed here also.
//   


///////////////////////////////////////////////////////////////////////////////
// This pref sets the default policy to allow all controls or deny them all
// default. If the value is false, only controls explicitly enabled by their
// classid will be allowed. Otherwise all controls are allowed except those
// explicitly disabled by their classid.
//
// If you are writing an embedding application that only needs to run
// certain known controls, (e.g. an intranet control of some kind) you are
// advised to use the false value and enable the control explicitly.

pref("security.classID.allowByDefault", true);


///////////////////////////////////////////////////////////////////////////////
// Specify below the controls that should be explicitly enabled or disabled.
// This is achieved by writing a policy rule, specifiying the classid of the
// control and giving the control "AllAccess" or "NoAccess".
//
// CIDaaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee
//
// You could explicitly ban a control (using the appropriate classid) like this
//
// pref("capability.policy.default.ClassID.CID039ef260-2a0d-11d5-90a7-0010a4e73d9a", "NoAccess");
//
// If you want to explicity enable a control then do this:
//
// pref("capability.policy.default.ClassID.CID039ef260-2a0d-11d5-90a7-0010a4e73d9a", "AllAccess");
//
// If you want to explicitly ban or allow a control for one or more sites then
// you can create a policy for those sites. This example creates a domain
// called 'trustable' containing sites where you allow an additional control
// to be hosted.:
//
// user_pref("capability.policy.policynames", "trustable");
// user_pref("capability.policy.trustable.sites", "http://www.site1.net http://www.site2.net");
// user_pref("capability.policy.trustable.ClassID.CID039ef260-2a0d-11d5-90a7-0010a4e73d9a", "AllAccess");
//


