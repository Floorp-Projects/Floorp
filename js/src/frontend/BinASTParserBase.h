/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinASTParserBase_h
#define frontend_BinASTParserBase_h

#include <stddef.h>

#include "ds/LifoAlloc.h"
#include "frontend/BinTokenReaderMultipart.h"
#include "frontend/BinTokenReaderTester.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Utility.h"
#include "vm/JSContext.h"
#include "vm/JSScript.h"

namespace js {
namespace frontend {

class BinASTParserBase : private JS::AutoGCRooter {
 public:
  BinASTParserBase(JSContext* cx, LifoAlloc& alloc, UsedNameTracker& usedNames,
                   HandleScriptSourceObject sourceObject,
                   Handle<LazyScript*> lazyScript);
  ~BinASTParserBase();

 public:
  // Names

  bool hasUsedName(HandlePropertyName name);

  // --- GC.

  virtual void doTrace(JSTracer* trc) {}

  void trace(JSTracer* trc) {
    TraceListNode::TraceList(trc, traceListHead_);
    doTrace(trc);
  }

 public:
  ParseNode* allocParseNode(size_t size) {
    MOZ_ASSERT(size == sizeof(ParseNode));
    return static_cast<ParseNode*>(nodeAlloc_.allocNode());
  }

  JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

  // Needs access to AutoGCRooter.
  friend void TraceBinParser(JSTracer* trc, JS::AutoGCRooter* parser);

 protected:
  JSContext* cx_;

  // ---- Memory-related stuff
 protected:
  LifoAlloc& alloc_;
  TraceListNode* traceListHead_;
  UsedNameTracker& usedNames_;

 private:
  LifoAlloc::Mark tempPoolMark_;
  ParseNodeAllocator nodeAlloc_;

  // ---- Parsing-related stuff
 protected:
  // Root atoms and objects allocated for the parse tree.
  AutoKeepAtoms keepAtoms_;

  RootedScriptSourceObject sourceObject_;
  Rooted<LazyScript*> lazyScript_;
  ParseContext* parseContext_;
  FullParseHandler factory_;

  friend class BinParseContext;
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_BinASTParserBase_h
