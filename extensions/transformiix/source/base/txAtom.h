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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *   Axel Hecht <axel@pike.org>
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

/*
 * Atom implementation for standalone and wrapper for module
 *
 * For module,
 * txAtom is typedef'd to nsIAtom,
 * for standalone there is a separate implementation. The
 * code is all inlined in this header file.
 *
 * There is one major difference between the standalone and
 * the module implementation, module atoms are refcounted,
 * standalone atoms are not. Therefor code that is standalone
 * only may not use TX_RELEASE..ATOM, code that works in both
 * module and standalone has to pair these. Or you leak.
 *
 * To use this code standalone, 
 *  TX_IMPL_ATOM_STATICS;
 * has to appear in your source file.
 * XSLTProcessor.cpp does this.
 */
 
#ifndef TRANSFRMX_ATOM_H
#define TRANSFRMX_ATOM_H

#include "TxString.h"

#ifndef TX_EXE
#include "nsIAtom.h"
#else
#include "NamedMap.h"
#endif

#ifdef TX_EXE

class txAtom : public TxObject
{
public:
    txAtom(const String& aString)
    {
        mString = aString;
    }
    MBool getString(String& aString)
    {
        aString = mString;
        return MB_TRUE;
    }
private:
    String mString;
};

class txAtomService
{
public:
    static txAtom* getAtom(const String& aString)
    {
        if (!mAtoms && !Init())
            return NULL;
        txAtom* atom = (txAtom*)mAtoms->get(aString);
        if (!atom) {
            atom = new txAtom(aString);
            if (!atom)
                return 0;
            mAtoms->put(aString, atom);
        }
        return atom;
    }
    
    static MBool Init()
    {
        NS_ASSERTION(!mAtoms, "called without matching Shutdown()");
        if (mAtoms)
            return MB_TRUE;
        mAtoms = new NamedMap();
        if (!mAtoms)
            return MB_FALSE;
        mAtoms->setObjectDeletion(MB_TRUE);
        return MB_TRUE;
    }

    static void Shutdown()
    {
        NS_ASSERTION(mAtoms, "called without matching Init()");
        if (!mAtoms)
            return;
        delete mAtoms;
        mAtoms = NULL;
    }

private:
    static NamedMap* mAtoms;
};

#define TX_GET_ATOM(str) \
    (txAtomService::getAtom(str))

#define TX_ADDREF_ATOM(atom) {}

#define TX_RELEASE_ATOM(atom) {}

#define TX_IF_RELEASE_ATOM(atom) {}

#define TX_GET_ATOM_STRING(atom, str) \
    ((atom)->getString(str))

#define TX_IMPL_ATOM_STATICS \
      NamedMap* txAtomService::mAtoms = 0

#else

typedef nsIAtom txAtom;

#define TX_GET_ATOM(str) \
    NS_NewAtom((str).getConstNSString())

#define TX_ADDREF_ATOM(atom) NS_ADDREF(atom)

#define TX_RELEASE_ATOM(atom) NS_RELEASE(atom)

#define TX_IF_RELEASE_ATOM(atom) NS_IF_RELEASE(atom)

#define TX_GET_ATOM_STRING(atom, string) \
    NS_SUCCEEDED((atom)->ToString(string.getNSString()))

#endif  // TX_EXE

#endif  // TRANSFRMX_ATOM_H
