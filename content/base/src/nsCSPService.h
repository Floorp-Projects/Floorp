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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brandon Sterne <bsterne@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXPCOM.h"
#include "nsIContentPolicy.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"

#define CSPSERVICE_CONTRACTID "@mozilla.org/cspservice;1"
#define CSPSERVICE_CID \
  { 0x8d2f40b2, 0x4875, 0x4c95, \
    { 0x97, 0xd9, 0x3f, 0x7d, 0xca, 0x2c, 0xb4, 0x60 } }
class CSPService : public nsIContentPolicy,
                   public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY
  NS_DECL_NSICHANNELEVENTSINK

  CSPService();
  virtual ~CSPService();
  static bool sCSPEnabled;
};
