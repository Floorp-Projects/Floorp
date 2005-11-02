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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
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

#define TX_ATOM(_name, _value) txAtom* txXMLAtoms::_name = 0
XML_ATOMS;
#undef TX_ATOM
#define TX_ATOM(_name, _value) txAtom* txXPathAtoms::_name = 0
#include "txXPathAtomList.h"
#undef TX_ATOM
#define TX_ATOM(_name, _value) txAtom* txXSLTAtoms::_name = 0
#include "txXSLTAtomList.h"
#undef TX_ATOM
#ifndef TX_EXE
#define TX_ATOM(_name, _value) txAtom* txHTMLAtoms::_name = 0
#include "txHTMLAtomList.h"
#undef TX_ATOM
#endif

static PRUint32 gXMLRefCnt = 0;
static PRUint32 gXPathRefCnt = 0;
static PRUint32 gXSLTRefCnt = 0;
#ifndef TX_EXE
static PRUint32 gHTMLRefCnt = 0;
#endif

#ifdef TX_EXE
#define TX_ATOM(_name, _value)           \
    _name = TX_GET_ATOM(String(_value)); \
    if (!_name)                          \
        return MB_FALSE
#else
#define TX_ATOM(_name, _value)      \
    _name = NS_NewAtom(_value);     \
    NS_ENSURE_TRUE(_name, MB_FALSE)
#endif

MBool txXMLAtoms::init()
{
    if (0 == gXMLRefCnt++) {
        // create atoms
XML_ATOMS;
    }
    return MB_TRUE;
}

MBool txXPathAtoms::init()
{
    if (0 == gXPathRefCnt++) {
        // create atoms
#include "txXPathAtomList.h"
    }
    return MB_TRUE;
}

MBool txXSLTAtoms::init()
{
    if (0 == gXSLTRefCnt++) {
        // create atoms
#include "txXSLTAtomList.h"
    }
    return MB_TRUE;
}

#ifndef TX_EXE
MBool txHTMLAtoms::init()
{
    if (0 == gHTMLRefCnt++) {
        // create atoms
#include "txHTMLAtomList.h"
    }
    return MB_TRUE;
}
#endif

#undef TX_ATOM

#define TX_ATOM(_name, _value) \
TX_IF_RELEASE_ATOM(_name)

void txXMLAtoms::shutdown()
{
    NS_ASSERTION(gXMLRefCnt != 0, "bad release atoms");
    if (--gXMLRefCnt == 0) {
        // release atoms
XML_ATOMS;
    }
}

void txXPathAtoms::shutdown()
{
    NS_ASSERTION(gXPathRefCnt != 0, "bad release atoms");
    if (--gXPathRefCnt == 0) {
        // release atoms
#include "txXPathAtomList.h"
    }
}

void txXSLTAtoms::shutdown()
{
    NS_ASSERTION(gXSLTRefCnt != 0, "bad release atoms");
    if (--gXSLTRefCnt == 0) {
        // release atoms
#include "txXSLTAtomList.h"
    }
}

#ifndef TX_EXE
void txHTMLAtoms::shutdown()
{
    NS_ASSERTION(gHTMLRefCnt != 0, "bad release atoms");
    if (--gHTMLRefCnt == 0) {
        // release atoms
#include "txHTMLAtomList.h"
    }
}
#endif

#undef TX_ATOM
