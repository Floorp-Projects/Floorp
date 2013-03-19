# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

generated_webidl_files = \
  CSS2Properties.webidl \
  $(NULL)

webidl_files = \
  AnimationEvent.webidl \
  ArchiveReader.webidl \
  ArchiveRequest.webidl \
  AudioBuffer.webidl \
  AudioBufferSourceNode.webidl \
  AudioContext.webidl \
  AudioDestinationNode.webidl \
  AudioListener.webidl \
  AudioNode.webidl \
  AudioParam.webidl \
  AudioSourceNode.webidl \
  BatteryManager.webidl \
  BiquadFilterNode.webidl \
  Blob.webidl \
  CanvasRenderingContext2D.webidl \
  CaretPosition.webidl \
  CDATASection.webidl \
  CFStateChangeEvent.webidl \
  CharacterData.webidl \
  ClientRect.webidl \
  ClientRectList.webidl \
  CommandEvent.webidl \
  Comment.webidl \
  CSS.webidl \
  CSSPrimitiveValue.webidl \
  CSSStyleDeclaration.webidl \
  CSSStyleSheet.webidl \
  CSSValue.webidl \
  CSSValueList.webidl \
  DelayNode.webidl \
  Document.webidl \
  DocumentFragment.webidl \
  DocumentType.webidl \
  DOMCursor.webidl \
  DOMImplementation.webidl \
  DOMParser.webidl \
  DOMRequest.webidl \
  DOMSettableTokenList.webidl \
  DOMStringMap.webidl \
  DOMTokenList.webidl \
  DOMTransaction.webidl \
  DummyBinding.webidl \
  DynamicsCompressorNode.webidl \
  Element.webidl \
  Event.webidl \
  EventHandler.webidl \
  EventListener.webidl \
  EventSource.webidl \
  EventTarget.webidl \
  File.webidl \
  FileHandle.webidl \
  FileList.webidl \
  FileReaderSync.webidl \
  FileRequest.webidl \
  FormData.webidl \
  Function.webidl \
  GainNode.webidl \
  HTMLAnchorElement.webidl \
  HTMLAppletElement.webidl \
  HTMLAreaElement.webidl \
  HTMLAudioElement.webidl \
  HTMLBaseElement.webidl \
  HTMLBodyElement.webidl \
  HTMLBRElement.webidl \
  HTMLButtonElement.webidl \
  HTMLCollection.webidl \
  HTMLDataElement.webidl \
  HTMLDataListElement.webidl \
  HTMLDirectoryElement.webidl \
  HTMLDivElement.webidl \
  HTMLDListElement.webidl \
  HTMLDocument.webidl \
  HTMLElement.webidl \
  HTMLEmbedElement.webidl \
  HTMLFieldSetElement.webidl \
  HTMLFontElement.webidl \
  HTMLFrameElement.webidl \
  HTMLFrameSetElement.webidl \
  HTMLHeadElement.webidl \
  HTMLHeadingElement.webidl \
  HTMLHRElement.webidl \
  HTMLHtmlElement.webidl \
  HTMLImageElement.webidl \
  HTMLLabelElement.webidl \
  HTMLLegendElement.webidl \
  HTMLLIElement.webidl \
  HTMLLinkElement.webidl \
  HTMLMapElement.webidl \
  HTMLMediaElement.webidl \
  HTMLMenuElement.webidl \
  HTMLMenuItemElement.webidl \
  HTMLMetaElement.webidl \
  HTMLMeterElement.webidl \
  HTMLModElement.webidl \
  HTMLObjectElement.webidl \
  HTMLOListElement.webidl \
  HTMLOptGroupElement.webidl \
  HTMLOptionElement.webidl \
  HTMLOptionsCollection.webidl \
  HTMLOutputElement.webidl \
  HTMLParagraphElement.webidl \
  HTMLParamElement.webidl \
  HTMLPreElement.webidl \
  HTMLProgressElement.webidl \
  HTMLPropertiesCollection.webidl \
  HTMLQuoteElement.webidl \
  HTMLScriptElement.webidl \
  HTMLSpanElement.webidl \
  HTMLStyleElement.webidl \
  HTMLTableCaptionElement.webidl \
  HTMLTableCellElement.webidl \
  HTMLTableColElement.webidl \
  HTMLTableElement.webidl \
  HTMLTableRowElement.webidl \
  HTMLTableSectionElement.webidl \
  HTMLTextAreaElement.webidl \
  HTMLTimeElement.webidl \
  HTMLTitleElement.webidl \
  HTMLUListElement.webidl \
  IDBVersionChangeEvent.webidl \
  ImageData.webidl \
  InspectorUtils.webidl \
  LinkStyle.webidl \
  LocalMediaStream.webidl \
  Location.webidl \
  MediaStream.webidl \
  MessageEvent.webidl \
  MouseEvent.webidl \
  MozActivity.webidl \
  MutationEvent.webidl \
  MutationObserver.webidl \
  Node.webidl \
  NodeFilter.webidl \
  NodeIterator.webidl \
  NodeList.webidl \
  Notification.webidl \
  PaintRequest.webidl \
  PaintRequestList.webidl \
  PannerNode.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  ProcessingInstruction.webidl \
  Range.webidl \
  Rect.webidl \
  RGBColor.webidl \
  RTCConfiguration.webidl \
  Screen.webidl \
  StyleSheet.webidl \
  SVGAElement.webidl \
  SVGAltGlyphElement.webidl \
  SVGAngle.webidl \
  SVGAnimatedAngle.webidl \
  SVGAnimatedBoolean.webidl \
  SVGAnimatedLength.webidl \
  SVGAnimatedLengthList.webidl \
  SVGAnimatedNumberList.webidl \
  SVGAnimatedPathData.webidl \
  SVGAnimatedPoints.webidl \
  SVGAnimatedPreserveAspectRatio.webidl \
  SVGAnimatedTransformList.webidl \
  SVGAnimateElement.webidl \
  SVGAnimateMotionElement.webidl \
  SVGAnimateTransformElement.webidl \
  SVGAnimationElement.webidl \
  SVGCircleElement.webidl \
  SVGClipPathElement.webidl \
  SVGComponentTransferFunctionElement.webidl \
  SVGDefsElement.webidl \
  SVGDescElement.webidl \
  SVGElement.webidl \
  SVGEllipseElement.webidl \
  SVGFilterElement.webidl \
  SVGFilterPrimitiveStandardAttributes.webidl \
  SVGFEBlendElement.webidl \
  SVGFEColorMatrixElement.webidl \
  SVGFEComponentTransferElement.webidl \
  SVGFECompositeElement.webidl \
  SVGFEDistantLightElement.webidl \
  SVGFEFloodElement.webidl \
  SVGFEFuncAElement.webidl \
  SVGFEFuncBElement.webidl \
  SVGFEFuncGElement.webidl \
  SVGFEFuncRElement.webidl \
  SVGFEImageElement.webidl \
  SVGFEMergeElement.webidl \
  SVGFEMergeNodeElement.webidl \
  SVGFEPointLightElement.webidl \
  SVGFETileElement.webidl \
  SVGFitToViewBox.webidl \
  SVGForeignObjectElement.webidl \
  SVGGElement.webidl \
  SVGGradientElement.webidl \
  SVGGraphicsElement.webidl \
  SVGImageElement.webidl \
  SVGLengthList.webidl \
  SVGLinearGradientElement.webidl \
  SVGLineElement.webidl \
  SVGMarkerElement.webidl \
  SVGMaskElement.webidl \
  SVGMatrix.webidl \
  SVGMetadataElement.webidl \
  SVGMPathElement.webidl \
  SVGNumberList.webidl \
  SVGPathElement.webidl \
  SVGPathSeg.webidl \
  SVGPathSegList.webidl \
  SVGPatternElement.webidl \
  SVGPoint.webidl \
  SVGPointList.webidl \
  SVGPolygonElement.webidl \
  SVGPolylineElement.webidl \
  SVGPreserveAspectRatio.webidl \
  SVGRadialGradientElement.webidl \
  SVGRectElement.webidl \
  SVGScriptElement.webidl \
  SVGSetElement.webidl \
  SVGStopElement.webidl \
  SVGStyleElement.webidl \
  SVGSVGElement.webidl \
  SVGSwitchElement.webidl \
  SVGSymbolElement.webidl \
  SVGTests.webidl \
  SVGTextContentElement.webidl \
  SVGTextElement.webidl \
  SVGTextPathElement.webidl \
  SVGTextPositioningElement.webidl \
  SVGTitleElement.webidl \
  SVGTransform.webidl \
  SVGTransformList.webidl \
  SVGTSpanElement.webidl \
  SVGUnitTypes.webidl \
  SVGUseElement.webidl \
  SVGURIReference.webidl \
  SVGViewElement.webidl \
  SVGZoomAndPan.webidl \
  Text.webidl \
  TextDecoder.webidl \
  TextEncoder.webidl \
  TimeRanges.webidl \
  TransitionEvent.webidl \
  TreeWalker.webidl \
  UIEvent.webidl \
  URL.webidl \
  ValidityState.webidl \
  WebComponents.webidl \
  WebSocket.webidl \
  UndoManager.webidl \
  URLUtils.webidl \
  USSDReceivedEvent.webidl \
  XMLHttpRequest.webidl \
  XMLHttpRequestEventTarget.webidl \
  XMLHttpRequestUpload.webidl \
  XMLSerializer.webidl \
  XMLStylesheetProcessingInstruction.webidl \
  XPathEvaluator.webidl \
  XULElement.webidl \
  $(NULL)

ifdef MOZ_AUDIO_CHANNEL_MANAGER
webidl_files += \
  AudioChannelManager.webidl \
  $(NULL)
endif

ifdef MOZ_MEDIA
webidl_files += \
  HTMLSourceElement.webidl \
  MediaError.webidl \
  $(NULL)
endif

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

ifdef ENABLE_TESTS
test_webidl_files := \
  TestCodeGen.webidl \
  TestDictionary.webidl \
  TestExampleGen.webidl \
  TestJSImplGen.webidl \
  TestTypedef.webidl \
  $(NULL)
else
test_webidl_files := $(NULL)
endif

