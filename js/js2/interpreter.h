// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 2000 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef interpreter_h
#define interpreter_h

#include "utilities.h"
#include "jstypes.h"
#include "icodegenerator.h"

namespace JavaScript {    
namespace Interpreter {

    using namespace ICG;
    using namespace JSTypes;
    
    JSValue interpret(ICodeModule* iCode, const JSValues& args);
    
    JSValue& defineGlobalProperty(const String& name,
                                  const JSValue& value);
    JSValue& defineFunction(const String& name, ICodeModule* iCode);

} /* namespace Interpreter */
} /* namespace JavaScript */

#endif /* interpreter_h */
