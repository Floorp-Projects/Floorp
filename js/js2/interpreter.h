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
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#ifndef interpreter_h
#define interpreter_h

#include "icodegenerator.h"

namespace JavaScript {
	union JSValue {
		int8 i8;
		uint8 u8;
		int16 i16;
		uint16 u16;
		int32 i32;
		uint32 u32;
		int64 i64;
		uint64 u64;
		float32 f32;
		float64 f64;
		
		JSValue() : i64(0) {}
		
		explicit JSValue(float64 f64) : f64(f64) {}
	};
	
	using std::vector;
	
	typedef vector<JSValue> JSValues;

	JSValue interpret(InstructionStream& iCode, const JSValues& args);
}

#endif /* interpreter_h */
