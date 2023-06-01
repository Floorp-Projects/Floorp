/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Support for Wizer-based snapshotting of the JS shell, when built
 * for a Wasm target (i.e., running inside a Wasm module). */

#include "js/CallAndConstruct.h"  // JS_CallFunctionName
#include "shell/jsshell.h"

using namespace js;
using namespace js::shell;

#ifdef JS_SHELL_WIZER

#  include <wizer.h>

static std::optional<JSAndShellContext> wizenedContext;

static void WizerInit() {
  const int argc = 1;
  char* argv[2] = {strdup("js"), NULL};

  auto ret = ShellMain(argc, argv, /* retainContext = */ true);
  if (!ret.is<JSAndShellContext>()) {
    fprintf(stderr, "Could not execute shell main during Wizening!\n");
    abort();
  }

  wizenedContext = std::move(ret.as<JSAndShellContext>());
}

WIZER_INIT(WizerInit);

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (wizenedContext) {
    JSContext* cx = wizenedContext.value().cx;
    RootedObject glob(cx, wizenedContext.value().glob);

    JSAutoRealm ar(cx, glob);

    // Look up a function called "main" in the global.
    JS::Rooted<JS::Value> ret(cx);
    if (!JS_CallFunctionName(cx, cx->global(), "main",
                             JS::HandleValueArray::empty(), &ret)) {
      fprintf(stderr, "Failed to call main() in Wizened JS source!\n");
      abort();
    }
  }
}

#endif  // JS_SHELL_WIZER
