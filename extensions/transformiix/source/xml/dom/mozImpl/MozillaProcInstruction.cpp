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
 */
// Tom Kneeland (02/02/2000)
//
// Implementation of the wrapper class to convert a 
// Mozilla nsIDOMProcessingInstruction into a TransforMIIX ProcessingInstruction
// interface.
//
// Modification History:
// Who  When      What
//

#include "mozilladom.h"

//
//Construct a wrapper class for the given object and document
//
ProcessingInstruction::ProcessingInstruction(
                       nsIDOMProcessingInstruction* procInstr,
                       Document* owner) : 
                       Node (procInstr, owner)
{
  nsProcessingInstruction = procInstr;
}

//
//Destructor.  Do nothing
//
ProcessingInstruction::~ProcessingInstruction()
{
}

//
//Wrap the new object in this wrapper
//
void ProcessingInstruction::setNSObj(nsIDOMProcessingInstruction* procInstr)
{
  Node::setNSObj(procInstr);
  nsProcessingInstruction = procInstr;
}

//
//Retrieve the Target from the Mozilla object, then wrap appropriately in
//a String wrapper.
//
const String& ProcessingInstruction::getTarget() const
{
  nsString* target = new nsString();

  if (nsProcessingInstruction->GetTarget(*target) == NS_OK)
    return *(ownerDocument->createDOMString(target));
  else
    {
      delete target;
      return NULL_STRING;
    }
}

//
//Retrieve the data from the Mozilla object, then wrap appropriately in a 
//String wrapper.
//
const String& ProcessingInstruction::getData() const
{
  nsString* data = new nsString();
  
  if (nsProcessingInstruction->GetData(*data) == NS_OK)
    return *(ownerDocument->createDOMString(data));
  else
    {
      delete data;
      return NULL_STRING;
    }
}

//
//Pass the nsString wapped by theData to the Mozilla object.
void ProcessingInstruction::setData(const String& theData)
{
  nsProcessingInstruction->SetData(theData.getConstNSString());
}
