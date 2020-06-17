/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTParserBase_h
#define frontend_BinASTParserBase_h

#include <stddef.h>

#include "ds/LifoAlloc.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Utility.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"

namespace js {
namespace frontend {

class BinASTParserBase : public ParserSharedBase {
 public:
  BinASTParserBase(JSContext* cx, CompilationInfo& compilationInfo);
  ~BinASTParserBase() = default;

 public:
  void trace(JSTracer* trc) override {}
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_BinASTParserBase_h
