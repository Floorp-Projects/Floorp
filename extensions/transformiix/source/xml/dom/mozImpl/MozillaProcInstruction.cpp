/*
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

/* Implementation of the wrapper class to convert the Mozilla
   nsIDOMProcessingInstruction interface into a TransforMIIX
   ProcessingInstruction interface.
*/

#include "mozilladom.h"

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
    nsProcessingInstruction = aProcInstr;
}

/**
 * Destructor
 */
ProcessingInstruction::~ProcessingInstruction()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aProcInstr the nsIDOMProcessingInstruction you want to wrap
 */
void ProcessingInstruction::setNSObj(nsIDOMProcessingInstruction* aProcInstr)
{
    Node::setNSObj(aProcInstr);
    nsProcessingInstruction = aProcInstr;
}

/**
 * Call nsIDOMProcessingInstruction::GetTarget to retrieve the target of the
 * processing instruction.
 *
 * @return the target of the processing instruction
 */
const String& ProcessingInstruction::getTarget()
{
    target.clear();
    nsProcessingInstruction->GetTarget(target.getNSString());
    return target;
}

/**
 * Call nsIDOMProcessingInstruction::GetData to retrieve the data of the
 * processing instruction.
 *
 * @return the data of the processing instruction
 */
const String& ProcessingInstruction::getData()
{
    data.clear();
    nsProcessingInstruction->GetData(data.getNSString());
    return data;
}

/**
 * Call nsIDOMProcessingInstruction::SetData to set the data of the
 * processing instruction.
 *
 * @param aData the value to which you want to set the data of the PI
 */
void ProcessingInstruction::setData(const String& aData)
{
    nsProcessingInstruction->SetData(aData.getConstNSString());
}
