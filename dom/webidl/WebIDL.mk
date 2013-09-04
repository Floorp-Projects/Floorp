# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webidl_base = $(topsrcdir)/dom/webidl

generated_webidl_files = \
  CSS2Properties.webidl \
  $(NULL)

preprocessed_webidl_files = \
  Crypto.webidl \
  Navigator.webidl \
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
  AudioProcessingEvent.webidl \
  AudioStreamTrack.webidl \
  BarProp.webidl \
  BatteryManager.webidl \
  BeforeUnloadEvent.webidl \
  BiquadFilterNode.webidl \
  Blob.webidl \
  BrowserElementDictionaries.webidl \
  CameraControl.webidl \
  CameraManager.webidl \
  CanvasRenderingContext2D.webidl \
  CaretPosition.webidl \
  CDATASection.webidl \
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
  ConvolverNode.webidl \
  Coordinates.webidl \
  CSS.webidl \
  CSSPrimitiveValue.webidl \
  CSSStyleDeclaration.webidl \
  CSSStyleSheet.webidl \
  CSSValue.webidl \
  CSSValueList.webidl \
  DataContainerEvent.webidl \
  DelayNode.webidl \
  DesktopNotification.webidl \
  DeviceMotionEvent.webidl \
  DeviceStorage.webidl \
  Document.webidl \
  DocumentFragment.webidl \
  DocumentType.webidl \
  DOMCursor.webidl \
  DOMError.webidl \
  DOMImplementation.webidl \
  DOMMMIError.webidl \
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
  FileMode.webidl \
  FileReader.webidl \
  FileReaderSync.webidl \
  FileRequest.webidl \
  FocusEvent.webidl \
  FormData.webidl \
  Function.webidl \
  GainNode.webidl \
  Geolocation.webidl \
  History.webidl \
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
  HTMLFormElement.webidl \
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
  HTMLTrackElement.webidl \
  HTMLUListElement.webidl \
  HTMLVideoElement.webidl \
  IDBCursor.webidl \
  IDBDatabase.webidl \
  IDBFactory.webidl \
  IDBFileHandle.webidl \
  IDBIndex.webidl \
  IDBObjectStore.webidl \
  IDBOpenDBRequest.webidl \
  IDBRequest.webidl \
  IDBTransaction.webidl \
  IDBVersionChangeEvent.webidl \
  ImageData.webidl \
  ImageDocument.webidl \
  InspectorUtils.webidl \
  KeyboardEvent.webidl \
  KeyEvent.webidl \
  LinkStyle.webidl \
  LocalMediaStream.webidl \
  Location.webidl \
  MediaElementAudioSourceNode.webidl \
  MediaError.webidl \
  MediaRecorder.webidl \
  MediaSource.webidl \
  MediaStream.webidl \
  MediaStreamAudioDestinationNode.webidl \
  MediaStreamAudioSourceNode.webidl \
  MediaStreamEvent.webidl \
  MediaStreamTrack.webidl \
  MessageChannel.webidl \
  MessageEvent.webidl \
  MessagePort.webidl \
  MimeType.webidl \
  MimeTypeArray.webidl \
  MobileMessageManager.webidl \
  MouseEvent.webidl \
  MouseScrollEvent.webidl \
  MozActivity.webidl \
  MozMmsMessage.webidl \
  MozNamedAttrMap.webidl \
  MozTimeManager.webidl \
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
  OfflineAudioCompletionEvent.webidl \
  OfflineAudioContext.webidl \
  OfflineResourceList.webidl \
  OscillatorNode.webidl \
  PaintRequest.webidl \
  PaintRequestList.webidl \
  PannerNode.webidl \
  ParentNode.webidl \
  Performance.webidl \
  PerformanceNavigation.webidl \
  PerformanceTiming.webidl \
  PeriodicWave.webidl \
  PhoneNumberService.webidl \
  Plugin.webidl \
  PluginArray.webidl \
  Position.webidl \
  PositionError.webidl \
  ProcessingInstruction.webidl \
  Promise.webidl \
  PushManager.webidl \
  Range.webidl \
  Rect.webidl \
  RGBColor.webidl \
  RTCConfiguration.webidl \
  RTCDataChannelEvent.webidl \
  RTCIceCandidate.webidl \
  RTCPeerConnection.webidl \
  RTCPeerConnectionIceEvent.webidl \
  RTCSessionDescription.webidl \
  Screen.webidl \
  ScriptProcessorNode.webidl \
  ScrollAreaEvent.webidl \
  SettingsManager.webidl \
  SimpleGestureEvent.webidl \
  SourceBuffer.webidl \
  SourceBufferList.webidl \
  StyleSheet.webidl \
  SVGAElement.webidl \
  SVGAltGlyphElement.webidl \
  SVGAngle.webidl \
  SVGAnimatedAngle.webidl \
  SVGAnimatedBoolean.webidl \
  SVGAnimatedEnumeration.webidl \
  SVGAnimatedInteger.webidl \
  SVGAnimatedLength.webidl \
  SVGAnimatedLengthList.webidl \
  SVGAnimatedNumber.webidl \
  SVGAnimatedNumberList.webidl \
  SVGAnimatedPathData.webidl \
  SVGAnimatedPoints.webidl \
  SVGAnimatedPreserveAspectRatio.webidl \
  SVGAnimatedRect.webidl \
  SVGAnimatedString.webidl \
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
  SVGFilterElement.webidl \
  SVGFilterPrimitiveStandardAttributes.webidl \
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
  SVGURIReference.webidl \
  SVGUseElement.webidl \
  SVGViewElement.webidl \
  SVGZoomAndPan.webidl \
  SVGZoomEvent.webidl \
  Text.webidl \
  TextDecoder.webidl \
  TextEncoder.webidl \
  TextTrack.webidl \
  TextTrackCueList.webidl \
  TextTrackList.webidl \
  TimeEvent.webidl \
  TimeRanges.webidl \
  Touch.webidl \
  TouchEvent.webidl \
  TouchList.webidl \
  TransitionEvent.webidl \
  TreeColumns.webidl \
  TreeWalker.webidl \
  UIEvent.webidl \
  UndoManager.webidl \
  URL.webidl \
  URLUtils.webidl \
  URLUtilsReadOnly.webidl \
  VTTCue.webidl \
  ValidityState.webidl \
  VideoPlaybackQuality.webidl \
  VideoStreamTrack.webidl \
  WaveShaperNode.webidl \
  WebComponents.webidl \
  WebSocket.webidl \
  WheelEvent.webidl \
  WifiOptions.webidl \
  Window.webidl \
  WorkerLocation.webidl \
  WorkerNavigator.webidl \
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
  WebGL2RenderingContext.webidl \
  $(NULL)
