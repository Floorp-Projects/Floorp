# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

generated_webidl_files = \
  CSS2Properties.webidl \
  $(NULL)

webidl_files = \
  AnalyserNode.webidl \
  AnimationEvent.webidl \
  ArchiveReader.webidl \
  ArchiveRequest.webidl \
  Attr.webidl \
  AudioBuffer.webidl \
  AudioBufferSourceNode.webidl \
  AudioContext.webidl \
  AudioDestinationNode.webidl \
  AudioListener.webidl \
  AudioNode.webidl \
  AudioParam.webidl \
  AudioStreamTrack.webidl \
  AudioProcessingEvent.webidl \
  BatteryManager.webidl \
  BeforeUnloadEvent.webidl \
  BiquadFilterNode.webidl \
  Blob.webidl \
  CanvasRenderingContext2D.webidl \
  CaretPosition.webidl \
  CDATASection.webidl \
  CFStateChangeEvent.webidl \
  ChannelMergerNode.webidl \
  ChannelSplitterNode.webidl \
  CharacterData.webidl \
  ChildNode.webidl \
  ClientRect.webidl \
  ClientRectList.webidl \
  ClipboardEvent.webidl \
  CommandEvent.webidl \
  Comment.webidl \
  CompositionEvent.webidl \
  Coordinates.webidl \
  CSS.webidl \
  CSSPrimitiveValue.webidl \
  CSSStyleDeclaration.webidl \
  CSSStyleSheet.webidl \
  CSSValue.webidl \
  CSSValueList.webidl \
  DelayNode.webidl \
  DesktopNotification.webidl \
  DeviceMotionEvent.webidl \
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
  DragEvent.webidl \
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
  FileReader.webidl \
  FileReaderSync.webidl \
  FileRequest.webidl \
  FormData.webidl \
  Function.webidl \
  GainNode.webidl \
  Geolocation.webidl \
  HTMLAnchorElement.webidl \
  HTMLAppletElement.webidl \
  HTMLAreaElement.webidl \
  HTMLAudioElement.webidl \
  HTMLBaseElement.webidl \
  HTMLBodyElement.webidl \
  HTMLBRElement.webidl \
  HTMLButtonElement.webidl \
  HTMLCanvasElement.webidl \
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
  HTMLIFrameElement.webidl \
  HTMLImageElement.webidl \
  HTMLInputElement.webidl \
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
  HTMLSelectElement.webidl \
  HTMLSourceElement.webidl \
  HTMLSpanElement.webidl \
  HTMLStyleElement.webidl \
  HTMLTableCaptionElement.webidl \
  HTMLTableCellElement.webidl \
  HTMLTableColElement.webidl \
  HTMLTableElement.webidl \
  HTMLTableRowElement.webidl \
  HTMLTableSectionElement.webidl \
  HTMLTemplateElement.webidl \
  HTMLTextAreaElement.webidl \
  HTMLTimeElement.webidl \
  HTMLTitleElement.webidl \
  HTMLUListElement.webidl \
  HTMLVideoElement.webidl \
  IDBFactory.webidl \
  IDBVersionChangeEvent.webidl \
  ImageData.webidl \
  ImageDocument.webidl \
  InspectorUtils.webidl \
  KeyboardEvent.webidl \
  LinkStyle.webidl \
  LocalMediaStream.webidl \
  Location.webidl \
  MediaError.webidl \
  MediaStream.webidl \
  MediaStreamTrack.webidl \
  MessageEvent.webidl \
  MouseEvent.webidl \
  MouseScrollEvent.webidl \
  MozActivity.webidl \
  MozNamedAttrMap.webidl \
  MutationEvent.webidl \
  MutationObserver.webidl \
  NetDashboard.webidl \
  Node.webidl \
  NodeFilter.webidl \
  NodeIterator.webidl \
  NodeList.webidl \
  Notification.webidl \
  NotifyAudioAvailableEvent.webidl \
  NotifyPaintEvent.webidl \
  PaintRequest.webidl \
  PaintRequestList.webidl \
  PannerNode.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  Position.webidl \
  PositionError.webidl \
  ProcessingInstruction.webidl \
  Range.webidl \
  Rect.webidl \
  RGBColor.webidl \
  RTCConfiguration.webidl \
  Screen.webidl \
  ScriptProcessorNode.webidl \
  ScrollAreaEvent.webidl \
  SimpleGestureEvent.webidl \
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
  SVGAnimatedRect.webidl \
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
  SVGDocument.webidl \
  SVGElement.webidl \
  SVGEllipseElement.webidl \
  SVGFilterElement.webidl \
  SVGFilterPrimitiveStandardAttributes.webidl \
  SVGFEBlendElement.webidl \
  SVGFEColorMatrixElement.webidl \
  SVGFEComponentTransferElement.webidl \
  SVGFECompositeElement.webidl \
  SVGFEConvolveMatrixElement.webidl \
  SVGFEDiffuseLightingElement.webidl \
  SVGFEDisplacementMapElement.webidl \
  SVGFEDistantLightElement.webidl \
  SVGFEFloodElement.webidl \
  SVGFEFuncAElement.webidl \
  SVGFEFuncBElement.webidl \
  SVGFEFuncGElement.webidl \
  SVGFEFuncRElement.webidl \
  SVGFEGaussianBlurElement.webidl \
  SVGFEImageElement.webidl \
  SVGFEMergeElement.webidl \
  SVGFEMergeNodeElement.webidl \
  SVGFEMorphologyElement.webidl \
  SVGFEOffsetElement.webidl \
  SVGFEPointLightElement.webidl \
  SVGFESpecularLightingElement.webidl \
  SVGFESpotLightElement.webidl \
  SVGFETileElement.webidl \
  SVGFETurbulenceElement.webidl \
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
  SVGRect.webidl \
  SVGRectElement.webidl \
  SVGScriptElement.webidl \
  SVGSetElement.webidl \
  SVGStopElement.webidl \
  SVGStringList.webidl \
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
  SVGZoomEvent.webidl \
  Text.webidl \
  TextDecoder.webidl \
  TextEncoder.webidl \
  TimeRanges.webidl \
  Touch.webidl \
  TouchEvent.webidl \
  TransitionEvent.webidl \
  TreeColumns.webidl \
  TreeWalker.webidl \
  UIEvent.webidl \
  URL.webidl \
  ValidityState.webidl \
  WebComponents.webidl \
  WebSocket.webidl \
  WheelEvent.webidl \
  UndoManager.webidl \
  URLUtils.webidl \
  USSDReceivedEvent.webidl \
  VideoStreamTrack.webidl \
  WaveShaperNode.webidl \
  Window.webidl \
  XMLDocument.webidl \
  XMLHttpRequest.webidl \
  XMLHttpRequestEventTarget.webidl \
  XMLHttpRequestUpload.webidl \
  XMLSerializer.webidl \
  XMLStylesheetProcessingInstruction.webidl \
  XPathEvaluator.webidl \
  XULCommandEvent.webidl \
  XULDocument.webidl \
  XULElement.webidl \
  $(NULL)

ifdef MOZ_AUDIO_CHANNEL_MANAGER
webidl_files += \
  AudioChannelManager.webidl \
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

ifdef MOZ_WEBSPEECH
webidl_files += \
  SpeechGrammar.webidl \
  SpeechGrammarList.webidl \
  SpeechRecognitionAlternative.webidl \
  SpeechRecognitionResultList.webidl \
  SpeechRecognitionResult.webidl \
  SpeechRecognition.webidl \
  SpeechSynthesisUtterance.webidl \
  SpeechSynthesisVoice.webidl \
  SpeechSynthesis.webidl \
  $(NULL)
endif

ifdef MOZ_GAMEPAD
webidl_files += \
  Gamepad.webidl \
  $(NULL)
endif

ifdef MOZ_B2G_RIL
webidl_files += \
  MozStkCommandEvent.webidl \
  $(NULL)
endif

ifdef ENABLE_TESTS
test_webidl_files := \
  TestCodeGen.webidl \
  TestDictionary.webidl \
  TestExampleGen.webidl \
  TestJSImplGen.webidl \
  TestJSImplInheritanceGen.webidl \
  TestTypedef.webidl \
  $(NULL)
else
test_webidl_files := $(NULL)
endif

