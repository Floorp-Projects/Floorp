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
// Tom Kneeland (02/01/2000)
//
//  Implementation of the wrapper class to convert the Mozilla 
//  nsIDOMCDATASection interface into a TransforMIIX CDATASection interface.
//
// Modification History:
// Who  When      What
//

#include "mozilladom.h"

//
//Construct a text object with the specified document owner and data
//
CDATASection::CDATASection(nsIDOMCDATASection* cdataSection, Document* owner) :
  Text(cdataSection, owner)
{
  nsCDATASection = cdataSection;
}

//
//Destructor.  Do nothing
//
CDATASection::~CDATASection()
{
}

//
//Use this object to wrap a different Mozilla Object
//
void CDATASection::setNSObj(nsIDOMCDATASection* cdataSection)
{
  Text::setNSObj(cdataSection);
  nsCDATASection = cdataSection;
}
