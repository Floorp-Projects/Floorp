# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

generated_webidl_files = \
  CSS2Properties.webidl \
  $(NULL)

webidl_files = \
  AudioContext.webidl \
  Blob.webidl \
  CanvasRenderingContext2D.webidl \
  ClientRectList.webidl \
  CSSStyleDeclaration.webidl \
  DOMTokenList.webidl \
  DOMSettableTokenList.webidl \
  Function.webidl \
  EventListener.webidl \
  EventTarget.webidl \
  FileList.webidl \
  FileReaderSync.webidl \
  HTMLCollection.webidl \
  HTMLOptionsCollection.webidl \
  HTMLPropertiesCollection.webidl \
  NodeList.webidl \
  PaintRequestList.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  SVGLengthList.webidl \
  SVGNumberList.webidl \
  SVGPathSegList.webidl \
  SVGPointList.webidl \
  SVGTransformList.webidl \
  WebSocket.webidl \
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
  TestTypedef.webidl \
  $(NULL)
else
test_webidl_files := $(NULL)
endif

