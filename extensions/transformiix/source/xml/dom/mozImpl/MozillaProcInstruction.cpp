/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 * Contributor(s): Tom Kneeland
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 */

/**
 * Implementation of the wrapper class to convert the Mozilla
 * nsIDOMProcessingInstruction interface into a TransforMiiX
 * ProcessingInstruction interface.
 */

#include "mozilladom.h"
#include "nsIDOMProcessingInstruction.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aProcInstr the nsIDOMProcessingInstruction you want to wrap
 * @param aOwner the document that owns this object
 */
ProcessingInstruction::ProcessingInstruction(
            nsIDOMProcessingInstruction* aProcInstr,
            Document* aOwner) :
        Node (aProcInstr, aOwner)
{
}

/**
 * Destructor
 */
ProcessingInstruction::~ProcessingInstruction()
{
}

/**
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool ProcessingInstruction::getLocalName(txAtom** aLocalName)
{
    if (!aLocalName) {
        return MB_FALSE;
    }
    NSI_FROM_TX(ProcessingInstruction);
    nsAutoString target;
    nsProcessingInstruction->GetNodeName(target);
    *aLocalName = NS_NewAtom(target);
    NS_ENSURE_TRUE(*aLocalName, MB_FALSE);
    return MB_TRUE;
}
