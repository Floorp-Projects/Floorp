/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "txAtoms.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"

// define storage for all atoms
#define TX_ATOM(_name, _value) nsIAtom* txXMLAtoms::_name;
XML_ATOMS
#undef TX_ATOM

#define TX_ATOM(_name, _value) nsIAtom* txXPathAtoms::_name;
#include "txXPathAtomList.h"
#undef TX_ATOM

#define TX_ATOM(_name, _value) nsIAtom* txXSLTAtoms::_name;
#include "txXSLTAtomList.h"
#undef TX_ATOM

#define TX_ATOM(_name, _value) nsIAtom* txHTMLAtoms::_name;
#include "txHTMLAtomList.h"
#undef TX_ATOM

static const nsStaticAtom XMLAtoms_info[] = {
#define TX_ATOM(name_, value_) { value_, &txXMLAtoms::name_ },
XML_ATOMS
#undef TX_ATOM
};

static const nsStaticAtom XPathAtoms_info[] = {
#define TX_ATOM(name_, value_) { value_, &txXPathAtoms::name_ },
#include "txXPathAtomList.h"
#undef TX_ATOM
};

static const nsStaticAtom XSLTAtoms_info[] = {
#define TX_ATOM(name_, value_) { value_, &txXSLTAtoms::name_ },
#include "txXSLTAtomList.h"
#undef TX_ATOM
};

static const nsStaticAtom HTMLAtoms_info[] = {
#define TX_ATOM(name_, value_) { value_, &txHTMLAtoms::name_ },
#include "txHTMLAtomList.h"
#undef TX_ATOM
};

void txXMLAtoms::init()
{
    NS_RegisterStaticAtoms(XMLAtoms_info, NS_ARRAY_LENGTH(XMLAtoms_info));
}

void txXPathAtoms::init()
{
    NS_RegisterStaticAtoms(XPathAtoms_info, NS_ARRAY_LENGTH(XPathAtoms_info));
}

void txXSLTAtoms::init()
{
    NS_RegisterStaticAtoms(XSLTAtoms_info, NS_ARRAY_LENGTH(XSLTAtoms_info));
}

void txHTMLAtoms::init()
{
    NS_RegisterStaticAtoms(HTMLAtoms_info, NS_ARRAY_LENGTH(HTMLAtoms_info));
}
