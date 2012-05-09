//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_PREPROCESSOR_LEXER_GLUE_H_
#define COMPILER_PREPROCESSOR_LEXER_GLUE_H_

struct InputSrc;

InputSrc* LexerInputSrc(int count, const char* const string[], const int length[]);

#endif  // COMPILER_PREPROCESSOR_LEXER_GLUE_H_

