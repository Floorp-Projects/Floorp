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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

#ifndef __inDOMRDFResource_h__
#define __inDOMRDFResource_h__

#include "inIDOMRDFResource.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFResource.h"

#define IN_DOMRDFRESOURCE_ID "InspectorDOMResource"

/////////////////////////////////////////////////////////////////////
// This object is registered with the datasource factory to
// be instantiated for all resources created with a uri that begins 
// with InspectorDOMResource://
//
// This is a multi-purpose object that is used by datasources as
// a temporary transport object for data.  When ::GetTargets is called,
// a list of these objects is created by the datasource.  Each object 
// holds a reference back to the real object it represents.  This
// object is then sent back to GetTarget, where we get the real 
// object and query it for info.
/////////////////////////////////////////////////////////////////////

class inDOMRDFResource : public nsRDFResource,
                         public inIDOMRDFResource
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_INIDOMRDFRESOURCE

  inDOMRDFResource();
  virtual ~inDOMRDFResource();

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& iid, void **result);
  
protected:
  nsCOMPtr<nsISupports> mObject;
};

#endif // __inDOMRDFResource_h__

