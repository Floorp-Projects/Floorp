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

/*
 * Implementation of the wrapper class to convert the Mozilla
 * nsIDOMProcessingInstruction interface into a TransforMiiX
 * ProcessingInstruction interface.
 */

#include "mozilladom.h"
#include "nsIAtom.h"

/*
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

/*
 * Destructor
 */
ProcessingInstruction::~ProcessingInstruction()
{
}

/*
 * Call nsIDOMProcessingInstruction::GetTarget to retrieve the target of the
 * processing instruction.
 *
 * @return the target of the processing instruction
 */
const String& ProcessingInstruction::getTarget()
{
    NSI_FROM_TX(ProcessingInstruction)

    target.clear();
    if (nsProcessingInstruction)
        nsProcessingInstruction->GetTarget(target.getNSString());
    return target;
}

/*
 * Call nsIDOMProcessingInstruction::GetData to retrieve the data of the
 * processing instruction.
 *
 * @return the data of the processing instruction
 */
const String& ProcessingInstruction::getData()
{
    NSI_FROM_TX(ProcessingInstruction)

    data.clear();
    if (nsProcessingInstruction)
        nsProcessingInstruction->GetData(data.getNSString());
    return data;
}

/*
 * Call nsIDOMProcessingInstruction::SetData to set the data of the
 * processing instruction.
 *
 * @param aData the value to which you want to set the data of the PI
 */
void ProcessingInstruction::setData(const String& aData)
{
    NSI_FROM_TX(ProcessingInstruction)

    if (nsProcessingInstruction)
        nsProcessingInstruction->SetData(aData.getConstNSString());
}

/*
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool ProcessingInstruction::getLocalName(txAtom** aLocalName)
{
    if (!aLocalName)
        return MB_FALSE;
    NSI_FROM_TX(ProcessingInstruction)
    if (!nsProcessingInstruction)
        return MB_FALSE;
    nsAutoString target;
    nsProcessingInstruction->GetNodeName(target);
    *aLocalName = NS_NewAtom(target);
    NS_ENSURE_TRUE(*aLocalName, MB_FALSE);
    return MB_TRUE;
}
