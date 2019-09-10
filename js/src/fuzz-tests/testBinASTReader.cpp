/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScopeExit.h"

#include "jsapi.h"

#include "frontend/BinASTParser.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/Parser.h"
#include "fuzz-tests/tests.h"
#include "js/CompileOptions.h"
#include "vm/Interpreter.h"

#include "vm/JSContext-inl.h"

using UsedNameTracker = js::frontend::UsedNameTracker;
using namespace js;

using JS::CompileOptions;

// These are defined and pre-initialized by the harness (in tests.cpp).
extern JS::PersistentRootedObject gGlobal;
extern JSContext* gCx;

static int testBinASTReaderInit(int* argc, char*** argv) { return 0; }

static int testBinASTReaderFuzz(const uint8_t* buf, size_t size) {
  using namespace js::frontend;

  auto gcGuard = mozilla::MakeScopeExit([&] {
    JS::PrepareForFullGC(gCx);
    JS::NonIncrementalGC(gCx, GC_NORMAL, JS::GCReason::API);
  });

  if (!size) return 0;

  CompileOptions options(gCx);
  options.setIntroductionType("fuzzing parse").setFileAndLine("<string>", 1);

  js::Vector<uint8_t> binSource(gCx);
  if (!binSource.append(buf, size)) {
    ReportOutOfMemory(gCx);
    return 0;
  }

  LifoAllocScope allocScope(&gCx->tempLifoAlloc());
  ParseInfo binParseInfo(gCx, allocScope);

  Directives directives(false);
  GlobalSharedContext globalsc(gCx, ScopeKind::Global, directives, false);

  RootedScriptSourceObject sourceObj(
      gCx,
      frontend::CreateScriptSourceObject(gCx, options, mozilla::Nothing()));
  if (!sourceObj) {
    ReportOutOfMemory(gCx);
    return 0;
  }
  BinASTParser<js::frontend::BinASTTokenReaderMultipart> reader(
      gCx, binParseInfo, options, sourceObj);

  // Will be deallocated once `reader` goes out of scope.
  auto binParsed = reader.parse(&globalsc, binSource);
  RootedValue binExn(gCx);
  if (binParsed.isErr()) {
    js::GetAndClearException(gCx, &binExn);
    return 0;
  }

#if defined(DEBUG)  // Dumping an AST is only defined in DEBUG builds
  Sprinter binPrinter(gCx);
  if (!binPrinter.init()) {
    ReportOutOfMemory(gCx);
    return 0;
  }
  DumpParseTree(binParsed.unwrap(), binPrinter);
#endif  // defined(DEBUG)

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(testBinASTReaderInit, testBinASTReaderFuzz, BinAST);
