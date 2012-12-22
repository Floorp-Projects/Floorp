# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

generated_webidl_files = \
  CSS2Properties.webidl \
  $(NULL)

webidl_files = \
  AudioBuffer.webidl \
  AudioBufferSourceNode.webidl \
  AudioContext.webidl \
  AudioDestinationNode.webidl \
  AudioListener.webidl \
  AudioNode.webidl \
  AudioParam.webidl \
  AudioSourceNode.webidl \
  BiquadFilterNode.webidl \
  Blob.webidl \
  CanvasRenderingContext2D.webidl \
  ClientRectList.webidl \
  CSSPrimitiveValue.webidl \
  CSSStyleDeclaration.webidl \
  CSSValue.webidl \
  CSSValueList.webidl \
  DelayNode.webidl \
  DOMImplementation.webidl \
  DOMParser.webidl \
  DOMSettableTokenList.webidl \
  DOMStringMap.webidl \
  DOMTokenList.webidl \
  DynamicsCompressorNode.webidl \
  Element.webidl \
  EventHandler.webidl \
  EventListener.webidl \
  EventTarget.webidl \
  File.webidl \
  FileHandle.webidl \
  FileList.webidl \
  FileReaderSync.webidl \
  FormData.webidl \
  Function.webidl \
  GainNode.webidl \
  HTMLCollection.webidl \
  HTMLElement.webidl \
  HTMLOptionsCollection.webidl \
  HTMLPropertiesCollection.webidl \
  ImageData.webidl \
  MutationObserver.webidl \
  Node.webidl \
  NodeFilter.webidl \
  NodeList.webidl \
  PaintRequest.webidl \
  PaintRequestList.webidl \
  PannerNode.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  RGBColor.webidl \
  Screen.webidl \
  SVGLengthList.webidl \
  SVGNumberList.webidl \
  SVGPathSegList.webidl \
  SVGPointList.webidl \
  SVGTransformList.webidl \
  TextDecoder.webidl \
  TextEncoder.webidl \
  URL.webidl \
  WebSocket.webidl \
  XMLHttpRequest.webidl \
  XMLHttpRequestEventTarget.webidl \
  XMLHttpRequestUpload.webidl \
  XMLSerializer.webidl \
  XPathEvaluator.webidl \
  $(NULL)

ifdef MOZ_WEBGL
webidl_files += \
  WebGLRenderingContext.webidl \
  $(NULL)
endif

ifdef MOZ_WEBRTC
webidl_files += \
  MediaStreamList.webidl \
  $(NULL)
endif

ifdef MOZ_B2G_RIL
webidl_files += \
  USSDReceivedEvent.webidl \
  $(NULL)
endif

ifdef ENABLE_TESTS
test_webidl_files := \
  TestCodeGen.webidl \
  TestDictionary.webidl \
  TestExampleGen.webidl \
  TestTypedef.webidl \
  $(NULL)
else
test_webidl_files := $(NULL)
endif

