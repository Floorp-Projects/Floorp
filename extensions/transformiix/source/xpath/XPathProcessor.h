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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Van der Beken, peterv@netscape.com
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

#ifndef XPathProcessor_h__
#define XPathProcessor_h__

#include "nsIXPathNodeSelector.h"

/* e4172588-1dd1-11b2-bf09-ec309437245a */
#define TRANSFORMIIX_XPATH_PROCESSOR_CID   \
{ 0xe4172588, 0x1dd1, 0x11b2, {0xbf, 0x09, 0xec, 0x30, 0x94, 0x37, 0x24, 0x5a} }

#define TRANSFORMIIX_XPATH_PROCESSOR_CONTRACTID \
"@mozilla.org/xpath-nodeselector;1"

/**
 * A class for processing an XPath query
**/
class XPathProcessor : public nsIXPathNodeSelector
{
public:
    /**
     * Creates a new XPathProcessor
    **/
    XPathProcessor();

    /**
     * Default destructor for XPathProcessor
    **/
    virtual ~XPathProcessor();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIXPathNodeSelector interface
    NS_DECL_NSIXPATHNODESELECTOR
};

#endif