endif

ifdef MOZ_WEBRTC
webidl_files += \
  DataChannel.webidl \
  MediaStreamList.webidl \
  $(NULL)
endif

ifdef MOZ_WEBSPEECH
webidl_files += \
  SpeechGrammar.webidl \
  SpeechGrammarList.webidl \
  SpeechRecognition.webidl \
  SpeechRecognitionAlternative.webidl \
  SpeechRecognitionResult.webidl \
  SpeechRecognitionResultList.webidl \
  SpeechSynthesis.webidl \
  SpeechSynthesisEvent.webidl \
  SpeechSynthesisUtterance.webidl \
  SpeechSynthesisVoice.webidl \
  $(NULL)
endif

ifdef MOZ_GAMEPAD
webidl_files += \
  Gamepad.webidl \
  $(NULL)
endif

ifdef MOZ_B2G_RIL
webidl_files += \
  CallsList.webidl \
  MozStkCommandEvent.webidl \
  MozVoicemail.webidl \
  Telephony.webidl \
  TelephonyCall.webidl \
  TelephonyCallGroup.webidl \
  $(NULL)
endif

webidl_files += \
  BlobEvent.webidl \
  CloseEvent.webidl \
  CustomEvent.webidl \
  DeviceLightEvent.webidl \
  DeviceOrientationEvent.webidl \
  DeviceProximityEvent.webidl \
  DeviceStorageChangeEvent.webidl \
  DOMTransactionEvent.webidl \
  ElementReplaceEvent.webidl \
  HashChangeEvent.webidl \
  MozApplicationEvent.webidl \
  MozContactChangeEvent.webidl \
  MozMmsEvent.webidl \
  MozSettingsEvent.webidl \
  MozSmsEvent.webidl \
  PageTransitionEvent.webidl \
  PopStateEvent.webidl \
  PopupBlockedEvent.webidl \
  ProgressEvent.webidl \
  RecordErrorEvent.webidl \
  SmartCardEvent.webidl \
  StorageEvent.webidl \
  StyleRuleChangeEvent.webidl \
  StyleSheetApplicableStateChangeEvent.webidl \
  StyleSheetChangeEvent.webidl \
  UserProximityEvent.webidl \
  $(NULL)

ifdef MOZ_B2G_BT
webidl_files += \
  BluetoothAdapter.webidl \
  BluetoothDevice.webidl \
  BluetoothDeviceEvent.webidl \
  BluetoothManager.webidl \
  BluetoothStatusChangedEvent.webidl \
  $(NULL)
endif

ifdef MOZ_B2G_RIL
webidl_files += \
  CallEvent.webidl \
  CFStateChangeEvent.webidl \
  DataErrorEvent.webidl \
  IccCardLockErrorEvent.webidl \
  MozCellBroadcast.webidl \
  MozCellBroadcastEvent.webidl \
  MozEmergencyCbModeEvent.webidl \
  MozOtaStatusEvent.webidl \
  MozVoicemailEvent.webidl \
  MozWifiConnectionInfoEvent.webidl \
  MozWifiStatusChangeEvent.webidl \
  USSDReceivedEvent.webidl \
  $(NULL)
endif

ifdef MOZ_GAMEPAD
webidl_files += \
  GamepadAxisMoveEvent.webidl \
  GamepadButtonEvent.webidl \
  GamepadEvent.webidl \
  $(NULL)
endif

ifdef MOZ_WEBSPEECH
webidl_files += \
  SpeechRecognitionError.webidl \
  SpeechRecognitionEvent.webidl \
  $(NULL)
endif

ifdef MOZ_B2G_FM
webidl_files += FMRadio.webidl
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

ifdef MOZ_B2G
webidl_files += \
  InputMethod.webidl \
  $(NULL)
endif
