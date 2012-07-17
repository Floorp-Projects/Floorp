# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

webidl_files = \
  CanvasRenderingContext2D.webidl \
  Function.webidl \
  EventListener.webidl \
  EventTarget.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  XMLHttpRequest.webidl \
  XMLHttpRequestEventTarget.webidl \
  XMLHttpRequestUpload.webidl \
  $(NULL)

ifdef MOZ_WEBGL
webidl_files += \
  WebGLRenderingContext.webidl \
  $(NULL)
endif

ifdef ENABLE_TESTS
test_webidl_files := \
  TestCodeGen.webidl \
  TestDictionary.webidl \
  $(NULL)
else
test_webidl_files := $(NULL)
endif

