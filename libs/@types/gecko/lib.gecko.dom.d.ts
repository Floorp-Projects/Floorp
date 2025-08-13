/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source .webidl files.
 * If you're updating some of the sources, see README for instructions.
 */

/// <reference no-default-lib="true" />
/// <reference lib="es2023" />

interface Principal extends nsIPrincipal {}
interface URI extends nsIURI {}
interface WindowProxy extends Window {}

type HTMLCollectionOf<T> = any;
type IsInstance<T> = (obj: any) => obj is T;
type NodeListOf<T> = any;

/////////////////////////////
/// Window APIs
/////////////////////////////

interface ActivateMenuItemOptions {
    altKey?: boolean;
    button?: number;
    ctrlKey?: boolean;
    metaKey?: boolean;
    shiftKey?: boolean;
}

interface AddEventListenerOptions extends EventListenerOptions {
    once?: boolean;
    passive?: boolean;
    signal?: AbortSignal;
    wantUntrusted?: boolean;
}

interface AddonEventInit extends EventInit {
    id: string;
}

interface AddressErrors {
    addressLine?: string;
    city?: string;
    country?: string;
    dependentLocality?: string;
    organization?: string;
    phone?: string;
    postalCode?: string;
    recipient?: string;
    region?: string;
    regionCode?: string;
    sortingCode?: string;
}

interface AnalyserOptions extends AudioNodeOptions {
    fftSize?: number;
    maxDecibels?: number;
    minDecibels?: number;
    smoothingTimeConstant?: number;
}

interface AnimationEventInit extends EventInit {
    animationName?: string;
    elapsedTime?: number;
    pseudoElement?: string;
}

interface AnimationPlaybackEventInit extends EventInit {
    currentTime?: number | null;
    timelineTime?: number | null;
}

interface AnimationPropertyDetails {
    property: string;
    runningOnCompositor: boolean;
    values: AnimationPropertyValueDetails[];
    warning?: string;
}

interface AnimationPropertyValueDetails {
    composite: CompositeOperation;
    easing?: string;
    offset: number;
    value?: string;
}

interface AssignedNodesOptions {
    flatten?: boolean;
}

interface AttributeNameValue {
    name: string;
    value: string;
}

interface AudioBufferOptions {
    length: number;
    numberOfChannels?: number;
    sampleRate: number;
}

interface AudioBufferSourceOptions {
    buffer?: AudioBuffer | null;
    detune?: number;
    loop?: boolean;
    loopEnd?: number;
    loopStart?: number;
    playbackRate?: number;
}

interface AudioConfiguration {
    bitrate?: number;
    channels?: string;
    contentType: string;
    samplerate?: number;
}

interface AudioContextOptions {
    sampleRate?: number;
}

interface AudioDataCopyToOptions {
    format?: AudioSampleFormat;
    frameCount?: number;
    frameOffset?: number;
    planeIndex: number;
}

interface AudioDataInit {
    data: ArrayBufferView | ArrayBuffer;
    format: AudioSampleFormat;
    numberOfChannels: number;
    numberOfFrames: number;
    sampleRate: number;
    timestamp: number;
    transfer?: ArrayBuffer[];
}

interface AudioDecoderConfig {
    codec: string;
    description?: ArrayBufferView | ArrayBuffer;
    numberOfChannels: number;
    sampleRate: number;
}

interface AudioDecoderInit {
    error: WebCodecsErrorCallback;
    output: AudioDataOutputCallback;
}

interface AudioDecoderSupport {
    config?: AudioDecoderConfig;
    supported?: boolean;
}

interface AudioEncoderConfig {
    bitrate?: number;
    bitrateMode?: BitrateMode;
    codec: string;
    numberOfChannels?: number;
    opus?: OpusEncoderConfig;
    sampleRate?: number;
}

interface AudioEncoderInit {
    error: WebCodecsErrorCallback;
    output: EncodedAudioChunkOutputCallback;
}

interface AudioEncoderSupport {
    config?: AudioEncoderConfig;
    supported?: boolean;
}

interface AudioNodeOptions {
    channelCount?: number;
    channelCountMode?: ChannelCountMode;
    channelInterpretation?: ChannelInterpretation;
}

interface AudioOutputOptions {
    deviceId?: string;
}

interface AudioSinkDebugInfo {
    audioEnded?: boolean;
    hasErrored?: boolean;
    isPlaying?: boolean;
    isStarted?: boolean;
    lastGoodPosition?: number;
    outputRate?: number;
    playbackComplete?: boolean;
    startTime?: number;
    written?: number;
}

interface AudioSinkWrapperDebugInfo {
    audioEnded?: boolean;
    audioSink?: AudioSinkDebugInfo;
    isPlaying?: boolean;
    isStarted?: boolean;
}

interface AudioTimestamp {
    contextTime?: number;
    performanceTime?: DOMHighResTimeStamp;
}

interface AudioWorkletNodeOptions extends AudioNodeOptions {
    numberOfInputs?: number;
    numberOfOutputs?: number;
    outputChannelCount?: number[];
    parameterData?: Record<string, number>;
    processorOptions?: any;
}

interface AuthenticationExtensionsClientInputs {
    appid?: string;
    credProps?: boolean;
    hmacCreateSecret?: boolean;
    minPinLength?: boolean;
}

interface AuthenticationExtensionsClientInputsJSON {
    appid?: string;
    credProps?: boolean;
    hmacCreateSecret?: boolean;
    minPinLength?: boolean;
}

interface AuthenticationExtensionsClientOutputs {
    appid?: boolean;
    credProps?: CredentialPropertiesOutput;
    hmacCreateSecret?: boolean;
}

interface AuthenticatorSelectionCriteria {
    authenticatorAttachment?: string;
    requireResidentKey?: boolean;
    residentKey?: string;
    userVerification?: string;
}

interface AutocompleteInfo {
    addressType?: string;
    canAutomaticallyPersist?: boolean;
    contactType?: string;
    credentialType?: string;
    fieldName?: string;
    section?: string;
}

interface AvcEncoderConfig {
    format?: AvcBitstreamFormat;
}

interface Base64URLDecodeOptions {
    padding: Base64URLDecodePadding;
}

interface Base64URLEncodeOptions {
    pad: boolean;
}

interface BiquadFilterOptions extends AudioNodeOptions {
    Q?: number;
    detune?: number;
    frequency?: number;
    gain?: number;
    type?: BiquadFilterType;
}

interface BlobEventInit extends EventInit {
    data: Blob;
}

interface BlobPropertyBag {
    endings?: EndingType;
    type?: string;
}

interface BlockParsingOptions {
    blockScriptCreated?: boolean;
}

interface BoxQuadOptions {
    box?: CSSBoxType;
    createFramesForSuppressedWhitespace?: boolean;
    relativeTo?: GeometryNode;
}

interface BufferRange {
    end?: number;
    start?: number;
}

interface CDMInformation {
    capabilities: string;
    clearlead: boolean;
    isHDCP22Compatible: boolean;
    keySystemName: string;
}

interface CSSCustomPropertyRegisteredEventInit extends EventInit {
    propertyDefinition: InspectorCSSPropertyDefinition;
}

interface CSSStyleSheetInit {
    baseURL?: string;
    disabled?: boolean;
    media?: MediaList | string;
}

interface CacheQueryOptions {
    ignoreMethod?: boolean;
    ignoreSearch?: boolean;
    ignoreVary?: boolean;
}

interface CanvasRenderingContext2DSettings {
    alpha?: boolean;
    colorSpace?: PredefinedColorSpace;
    desynchronized?: boolean;
    willReadFrequently?: boolean;
}

interface CaretPositionFromPointOptions {
    shadowRoots?: ShadowRoot[];
}

interface CaretStateChangedEventInit extends EventInit {
    boundingClientRect?: DOMRectReadOnly | null;
    caretVisible?: boolean;
    caretVisuallyVisible?: boolean;
    clientX?: number;
    clientY?: number;
    collapsed?: boolean;
    reason?: CaretChangedReason;
    selectedTextContent?: string;
    selectionEditable?: boolean;
    selectionVisible?: boolean;
}

interface ChannelMergerOptions extends AudioNodeOptions {
    numberOfInputs?: number;
}

interface ChannelSplitterOptions extends AudioNodeOptions {
    numberOfOutputs?: number;
}

interface CheckVisibilityOptions {
    checkOpacity?: boolean;
    checkVisibilityCSS?: boolean;
    contentVisibilityAuto?: boolean;
    flush?: boolean;
    opacityProperty?: boolean;
    visibilityProperty?: boolean;
}

interface CheckerboardReport {
    log?: string;
    reason?: CheckerboardReason;
    severity?: number;
    timestamp?: DOMTimeStamp;
}

interface ChildProcInfoDictionary {
    childID?: number;
    cpuCycleCount?: number;
    cpuTime?: number;
    memory?: number;
    origin?: string;
    pid?: number;
    threads?: ThreadInfoDictionary[];
    type?: WebIDLProcType;
    utilityActors?: UtilityActorsDictionary[];
    windows?: WindowInfoDictionary[];
}

interface ChromeFilePropertyBag extends FilePropertyBag {
    existenceCheck?: boolean;
    name?: string;
}

interface ClientRectsAndTexts {
    rectList: DOMRectList;
    textList: string[];
}

interface ClipboardEventInit extends EventInit {
    data?: string;
    dataType?: string;
}

interface ClipboardItemOptions {
    presentationStyle?: PresentationStyle;
}

interface CloseEventInit extends EventInit {
    code?: number;
    reason?: string;
    wasClean?: boolean;
}

interface CollectedData {
    children?: any[];
    id?: Record<string, CollectedFormDataValue>;
    innerHTML?: string;
    scroll?: string;
    url?: string;
    xpath?: Record<string, CollectedFormDataValue>;
}

interface CompileScriptOptionsDictionary {
    charset?: string;
    filename?: string;
    hasReturnValue?: boolean;
    lazilyParse?: boolean;
}

interface CompositionEventInit extends UIEventInit {
    data?: string;
}

interface ComputedEffectTiming extends EffectTiming {
    activeDuration?: number;
    currentIteration?: number | null;
    endTime?: number;
    localTime?: number | null;
    progress?: number | null;
}

interface ConsoleInstanceOptions {
    consoleID?: string;
    dump?: ConsoleInstanceDumpCallback;
    innerID?: string;
    maxLogLevel?: ConsoleLogLevel;
    maxLogLevelPref?: string;
    prefix?: string;
}

interface ConstantSourceOptions {
    offset?: number;
}

interface ConstrainBooleanParameters {
    exact?: boolean;
    ideal?: boolean;
}

interface ConstrainDOMStringParameters {
    exact?: string | string[];
    ideal?: string | string[];
}

interface ConstrainDoubleRange {
    exact?: number;
    ideal?: number;
    max?: number;
    min?: number;
}

interface ConstrainLongRange {
    exact?: number;
    ideal?: number;
    max?: number;
    min?: number;
}

interface ContentVisibilityAutoStateChangeEventInit extends EventInit {
    skipped?: boolean;
}

interface ConvertCoordinateOptions {
    fromBox?: CSSBoxType;
    toBox?: CSSBoxType;
}

interface ConvolverOptions extends AudioNodeOptions {
    buffer?: AudioBuffer | null;
    disableNormalization?: boolean;
}

interface CopyOptions {
    noOverwrite?: boolean;
    recursive?: boolean;
}

interface CredentialCreationOptions {
    identity?: IdentityCredentialInit;
    publicKey?: PublicKeyCredentialCreationOptions;
    signal?: AbortSignal;
}

interface CredentialPropertiesOutput {
    rk?: boolean;
}

interface CredentialRequestOptions {
    identity?: IdentityCredentialRequestOptions;
    mediation?: CredentialMediationRequirement;
    publicKey?: PublicKeyCredentialRequestOptions;
    signal?: AbortSignal;
}

interface CustomEventInit extends EventInit {
    detail?: any;
}

interface DOMMatrix2DInit {
    a?: number;
    b?: number;
    c?: number;
    d?: number;
    e?: number;
    f?: number;
    m11?: number;
    m12?: number;
    m21?: number;
    m22?: number;
    m41?: number;
    m42?: number;
}

interface DOMMatrixInit extends DOMMatrix2DInit {
    is2D?: boolean;
    m13?: number;
    m14?: number;
    m23?: number;
    m24?: number;
    m31?: number;
    m32?: number;
    m33?: number;
    m34?: number;
    m43?: number;
    m44?: number;
}

interface DOMPointInit {
    w?: number;
    x?: number;
    y?: number;
    z?: number;
}

interface DOMQuadInit {
    p1?: DOMPointInit;
    p2?: DOMPointInit;
    p3?: DOMPointInit;
    p4?: DOMPointInit;
}

interface DOMRectInit {
    height?: number;
    width?: number;
    x?: number;
    y?: number;
}

interface DateTimeValue {
    day?: number;
    hour?: number;
    minute?: number;
    month?: number;
    year?: number;
}

interface DecodedStreamDataDebugInfo {
    audioFramesWritten?: number;
    haveSentFinishAudio?: boolean;
    haveSentFinishVideo?: boolean;
    instance?: string;
    lastVideoEndTime?: number;
    lastVideoStartTime?: number;
    nextAudioTime?: number;
    streamAudioWritten?: number;
    streamVideoWritten?: number;
}

interface DecodedStreamDebugInfo {
    audioQueueFinished?: boolean;
    audioQueueSize?: number;
    data?: DecodedStreamDataDebugInfo;
    instance?: string;
    lastAudio?: number;
    lastOutputTime?: number;
    playing?: number;
    startTime?: number;
}

interface DelayOptions extends AudioNodeOptions {
    delayTime?: number;
    maxDelayTime?: number;
}

interface DeviceAccelerationInit {
    x?: number | null;
    y?: number | null;
    z?: number | null;
}

interface DeviceLightEventInit extends EventInit {
    value?: number;
}

interface DeviceMotionEventInit extends EventInit {
    acceleration?: DeviceAccelerationInit;
    accelerationIncludingGravity?: DeviceAccelerationInit;
    interval?: number | null;
    rotationRate?: DeviceRotationRateInit;
}

interface DeviceOrientationEventInit extends EventInit {
    absolute?: boolean;
    alpha?: number | null;
    beta?: number | null;
    gamma?: number | null;
}

interface DeviceRotationRateInit {
    alpha?: number | null;
    beta?: number | null;
    gamma?: number | null;
}

interface DictWithAllowSharedBufferSource {
    allowSharedArrayBuffer?: ArrayBuffer;
    allowSharedArrayBufferView?: ArrayBufferView;
    arrayBuffer?: ArrayBuffer;
    arrayBufferView?: ArrayBufferView;
}

interface DisplayMediaStreamConstraints {
    audio?: boolean | MediaTrackConstraints;
    video?: boolean | MediaTrackConstraints;
}

interface DisplayNameOptions {
    calendar?: string;
    keys?: string[];
    style?: string;
    type?: string;
}

interface DisplayNameResult {
    calendar?: string;
    locale?: string;
    style?: string;
    type?: string;
    values?: string[];
}

interface DocumentTimelineOptions {
    originTime?: DOMHighResTimeStamp;
}

interface DragEventInit extends MouseEventInit {
    dataTransfer?: DataTransfer | null;
}

interface DynamicsCompressorOptions extends AudioNodeOptions {
    attack?: number;
    knee?: number;
    ratio?: number;
    release?: number;
    threshold?: number;
}

interface EMEDebugInfo {
    keySystem?: string;
    sessionsInfo?: string;
}

interface EffectTiming {
    delay?: number;
    direction?: PlaybackDirection;
    duration?: number | string;
    easing?: string;
    endDelay?: number;
    fill?: FillMode;
    iterationStart?: number;
    iterations?: number;
}

interface ElementCreationOptions {
    is?: string;
    pseudo?: string;
}

interface ElementDefinitionOptions {
    extends?: string;
}

interface EncodedAudioChunkInit {
    data: ArrayBufferView | ArrayBuffer;
    duration?: number;
    timestamp: number;
    transfer?: ArrayBuffer[];
    type: EncodedAudioChunkType;
}

interface EncodedAudioChunkMetadata {
    decoderConfig?: AudioDecoderConfig;
}

interface EncodedVideoChunkInit {
    data: ArrayBufferView | ArrayBuffer;
    duration?: number;
    timestamp: number;
    type: EncodedVideoChunkType;
}

interface EncodedVideoChunkMetadata {
    decoderConfig?: VideoDecoderConfig;
    svc?: SvcOutputMetadata;
}

interface ErrorEventInit extends EventInit {
    colno?: number;
    error?: any;
    filename?: string;
    lineno?: number;
    message?: string;
}

interface EventInit {
    bubbles?: boolean;
    cancelable?: boolean;
    composed?: boolean;
}

interface EventListenerOptions {
    capture?: boolean;
    mozSystemGroup?: boolean;
}

interface EventModifierInit extends UIEventInit {
    altKey?: boolean;
    ctrlKey?: boolean;
    metaKey?: boolean;
    modifierAltGraph?: boolean;
    modifierCapsLock?: boolean;
    modifierFn?: boolean;
    modifierFnLock?: boolean;
    modifierNumLock?: boolean;
    modifierOS?: boolean;
    modifierScrollLock?: boolean;
    modifierSymbol?: boolean;
    modifierSymbolLock?: boolean;
    shiftKey?: boolean;
}

interface EventSourceInit {
    withCredentials?: boolean;
}

interface ExecuteInGlobalOptions {
    reportExceptions?: boolean;
}

interface FailedCertSecurityInfo {
    certChainStrings?: string[];
    certValidityRangeNotAfter?: DOMTimeStamp;
    certValidityRangeNotBefore?: DOMTimeStamp;
    channelStatus?: number;
    errorCodeString?: string;
    errorMessage?: string;
    hasHPKP?: boolean;
    hasHSTS?: boolean;
    issuerCommonName?: string;
    overridableErrorCategory?: OverridableErrorCategory;
    validNotAfter?: DOMTimeStamp;
    validNotBefore?: DOMTimeStamp;
}

interface FileInfo {
    creationTime?: number;
    lastAccessed?: number;
    lastModified?: number;
    path?: string;
    permissions?: number;
    size?: number;
    type?: FileType;
}

interface FilePropertyBag extends BlobPropertyBag {
    lastModified?: number;
}

interface FileSourceOptions {
    addResourceOptions?: FluentBundleAddResourceOptions;
}

interface FileSystemCreateWritableOptions {
    keepExistingData?: boolean;
}

interface FileSystemFlags {
    create?: boolean;
    exclusive?: boolean;
}

interface FileSystemGetDirectoryOptions {
    create?: boolean;
}

interface FileSystemGetFileOptions {
    create?: boolean;
}

interface FileSystemRemoveOptions {
    recursive?: boolean;
}

interface FluentBundleAddResourceOptions {
    allowOverrides?: boolean;
}

interface FluentBundleIteratorResult {
    done: boolean;
    value: FluentBundle | null;
}

interface FluentBundleOptions {
    pseudoStrategy?: string;
    useIsolating?: boolean;
}

interface FluentMessage {
    attributes: Record<string, FluentPattern>;
    value?: FluentPattern | null;
}

interface FluentTextElementItem {
    attr?: string;
    id?: string;
    text?: string;
}

interface FocusEventInit extends UIEventInit {
    relatedTarget?: EventTarget | null;
}

interface FocusOptions {
    focusVisible?: boolean;
    preventScroll?: boolean;
}

interface FontFaceDescriptors {
    ascentOverride?: string;
    descentOverride?: string;
    display?: string;
    featureSettings?: string;
    lineGapOverride?: string;
    sizeAdjust?: string;
    stretch?: string;
    style?: string;
    unicodeRange?: string;
    variant?: string;
    variationSettings?: string;
    weight?: string;
}

interface FontFaceSetIteratorResult {
    done: boolean;
    value: any;
}

interface FontFaceSetLoadEventInit extends EventInit {
    fontfaces?: FontFace[];
}

interface FormAutofillConfidences {
    ccName?: number;
    ccNumber?: number;
}

interface FormDataEventInit extends EventInit {
    formData: FormData;
}

interface FrameCrashedEventInit extends EventInit {
    browsingContextId?: number;
    childID?: number;
    isTopFrame?: boolean;
}

interface GPUBindGroupDescriptor extends GPUObjectDescriptorBase {
    entries: GPUBindGroupEntry[];
    layout: GPUBindGroupLayout;
}

interface GPUBindGroupEntry {
    binding: GPUIndex32;
    resource: GPUBindingResource;
}

interface GPUBindGroupLayoutDescriptor extends GPUObjectDescriptorBase {
    entries: GPUBindGroupLayoutEntry[];
}

interface GPUBindGroupLayoutEntry {
    binding: GPUIndex32;
    buffer?: GPUBufferBindingLayout;
    sampler?: GPUSamplerBindingLayout;
    storageTexture?: GPUStorageTextureBindingLayout;
    texture?: GPUTextureBindingLayout;
    visibility: GPUShaderStageFlags;
}

interface GPUBlendComponent {
    dstFactor?: GPUBlendFactor;
    operation?: GPUBlendOperation;
    srcFactor?: GPUBlendFactor;
}

interface GPUBlendState {
    alpha: GPUBlendComponent;
    color: GPUBlendComponent;
}

interface GPUBufferBinding {
    buffer: GPUBuffer;
    offset?: GPUSize64;
    size?: GPUSize64;
}

interface GPUBufferBindingLayout {
    hasDynamicOffset?: boolean;
    minBindingSize?: GPUSize64;
    type?: GPUBufferBindingType;
}

interface GPUBufferDescriptor extends GPUObjectDescriptorBase {
    mappedAtCreation?: boolean;
    size: GPUSize64;
    usage: GPUBufferUsageFlags;
}

interface GPUCanvasConfiguration {
    alphaMode?: GPUCanvasAlphaMode;
    device: GPUDevice;
    format: GPUTextureFormat;
    usage?: GPUTextureUsageFlags;
    viewFormats?: GPUTextureFormat[];
}

interface GPUColorDict {
    a: number;
    b: number;
    g: number;
    r: number;
}

interface GPUColorTargetState {
    blend?: GPUBlendState;
    format: GPUTextureFormat;
    writeMask?: GPUColorWriteFlags;
}

interface GPUCommandBufferDescriptor extends GPUObjectDescriptorBase {
}

interface GPUCommandEncoderDescriptor extends GPUObjectDescriptorBase {
}

interface GPUComputePassDescriptor extends GPUObjectDescriptorBase {
}

interface GPUComputePipelineDescriptor extends GPUPipelineDescriptorBase {
    compute: GPUProgrammableStage;
}

interface GPUDepthStencilState {
    depthBias?: GPUDepthBias;
    depthBiasClamp?: number;
    depthBiasSlopeScale?: number;
    depthCompare?: GPUCompareFunction;
    depthWriteEnabled?: boolean;
    format: GPUTextureFormat;
    stencilBack?: GPUStencilFaceState;
    stencilFront?: GPUStencilFaceState;
    stencilReadMask?: GPUStencilValue;
    stencilWriteMask?: GPUStencilValue;
}

interface GPUDeviceDescriptor extends GPUObjectDescriptorBase {
    defaultQueue?: GPUQueueDescriptor;
    requiredFeatures?: GPUFeatureName[];
    requiredLimits?: Record<string, GPUSize64>;
}

interface GPUExtent3DDict {
    depthOrArrayLayers?: GPUIntegerCoordinate;
    height?: GPUIntegerCoordinate;
    width: GPUIntegerCoordinate;
}

interface GPUFragmentState extends GPUProgrammableStage {
    targets: GPUColorTargetState[];
}

interface GPUImageCopyBuffer extends GPUImageDataLayout {
    buffer: GPUBuffer;
}

interface GPUImageCopyExternalImage {
    flipY?: boolean;
    origin?: GPUOrigin2D;
    source: ImageBitmap | HTMLCanvasElement | OffscreenCanvas;
}

interface GPUImageCopyTexture {
    aspect?: GPUTextureAspect;
    mipLevel?: GPUIntegerCoordinate;
    origin?: GPUOrigin3D;
    texture: GPUTexture;
}

interface GPUImageCopyTextureTagged extends GPUImageCopyTexture {
    premultipliedAlpha?: boolean;
}

interface GPUImageDataLayout {
    bytesPerRow?: GPUSize32;
    offset?: GPUSize64;
    rowsPerImage?: GPUSize32;
}

interface GPUMultisampleState {
    alphaToCoverageEnabled?: boolean;
    count?: GPUSize32;
    mask?: GPUSampleMask;
}

interface GPUObjectDescriptorBase {
    label?: string;
}

interface GPUOrigin2DDict {
    x?: GPUIntegerCoordinate;
    y?: GPUIntegerCoordinate;
}

interface GPUOrigin3DDict {
    x?: GPUIntegerCoordinate;
    y?: GPUIntegerCoordinate;
    z?: GPUIntegerCoordinate;
}

interface GPUPipelineDescriptorBase extends GPUObjectDescriptorBase {
    layout: GPUPipelineLayout | GPUAutoLayoutMode;
}

interface GPUPipelineLayoutDescriptor extends GPUObjectDescriptorBase {
    bindGroupLayouts: GPUBindGroupLayout[];
}

interface GPUPrimitiveState {
    cullMode?: GPUCullMode;
    frontFace?: GPUFrontFace;
    stripIndexFormat?: GPUIndexFormat;
    topology?: GPUPrimitiveTopology;
    unclippedDepth?: boolean;
}

interface GPUProgrammableStage {
    constants?: Record<string, GPUPipelineConstantValue>;
    entryPoint?: string;
    module: GPUShaderModule;
}

interface GPUQueueDescriptor extends GPUObjectDescriptorBase {
}

interface GPURenderBundleDescriptor extends GPUObjectDescriptorBase {
}

interface GPURenderBundleEncoderDescriptor extends GPURenderPassLayout {
    depthReadOnly?: boolean;
    stencilReadOnly?: boolean;
}

interface GPURenderPassColorAttachment {
    clearValue?: GPUColor;
    loadOp: GPULoadOp;
    resolveTarget?: GPUTextureView;
    storeOp: GPUStoreOp;
    view: GPUTextureView;
}

interface GPURenderPassDepthStencilAttachment {
    depthClearValue?: number;
    depthLoadOp?: GPULoadOp;
    depthReadOnly?: boolean;
    depthStoreOp?: GPUStoreOp;
    stencilClearValue?: GPUStencilValue;
    stencilLoadOp?: GPULoadOp;
    stencilReadOnly?: boolean;
    stencilStoreOp?: GPUStoreOp;
    view: GPUTextureView;
}

interface GPURenderPassDescriptor extends GPUObjectDescriptorBase {
    colorAttachments: GPURenderPassColorAttachment[];
    depthStencilAttachment?: GPURenderPassDepthStencilAttachment;
    occlusionQuerySet?: GPUQuerySet;
}

interface GPURenderPassLayout extends GPUObjectDescriptorBase {
    colorFormats: GPUTextureFormat[];
    depthStencilFormat?: GPUTextureFormat;
    sampleCount?: GPUSize32;
}

interface GPURenderPipelineDescriptor extends GPUPipelineDescriptorBase {
    depthStencil?: GPUDepthStencilState;
    fragment?: GPUFragmentState;
    multisample?: GPUMultisampleState;
    primitive?: GPUPrimitiveState;
    vertex: GPUVertexState;
}

interface GPURequestAdapterOptions {
    forceFallbackAdapter?: boolean;
    powerPreference?: GPUPowerPreference;
}

interface GPUSamplerBindingLayout {
    type?: GPUSamplerBindingType;
}

interface GPUSamplerDescriptor extends GPUObjectDescriptorBase {
    addressModeU?: GPUAddressMode;
    addressModeV?: GPUAddressMode;
    addressModeW?: GPUAddressMode;
    compare?: GPUCompareFunction;
    lodMaxClamp?: number;
    lodMinClamp?: number;
    magFilter?: GPUFilterMode;
    maxAnisotropy?: number;
    minFilter?: GPUFilterMode;
    mipmapFilter?: GPUMipmapFilterMode;
}

interface GPUShaderModuleDescriptor extends GPUObjectDescriptorBase {
    code: string;
    sourceMap?: any;
}

interface GPUStencilFaceState {
    compare?: GPUCompareFunction;
    depthFailOp?: GPUStencilOperation;
    failOp?: GPUStencilOperation;
    passOp?: GPUStencilOperation;
}

interface GPUStorageTextureBindingLayout {
    access?: GPUStorageTextureAccess;
    format: GPUTextureFormat;
    viewDimension?: GPUTextureViewDimension;
}

interface GPUTextureBindingLayout {
    multisampled?: boolean;
    sampleType?: GPUTextureSampleType;
    viewDimension?: GPUTextureViewDimension;
}

interface GPUTextureDescriptor extends GPUObjectDescriptorBase {
    dimension?: GPUTextureDimension;
    format: GPUTextureFormat;
    mipLevelCount?: GPUIntegerCoordinate;
    sampleCount?: GPUSize32;
    size: GPUExtent3D;
    usage: GPUTextureUsageFlags;
    viewFormats?: GPUTextureFormat[];
}

interface GPUTextureViewDescriptor extends GPUObjectDescriptorBase {
    arrayLayerCount?: GPUIntegerCoordinate;
    aspect?: GPUTextureAspect;
    baseArrayLayer?: GPUIntegerCoordinate;
    baseMipLevel?: GPUIntegerCoordinate;
    dimension?: GPUTextureViewDimension;
    format?: GPUTextureFormat;
    mipLevelCount?: GPUIntegerCoordinate;
}

interface GPUUncapturedErrorEventInit extends EventInit {
    error: GPUError;
}

interface GPUVertexAttribute {
    format: GPUVertexFormat;
    offset: GPUSize64;
    shaderLocation: GPUIndex32;
}

interface GPUVertexBufferLayout {
    arrayStride: GPUSize64;
    attributes: GPUVertexAttribute[];
    stepMode?: GPUVertexStepMode;
}

interface GPUVertexState extends GPUProgrammableStage {
    buffers?: (GPUVertexBufferLayout | null)[];
}

interface GainOptions extends AudioNodeOptions {
    gain?: number;
}

interface GamepadAxisMoveEventInit extends GamepadEventInit {
    axis?: number;
    value?: number;
}

interface GamepadButtonEventInit extends GamepadEventInit {
    button?: number;
}

interface GamepadEventInit extends EventInit {
    gamepad?: Gamepad | null;
}

interface GamepadLightColor {
    blue: number;
    green: number;
    red: number;
}

interface GetAnimationsOptions {
    subtree?: boolean;
}

interface GetChildrenOptions {
    ignoreAbsent?: boolean;
}

interface GetHTMLOptions {
    serializableShadowRoots?: boolean;
    shadowRoots?: ShadowRoot[];
}

interface GetNotificationOptions {
    tag?: string;
}

interface GetRootNodeOptions {
    composed?: boolean;
}

interface GleanDistributionData {
    count: number;
    sum: number;
    values: Record<string, number>;
}

interface GleanEventRecord {
    category: string;
    extra?: Record<string, string>;
    name: string;
    timestamp: number;
}

interface GleanRateData {
    denominator: number;
    numerator: number;
}

interface HTMLMediaElementDebugInfo {
    EMEInfo?: EMEDebugInfo;
    compositorDroppedFrames?: number;
    decoder?: MediaDecoderDebugInfo;
}

interface HashChangeEventInit extends EventInit {
    newURL?: string;
    oldURL?: string;
}

interface HeapSnapshotBoundaries {
    debugger?: any;
    globals?: any[];
    runtime?: boolean;
}

interface IDBDatabaseInfo {
    name?: string;
    version?: number;
}

interface IDBIndexParameters {
    locale?: string | null;
    multiEntry?: boolean;
    unique?: boolean;
}

interface IDBObjectStoreParameters {
    autoIncrement?: boolean;
    keyPath?: string | string[] | null;
}

interface IDBOpenDBOptions {
    version?: number;
}

interface IDBTransactionOptions {
    durability?: IDBTransactionDurability;
}

interface IDBVersionChangeEventInit extends EventInit {
    newVersion?: number | null;
    oldVersion?: number;
}

interface IIRFilterOptions extends AudioNodeOptions {
    feedback: number[];
    feedforward: number[];
}

interface IdentityCredentialInit {
    effectiveOrigins?: string[];
    effectiveQueryURL?: string;
    id: string;
    token?: string;
    uiHint?: IdentityCredentialUserData;
}

interface IdentityCredentialRequestOptions {
    providers?: IdentityProviderConfig[];
}

interface IdentityCredentialUserData {
    expiresAfter?: number;
    iconURL: string;
    name: string;
}

interface IdentityProviderConfig {
    clientId?: string;
    configURL?: string;
    data?: string;
    effectiveQueryURL?: string;
    loginTarget?: IdentityLoginTargetType;
    loginURL?: string;
    nonce?: string;
    origin?: string;
}

interface IdleRequestOptions {
    timeout?: number;
}

interface ImageBitmapOptions {
    colorSpaceConversion?: ColorSpaceConversion;
    imageOrientation?: ImageOrientation;
    premultiplyAlpha?: PremultiplyAlpha;
    resizeHeight?: number;
    resizeWidth?: number;
}

interface ImageCaptureErrorEventInit extends EventInit {
    imageCaptureError?: ImageCaptureError | null;
}

interface ImageDecodeOptions {
    completeFramesOnly?: boolean;
    frameIndex?: number;
}

interface ImageDecodeResult {
    complete: boolean;
    image: VideoFrame;
}

interface ImageDecoderInit {
    colorSpaceConversion?: ColorSpaceConversion;
    data: ImageBufferSource;
    desiredHeight?: number;
    desiredWidth?: number;
    preferAnimation?: boolean;
    transfer?: ArrayBuffer[];
    type: string;
}

interface ImageEncodeOptions {
    quality?: number;
    type?: string;
}

interface ImageText {
    confidence: number;
    quad: DOMQuad;
    string: string;
}

interface ImportESModuleOptionsDictionary {
    global?: ImportESModuleTargetGlobal;
}

interface InputEventInit extends UIEventInit {
    data?: string | null;
    dataTransfer?: DataTransfer | null;
    inputType?: string;
    isComposing?: boolean;
    targetRanges?: StaticRange[];
}

interface InspectorCSSPropertyDefinition {
    fromJS: boolean;
    inherits: boolean;
    initialValue: string | null;
    name: string;
    syntax: string;
}

interface InspectorCSSToken {
    number?: number | null;
    text: string;
    tokenType: string;
    unit: string | null;
    value: string | null;
}

interface InspectorColorToResult {
    adjusted: boolean;
    color: string;
    components: number[] | Float32Array;
}

interface InspectorFontFeature {
    languageSystem: string;
    script: string;
    tag: string;
}

interface InspectorRGBATuple {
    a?: number;
    b?: number;
    g?: number;
    r?: number;
}

interface InspectorStyleSheetRuleCountAndAtRulesResult {
    atRules: CSSRule[];
    ruleCount: number;
}

interface InspectorVariationAxis {
    defaultValue: number;
    maxValue: number;
    minValue: number;
    name: string;
    tag: string;
}

interface InspectorVariationInstance {
    name: string;
    values: InspectorVariationValue[];
}

interface InspectorVariationValue {
    axis: string;
    value: number;
}

interface InstallTriggerData {
    Hash?: string | null;
    IconURL?: string | null;
    URL?: string;
}

interface InteractionData {
    interactionCount?: number;
    interactionTimeInMilliseconds?: number;
    scrollingDistanceInPixels?: number;
}

interface IntersectionObserverInit {
    root?: Element | Document | null;
    rootMargin?: string;
    threshold?: number | number[];
}

interface InvokeEventInit extends EventInit {
    action?: string;
    invoker?: Element | null;
}

interface KeySystemTrackConfiguration {
    encryptionScheme?: string | null;
    robustness?: string;
}

interface KeyboardEventInit extends EventModifierInit {
    charCode?: number;
    code?: string;
    isComposing?: boolean;
    key?: string;
    keyCode?: number;
    location?: number;
    repeat?: boolean;
    which?: number;
}

interface KeyframeAnimationOptions extends KeyframeEffectOptions {
    id?: string;
}

interface KeyframeEffectOptions extends EffectTiming {
    composite?: CompositeOperation;
    iterationComposite?: IterationCompositeOperation;
    pseudoElement?: string | null;
}

interface L10nFileSourceMockFile {
    path: string;
    source: string;
}

interface L10nIdArgs {
    args?: L10nArgs | null;
    id?: string | null;
}

interface L10nMessage {
    attributes?: AttributeNameValue[] | null;
    value?: string | null;
}

interface L10nOverlaysError {
    code?: number;
    l10nName?: string;
    sourceElementName?: string;
    translatedElementName?: string;
}

interface L10nRegistryOptions {
    bundleOptions?: FluentBundleOptions;
}

interface LibcConstants {
    AT_EACCESS?: number;
    EACCES?: number;
    EAGAIN?: number;
    EINTR?: number;
    EINVAL?: number;
    ENOSYS?: number;
    FD_CLOEXEC?: number;
    F_SETFD?: number;
    F_SETFL?: number;
    O_CREAT?: number;
    O_NONBLOCK?: number;
    O_WRONLY?: number;
    POLLERR?: number;
    POLLHUP?: number;
    POLLIN?: number;
    POLLNVAL?: number;
    POLLOUT?: number;
    PR_CAPBSET_READ?: number;
    WNOHANG?: number;
}

interface LoadURIOptions {
    baseURI?: URI | null;
    cancelContentJSEpoch?: number;
    csp?: ContentSecurityPolicy | null;
    hasValidUserGestureActivation?: boolean;
    headers?: InputStream | null;
    loadFlags?: number;
    postData?: InputStream | null;
    referrerInfo?: ReferrerInfo | null;
    remoteTypeOverride?: string | null;
    textDirectiveUserActivation?: boolean;
    triggeringPrincipal?: Principal | null;
    triggeringRemoteType?: string | null;
    triggeringSandboxFlags?: number;
    triggeringStorageAccess?: boolean;
    triggeringWindowId?: number;
    schemelessInput?: SchemelessInputType;
}

interface LockInfo {
    clientId?: string;
    mode?: LockMode;
    name?: string;
}

interface LockManagerSnapshot {
    held?: LockInfo[];
    pending?: LockInfo[];
}

interface LockOptions {
    ifAvailable?: boolean;
    mode?: LockMode;
    signal?: AbortSignal;
    steal?: boolean;
}

interface MIDIConnectionEventInit extends EventInit {
    port?: MIDIPort | null;
}

interface MIDIMessageEventInit extends EventInit {
    data?: Uint8Array;
}

interface MIDIOptions {
    software?: boolean;
    sysex?: boolean;
}

interface MakeDirectoryOptions {
    createAncestors?: boolean;
    ignoreExisting?: boolean;
    permissions?: number;
}

interface MatchPatternOptions {
    ignorePath?: boolean;
    restrictSchemes?: boolean;
}

interface MediaCacheStreamDebugInfo {
    cacheSuspended?: boolean;
    channelEnded?: boolean;
    channelOffset?: number;
    loadID?: number;
    streamLength?: number;
}

interface MediaCapabilitiesDecodingInfo extends MediaCapabilitiesInfo {
    keySystemAccess: MediaKeySystemAccess | null;
}

interface MediaCapabilitiesInfo {
    powerEfficient: boolean;
    smooth: boolean;
    supported: boolean;
}

interface MediaCapabilitiesKeySystemConfiguration {
    audio?: KeySystemTrackConfiguration;
    distinctiveIdentifier?: MediaKeysRequirement;
    initDataType?: string;
    keySystem: string;
    persistentState?: MediaKeysRequirement;
    sessionTypes?: string[];
    video?: KeySystemTrackConfiguration;
}

interface MediaConfiguration {
    audio?: AudioConfiguration;
    video?: VideoConfiguration;
}

interface MediaDecoderDebugInfo {
    PlayState?: string;
    channels?: number;
    containerType?: string;
    hasAudio?: boolean;
    hasVideo?: boolean;
    instance?: string;
    rate?: number;
    reader?: MediaFormatReaderDebugInfo;
    resource?: MediaResourceDebugInfo;
    stateMachine?: MediaDecoderStateMachineDebugInfo;
}

interface MediaDecoderStateMachineDebugInfo {
    audioCompleted?: boolean;
    audioRequestStatus?: string;
    clock?: number;
    decodedAudioEndTime?: number;
    decodedVideoEndTime?: number;
    duration?: number;
    isPlaying?: boolean;
    mediaSink?: MediaSinkDebugInfo;
    mediaTime?: number;
    playState?: number;
    sentFirstFrameLoadedEvent?: boolean;
    state?: string;
    stateObj?: MediaDecoderStateMachineDecodingStateDebugInfo;
    totalBufferingTimeMs?: number;
    videoCompleted?: boolean;
    videoRequestStatus?: string;
}

interface MediaDecoderStateMachineDecodingStateDebugInfo {
    isPrerolling?: boolean;
}

interface MediaDecodingConfiguration extends MediaConfiguration {
    keySystemConfiguration?: MediaCapabilitiesKeySystemConfiguration;
    type: MediaDecodingType;
}

interface MediaElementAudioSourceOptions {
    mediaElement: HTMLMediaElement;
}

interface MediaEncodingConfiguration extends MediaConfiguration {
    type: MediaEncodingType;
}

interface MediaFormatReaderDebugInfo {
    audioChannels?: number;
    audioDecoderName?: string;
    audioFramesDecoded?: number;
    audioRate?: number;
    audioState?: MediaStateDebugInfo;
    audioType?: string;
    frameStats?: MediaFrameStats;
    totalReadMetadataTimeMs?: number;
    totalWaitingForVideoDataTimeMs?: number;
    videoDecoderName?: string;
    videoHardwareAccelerated?: boolean;
    videoHeight?: number;
    videoNumSamplesOutputTotal?: number;
    videoNumSamplesSkippedTotal?: number;
    videoRate?: number;
    videoState?: MediaStateDebugInfo;
    videoType?: string;
    videoWidth?: number;
}

interface MediaFrameStats {
    droppedCompositorFrames?: number;
    droppedDecodedFrames?: number;
    droppedSinkFrames?: number;
}

interface MediaImage {
    sizes?: string;
    src: string;
    type?: string;
}

interface MediaKeyMessageEventInit extends EventInit {
    message: ArrayBuffer;
    messageType: MediaKeyMessageType;
}

interface MediaKeyNeededEventInit extends EventInit {
    initData?: ArrayBuffer | null;
    initDataType?: string;
}

interface MediaKeySystemConfiguration {
    audioCapabilities?: MediaKeySystemMediaCapability[];
    distinctiveIdentifier?: MediaKeysRequirement;
    initDataTypes?: string[];
    label?: string;
    persistentState?: MediaKeysRequirement;
    sessionTypes?: string[];
    videoCapabilities?: MediaKeySystemMediaCapability[];
}

interface MediaKeySystemMediaCapability {
    contentType?: string;
    encryptionScheme?: string | null;
    robustness?: string;
}

interface MediaKeysPolicy {
    minHdcpVersion?: HDCPVersion;
}

interface MediaMetadataInit {
    album?: string;
    artist?: string;
    artwork?: MediaImage[];
    title?: string;
}

interface MediaPositionState {
    duration?: number;
    playbackRate?: number;
    position?: number;
}

interface MediaQueryListEventInit extends EventInit {
    matches?: boolean;
    media?: string;
}

interface MediaRecorderErrorEventInit extends EventInit {
    error: DOMException;
}

interface MediaRecorderOptions {
    audioBitsPerSecond?: number;
    bitsPerSecond?: number;
    mimeType?: string;
    videoBitsPerSecond?: number;
}

interface MediaResourceDebugInfo {
    cacheStream?: MediaCacheStreamDebugInfo;
}

interface MediaSessionActionDetails {
    action: MediaSessionAction;
    fastSeek?: boolean;
    seekOffset?: number;
    seekTime?: number;
}

interface MediaSinkDebugInfo {
    audioSinkWrapper?: AudioSinkWrapperDebugInfo;
    decodedStream?: DecodedStreamDebugInfo;
    videoSink?: VideoSinkDebugInfo;
}

interface MediaSourceDecoderDebugInfo {
    demuxer?: MediaSourceDemuxerDebugInfo;
    reader?: MediaFormatReaderDebugInfo;
}

interface MediaSourceDemuxerDebugInfo {
    audioTrack?: TrackBuffersManagerDebugInfo;
    videoTrack?: TrackBuffersManagerDebugInfo;
}

interface MediaStateDebugInfo {
    demuxEOS?: number;
    demuxQueueSize?: number;
    drainState?: number;
    hasDecoder?: boolean;
    hasDemuxRequest?: boolean;
    hasPromise?: boolean;
    lastStreamSourceID?: number;
    needInput?: boolean;
    numSamplesInput?: number;
    numSamplesOutput?: number;
    pending?: number;
    queueSize?: number;
    timeTreshold?: number;
    timeTresholdHasSeeked?: boolean;
    waitingForData?: boolean;
    waitingForKey?: boolean;
    waitingPromise?: boolean;
}

interface MediaStreamAudioSourceOptions {
    mediaStream: MediaStream;
}

interface MediaStreamConstraints {
    audio?: boolean | MediaTrackConstraints;
    fake?: boolean;
    peerIdentity?: string | null;
    picture?: boolean;
    video?: boolean | MediaTrackConstraints;
}

interface MediaStreamEventInit extends EventInit {
    stream?: MediaStream | null;
}

interface MediaStreamTrackAudioSourceOptions {
    mediaStreamTrack: MediaStreamTrack;
}

interface MediaStreamTrackEventInit extends EventInit {
    track: MediaStreamTrack;
}

interface MediaTrackConstraintSet {
    autoGainControl?: ConstrainBoolean;
    browserWindow?: number;
    channelCount?: ConstrainLong;
    deviceId?: ConstrainDOMString;
    echoCancellation?: ConstrainBoolean;
    facingMode?: ConstrainDOMString;
    frameRate?: ConstrainDouble;
    groupId?: ConstrainDOMString;
    height?: ConstrainLong;
    mediaSource?: string;
    noiseSuppression?: ConstrainBoolean;
    scrollWithPage?: boolean;
    viewportHeight?: ConstrainLong;
    viewportOffsetX?: ConstrainLong;
    viewportOffsetY?: ConstrainLong;
    viewportWidth?: ConstrainLong;
    width?: ConstrainLong;
}

interface MediaTrackConstraints extends MediaTrackConstraintSet {
    advanced?: MediaTrackConstraintSet[];
}

interface MediaTrackSettings {
    autoGainControl?: boolean;
    browserWindow?: number;
    channelCount?: number;
    deviceId?: string;
    echoCancellation?: boolean;
    facingMode?: string;
    frameRate?: number;
    groupId?: string;
    height?: number;
    mediaSource?: string;
    noiseSuppression?: boolean;
    scrollWithPage?: boolean;
    viewportHeight?: number;
    viewportOffsetX?: number;
    viewportOffsetY?: number;
    viewportWidth?: number;
    width?: number;
}

interface MediaTrackSupportedConstraints {
    aspectRatio?: boolean;
    autoGainControl?: boolean;
    browserWindow?: boolean;
    channelCount?: boolean;
    deviceId?: boolean;
    echoCancellation?: boolean;
    facingMode?: boolean;
    frameRate?: boolean;
    groupId?: boolean;
    height?: boolean;
    latency?: boolean;
    mediaSource?: boolean;
    noiseSuppression?: boolean;
    sampleRate?: boolean;
    sampleSize?: boolean;
    scrollWithPage?: boolean;
    viewportHeight?: boolean;
    viewportOffsetX?: boolean;
    viewportOffsetY?: boolean;
    viewportWidth?: boolean;
    volume?: boolean;
    width?: boolean;
}

interface MerchantValidationEventInit extends EventInit {
    methodName?: string;
    validationURL?: string;
}

interface MessageEventInit extends EventInit {
    data?: any;
    lastEventId?: string;
    origin?: string;
    ports?: MessagePort[];
    source?: MessageEventSource | null;
}

interface MouseEventInit extends EventModifierInit {
    button?: number;
    buttons?: number;
    clientX?: number;
    clientY?: number;
    movementX?: number;
    movementY?: number;
    relatedTarget?: EventTarget | null;
    screenX?: number;
    screenY?: number;
}

interface MoveOptions {
    noOverwrite?: boolean;
}

interface MozDocumentMatcherInit {
    allFrames?: boolean;
    checkPermissions?: boolean;
    excludeGlobs?: MatchGlobOrString[] | null;
    excludeMatches?: MatchPatternSetOrStringSequence | null;
    frameID?: number | null;
    hasActiveTabPermission?: boolean;
    includeGlobs?: MatchGlobOrString[] | null;
    matchAboutBlank?: boolean;
    matchOriginAsFallback?: boolean;
    matches: MatchPatternSetOrStringSequence;
    originAttributesPatterns?: OriginAttributesPatternDictionary[] | null;
}

interface MozFrameAncestorInfo {
    frameId: number;
    url: string;
}

interface MozHTTPHeader {
    name: string;
    value: string;
}

interface MozProxyInfo {
    connectionIsolationKey?: string | null;
    failoverTimeout?: number;
    host: string;
    port: number;
    proxyAuthorizationHeader?: string | null;
    proxyDNS: boolean;
    type: string;
    username?: string | null;
}

interface MozRequestFilter {
    incognito?: boolean | null;
    types?: MozContentPolicyType[] | null;
    urls?: MatchPatternSet | null;
}

interface MozRequestMatchOptions {
    isProxy?: boolean;
}

interface MozUrlClassification {
    firstParty: MozUrlClassificationFlags[];
    thirdParty: MozUrlClassificationFlags[];
}

interface MozXMLHttpRequestParameters {
    mozAnon?: boolean;
    mozSystem?: boolean;
}

interface MultiCacheQueryOptions extends CacheQueryOptions {
    cacheName?: string;
}

interface MutationObserverInit {
    animations?: boolean;
    attributeFilter?: string[];
    attributeOldValue?: boolean;
    attributes?: boolean;
    characterData?: boolean;
    characterDataOldValue?: boolean;
    childList?: boolean;
    chromeOnlyNodes?: boolean;
    subtree?: boolean;
}

interface MutationObservingInfo extends MutationObserverInit {
    observedNode?: Node | null;
}

interface NavigateEventInit extends EventInit {
    canIntercept?: boolean;
    destination: NavigationDestination;
    downloadRequest?: string | null;
    formData?: FormData | null;
    hasUAVisualTransition?: boolean;
    hashChange?: boolean;
    info?: any;
    navigationType?: NavigationType;
    signal: AbortSignal;
    userInitiated?: boolean;
}

interface NavigationCurrentEntryChangeEventInit extends EventInit {
    from: NavigationHistoryEntry;
    navigationType?: NavigationType | null;
}

interface NavigationInterceptOptions {
    focusReset?: NavigationFocusReset;
    handler?: NavigationInterceptHandler;
    scroll?: NavigationScrollBehavior;
}

interface NavigationNavigateOptions extends NavigationOptions {
    history?: NavigationHistoryBehavior;
    state?: any;
}

interface NavigationOptions {
    info?: any;
}

interface NavigationPreloadState {
    enabled?: boolean;
    headerValue?: string;
}

interface NavigationReloadOptions extends NavigationOptions {
    state?: any;
}

interface NavigationResult {
    committed?: Promise<NavigationHistoryEntry>;
    finished?: Promise<NavigationHistoryEntry>;
}

interface NavigationUpdateCurrentEntryOptions {
    state: any;
}

interface NetErrorInfo {
    channelStatus?: number;
    errorCodeString?: string;
}

interface NotificationBehavior {
    noclear?: boolean;
    noscreen?: boolean;
    showOnlyOnce?: boolean;
    soundFile?: string;
    vibrationPattern?: number[];
}

interface NotificationOptions {
    body?: string;
    data?: any;
    dir?: NotificationDirection;
    icon?: string;
    lang?: string;
    mozbehavior?: NotificationBehavior;
    requireInteraction?: boolean;
    silent?: boolean;
    tag?: string;
    vibrate?: VibratePattern;
}

interface ObservableArrayCallbacks {
    deleteBooleanCallback?: SetDeleteBooleanCallback;
    deleteInterfaceCallback?: SetDeleteInterfaceCallback;
    deleteObjectCallback?: SetDeleteObjectCallback;
    setBooleanCallback?: SetDeleteBooleanCallback;
    setInterfaceCallback?: SetDeleteInterfaceCallback;
    setObjectCallback?: SetDeleteObjectCallback;
}

interface OfflineAudioCompletionEventInit extends EventInit {
    renderedBuffer: AudioBuffer;
}

interface OfflineAudioContextOptions {
    length: number;
    numberOfChannels?: number;
    sampleRate: number;
}

interface OpenPopupOptions {
    attributesOverride?: boolean;
    isContextMenu?: boolean;
    position?: string;
    triggerEvent?: Event | null;
    x?: number;
    y?: number;
}

interface OptionalEffectTiming {
    delay?: number;
    direction?: PlaybackDirection;
    duration?: number | string;
    easing?: string;
    endDelay?: number;
    fill?: FillMode;
    iterationStart?: number;
    iterations?: number;
}

interface OpusEncoderConfig {
    complexity?: number;
    format?: OpusBitstreamFormat;
    frameDuration?: number;
    packetlossperc?: number;
    usedtx?: boolean;
    useinbandfec?: boolean;
}

interface OriginAttributesDictionary {
    firstPartyDomain?: string;
    geckoViewSessionContextId?: string;
    partitionKey?: string;
    privateBrowsingId?: number;
    userContextId?: number;
}

interface OriginAttributesPatternDictionary {
    firstPartyDomain?: string;
    geckoViewSessionContextId?: string;
    partitionKey?: string;
    partitionKeyPattern?: PartitionKeyPatternDictionary;
    privateBrowsingId?: number;
    userContextId?: number;
}

interface OscillatorOptions extends AudioNodeOptions {
    detune?: number;
    frequency?: number;
    periodicWave?: PeriodicWave;
    type?: OscillatorType;
}

interface PCErrorData {
    message: string;
    name: PCError;
}

interface PageTransitionEventInit extends EventInit {
    inFrameSwap?: boolean;
    persisted?: boolean;
}

interface PannerOptions extends AudioNodeOptions {
    coneInnerAngle?: number;
    coneOuterAngle?: number;
    coneOuterGain?: number;
    distanceModel?: DistanceModelType;
    maxDistance?: number;
    orientationX?: number;
    orientationY?: number;
    orientationZ?: number;
    panningModel?: PanningModelType;
    positionX?: number;
    positionY?: number;
    positionZ?: number;
    refDistance?: number;
    rolloffFactor?: number;
}

interface ParentProcInfoDictionary {
    children?: ChildProcInfoDictionary[];
    cpuCycleCount?: number;
    cpuTime?: number;
    memory?: number;
    pid?: number;
    threads?: ThreadInfoDictionary[];
    type?: WebIDLProcType;
}

interface PartitionKeyPatternDictionary {
    baseDomain?: string;
    foreignByAncestorContext?: boolean;
    port?: number;
    scheme?: string;
}

interface PayerErrors {
    email?: string;
    name?: string;
    phone?: string;
}

interface PaymentCurrencyAmount {
    currency: string;
    value: string;
}

interface PaymentDetailsBase {
    displayItems?: PaymentItem[];
    modifiers?: PaymentDetailsModifier[];
    shippingOptions?: PaymentShippingOption[];
}

interface PaymentDetailsInit extends PaymentDetailsBase {
    id?: string;
    total: PaymentItem;
}

interface PaymentDetailsModifier {
    additionalDisplayItems?: PaymentItem[];
    data?: any;
    supportedMethods: string;
    total?: PaymentItem;
}

interface PaymentDetailsUpdate extends PaymentDetailsBase {
    error?: string;
    payerErrors?: PayerErrors;
    paymentMethodErrors?: any;
    shippingAddressErrors?: AddressErrors;
    total?: PaymentItem;
}

interface PaymentItem {
    amount: PaymentCurrencyAmount;
    label: string;
    pending?: boolean;
}

interface PaymentMethodChangeEventInit extends PaymentRequestUpdateEventInit {
    methodDetails?: any;
    methodName?: string;
}

interface PaymentMethodData {
    data?: any;
    supportedMethods: string;
}

interface PaymentOptions {
    requestBillingAddress?: boolean;
    requestPayerEmail?: boolean;
    requestPayerName?: boolean;
    requestPayerPhone?: boolean;
    requestShipping?: boolean;
    shippingType?: PaymentShippingType;
}

interface PaymentRequestUpdateEventInit extends EventInit {
}

interface PaymentShippingOption {
    amount: PaymentCurrencyAmount;
    id: string;
    label: string;
    selected?: boolean;
}

interface PaymentValidationErrors {
    error?: string;
    payer?: PayerErrors;
    paymentMethod?: any;
    shippingAddress?: AddressErrors;
}

interface PerformanceEntryEventInit extends EventInit {
    duration?: DOMHighResTimeStamp;
    entryType?: string;
    epoch?: number;
    name?: string;
    origin?: string;
    startTime?: DOMHighResTimeStamp;
}

interface PerformanceEntryFilterOptions {
    entryType?: string;
    initiatorType?: string;
    name?: string;
}

interface PerformanceMarkOptions {
    detail?: any;
    startTime?: DOMHighResTimeStamp;
}

interface PerformanceMeasureOptions {
    detail?: any;
    duration?: DOMHighResTimeStamp;
    end?: string | DOMHighResTimeStamp;
    start?: string | DOMHighResTimeStamp;
}

interface PerformanceObserverInit {
    buffered?: boolean;
    durationThreshold?: DOMHighResTimeStamp;
    entryTypes?: string[];
    type?: string;
}

interface PeriodicWaveConstraints {
    disableNormalization?: boolean;
}

interface PeriodicWaveOptions extends PeriodicWaveConstraints {
    imag?: number[] | Float32Array;
    real?: number[] | Float32Array;
}

interface PermissionSetParameters {
    descriptor: any;
    state: PermissionState;
}

interface PlacesBookmarkAdditionInit {
    dateAdded: number;
    frecency: number;
    guid: string;
    hidden: boolean;
    id: number;
    index: number;
    isTagging: boolean;
    itemType: number;
    lastVisitDate: number | null;
    parentGuid: string;
    parentId: number;
    source: number;
    tags: string | null;
    targetFolderGuid: string | null;
    targetFolderItemId: number;
    targetFolderTitle: string | null;
    title: string;
    url: string;
    visitCount: number;
}

interface PlacesBookmarkGuidInit {
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    lastModified: number;
    parentGuid: string;
    source: number;
    url?: string | null;
}

interface PlacesBookmarkKeywordInit {
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    keyword: string;
    lastModified: number;
    parentGuid: string;
    source: number;
    url?: string | null;
}

interface PlacesBookmarkMovedInit {
    dateAdded: number;
    frecency: number;
    guid: string;
    hidden: boolean;
    id: number;
    index: number;
    isTagging: boolean;
    itemType: number;
    lastVisitDate: number | null;
    oldIndex: number;
    oldParentGuid: string;
    parentGuid: string;
    source: number;
    tags: string | null;
    title: string;
    url?: string | null;
    visitCount: number;
}

interface PlacesBookmarkRemovedInit {
    guid: string;
    id: number;
    index: number;
    isDescendantRemoval?: boolean;
    isTagging: boolean;
    itemType: number;
    parentGuid: string;
    parentId: number;
    source: number;
    title: string;
    url: string;
}

interface PlacesBookmarkTagsInit {
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    lastModified: number;
    parentGuid: string;
    source: number;
    tags: string[];
    url?: string | null;
}

interface PlacesBookmarkTimeInit {
    dateAdded: number;
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    lastModified: number;
    parentGuid: string;
    source: number;
    url?: string | null;
}

interface PlacesBookmarkTitleInit {
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    lastModified: number;
    parentGuid: string;
    source: number;
    title: string;
    url?: string | null;
}

interface PlacesBookmarkUrlInit {
    guid: string;
    id: number;
    isTagging: boolean;
    itemType: number;
    lastModified: number;
    parentGuid: string;
    source: number;
    url: string;
}

interface PlacesFaviconInit {
    faviconUrl: string;
    pageGuid: string;
    url: string;
}

interface PlacesVisitRemovedInit {
    isPartialVisistsRemoval?: boolean;
    isRemovedFromStore?: boolean;
    pageGuid: string;
    reason: number;
    transitionType?: number;
    url: string;
}

interface PlacesVisitTitleInit {
    pageGuid: string;
    title: string;
    url: string;
}

interface PlaneLayout {
    offset: number;
    stride: number;
}

interface PluginCrashedEventInit extends EventInit {
    gmpPlugin?: boolean;
    pluginDumpID?: string;
    pluginFilename?: string | null;
    pluginID?: number;
    pluginName?: string;
    submittedCrashReport?: boolean;
}

interface PointerEventInit extends MouseEventInit {
    coalescedEvents?: PointerEvent[];
    height?: number;
    isPrimary?: boolean;
    pointerId?: number;
    pointerType?: string;
    predictedEvents?: PointerEvent[];
    pressure?: number;
    tangentialPressure?: number;
    tiltX?: number;
    tiltY?: number;
    twist?: number;
    width?: number;
}

interface PopStateEventInit extends EventInit {
    state?: any;
}

interface PopupBlockedEventInit extends EventInit {
    popupWindowFeatures?: string;
    popupWindowName?: string;
    popupWindowURI?: URI | null;
    requestingWindow?: Window | null;
}

interface PopupPositionedEventInit extends EventInit {
    alignmentOffset?: number;
    alignmentPosition?: string;
    isAnchored?: boolean;
    popupAlignment?: string;
}

interface PositionOptions {
    enableHighAccuracy?: boolean;
    maximumAge?: number;
    timeout?: number;
}

interface PositionStateEventInit extends EventInit {
    duration: number;
    playbackRate: number;
    position: number;
}

interface PrivateAttributionConversionOptions {
    ads?: string[];
    histogramSize: number;
    impression?: PrivateAttributionImpressionType;
    lookbackDays?: number;
    sources?: string[];
    task: string;
}

interface PrivateAttributionImpressionOptions {
    ad: string;
    index: number;
    target: string;
    type?: PrivateAttributionImpressionType;
}

interface ProcessActorChildOptions extends ProcessActorSidedOptions {
    observers?: string[];
}

interface ProcessActorOptions {
    child?: ProcessActorChildOptions;
    includeParent?: boolean;
    loadInDevToolsLoader?: boolean;
    parent?: ProcessActorSidedOptions;
    remoteTypes?: string[];
}

interface ProcessActorSidedOptions {
    esModuleURI?: string;
    moduleURI?: string;
}

interface ProfilerMarkerOptions {
    captureStack?: boolean;
    category?: string;
    innerWindowId?: number;
    startTime?: DOMHighResTimeStamp;
}

interface ProgressEventInit extends EventInit {
    lengthComputable?: boolean;
    loaded?: number;
    total?: number;
}

interface PromiseDebuggingStateHolder {
    reason?: any;
    state?: PromiseDebuggingState;
    value?: any;
}

interface PromiseRejectionEventInit extends EventInit {
    promise: Promise<any>;
    reason?: any;
}

interface PropertyDefinition {
    inherits: boolean;
    initialValue?: string;
    name: string;
    syntax?: string;
}

interface PropertyNamesOptions {
    includeAliases?: boolean;
    includeExperimentals?: boolean;
    includeShorthands?: boolean;
}

interface PropertyPref {
    name: string;
    pref: string;
}

interface PublicKeyCredentialCreationOptions {
    attestation?: string;
    authenticatorSelection?: AuthenticatorSelectionCriteria;
    challenge: BufferSource;
    excludeCredentials?: PublicKeyCredentialDescriptor[];
    extensions?: AuthenticationExtensionsClientInputs;
    pubKeyCredParams: PublicKeyCredentialParameters[];
    rp: PublicKeyCredentialRpEntity;
    timeout?: number;
    user: PublicKeyCredentialUserEntity;
}

interface PublicKeyCredentialCreationOptionsJSON {
    attestation?: string;
    attestationFormats?: string[];
    authenticatorSelection?: AuthenticatorSelectionCriteria;
    challenge: Base64URLString;
    excludeCredentials?: PublicKeyCredentialDescriptorJSON[];
    extensions?: AuthenticationExtensionsClientInputsJSON;
    hints?: string[];
    pubKeyCredParams: PublicKeyCredentialParameters[];
    rp: PublicKeyCredentialRpEntity;
    timeout?: number;
    user: PublicKeyCredentialUserEntityJSON;
}

interface PublicKeyCredentialDescriptor {
    id: BufferSource;
    transports?: string[];
    type: string;
}

interface PublicKeyCredentialDescriptorJSON {
    id: Base64URLString;
    transports?: string[];
    type: string;
}

interface PublicKeyCredentialEntity {
    name: string;
}

interface PublicKeyCredentialParameters {
    alg: COSEAlgorithmIdentifier;
    type: string;
}

interface PublicKeyCredentialRequestOptions {
    allowCredentials?: PublicKeyCredentialDescriptor[];
    challenge: BufferSource;
    extensions?: AuthenticationExtensionsClientInputs;
    rpId?: string;
    timeout?: number;
    userVerification?: string;
}

interface PublicKeyCredentialRequestOptionsJSON {
    allowCredentials?: PublicKeyCredentialDescriptorJSON[];
    attestation?: string;
    attestationFormats?: string[];
    challenge: Base64URLString;
    extensions?: AuthenticationExtensionsClientInputsJSON;
    hints?: string[];
    rpId?: string;
    timeout?: number;
    userVerification?: string;
}

interface PublicKeyCredentialRpEntity extends PublicKeyCredentialEntity {
    id?: string;
}

interface PublicKeyCredentialUserEntity extends PublicKeyCredentialEntity {
    displayName: string;
    id: BufferSource;
}

interface PublicKeyCredentialUserEntityJSON {
    displayName: string;
    id: Base64URLString;
    name: string;
}

interface PushSubscriptionInit {
    appServerKey?: BufferSource | null;
    authSecret?: ArrayBuffer | null;
    endpoint: string;
    expirationTime?: EpochTimeStamp | null;
    p256dhKey?: ArrayBuffer | null;
    scope: string;
}

interface PushSubscriptionJSON {
    endpoint?: string;
    expirationTime?: EpochTimeStamp | null;
    keys?: PushSubscriptionKeys;
}

interface PushSubscriptionKeys {
    auth?: string;
    p256dh?: string;
}

interface PushSubscriptionOptionsInit {
    applicationServerKey?: BufferSource | string | null;
}

interface QueuingStrategy {
    highWaterMark?: number;
    size?: QueuingStrategySize;
}

interface QueuingStrategyInit {
    highWaterMark: number;
}

interface RTCBandwidthEstimationInternal {
    maxPaddingBps?: number;
    pacerDelayMs?: number;
    receiveBandwidthBps?: number;
    rttMs?: number;
    sendBandwidthBps?: number;
    trackIdentifier: string;
}

interface RTCCodecStats extends RTCStats {
    channels?: number;
    clockRate?: number;
    codecType?: RTCCodecType;
    mimeType: string;
    payloadType: number;
    sdpFmtpLine?: string;
    transportId: string;
}

interface RTCConfiguration {
    bundlePolicy?: RTCBundlePolicy;
    certificates?: RTCCertificate[];
    iceServers?: RTCIceServer[];
    iceTransportPolicy?: RTCIceTransportPolicy;
    peerIdentity?: string | null;
    sdpSemantics?: string;
}

interface RTCConfigurationInternal {
    bundlePolicy?: RTCBundlePolicy;
    certificatesProvided: boolean;
    iceServers?: RTCIceServerInternal[];
    iceTransportPolicy?: RTCIceTransportPolicy;
    peerIdentityProvided: boolean;
    sdpSemantics?: string;
}

interface RTCDTMFToneChangeEventInit extends EventInit {
    tone?: string;
}

interface RTCDataChannelEventInit extends EventInit {
    channel: RTCDataChannel;
}

interface RTCDataChannelInit {
    id?: number;
    maxPacketLifeTime?: number;
    maxRetransmitTime?: number;
    maxRetransmits?: number;
    negotiated?: boolean;
    ordered?: boolean;
    protocol?: string;
}

interface RTCDataChannelStats extends RTCStats {
    bytesReceived?: number;
    bytesSent?: number;
    dataChannelIdentifier?: number;
    label?: string;
    messagesReceived?: number;
    messagesSent?: number;
    protocol?: string;
    state?: RTCDataChannelState;
}

interface RTCEncodedAudioFrameMetadata {
    contributingSources?: number[];
    payloadType?: number;
    sequenceNumber?: number;
    synchronizationSource?: number;
}

interface RTCEncodedVideoFrameMetadata {
    contributingSources?: number[];
    dependencies?: number[];
    frameId?: number;
    height?: number;
    payloadType?: number;
    spatialIndex?: number;
    synchronizationSource?: number;
    temporalIndex?: number;
    timestamp?: number;
    width?: number;
}

interface RTCIceCandidateInit {
    candidate?: string;
    sdpMLineIndex?: number | null;
    sdpMid?: string | null;
    usernameFragment?: string | null;
}

interface RTCIceCandidatePairStats extends RTCStats {
    bytesReceived?: number;
    bytesSent?: number;
    componentId?: number;
    lastPacketReceivedTimestamp?: DOMHighResTimeStamp;
    lastPacketSentTimestamp?: DOMHighResTimeStamp;
    localCandidateId?: string;
    nominated?: boolean;
    priority?: number;
    readable?: boolean;
    remoteCandidateId?: string;
    selected?: boolean;
    state?: RTCStatsIceCandidatePairState;
    transportId?: string;
    writable?: boolean;
}

interface RTCIceCandidateStats extends RTCStats {
    address?: string;
    candidateType?: RTCIceCandidateType;
    port?: number;
    priority?: number;
    protocol?: string;
    proxied?: string;
    relayProtocol?: string;
    transportId?: string;
}

interface RTCIceServer {
    credential?: string;
    credentialType?: RTCIceCredentialType;
    url?: string;
    urls?: string | string[];
    username?: string;
}

interface RTCIceServerInternal {
    credentialProvided: boolean;
    urls?: string[];
    userNameProvided: boolean;
}

interface RTCIdentityAssertion {
    idp?: string;
    name?: string;
}

interface RTCIdentityAssertionResult {
    assertion: string;
    idp: RTCIdentityProviderDetails;
}

interface RTCIdentityProvider {
    generateAssertion: GenerateAssertionCallback;
    validateAssertion: ValidateAssertionCallback;
}

interface RTCIdentityProviderDetails {
    domain: string;
    protocol?: string;
}

interface RTCIdentityProviderOptions {
    peerIdentity?: string;
    protocol?: string;
    usernameHint?: string;
}

interface RTCIdentityValidationResult {
    contents: string;
    identity: string;
}

interface RTCInboundRtpStreamStats extends RTCReceivedRtpStreamStats {
    audioLevel?: number;
    bytesReceived?: number;
    concealedSamples?: number;
    concealmentEvents?: number;
    fecPacketsDiscarded?: number;
    fecPacketsReceived?: number;
    firCount?: number;
    frameHeight?: number;
    frameWidth?: number;
    framesDecoded?: number;
    framesDropped?: number;
    framesPerSecond?: number;
    framesReceived?: number;
    headerBytesReceived?: number;
    insertedSamplesForDeceleration?: number;
    jitterBufferDelay?: number;
    jitterBufferEmittedCount?: number;
    lastPacketReceivedTimestamp?: DOMHighResTimeStamp;
    nackCount?: number;
    pliCount?: number;
    qpSum?: number;
    remoteId?: string;
    removedSamplesForAcceleration?: number;
    silentConcealedSamples?: number;
    totalAudioEnergy?: number;
    totalDecodeTime?: number;
    totalInterFrameDelay?: number;
    totalProcessingDelay?: number;
    totalSamplesDuration?: number;
    totalSamplesReceived?: number;
    totalSquaredInterFrameDelay?: number;
    trackIdentifier: string;
}

interface RTCLocalSessionDescriptionInit {
    sdp?: string;
    type?: RTCSdpType;
}

interface RTCMediaSourceStats extends RTCStats {
    kind: string;
    trackIdentifier: string;
}

interface RTCOfferAnswerOptions {
}

interface RTCOfferOptions extends RTCOfferAnswerOptions {
    iceRestart?: boolean;
    offerToReceiveAudio?: boolean;
    offerToReceiveVideo?: boolean;
}

interface RTCOutboundRtpStreamStats extends RTCSentRtpStreamStats {
    firCount?: number;
    frameHeight?: number;
    frameWidth?: number;
    framesEncoded?: number;
    framesPerSecond?: number;
    framesSent?: number;
    headerBytesSent?: number;
    hugeFramesSent?: number;
    nackCount?: number;
    pliCount?: number;
    qpSum?: number;
    remoteId?: string;
    retransmittedBytesSent?: number;
    retransmittedPacketsSent?: number;
    totalEncodeTime?: number;
    totalEncodedBytesTarget?: number;
}

interface RTCPeerConnectionIceEventInit extends EventInit {
    candidate?: RTCIceCandidate | null;
}

interface RTCPeerConnectionStats extends RTCStats {
    dataChannelsClosed?: number;
    dataChannelsOpened?: number;
}

interface RTCRTPContributingSourceStats extends RTCStats {
    contributorSsrc?: number;
    inboundRtpStreamId?: string;
}

interface RTCReceivedRtpStreamStats extends RTCRtpStreamStats {
    discardedPackets?: number;
    jitter?: number;
    packetsDiscarded?: number;
    packetsLost?: number;
    packetsReceived?: number;
}

interface RTCRemoteInboundRtpStreamStats extends RTCReceivedRtpStreamStats {
    fractionLost?: number;
    localId?: string;
    roundTripTime?: number;
    roundTripTimeMeasurements?: number;
    totalRoundTripTime?: number;
}

interface RTCRemoteOutboundRtpStreamStats extends RTCSentRtpStreamStats {
    localId?: string;
    remoteTimestamp?: DOMHighResTimeStamp;
}

interface RTCRtcpParameters {
    cname?: string;
    reducedSize?: boolean;
}

interface RTCRtpCapabilities {
    codecs: RTCRtpCodec[];
    headerExtensions: RTCRtpHeaderExtensionCapability[];
}

interface RTCRtpCodec {
    channels?: number;
    clockRate: number;
    mimeType: string;
    sdpFmtpLine?: string;
}

interface RTCRtpCodecParameters extends RTCRtpCodec {
    payloadType: number;
}

interface RTCRtpContributingSource {
    audioLevel?: number;
    rtpTimestamp: number;
    source: number;
    timestamp: DOMHighResTimeStamp;
}

interface RTCRtpEncodingParameters {
    active?: boolean;
    maxBitrate?: number;
    maxFramerate?: number;
    priority?: RTCPriorityType;
    rid?: string;
    scaleResolutionDownBy?: number;
}

interface RTCRtpHeaderExtensionCapability {
    uri: string;
}

interface RTCRtpHeaderExtensionParameters {
    encrypted?: boolean;
    id?: number;
    uri?: string;
}

interface RTCRtpParameters {
    codecs?: RTCRtpCodecParameters[];
    headerExtensions?: RTCRtpHeaderExtensionParameters[];
    rtcp?: RTCRtcpParameters;
}

interface RTCRtpReceiveParameters extends RTCRtpParameters {
}

interface RTCRtpSendParameters extends RTCRtpParameters {
    encodings: RTCRtpEncodingParameters[];
    transactionId?: string;
}

interface RTCRtpStreamStats extends RTCStats {
    codecId?: string;
    kind: string;
    mediaType?: string;
    ssrc: number;
    transportId?: string;
}

interface RTCRtpSynchronizationSource extends RTCRtpContributingSource {
    voiceActivityFlag?: boolean | null;
}

interface RTCRtpTransceiverInit {
    direction?: RTCRtpTransceiverDirection;
    sendEncodings?: RTCRtpEncodingParameters[];
    streams?: MediaStream[];
}

interface RTCSdpHistoryEntryInternal {
    errors?: RTCSdpParsingErrorInternal[];
    isLocal: boolean;
    sdp: string;
    timestamp: DOMHighResTimeStamp;
}

interface RTCSdpHistoryInternal {
    pcid: string;
    sdpHistory?: RTCSdpHistoryEntryInternal[];
}

interface RTCSdpParsingErrorInternal {
    error: string;
    lineNumber: number;
}

interface RTCSentRtpStreamStats extends RTCRtpStreamStats {
    bytesSent?: number;
    packetsSent?: number;
}

interface RTCSessionDescriptionInit {
    sdp?: string;
    type: RTCSdpType;
}

interface RTCStats {
    id?: string;
    timestamp?: DOMHighResTimeStamp;
    type?: RTCStatsType;
}

interface RTCStatsCollection {
    bandwidthEstimations?: RTCBandwidthEstimationInternal[];
    codecStats?: RTCCodecStats[];
    dataChannelStats?: RTCDataChannelStats[];
    iceCandidatePairStats?: RTCIceCandidatePairStats[];
    iceCandidateStats?: RTCIceCandidateStats[];
    inboundRtpStreamStats?: RTCInboundRtpStreamStats[];
    mediaSourceStats?: RTCMediaSourceStats[];
    outboundRtpStreamStats?: RTCOutboundRtpStreamStats[];
    peerConnectionStats?: RTCPeerConnectionStats[];
    rawLocalCandidates?: string[];
    rawRemoteCandidates?: string[];
    remoteInboundRtpStreamStats?: RTCRemoteInboundRtpStreamStats[];
    remoteOutboundRtpStreamStats?: RTCRemoteOutboundRtpStreamStats[];
    rtpContributingSourceStats?: RTCRTPContributingSourceStats[];
    trickledIceCandidateStats?: RTCIceCandidateStats[];
    videoFrameHistories?: RTCVideoFrameHistoryInternal[];
    videoSourceStats?: RTCVideoSourceStats[];
}

interface RTCStatsReportInternal extends RTCStatsCollection {
    browserId: number;
    callDurationMs?: number;
    closed: boolean;
    configuration?: RTCConfigurationInternal;
    iceRestarts: number;
    iceRollbacks: number;
    jsepSessionErrors?: string;
    offerer?: boolean;
    pcid: string;
    sdpHistory?: RTCSdpHistoryEntryInternal[];
    timestamp: DOMHighResTimeStamp;
}

interface RTCTrackEventInit extends EventInit {
    receiver: RTCRtpReceiver;
    streams?: MediaStream[];
    track: MediaStreamTrack;
    transceiver: RTCRtpTransceiver;
}

interface RTCVideoFrameHistoryEntryInternal {
    consecutiveFrames: number;
    firstFrameTimestamp: DOMHighResTimeStamp;
    height: number;
    lastFrameTimestamp: DOMHighResTimeStamp;
    localSsrc: number;
    remoteSsrc: number;
    rotationAngle: number;
    width: number;
}

interface RTCVideoFrameHistoryInternal {
    entries?: RTCVideoFrameHistoryEntryInternal[];
    trackIdentifier: string;
}

interface RTCVideoSourceStats extends RTCMediaSourceStats {
    frames?: number;
    framesPerSecond?: number;
    height?: number;
    width?: number;
}

interface ReadOptions extends ReadUTF8Options {
    maxBytes?: number | null;
    offset?: number;
}

interface ReadUTF8Options {
    decompress?: boolean;
}

interface ReadableStreamGetReaderOptions {
    mode?: ReadableStreamReaderMode;
}

interface ReadableStreamIteratorOptions {
    preventCancel?: boolean;
}

interface ReadableStreamReadResult {
    done?: boolean;
    value?: any;
}

interface ReadableWritablePair {
    readable: ReadableStream;
    writable: WritableStream;
}

interface ReceiveMessageArgument {
    data?: any;
    json?: any;
    name: string;
    ports?: MessagePort[];
    sync: boolean;
    target: nsISupports;
    targetFrameLoader?: FrameLoader;
}

interface RegistrationOptions {
    scope?: string;
    updateViaCache?: ServiceWorkerUpdateViaCache;
}

interface RemotenessOptions {
    pendingSwitchID?: number;
    remoteType: string | null;
    switchingInProgressLoad?: boolean;
}

interface RemoveOptions {
    ignoreAbsent?: boolean;
    recursive?: boolean;
    retryReadonly?: boolean;
}

interface ReportingObserverOptions {
    buffered?: boolean;
    types?: string[];
}

interface RequestInit {
    body?: BodyInit | null;
    cache?: RequestCache;
    credentials?: RequestCredentials;
    headers?: HeadersInit;
    integrity?: string;
    keepalive?: boolean;
    method?: string;
    mode?: RequestMode;
    mozErrors?: boolean;
    observe?: ObserverCallback;
    priority?: RequestPriority;
    redirect?: RequestRedirect;
    referrer?: string;
    referrerPolicy?: ReferrerPolicy;
    signal?: AbortSignal | null;
}

interface ResizeObserverOptions {
    box?: ResizeObserverBoxOptions;
}

interface ResourceId {
    optional?: boolean;
    path: string;
}

interface ResponseInit {
    headers?: HeadersInit;
    status?: number;
    statusText?: string;
}

interface SVGBoundingBoxOptions {
    clipped?: boolean;
    fill?: boolean;
    markers?: boolean;
    stroke?: boolean;
}

interface SanitizerAttributeNamespace {
    name: string;
    namespace?: string | null;
}

interface SanitizerConfig {
    attributes?: SanitizerAttribute[];
    comments?: boolean;
    customElements?: boolean;
    elements?: SanitizerElementWithAttributes[];
    removeAttributes?: SanitizerAttribute[];
    removeElements?: SanitizerElement[];
    replaceWithChildrenElements?: SanitizerElement[];
    unknownMarkup?: boolean;
}

interface SanitizerElementNamespace {
    name: string;
    namespace?: string | null;
}

interface SanitizerElementNamespaceWithAttributes extends SanitizerElementNamespace {
    attributes?: SanitizerAttribute[];
    removeAttributes?: SanitizerAttribute[];
}

interface SchedulerPostTaskOptions {
    delay?: number;
    priority?: TaskPriority;
    signal?: AbortSignal;
}

interface ScrollIntoViewOptions extends ScrollOptions {
    block?: ScrollLogicalPosition;
    inline?: ScrollLogicalPosition;
}

interface ScrollOptions {
    behavior?: ScrollBehavior;
}

interface ScrollToOptions extends ScrollOptions {
    left?: number;
    top?: number;
}

interface ScrollViewChangeEventInit extends EventInit {
    state?: ScrollState;
}

interface SecurityPolicyViolationEventInit extends EventInit {
    blockedURI?: string;
    columnNumber?: number;
    disposition?: SecurityPolicyViolationEventDisposition;
    documentURI?: string;
    effectiveDirective?: string;
    lineNumber?: number;
    originalPolicy?: string;
    referrer?: string;
    sample?: string;
    sourceFile?: string;
    statusCode?: number;
    violatedDirective?: string;
}

interface SelectorWarning {
    index: number;
    kind: SelectorWarningKind;
}

interface ServerSocketOptions {
    binaryType?: TCPSocketBinaryType;
}

interface SetHTMLOptions {
    sanitizer?: SanitizerConfig;
}

interface ShadowRootInit {
    clonable?: boolean;
    delegatesFocus?: boolean;
    mode: ShadowRootMode;
    serializable?: boolean;
    slotAssignment?: SlotAssignmentMode;
}

interface ShareData {
    files?: File[];
    text?: string;
    title?: string;
    url?: string;
}

interface SizeToContentConstraints {
    maxHeight?: number;
    maxWidth?: number;
    prefWidth?: number;
}

interface SocketOptions {
    binaryType?: TCPSocketBinaryType;
    useSecureTransport?: boolean;
}

interface SpeechRecognitionErrorInit extends EventInit {
    error?: SpeechRecognitionErrorCode;
    message?: string;
}

interface SpeechRecognitionEventInit extends EventInit {
    emma?: Document | null;
    interpretation?: any;
    resultIndex?: number;
    results?: SpeechRecognitionResultList | null;
}

interface SpeechSynthesisErrorEventInit extends SpeechSynthesisEventInit {
    error: SpeechSynthesisErrorCode;
}

interface SpeechSynthesisEventInit extends EventInit {
    charIndex?: number;
    charLength?: number | null;
    elapsedTime?: number;
    name?: string;
    utterance: SpeechSynthesisUtterance;
}

interface SplitRelativeOptions {
    allowCurrentDir?: boolean;
    allowEmpty?: boolean;
    allowParentDir?: boolean;
}

interface StaticRangeInit {
    endContainer: Node;
    endOffset: number;
    startContainer: Node;
    startOffset: number;
}

interface StereoPannerOptions extends AudioNodeOptions {
    pan?: number;
}

interface StorageEstimate {
    quota?: number;
    usage?: number;
}

interface StorageEventInit extends EventInit {
    key?: string | null;
    newValue?: string | null;
    oldValue?: string | null;
    storageArea?: Storage | null;
    url?: string;
}

interface StreamFilterDataEventInit extends EventInit {
    data: ArrayBuffer;
}

interface StreamPipeOptions {
    preventAbort?: boolean;
    preventCancel?: boolean;
    preventClose?: boolean;
    signal?: AbortSignal;
}

interface StructuredSerializeOptions {
    transfer?: any[];
}

interface StyleSheetApplicableStateChangeEventInit extends EventInit {
    applicable?: boolean;
    stylesheet?: CSSStyleSheet | null;
}

interface StyleSheetRemovedEventInit extends EventInit {
    stylesheet?: CSSStyleSheet | null;
}

interface SubmitEventInit extends EventInit {
    submitter?: HTMLElement | null;
}

interface SupportsOptions {
    chrome?: boolean;
    quirks?: boolean;
    userAgent?: boolean;
}

interface SvcOutputMetadata {
    temporalLayerId?: number;
}

interface TCPServerSocketEventInit extends EventInit {
    socket?: TCPSocket | null;
}

interface TCPSocketErrorEventInit extends EventInit {
    errorCode?: number;
    message?: string;
    name?: string;
}

interface TCPSocketEventInit extends EventInit {
    data?: any;
}

interface TaskControllerInit {
    priority?: TaskPriority;
}

interface TaskPriorityChangeEventInit extends EventInit {
    previousPriority: TaskPriority;
}

interface TelemetryStopwatchOptions {
    inSeconds?: boolean;
}

interface TestInterfaceAsyncIterableSingleOptions {
    failToInit?: boolean;
}

interface TestInterfaceAsyncIteratorOptions {
    blockingPromises?: Promise<any>[];
    failNextAfter?: number;
    multiplier?: number;
    throwFromNext?: boolean;
    throwFromReturn?: TestThrowingCallback;
}

interface TestInterfaceJSDictionary {
    anyMember?: any;
    anySequenceMember?: any[];
    innerDictionary?: TestInterfaceJSDictionary2;
    objectMember?: any;
    objectOrStringMember?: any;
    objectRecordMember?: Record<string, any>;
}

interface TestInterfaceJSDictionary2 {
    innerObject?: any;
}

interface TestInterfaceJSUnionableDictionary {
    anyMember?: any;
    objectMember?: any;
}

interface TextDecodeOptions {
    stream?: boolean;
}

interface TextDecoderOptions {
    fatal?: boolean;
    ignoreBOM?: boolean;
}

interface TextEncoderEncodeIntoResult {
    read?: number;
    written?: number;
}

interface ThreadInfoDictionary {
    cpuCycleCount?: number;
    cpuTime?: number;
    name?: string;
    tid?: number;
}

interface ToggleEventInit extends EventInit {
    newState?: string;
    oldState?: string;
}

interface TouchEventInit extends EventModifierInit {
    changedTouches?: Touch[];
    targetTouches?: Touch[];
    touches?: Touch[];
}

interface TouchInit {
    clientX?: number;
    clientY?: number;
    force?: number;
    identifier: number;
    pageX?: number;
    pageY?: number;
    radiusX?: number;
    radiusY?: number;
    rotationAngle?: number;
    screenX?: number;
    screenY?: number;
    target: EventTarget;
}

interface TrackBuffersManagerDebugInfo {
    bufferSize?: number;
    evictable?: number;
    nextGetSampleIndex?: number;
    nextInsertionIndex?: number;
    nextSampleTime?: number;
    numSamples?: number;
    ranges?: BufferRange[];
    type?: string;
}

interface TrackEventInit extends EventInit {
    track?: VideoTrack | AudioTrack | TextTrack | null;
}

interface TransitionEventInit extends EventInit {
    elapsedTime?: number;
    propertyName?: string;
    pseudoElement?: string;
}

interface TreeCellInfo {
    childElt?: string;
    col?: TreeColumn | null;
    row?: number;
}

interface TrustedTypePolicyOptions {
    createHTML?: CreateHTMLCallback;
    createScript?: CreateScriptCallback;
    createScriptURL?: CreateScriptURLCallback;
}

interface UDPMessageEventInit extends EventInit {
    data?: any;
    remoteAddress?: string;
    remotePort?: number;
}

interface UDPOptions {
    addressReuse?: boolean;
    localAddress?: string;
    localPort?: number;
    loopback?: boolean;
    remoteAddress?: string;
    remotePort?: number;
}

interface UIEventInit extends EventInit {
    detail?: number;
    view?: Window | null;
}

interface UniFFIScaffoldingCallResult {
    code: UniFFIScaffoldingCallCode;
    data?: UniFFIScaffoldingValue;
}

interface UserProximityEventInit extends EventInit {
    near?: boolean;
}

interface UtilityActorsDictionary {
    actorName?: WebIDLUtilityActorName;
}

interface VRDisplayEventInit extends EventInit {
    display: VRDisplay;
    reason?: VRDisplayEventReason;
}

interface VRLayer {
    leftBounds?: number[] | Float32Array;
    rightBounds?: number[] | Float32Array;
    source?: HTMLCanvasElement | null;
}

interface ValidityStateFlags {
    badInput?: boolean;
    customError?: boolean;
    patternMismatch?: boolean;
    rangeOverflow?: boolean;
    rangeUnderflow?: boolean;
    stepMismatch?: boolean;
    tooLong?: boolean;
    tooShort?: boolean;
    typeMismatch?: boolean;
    valueMissing?: boolean;
}

interface VideoColorSpaceInit {
    fullRange?: boolean | null;
    matrix?: VideoMatrixCoefficients | null;
    primaries?: VideoColorPrimaries | null;
    transfer?: VideoTransferCharacteristics | null;
}

interface VideoConfiguration {
    bitrate: number;
    colorGamut?: ColorGamut;
    contentType: string;
    framerate: number;
    hasAlphaChannel?: boolean;
    hdrMetadataType?: HdrMetadataType;
    height: number;
    scalabilityMode?: string;
    transferFunction?: TransferFunction;
    width: number;
}

interface VideoDecoderConfig {
    codec: string;
    codedHeight?: number;
    codedWidth?: number;
    colorSpace?: VideoColorSpaceInit;
    description?: ArrayBufferView | ArrayBuffer;
    displayAspectHeight?: number;
    displayAspectWidth?: number;
    hardwareAcceleration?: HardwareAcceleration;
    optimizeForLatency?: boolean;
}

interface VideoDecoderInit {
    error: WebCodecsErrorCallback;
    output: VideoFrameOutputCallback;
}

interface VideoDecoderSupport {
    config?: VideoDecoderConfig;
    supported?: boolean;
}

interface VideoEncoderConfig {
    alpha?: AlphaOption;
    avc?: AvcEncoderConfig;
    bitrate?: number;
    bitrateMode?: VideoEncoderBitrateMode;
    codec: string;
    contentHint?: string;
    displayHeight?: number;
    displayWidth?: number;
    framerate?: number;
    hardwareAcceleration?: HardwareAcceleration;
    height: number;
    latencyMode?: LatencyMode;
    scalabilityMode?: string;
    width: number;
}

interface VideoEncoderEncodeOptions {
    avc?: VideoEncoderEncodeOptionsForAvc;
    keyFrame?: boolean;
}

interface VideoEncoderEncodeOptionsForAvc {
    quantizer?: number | null;
}

interface VideoEncoderInit {
    error: WebCodecsErrorCallback;
    output: EncodedVideoChunkOutputCallback;
}

interface VideoEncoderSupport {
    config?: VideoEncoderConfig;
    supported?: boolean;
}

interface VideoFrameBufferInit {
    codedHeight: number;
    codedWidth: number;
    colorSpace?: VideoColorSpaceInit;
    displayHeight?: number;
    displayWidth?: number;
    duration?: number;
    format: VideoPixelFormat;
    layout?: PlaneLayout[];
    timestamp: number;
    visibleRect?: DOMRectInit;
}

interface VideoFrameCallbackMetadata {
    expectedDisplayTime: DOMHighResTimeStamp;
    height: number;
    mediaTime: number;
    presentationTime: DOMHighResTimeStamp;
    presentedFrames: number;
    width: number;
}

interface VideoFrameCopyToOptions {
    colorSpace?: PredefinedColorSpace;
    format?: VideoPixelFormat;
    layout?: PlaneLayout[];
    rect?: DOMRectInit;
}

interface VideoFrameInit {
    alpha?: AlphaOption;
    displayHeight?: number;
    displayWidth?: number;
    duration?: number;
    timestamp?: number;
    visibleRect?: DOMRectInit;
}

interface VideoSinkDebugInfo {
    endPromiseHolderIsEmpty?: boolean;
    finished?: boolean;
    hasVideo?: boolean;
    isPlaying?: boolean;
    isStarted?: boolean;
    size?: number;
    videoFrameEndTime?: number;
    videoSinkEndRequestExists?: boolean;
}

interface WaveShaperOptions extends AudioNodeOptions {
    curve?: number[] | Float32Array;
    oversample?: OverSampleType;
}

interface WebAccessibleResourceInit {
    extension_ids?: string[] | null;
    matches?: MatchPatternSetOrStringSequence | null;
    resources: MatchGlobOrString[];
}

interface WebExtensionContentScriptInit extends MozDocumentMatcherInit {
    cssPaths?: string[];
    jsPaths?: string[];
    runAt?: ContentScriptRunAt;
    world?: ContentScriptExecutionWorld;
}

interface WebExtensionInit {
    allowedOrigins: MatchPatternSetOrStringSequence;
    backgroundScripts?: string[] | null;
    backgroundTypeModule?: boolean;
    backgroundWorkerScript?: string | null;
    baseURL: string;
    contentScripts?: WebExtensionContentScriptInit[];
    extensionPageCSP?: string | null;
    id: string;
    ignoreQuarantine?: boolean;
    isPrivileged?: boolean;
    localizeCallback: WebExtensionLocalizeCallback;
    manifestVersion?: number;
    mozExtensionHostname: string;
    name?: string;
    permissions?: string[];
    readyPromise?: Promise<WebExtensionPolicy | null>;
    temporarilyInstalled?: boolean;
    type?: string;
    webAccessibleResources?: WebAccessibleResourceInit[];
}

interface WebGLContextAttributes {
    alpha?: GLboolean;
    antialias?: GLboolean;
    depth?: GLboolean;
    failIfMajorPerformanceCaveat?: GLboolean;
    powerPreference?: WebGLPowerPreference;
    premultipliedAlpha?: GLboolean;
    preserveDrawingBuffer?: GLboolean;
    stencil?: GLboolean;
    xrCompatible?: boolean;
}

interface WebGLContextEventInit extends EventInit {
    statusMessage?: string;
}

interface WebTransportCloseInfo {
    closeCode?: number;
    reason?: string;
}

interface WebTransportDatagramStats {
    droppedIncoming?: number;
    expiredOutgoing?: number;
    lostOutgoing?: number;
    timestamp?: DOMHighResTimeStamp;
}

interface WebTransportErrorInit {
    message?: string;
    streamErrorCode?: number;
}

interface WebTransportHash {
    algorithm?: string;
    value?: BufferSource;
}

interface WebTransportOptions {
    allowPooling?: boolean;
    congestionControl?: WebTransportCongestionControl;
    requireUnreliable?: boolean;
    serverCertificateHashes?: WebTransportHash[];
}

interface WebTransportReceiveStreamStats {
    bytesRead?: number;
    bytesReceived?: number;
    timestamp?: DOMHighResTimeStamp;
}

interface WebTransportSendStreamOptions {
    sendOrder?: number | null;
}

interface WebTransportSendStreamStats {
    bytesAcknowledged?: number;
    bytesSent?: number;
    bytesWritten?: number;
    timestamp?: DOMHighResTimeStamp;
}

interface WebTransportStats {
    bytesReceived?: number;
    bytesSent?: number;
    datagrams?: WebTransportDatagramStats;
    minRtt?: DOMHighResTimeStamp;
    numIncomingStreamsCreated?: number;
    numOutgoingStreamsCreated?: number;
    packetsLost?: number;
    packetsReceived?: number;
    packetsSent?: number;
    rttVariation?: DOMHighResTimeStamp;
    smoothedRtt?: DOMHighResTimeStamp;
    timestamp?: DOMHighResTimeStamp;
}

interface WebrtcGlobalMediaContext {
    hasH264Hardware: boolean;
}

interface WebrtcGlobalStatisticsReport {
    reports?: RTCStatsReportInternal[];
    sdpHistories?: RTCSdpHistoryInternal[];
}

interface WheelEventInit extends MouseEventInit {
    deltaMode?: number;
    deltaX?: number;
    deltaY?: number;
    deltaZ?: number;
}

interface WindowActorChildOptions extends WindowActorSidedOptions {
    events?: Record<string, WindowActorEventListenerOptions>;
    observers?: string[];
}

interface WindowActorEventListenerOptions extends AddEventListenerOptions {
    createActor?: boolean;
}

interface WindowActorOptions {
    allFrames?: boolean;
    child?: WindowActorChildOptions;
    includeChrome?: boolean;
    matches?: string[];
    messageManagerGroups?: string[];
    parent?: WindowActorSidedOptions;
    remoteTypes?: string[];
}

interface WindowActorSidedOptions {
    esModuleURI?: string;
    moduleURI?: string;
}

interface WindowInfoDictionary {
    documentTitle?: string;
    documentURI?: URI | null;
    isInProcess?: boolean;
    isProcessRoot?: boolean;
    outerWindowId?: number;
}

interface WindowPostMessageOptions extends StructuredSerializeOptions {
    targetOrigin?: string;
}

interface WindowsFileAttributes {
    hidden?: boolean;
    readOnly?: boolean;
    system?: boolean;
}

interface Wireframe {
    canvasBackground?: number;
    rects?: WireframeTaggedRect[];
    version?: number;
}

interface WireframeTaggedRect {
    color?: number;
    height?: number;
    node?: Node | null;
    type?: WireframeRectType;
    width?: number;
    x?: number;
    y?: number;
}

interface WorkerOptions {
    credentials?: RequestCredentials;
    name?: string;
    type?: WorkerType;
}

interface WorkletOptions {
    credentials?: RequestCredentials;
}

interface WriteOptions {
    backupFile?: string;
    compress?: boolean;
    flush?: boolean;
    mode?: WriteMode;
    tmpPath?: string;
}

interface WriteParams {
    data?: BufferSource | Blob | string | null;
    position?: number | null;
    size?: number | null;
    type: WriteCommandType;
}

interface XRInputSourceEventInit extends EventInit {
    frame: XRFrame;
    inputSource: XRInputSource;
}

interface XRInputSourcesChangeEventInit extends EventInit {
    added: XRInputSource[];
    removed: XRInputSource[];
    session: XRSession;
}

interface XRReferenceSpaceEventInit extends EventInit {
    referenceSpace: XRReferenceSpace;
    transform?: XRRigidTransform | null;
}

interface XRRenderStateInit {
    baseLayer?: XRWebGLLayer | null;
    depthFar?: number;
    depthNear?: number;
    inlineVerticalFieldOfView?: number;
}

interface XRSessionEventInit extends EventInit {
    session: XRSession;
}

interface XRSessionInit {
    optionalFeatures?: string[];
    requiredFeatures?: string[];
}

interface XRWebGLLayerInit {
    alpha?: boolean;
    antialias?: boolean;
    depth?: boolean;
    framebufferScaleFactor?: number;
    ignoreDepthValues?: boolean;
    stencil?: boolean;
}

interface addonInstallOptions {
    hash?: string | null;
    url: string;
}

interface sendAbuseReportOptions {
    authorization?: string | null;
}

type EventListener = ((event: Event) => void) | { handleEvent(event: Event): void; };

type MessageListener = ((argument: ReceiveMessageArgument) => any) | { receiveMessage(argument: ReceiveMessageArgument): any; };

type MozDocumentCallback = ((matcher: MozDocumentMatcher, window: WindowProxy) => void) | { onNewDocument(matcher: MozDocumentMatcher, window: WindowProxy): void; };

type NodeFilter = ((node: Node) => number) | { acceptNode(node: Node): number; };

declare var NodeFilter: {
    readonly FILTER_ACCEPT: 1;
    readonly FILTER_REJECT: 2;
    readonly FILTER_SKIP: 3;
    readonly SHOW_ALL: 0xFFFFFFFF;
    readonly SHOW_ELEMENT: 0x1;
    readonly SHOW_ATTRIBUTE: 0x2;
    readonly SHOW_TEXT: 0x4;
    readonly SHOW_CDATA_SECTION: 0x8;
    readonly SHOW_ENTITY_REFERENCE: 0x10;
    readonly SHOW_ENTITY: 0x20;
    readonly SHOW_PROCESSING_INSTRUCTION: 0x40;
    readonly SHOW_COMMENT: 0x80;
    readonly SHOW_DOCUMENT: 0x100;
    readonly SHOW_DOCUMENT_TYPE: 0x200;
    readonly SHOW_DOCUMENT_FRAGMENT: 0x400;
    readonly SHOW_NOTATION: 0x800;
};

type ObserverCallback = ((observer: FetchObserver) => void) | { handleEvent(observer: FetchObserver): void; };

type UncaughtRejectionObserver = ((p: any) => boolean) | { onLeftUncaught(p: any): boolean; };

type XPathNSResolver = ((prefix: string | null) => string | null) | { lookupNamespaceURI(prefix: string | null): string | null; };

interface ANGLE_instanced_arrays {
    drawArraysInstancedANGLE(mode: GLenum, first: GLint, count: GLsizei, primcount: GLsizei): void;
    drawElementsInstancedANGLE(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr, primcount: GLsizei): void;
    vertexAttribDivisorANGLE(index: GLuint, divisor: GLuint): void;
    readonly VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE: 0x88FE;
}

interface ARIAMixin {
    ariaActiveDescendantElement: Element | null;
    ariaAtomic: string | null;
    ariaAutoComplete: string | null;
    ariaBrailleLabel: string | null;
    ariaBrailleRoleDescription: string | null;
    ariaBusy: string | null;
    ariaChecked: string | null;
    ariaColCount: string | null;
    ariaColIndex: string | null;
    ariaColIndexText: string | null;
    ariaColSpan: string | null;
    ariaCurrent: string | null;
    ariaDescription: string | null;
    ariaDisabled: string | null;
    ariaExpanded: string | null;
    ariaHasPopup: string | null;
    ariaHidden: string | null;
    ariaInvalid: string | null;
    ariaKeyShortcuts: string | null;
    ariaLabel: string | null;
    ariaLevel: string | null;
    ariaLive: string | null;
    ariaModal: string | null;
    ariaMultiLine: string | null;
    ariaMultiSelectable: string | null;
    ariaOrientation: string | null;
    ariaPlaceholder: string | null;
    ariaPosInSet: string | null;
    ariaPressed: string | null;
    ariaReadOnly: string | null;
    ariaRelevant: string | null;
    ariaRequired: string | null;
    ariaRoleDescription: string | null;
    ariaRowCount: string | null;
    ariaRowIndex: string | null;
    ariaRowIndexText: string | null;
    ariaRowSpan: string | null;
    ariaSelected: string | null;
    ariaSetSize: string | null;
    ariaSort: string | null;
    ariaValueMax: string | null;
    ariaValueMin: string | null;
    ariaValueNow: string | null;
    ariaValueText: string | null;
    role: string | null;
}

interface AbortController {
    readonly signal: AbortSignal;
    abort(reason?: any): void;
}

declare var AbortController: {
    prototype: AbortController;
    new(): AbortController;
    isInstance: IsInstance<AbortController>;
};

interface AbortSignalEventMap {
    "abort": Event;
}

interface AbortSignal extends EventTarget {
    readonly aborted: boolean;
    onabort: ((this: AbortSignal, ev: Event) => any) | null;
    readonly reason: any;
    throwIfAborted(): void;
    addEventListener<K extends keyof AbortSignalEventMap>(type: K, listener: (this: AbortSignal, ev: AbortSignalEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AbortSignalEventMap>(type: K, listener: (this: AbortSignal, ev: AbortSignalEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AbortSignal: {
    prototype: AbortSignal;
    new(): AbortSignal;
    isInstance: IsInstance<AbortSignal>;
    abort(reason?: any): AbortSignal;
    any(signals: AbortSignal[]): AbortSignal;
    timeout(milliseconds: number): AbortSignal;
};

interface AbstractRange {
    readonly collapsed: boolean;
    readonly endContainer: Node;
    readonly endOffset: number;
    readonly startContainer: Node;
    readonly startOffset: number;
}

declare var AbstractRange: {
    prototype: AbstractRange;
    new(): AbstractRange;
    isInstance: IsInstance<AbstractRange>;
};

interface AbstractWorkerEventMap {
    "error": Event;
}

interface AbstractWorker {
    onerror: ((this: AbstractWorker, ev: Event) => any) | null;
    addEventListener<K extends keyof AbstractWorkerEventMap>(type: K, listener: (this: AbstractWorker, ev: AbstractWorkerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AbstractWorkerEventMap>(type: K, listener: (this: AbstractWorker, ev: AbstractWorkerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface AccessibleNode {
    readonly DOMNode: Node | null;
    activeDescendant: AccessibleNode | null;
    atomic: boolean | null;
    readonly attributes: string[];
    autocomplete: string | null;
    busy: boolean | null;
    checked: string | null;
    colCount: number | null;
    colIndex: number | null;
    colSpan: number | null;
    readonly computedRole: string;
    current: string | null;
    details: AccessibleNode | null;
    disabled: boolean | null;
    errorMessage: AccessibleNode | null;
    expanded: boolean | null;
    hasPopUp: string | null;
    hidden: boolean | null;
    invalid: string | null;
    keyShortcuts: string | null;
    label: string | null;
    level: number | null;
    live: string | null;
    modal: boolean | null;
    multiline: boolean | null;
    multiselectable: boolean | null;
    orientation: string | null;
    placeholder: string | null;
    posInSet: number | null;
    pressed: string | null;
    readOnly: boolean | null;
    relevant: string | null;
    required: boolean | null;
    role: string | null;
    roleDescription: string | null;
    rowCount: number | null;
    rowIndex: number | null;
    rowSpan: number | null;
    selected: boolean | null;
    setSize: number | null;
    sort: string | null;
    readonly states: string[];
    valueMax: number | null;
    valueMin: number | null;
    valueNow: number | null;
    valueText: string | null;
    get(attribute: string): any;
    has(...attributes: string[]): boolean;
    is(...states: string[]): boolean;
}

declare var AccessibleNode: {
    prototype: AccessibleNode;
    new(): AccessibleNode;
    isInstance: IsInstance<AccessibleNode>;
};

interface Addon {
    readonly canUninstall: boolean;
    readonly description: string;
    readonly id: string;
    readonly isActive: boolean;
    readonly isEnabled: boolean;
    readonly name: string;
    readonly type: string;
    readonly version: string;
    setEnabled(value: boolean): Promise<void>;
    uninstall(): Promise<boolean>;
}

declare var Addon: {
    prototype: Addon;
    new(): Addon;
    isInstance: IsInstance<Addon>;
};

interface AddonEvent extends Event {
    readonly id: string;
}

declare var AddonEvent: {
    prototype: AddonEvent;
    new(type: string, eventInitDict: AddonEventInit): AddonEvent;
    isInstance: IsInstance<AddonEvent>;
};

interface AddonInstall extends EventTarget {
    readonly error: string | null;
    readonly maxProgress: number;
    readonly progress: number;
    readonly state: string;
    cancel(): Promise<void>;
    install(): Promise<void>;
}

declare var AddonInstall: {
    prototype: AddonInstall;
    new(): AddonInstall;
    isInstance: IsInstance<AddonInstall>;
};

interface AddonManager extends EventTarget {
    createInstall(options?: addonInstallOptions): Promise<AddonInstall>;
    getAddonByID(id: string): Promise<Addon>;
    sendAbuseReport(addonId: string, data: Record<string, string | null>, options?: sendAbuseReportOptions): Promise<any>;
}

declare var AddonManager: {
    prototype: AddonManager;
    new(): AddonManager;
    isInstance: IsInstance<AddonManager>;
};

interface AnalyserNode extends AudioNode, AudioNodePassThrough {
    fftSize: number;
    readonly frequencyBinCount: number;
    maxDecibels: number;
    minDecibels: number;
    smoothingTimeConstant: number;
    getByteFrequencyData(array: Uint8Array): void;
    getByteTimeDomainData(array: Uint8Array): void;
    getFloatFrequencyData(array: Float32Array): void;
    getFloatTimeDomainData(array: Float32Array): void;
}

declare var AnalyserNode: {
    prototype: AnalyserNode;
    new(context: BaseAudioContext, options?: AnalyserOptions): AnalyserNode;
    isInstance: IsInstance<AnalyserNode>;
};

interface Animatable {
    animate(keyframes: any, options?: UnrestrictedDoubleOrKeyframeAnimationOptions): Animation;
    getAnimations(options?: GetAnimationsOptions): Animation[];
}

interface AnimationEventMap {
    "cancel": Event;
    "finish": Event;
    "remove": Event;
}

interface Animation extends EventTarget {
    currentTime: number | null;
    effect: AnimationEffect | null;
    readonly finished: Promise<Animation>;
    id: string;
    readonly isRunningOnCompositor: boolean;
    oncancel: ((this: Animation, ev: Event) => any) | null;
    onfinish: ((this: Animation, ev: Event) => any) | null;
    onremove: ((this: Animation, ev: Event) => any) | null;
    readonly pending: boolean;
    readonly playState: AnimationPlayState;
    playbackRate: number;
    readonly ready: Promise<Animation>;
    readonly replaceState: AnimationReplaceState;
    startTime: number | null;
    timeline: AnimationTimeline | null;
    cancel(): void;
    commitStyles(): void;
    finish(): void;
    pause(): void;
    persist(): void;
    play(): void;
    reverse(): void;
    updatePlaybackRate(playbackRate: number): void;
    addEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: Animation, ev: AnimationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: Animation, ev: AnimationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Animation: {
    prototype: Animation;
    new(effect?: AnimationEffect | null, timeline?: AnimationTimeline | null): Animation;
    isInstance: IsInstance<Animation>;
};

interface AnimationEffect {
    getComputedTiming(): ComputedEffectTiming;
    getTiming(): EffectTiming;
    updateTiming(timing?: OptionalEffectTiming): void;
}

declare var AnimationEffect: {
    prototype: AnimationEffect;
    new(): AnimationEffect;
    isInstance: IsInstance<AnimationEffect>;
};

interface AnimationEvent extends Event {
    readonly animationName: string;
    readonly elapsedTime: number;
    readonly pseudoElement: string;
}

declare var AnimationEvent: {
    prototype: AnimationEvent;
    new(type: string, eventInitDict?: AnimationEventInit): AnimationEvent;
    isInstance: IsInstance<AnimationEvent>;
};

interface AnimationFrameProvider {
    cancelAnimationFrame(handle: number): void;
    requestAnimationFrame(callback: FrameRequestCallback): number;
}

interface AnimationPlaybackEvent extends Event {
    readonly currentTime: number | null;
    readonly timelineTime: number | null;
}

declare var AnimationPlaybackEvent: {
    prototype: AnimationPlaybackEvent;
    new(type: string, eventInitDict?: AnimationPlaybackEventInit): AnimationPlaybackEvent;
    isInstance: IsInstance<AnimationPlaybackEvent>;
};

interface AnimationTimeline {
    readonly currentTime: number | null;
}

declare var AnimationTimeline: {
    prototype: AnimationTimeline;
    new(): AnimationTimeline;
    isInstance: IsInstance<AnimationTimeline>;
};

interface AnonymousContent {
    readonly root: ShadowRoot;
}

declare var AnonymousContent: {
    prototype: AnonymousContent;
    new(): AnonymousContent;
    isInstance: IsInstance<AnonymousContent>;
};

interface Attr extends Node {
    readonly localName: string;
    readonly name: string;
    readonly namespaceURI: string | null;
    readonly ownerElement: Element | null;
    readonly prefix: string | null;
    readonly specified: boolean;
    value: string;
}

declare var Attr: {
    prototype: Attr;
    new(): Attr;
    isInstance: IsInstance<Attr>;
};

interface AudioBuffer {
    readonly duration: number;
    readonly length: number;
    readonly numberOfChannels: number;
    readonly sampleRate: number;
    copyFromChannel(destination: Float32Array, channelNumber: number, startInChannel?: number): void;
    copyToChannel(source: Float32Array, channelNumber: number, startInChannel?: number): void;
    getChannelData(channel: number): Float32Array;
}

declare var AudioBuffer: {
    prototype: AudioBuffer;
    new(options: AudioBufferOptions): AudioBuffer;
    isInstance: IsInstance<AudioBuffer>;
};

interface AudioBufferSourceNode extends AudioScheduledSourceNode, AudioNodePassThrough {
    buffer: AudioBuffer | null;
    readonly detune: AudioParam;
    loop: boolean;
    loopEnd: number;
    loopStart: number;
    readonly playbackRate: AudioParam;
    start(when?: number, grainOffset?: number, grainDuration?: number): void;
    addEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: AudioBufferSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: AudioBufferSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioBufferSourceNode: {
    prototype: AudioBufferSourceNode;
    new(context: BaseAudioContext, options?: AudioBufferSourceOptions): AudioBufferSourceNode;
    isInstance: IsInstance<AudioBufferSourceNode>;
};

interface AudioContext extends BaseAudioContext {
    readonly baseLatency: number;
    readonly outputLatency: number;
    close(): Promise<void>;
    createMediaElementSource(mediaElement: HTMLMediaElement): MediaElementAudioSourceNode;
    createMediaStreamDestination(): MediaStreamAudioDestinationNode;
    createMediaStreamSource(mediaStream: MediaStream): MediaStreamAudioSourceNode;
    createMediaStreamTrackSource(mediaStreamTrack: MediaStreamTrack): MediaStreamTrackAudioSourceNode;
    getOutputTimestamp(): AudioTimestamp;
    suspend(): Promise<void>;
    addEventListener<K extends keyof BaseAudioContextEventMap>(type: K, listener: (this: AudioContext, ev: BaseAudioContextEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof BaseAudioContextEventMap>(type: K, listener: (this: AudioContext, ev: BaseAudioContextEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioContext: {
    prototype: AudioContext;
    new(contextOptions?: AudioContextOptions): AudioContext;
    isInstance: IsInstance<AudioContext>;
};

interface AudioData {
    readonly duration: number;
    readonly format: AudioSampleFormat | null;
    readonly numberOfChannels: number;
    readonly numberOfFrames: number;
    readonly sampleRate: number;
    readonly timestamp: number;
    allocationSize(options: AudioDataCopyToOptions): number;
    clone(): AudioData;
    close(): void;
    copyTo(destination: ArrayBufferView | ArrayBuffer, options: AudioDataCopyToOptions): void;
}

declare var AudioData: {
    prototype: AudioData;
    new(init: AudioDataInit): AudioData;
    isInstance: IsInstance<AudioData>;
};

interface AudioDecoderEventMap {
    "dequeue": Event;
}

/** Available only in secure contexts. */
interface AudioDecoder extends EventTarget {
    readonly decodeQueueSize: number;
    ondequeue: ((this: AudioDecoder, ev: Event) => any) | null;
    readonly state: CodecState;
    close(): void;
    configure(config: AudioDecoderConfig): void;
    decode(chunk: EncodedAudioChunk): void;
    flush(): Promise<void>;
    reset(): void;
    addEventListener<K extends keyof AudioDecoderEventMap>(type: K, listener: (this: AudioDecoder, ev: AudioDecoderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioDecoderEventMap>(type: K, listener: (this: AudioDecoder, ev: AudioDecoderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioDecoder: {
    prototype: AudioDecoder;
    new(init: AudioDecoderInit): AudioDecoder;
    isInstance: IsInstance<AudioDecoder>;
    isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport>;
};

interface AudioDestinationNode extends AudioNode {
    readonly maxChannelCount: number;
}

declare var AudioDestinationNode: {
    prototype: AudioDestinationNode;
    new(): AudioDestinationNode;
    isInstance: IsInstance<AudioDestinationNode>;
};

interface AudioEncoderEventMap {
    "dequeue": Event;
}

/** Available only in secure contexts. */
interface AudioEncoder extends EventTarget {
    readonly encodeQueueSize: number;
    ondequeue: ((this: AudioEncoder, ev: Event) => any) | null;
    readonly state: CodecState;
    close(): void;
    configure(config: AudioEncoderConfig): void;
    encode(data: AudioData): void;
    flush(): Promise<void>;
    reset(): void;
    addEventListener<K extends keyof AudioEncoderEventMap>(type: K, listener: (this: AudioEncoder, ev: AudioEncoderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioEncoderEventMap>(type: K, listener: (this: AudioEncoder, ev: AudioEncoderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioEncoder: {
    prototype: AudioEncoder;
    new(init: AudioEncoderInit): AudioEncoder;
    isInstance: IsInstance<AudioEncoder>;
    isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport>;
};

interface AudioListener {
    setOrientation(x: number, y: number, z: number, xUp: number, yUp: number, zUp: number): void;
    setPosition(x: number, y: number, z: number): void;
}

declare var AudioListener: {
    prototype: AudioListener;
    new(): AudioListener;
    isInstance: IsInstance<AudioListener>;
};

interface AudioNode extends EventTarget {
    channelCount: number;
    channelCountMode: ChannelCountMode;
    channelInterpretation: ChannelInterpretation;
    readonly context: BaseAudioContext;
    readonly id: number;
    readonly numberOfInputs: number;
    readonly numberOfOutputs: number;
    connect(destination: AudioNode, output?: number, input?: number): AudioNode;
    connect(destination: AudioParam, output?: number): void;
    disconnect(): void;
    disconnect(output: number): void;
    disconnect(destination: AudioNode): void;
    disconnect(destination: AudioNode, output: number): void;
    disconnect(destination: AudioNode, output: number, input: number): void;
    disconnect(destination: AudioParam): void;
    disconnect(destination: AudioParam, output: number): void;
}

declare var AudioNode: {
    prototype: AudioNode;
    new(): AudioNode;
    isInstance: IsInstance<AudioNode>;
};

interface AudioNodePassThrough {
    passThrough: boolean;
}

interface AudioParam {
    readonly defaultValue: number;
    readonly isTrackSuspended: boolean;
    readonly maxValue: number;
    readonly minValue: number;
    readonly name: string;
    readonly parentNodeId: number;
    value: number;
    cancelScheduledValues(startTime: number): AudioParam;
    exponentialRampToValueAtTime(value: number, endTime: number): AudioParam;
    linearRampToValueAtTime(value: number, endTime: number): AudioParam;
    setTargetAtTime(target: number, startTime: number, timeConstant: number): AudioParam;
    setValueAtTime(value: number, startTime: number): AudioParam;
    setValueCurveAtTime(values: number[] | Float32Array, startTime: number, duration: number): AudioParam;
}

declare var AudioParam: {
    prototype: AudioParam;
    new(): AudioParam;
    isInstance: IsInstance<AudioParam>;
};

/** Available only in secure contexts. */
interface AudioParamMap {
    forEach(callbackfn: (value: AudioParam, key: string, parent: AudioParamMap) => void, thisArg?: any): void;
}

declare var AudioParamMap: {
    prototype: AudioParamMap;
    new(): AudioParamMap;
    isInstance: IsInstance<AudioParamMap>;
};

interface AudioProcessingEvent extends Event {
    readonly inputBuffer: AudioBuffer;
    readonly outputBuffer: AudioBuffer;
    readonly playbackTime: number;
}

declare var AudioProcessingEvent: {
    prototype: AudioProcessingEvent;
    new(): AudioProcessingEvent;
    isInstance: IsInstance<AudioProcessingEvent>;
};

interface AudioScheduledSourceNodeEventMap {
    "ended": Event;
}

interface AudioScheduledSourceNode extends AudioNode {
    onended: ((this: AudioScheduledSourceNode, ev: Event) => any) | null;
    start(when?: number): void;
    stop(when?: number): void;
    addEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: AudioScheduledSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: AudioScheduledSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioScheduledSourceNode: {
    prototype: AudioScheduledSourceNode;
    new(): AudioScheduledSourceNode;
    isInstance: IsInstance<AudioScheduledSourceNode>;
};

interface AudioTrack {
    enabled: boolean;
    readonly id: string;
    readonly kind: string;
    readonly label: string;
    readonly language: string;
}

declare var AudioTrack: {
    prototype: AudioTrack;
    new(): AudioTrack;
    isInstance: IsInstance<AudioTrack>;
};

interface AudioTrackListEventMap {
    "addtrack": Event;
    "change": Event;
    "removetrack": Event;
}

interface AudioTrackList extends EventTarget {
    readonly length: number;
    onaddtrack: ((this: AudioTrackList, ev: Event) => any) | null;
    onchange: ((this: AudioTrackList, ev: Event) => any) | null;
    onremovetrack: ((this: AudioTrackList, ev: Event) => any) | null;
    getTrackById(id: string): AudioTrack | null;
    addEventListener<K extends keyof AudioTrackListEventMap>(type: K, listener: (this: AudioTrackList, ev: AudioTrackListEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioTrackListEventMap>(type: K, listener: (this: AudioTrackList, ev: AudioTrackListEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: AudioTrack;
}

declare var AudioTrackList: {
    prototype: AudioTrackList;
    new(): AudioTrackList;
    isInstance: IsInstance<AudioTrackList>;
};

/** Available only in secure contexts. */
interface AudioWorklet extends Worklet {
}

declare var AudioWorklet: {
    prototype: AudioWorklet;
    new(): AudioWorklet;
    isInstance: IsInstance<AudioWorklet>;
};

interface AudioWorkletNodeEventMap {
    "processorerror": Event;
}

/** Available only in secure contexts. */
interface AudioWorkletNode extends AudioNode {
    onprocessorerror: ((this: AudioWorkletNode, ev: Event) => any) | null;
    readonly parameters: AudioParamMap;
    readonly port: MessagePort;
    addEventListener<K extends keyof AudioWorkletNodeEventMap>(type: K, listener: (this: AudioWorkletNode, ev: AudioWorkletNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioWorkletNodeEventMap>(type: K, listener: (this: AudioWorkletNode, ev: AudioWorkletNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var AudioWorkletNode: {
    prototype: AudioWorkletNode;
    new(context: BaseAudioContext, name: string, options?: AudioWorkletNodeOptions): AudioWorkletNode;
    isInstance: IsInstance<AudioWorkletNode>;
};

/** Available only in secure contexts. */
interface AuthenticatorAssertionResponse extends AuthenticatorResponse {
    readonly authenticatorData: ArrayBuffer;
    readonly signature: ArrayBuffer;
    readonly userHandle: ArrayBuffer | null;
}

declare var AuthenticatorAssertionResponse: {
    prototype: AuthenticatorAssertionResponse;
    new(): AuthenticatorAssertionResponse;
    isInstance: IsInstance<AuthenticatorAssertionResponse>;
};

/** Available only in secure contexts. */
interface AuthenticatorAttestationResponse extends AuthenticatorResponse {
    readonly attestationObject: ArrayBuffer;
    getAuthenticatorData(): ArrayBuffer;
    getPublicKey(): ArrayBuffer | null;
    getPublicKeyAlgorithm(): COSEAlgorithmIdentifier;
    getTransports(): string[];
}

declare var AuthenticatorAttestationResponse: {
    prototype: AuthenticatorAttestationResponse;
    new(): AuthenticatorAttestationResponse;
    isInstance: IsInstance<AuthenticatorAttestationResponse>;
};

/** Available only in secure contexts. */
interface AuthenticatorResponse {
    readonly clientDataJSON: ArrayBuffer;
}

declare var AuthenticatorResponse: {
    prototype: AuthenticatorResponse;
    new(): AuthenticatorResponse;
    isInstance: IsInstance<AuthenticatorResponse>;
};

interface BarProp {
    visible: boolean;
}

declare var BarProp: {
    prototype: BarProp;
    new(): BarProp;
    isInstance: IsInstance<BarProp>;
};

interface BaseAudioContextEventMap {
    "statechange": Event;
}

interface BaseAudioContext extends EventTarget {
    /** Available only in secure contexts. */
    readonly audioWorklet: AudioWorklet;
    readonly currentTime: number;
    readonly destination: AudioDestinationNode;
    readonly listener: AudioListener;
    onstatechange: ((this: BaseAudioContext, ev: Event) => any) | null;
    readonly sampleRate: number;
    readonly state: AudioContextState;
    createAnalyser(): AnalyserNode;
    createBiquadFilter(): BiquadFilterNode;
    createBuffer(numberOfChannels: number, length: number, sampleRate: number): AudioBuffer;
    createBufferSource(): AudioBufferSourceNode;
    createChannelMerger(numberOfInputs?: number): ChannelMergerNode;
    createChannelSplitter(numberOfOutputs?: number): ChannelSplitterNode;
    createConstantSource(): ConstantSourceNode;
    createConvolver(): ConvolverNode;
    createDelay(maxDelayTime?: number): DelayNode;
    createDynamicsCompressor(): DynamicsCompressorNode;
    createGain(): GainNode;
    createIIRFilter(feedforward: number[], feedback: number[]): IIRFilterNode;
    createOscillator(): OscillatorNode;
    createPanner(): PannerNode;
    createPeriodicWave(real: number[] | Float32Array, imag: number[] | Float32Array, constraints?: PeriodicWaveConstraints): PeriodicWave;
    createScriptProcessor(bufferSize?: number, numberOfInputChannels?: number, numberOfOutputChannels?: number): ScriptProcessorNode;
    createStereoPanner(): StereoPannerNode;
    createWaveShaper(): WaveShaperNode;
    decodeAudioData(audioData: ArrayBuffer, successCallback?: DecodeSuccessCallback, errorCallback?: DecodeErrorCallback): Promise<AudioBuffer>;
    resume(): Promise<void>;
    addEventListener<K extends keyof BaseAudioContextEventMap>(type: K, listener: (this: BaseAudioContext, ev: BaseAudioContextEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof BaseAudioContextEventMap>(type: K, listener: (this: BaseAudioContext, ev: BaseAudioContextEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var BaseAudioContext: {
    prototype: BaseAudioContext;
    new(): BaseAudioContext;
    isInstance: IsInstance<BaseAudioContext>;
};

interface BatteryManagerEventMap {
    "chargingchange": Event;
    "chargingtimechange": Event;
    "dischargingtimechange": Event;
    "levelchange": Event;
}

interface BatteryManager extends EventTarget {
    readonly charging: boolean;
    readonly chargingTime: number;
    readonly dischargingTime: number;
    readonly level: number;
    onchargingchange: ((this: BatteryManager, ev: Event) => any) | null;
    onchargingtimechange: ((this: BatteryManager, ev: Event) => any) | null;
    ondischargingtimechange: ((this: BatteryManager, ev: Event) => any) | null;
    onlevelchange: ((this: BatteryManager, ev: Event) => any) | null;
    addEventListener<K extends keyof BatteryManagerEventMap>(type: K, listener: (this: BatteryManager, ev: BatteryManagerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof BatteryManagerEventMap>(type: K, listener: (this: BatteryManager, ev: BatteryManagerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var BatteryManager: {
    prototype: BatteryManager;
    new(): BatteryManager;
    isInstance: IsInstance<BatteryManager>;
};

// @ts-ignore
interface BeforeUnloadEvent extends Event {
    returnValue: string;
}

declare var BeforeUnloadEvent: {
    prototype: BeforeUnloadEvent;
    new(): BeforeUnloadEvent;
    isInstance: IsInstance<BeforeUnloadEvent>;
};

interface BiquadFilterNode extends AudioNode, AudioNodePassThrough {
    readonly Q: AudioParam;
    readonly detune: AudioParam;
    readonly frequency: AudioParam;
    readonly gain: AudioParam;
    type: BiquadFilterType;
    getFrequencyResponse(frequencyHz: Float32Array, magResponse: Float32Array, phaseResponse: Float32Array): void;
}

declare var BiquadFilterNode: {
    prototype: BiquadFilterNode;
    new(context: BaseAudioContext, options?: BiquadFilterOptions): BiquadFilterNode;
    isInstance: IsInstance<BiquadFilterNode>;
};

interface Blob {
    readonly blobImplType: string;
    readonly size: number;
    readonly type: string;
    arrayBuffer(): Promise<ArrayBuffer>;
    bytes(): Promise<Uint8Array>;
    slice(start?: number, end?: number, contentType?: string): Blob;
    stream(): ReadableStream;
    text(): Promise<string>;
}

declare var Blob: {
    prototype: Blob;
    new(blobParts?: BlobPart[], options?: BlobPropertyBag): Blob;
    isInstance: IsInstance<Blob>;
};

interface BlobEvent extends Event {
    readonly data: Blob;
}

declare var BlobEvent: {
    prototype: BlobEvent;
    new(type: string, eventInitDict: BlobEventInit): BlobEvent;
    isInstance: IsInstance<BlobEvent>;
};

interface Body {
    readonly bodyUsed: boolean;
    arrayBuffer(): Promise<ArrayBuffer>;
    blob(): Promise<Blob>;
    bytes(): Promise<Uint8Array>;
    formData(): Promise<FormData>;
    json(): Promise<JSON>;
    text(): Promise<string>;
}

interface BroadcastChannelEventMap {
    "message": Event;
    "messageerror": Event;
}

interface BroadcastChannel extends EventTarget {
    readonly name: string;
    onmessage: ((this: BroadcastChannel, ev: Event) => any) | null;
    onmessageerror: ((this: BroadcastChannel, ev: Event) => any) | null;
    close(): void;
    postMessage(message: any): void;
    addEventListener<K extends keyof BroadcastChannelEventMap>(type: K, listener: (this: BroadcastChannel, ev: BroadcastChannelEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof BroadcastChannelEventMap>(type: K, listener: (this: BroadcastChannel, ev: BroadcastChannelEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var BroadcastChannel: {
    prototype: BroadcastChannel;
    new(channel: string): BroadcastChannel;
    isInstance: IsInstance<BroadcastChannel>;
};

interface BrowsingContext extends LoadContextMixin {
    allowJavascript: boolean;
    readonly ancestorsAreCurrent: boolean;
    authorStyleDisabledDefault: boolean;
    browserId: number;
    readonly childOffset: number;
    readonly childSessionHistory: ChildSHistory | null;
    readonly children: BrowsingContext[];
    readonly createdDynamically: boolean;
    readonly currentWindowContext: WindowContext | null;
    customPlatform: string;
    customUserAgent: string;
    defaultLoadFlags: number;
    displayMode: DisplayMode;
    readonly docShell: nsIDocShell | null;
    readonly embedderElement: Element | null;
    readonly embedderElementType: string;
    forceDesktopViewport: boolean;
    forceOffline: boolean;
    fullZoom: number;
    readonly group: BrowsingContextGroup;
    hasSiblings: boolean;
    readonly historyID: any;
    readonly id: number;
    inRDMPane: boolean;
    readonly isActive: boolean;
    isAppTab: boolean;
    readonly isDiscarded: boolean;
    readonly isInBFCache: boolean;
    mediumOverride: string;
    readonly name: string;
    readonly opener: BrowsingContext | null;
    overrideDPPX: number;
    readonly parent: BrowsingContext | null;
    readonly parentWindowContext: WindowContext | null;
    prefersColorSchemeOverride: PrefersColorSchemeOverride;
    sandboxFlags: number;
    serviceWorkersTestingEnabled: boolean;
    suspendMediaWhenInactive: boolean;
    readonly targetTopLevelLinkClicksToBlank: boolean;
    textZoom: number;
    readonly top: BrowsingContext;
    readonly topWindowContext: WindowContext | null;
    readonly touchEventsOverride: TouchEventsOverride;
    useGlobalHistory: boolean;
    watchedByDevTools: boolean;
    readonly window: WindowProxy | null;
    getAllBrowsingContextsInSubtree(): BrowsingContext[];
    resetNavigationRateLimit(): void;
    setRDMPaneMaxTouchPoints(maxTouchPoints: number): void;
    setRDMPaneOrientation(type: OrientationType, rotationAngle: number): void;
}

declare var BrowsingContext: {
    prototype: BrowsingContext;
    new(): BrowsingContext;
    isInstance: IsInstance<BrowsingContext>;
    get(aId: number): BrowsingContext | null;
    getCurrentTopByBrowserId(aId: number): BrowsingContext | null;
    getFromWindow(window: WindowProxy): BrowsingContext | null;
};

interface BrowsingContextGroup {
    readonly id: number;
    getToplevels(): BrowsingContext[];
}

declare var BrowsingContextGroup: {
    prototype: BrowsingContextGroup;
    new(): BrowsingContextGroup;
    isInstance: IsInstance<BrowsingContextGroup>;
};

interface BufferSource {
}

interface ByteLengthQueuingStrategy {
    readonly highWaterMark: number;
    readonly size: Function;
}

declare var ByteLengthQueuingStrategy: {
    prototype: ByteLengthQueuingStrategy;
    new(init: QueuingStrategyInit): ByteLengthQueuingStrategy;
    isInstance: IsInstance<ByteLengthQueuingStrategy>;
};

interface CDATASection extends Text {
}

declare var CDATASection: {
    prototype: CDATASection;
    new(): CDATASection;
    isInstance: IsInstance<CDATASection>;
};

interface CSPViolationReportBody extends ReportBody {
    readonly blockedURL: string | null;
    readonly columnNumber: number | null;
    readonly disposition: SecurityPolicyViolationEventDisposition;
    readonly documentURL: string;
    readonly effectiveDirective: string;
    readonly lineNumber: number | null;
    readonly originalPolicy: string;
    readonly referrer: string | null;
    readonly sample: string | null;
    readonly sourceFile: string | null;
    readonly statusCode: number;
    toJSON(): any;
}

declare var CSPViolationReportBody: {
    prototype: CSPViolationReportBody;
    new(): CSPViolationReportBody;
    isInstance: IsInstance<CSPViolationReportBody>;
};

interface CSSAnimation extends Animation {
    readonly animationName: string;
    addEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: CSSAnimation, ev: AnimationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: CSSAnimation, ev: AnimationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var CSSAnimation: {
    prototype: CSSAnimation;
    new(): CSSAnimation;
    isInstance: IsInstance<CSSAnimation>;
};

interface CSSConditionRule extends CSSGroupingRule {
    readonly conditionText: string;
}

declare var CSSConditionRule: {
    prototype: CSSConditionRule;
    new(): CSSConditionRule;
    isInstance: IsInstance<CSSConditionRule>;
};

interface CSSContainerRule extends CSSConditionRule {
    readonly containerName: string;
    readonly containerQuery: string;
    queryContainerFor(element: Element): Element | null;
}

declare var CSSContainerRule: {
    prototype: CSSContainerRule;
    new(): CSSContainerRule;
    isInstance: IsInstance<CSSContainerRule>;
};

interface CSSCounterStyleRule extends CSSRule {
    additiveSymbols: string;
    fallback: string;
    name: string;
    negative: string;
    pad: string;
    prefix: string;
    range: string;
    speakAs: string;
    suffix: string;
    symbols: string;
    system: string;
}

declare var CSSCounterStyleRule: {
    prototype: CSSCounterStyleRule;
    new(): CSSCounterStyleRule;
    isInstance: IsInstance<CSSCounterStyleRule>;
};

interface CSSCustomPropertyRegisteredEvent extends Event {
    readonly propertyDefinition: InspectorCSSPropertyDefinition;
}

declare var CSSCustomPropertyRegisteredEvent: {
    prototype: CSSCustomPropertyRegisteredEvent;
    new(type: string, eventInitDict?: CSSCustomPropertyRegisteredEventInit): CSSCustomPropertyRegisteredEvent;
    isInstance: IsInstance<CSSCustomPropertyRegisteredEvent>;
};

interface CSSFontFaceRule extends CSSRule {
    readonly style: CSSStyleDeclaration;
}

declare var CSSFontFaceRule: {
    prototype: CSSFontFaceRule;
    new(): CSSFontFaceRule;
    isInstance: IsInstance<CSSFontFaceRule>;
};

interface CSSFontFeatureValuesRule extends CSSRule {
    fontFamily: string;
    valueText: string;
}

declare var CSSFontFeatureValuesRule: {
    prototype: CSSFontFeatureValuesRule;
    new(): CSSFontFeatureValuesRule;
    isInstance: IsInstance<CSSFontFeatureValuesRule>;
};

interface CSSFontPaletteValuesRule extends CSSRule {
    readonly basePalette: string;
    readonly fontFamily: string;
    readonly name: string;
    readonly overrideColors: string;
}

declare var CSSFontPaletteValuesRule: {
    prototype: CSSFontPaletteValuesRule;
    new(): CSSFontPaletteValuesRule;
    isInstance: IsInstance<CSSFontPaletteValuesRule>;
};

interface CSSGroupingRule extends CSSRule {
    readonly cssRules: CSSRuleList;
    deleteRule(index: number): void;
    insertRule(rule: string, index?: number): number;
}

declare var CSSGroupingRule: {
    prototype: CSSGroupingRule;
    new(): CSSGroupingRule;
    isInstance: IsInstance<CSSGroupingRule>;
};

interface CSSImportRule extends CSSRule {
    readonly href: string;
    readonly layerName: string | null;
    readonly media: MediaList | null;
    readonly styleSheet: CSSStyleSheet | null;
    readonly supportsText: string | null;
}

declare var CSSImportRule: {
    prototype: CSSImportRule;
    new(): CSSImportRule;
    isInstance: IsInstance<CSSImportRule>;
};

interface CSSKeyframeRule extends CSSRule {
    keyText: string;
    readonly style: CSSStyleDeclaration;
}

declare var CSSKeyframeRule: {
    prototype: CSSKeyframeRule;
    new(): CSSKeyframeRule;
    isInstance: IsInstance<CSSKeyframeRule>;
};

interface CSSKeyframesRule extends CSSRule {
    readonly cssRules: CSSRuleList;
    readonly length: number;
    name: string;
    appendRule(rule: string): void;
    deleteRule(select: string): void;
    findRule(select: string): CSSKeyframeRule | null;
    [index: number]: CSSKeyframeRule;
}

declare var CSSKeyframesRule: {
    prototype: CSSKeyframesRule;
    new(): CSSKeyframesRule;
    isInstance: IsInstance<CSSKeyframesRule>;
};

interface CSSLayerBlockRule extends CSSGroupingRule {
    readonly name: string;
}

declare var CSSLayerBlockRule: {
    prototype: CSSLayerBlockRule;
    new(): CSSLayerBlockRule;
    isInstance: IsInstance<CSSLayerBlockRule>;
};

interface CSSLayerStatementRule extends CSSRule {
    readonly nameList: string[];
}

declare var CSSLayerStatementRule: {
    prototype: CSSLayerStatementRule;
    new(): CSSLayerStatementRule;
    isInstance: IsInstance<CSSLayerStatementRule>;
};

interface CSSMarginRule extends CSSRule {
    readonly name: string;
    readonly style: CSSStyleDeclaration;
}

declare var CSSMarginRule: {
    prototype: CSSMarginRule;
    new(): CSSMarginRule;
    isInstance: IsInstance<CSSMarginRule>;
};

interface CSSMediaRule extends CSSConditionRule {
    readonly media: MediaList;
}

declare var CSSMediaRule: {
    prototype: CSSMediaRule;
    new(): CSSMediaRule;
    isInstance: IsInstance<CSSMediaRule>;
};

interface CSSMozDocumentRule extends CSSConditionRule {
}

declare var CSSMozDocumentRule: {
    prototype: CSSMozDocumentRule;
    new(): CSSMozDocumentRule;
    isInstance: IsInstance<CSSMozDocumentRule>;
};

interface CSSNamespaceRule extends CSSRule {
    readonly namespaceURI: string;
    readonly prefix: string;
}

declare var CSSNamespaceRule: {
    prototype: CSSNamespaceRule;
    new(): CSSNamespaceRule;
    isInstance: IsInstance<CSSNamespaceRule>;
};

interface CSSPageDescriptors {
}

interface CSSPageRule extends CSSGroupingRule {
    selectorText: string;
    readonly style: CSSPageDescriptors;
}

declare var CSSPageRule: {
    prototype: CSSPageRule;
    new(): CSSPageRule;
    isInstance: IsInstance<CSSPageRule>;
};

interface CSSPropertyRule extends CSSRule {
    readonly inherits: boolean;
    readonly initialValue: string | null;
    readonly name: string;
    readonly syntax: string;
}

declare var CSSPropertyRule: {
    prototype: CSSPropertyRule;
    new(): CSSPropertyRule;
    isInstance: IsInstance<CSSPropertyRule>;
};

interface CSSPseudoElement {
    readonly element: Element;
    readonly type: string;
}

declare var CSSPseudoElement: {
    prototype: CSSPseudoElement;
    new(): CSSPseudoElement;
    isInstance: IsInstance<CSSPseudoElement>;
};

interface CSSRule {
    cssText: string;
    readonly parentRule: CSSRule | null;
    readonly parentStyleSheet: CSSStyleSheet | null;
    readonly type: number;
    readonly STYLE_RULE: 1;
    readonly CHARSET_RULE: 2;
    readonly IMPORT_RULE: 3;
    readonly MEDIA_RULE: 4;
    readonly FONT_FACE_RULE: 5;
    readonly PAGE_RULE: 6;
    readonly NAMESPACE_RULE: 10;
    readonly KEYFRAMES_RULE: 7;
    readonly KEYFRAME_RULE: 8;
    readonly COUNTER_STYLE_RULE: 11;
    readonly SUPPORTS_RULE: 12;
    readonly DOCUMENT_RULE: 13;
    readonly FONT_FEATURE_VALUES_RULE: 14;
}

declare var CSSRule: {
    prototype: CSSRule;
    new(): CSSRule;
    readonly STYLE_RULE: 1;
    readonly CHARSET_RULE: 2;
    readonly IMPORT_RULE: 3;
    readonly MEDIA_RULE: 4;
    readonly FONT_FACE_RULE: 5;
    readonly PAGE_RULE: 6;
    readonly NAMESPACE_RULE: 10;
    readonly KEYFRAMES_RULE: 7;
    readonly KEYFRAME_RULE: 8;
    readonly COUNTER_STYLE_RULE: 11;
    readonly SUPPORTS_RULE: 12;
    readonly DOCUMENT_RULE: 13;
    readonly FONT_FEATURE_VALUES_RULE: 14;
    isInstance: IsInstance<CSSRule>;
};

interface CSSRuleList {
    readonly length: number;
    item(index: number): CSSRule | null;
    [index: number]: CSSRule;
}

declare var CSSRuleList: {
    prototype: CSSRuleList;
    new(): CSSRuleList;
    isInstance: IsInstance<CSSRuleList>;
};

interface CSSScopeRule extends CSSGroupingRule {
    readonly end: string | null;
    readonly start: string | null;
}

declare var CSSScopeRule: {
    prototype: CSSScopeRule;
    new(): CSSScopeRule;
    isInstance: IsInstance<CSSScopeRule>;
};

interface CSSStartingStyleRule extends CSSGroupingRule {
}

declare var CSSStartingStyleRule: {
    prototype: CSSStartingStyleRule;
    new(): CSSStartingStyleRule;
    isInstance: IsInstance<CSSStartingStyleRule>;
};

interface CSSStyleDeclaration {
    cssText: string;
    readonly length: number;
    readonly parentRule: CSSRule | null;
    readonly usedFontSize: number;
    getCSSImageURLs(property: string): string[];
    getPropertyPriority(property: string): string;
    getPropertyValue(property: string): string;
    item(index: number): string;
    removeProperty(property: string): string;
    setProperty(property: string, value: string | null, priority?: string): void;
    [index: number]: string;
}

declare var CSSStyleDeclaration: {
    prototype: CSSStyleDeclaration;
    new(): CSSStyleDeclaration;
    isInstance: IsInstance<CSSStyleDeclaration>;
};

interface CSSStyleRule extends CSSGroupingRule {
    readonly selectorCount: number;
    selectorText: string;
    readonly style: CSSStyleDeclaration;
    getSelectorWarnings(): SelectorWarning[];
    selectorMatchesElement(selectorIndex: number, element: Element, pseudo?: string, includeVisitedStyle?: boolean): boolean;
    selectorSpecificityAt(index: number, desugared?: boolean): number;
    selectorTextAt(index: number, desugared?: boolean): string;
}

declare var CSSStyleRule: {
    prototype: CSSStyleRule;
    new(): CSSStyleRule;
    isInstance: IsInstance<CSSStyleRule>;
};

interface CSSStyleSheet extends StyleSheet {
    readonly cssRules: CSSRuleList;
    readonly ownerRule: CSSRule | null;
    readonly parsingMode: CSSStyleSheetParsingMode;
    readonly rules: CSSRuleList;
    addRule(selector?: string, style?: string, index?: number): number;
    deleteRule(index: number): void;
    insertRule(rule: string, index?: number): number;
    removeRule(index?: number): void;
    replace(text: string): Promise<CSSStyleSheet>;
    replaceSync(text: string): void;
}

declare var CSSStyleSheet: {
    prototype: CSSStyleSheet;
    new(options?: CSSStyleSheetInit): CSSStyleSheet;
    isInstance: IsInstance<CSSStyleSheet>;
};

interface CSSSupportsRule extends CSSConditionRule {
}

declare var CSSSupportsRule: {
    prototype: CSSSupportsRule;
    new(): CSSSupportsRule;
    isInstance: IsInstance<CSSSupportsRule>;
};

interface CSSTransition extends Animation {
    readonly transitionProperty: string;
    addEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: CSSTransition, ev: AnimationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AnimationEventMap>(type: K, listener: (this: CSSTransition, ev: AnimationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var CSSTransition: {
    prototype: CSSTransition;
    new(): CSSTransition;
    isInstance: IsInstance<CSSTransition>;
};

interface Cache {
    add(request: RequestInfo | URL): Promise<void>;
    addAll(requests: RequestInfo[]): Promise<void>;
    delete(request: RequestInfo | URL, options?: CacheQueryOptions): Promise<boolean>;
    keys(request?: RequestInfo | URL, options?: CacheQueryOptions): Promise<Request[]>;
    match(request: RequestInfo | URL, options?: CacheQueryOptions): Promise<Response>;
    matchAll(request?: RequestInfo | URL, options?: CacheQueryOptions): Promise<Response[]>;
    put(request: RequestInfo | URL, response: Response): Promise<void>;
}

declare var Cache: {
    prototype: Cache;
    new(): Cache;
    isInstance: IsInstance<Cache>;
};

interface CacheStorage {
    delete(cacheName: string): Promise<boolean>;
    has(cacheName: string): Promise<boolean>;
    keys(): Promise<string[]>;
    match(request: RequestInfo | URL, options?: MultiCacheQueryOptions): Promise<Response>;
    open(cacheName: string): Promise<Cache>;
}

declare var CacheStorage: {
    prototype: CacheStorage;
    new(namespace: CacheStorageNamespace, principal: Principal): CacheStorage;
    isInstance: IsInstance<CacheStorage>;
};

interface CallbackDebuggerNotification extends DebuggerNotification {
    readonly phase: CallbackDebuggerNotificationPhase;
}

declare var CallbackDebuggerNotification: {
    prototype: CallbackDebuggerNotification;
    new(): CallbackDebuggerNotification;
    isInstance: IsInstance<CallbackDebuggerNotification>;
};

interface CanonicalBrowsingContext extends BrowsingContext {
    readonly activeSessionHistoryEntry: nsISHEntry | null;
    crossGroupOpener: CanonicalBrowsingContext | null;
    readonly currentRemoteType: string | null;
    readonly currentURI: URI | null;
    readonly currentWindowGlobal: WindowGlobalParent | null;
    readonly embedderWindowGlobal: WindowGlobalParent | null;
    forceAppWindowActive: boolean;
    isActive: boolean;
    readonly isReplaced: boolean;
    readonly isUnderHiddenEmbedderElement: boolean;
    readonly mediaController: MediaController | null;
    readonly mostRecentLoadingSessionHistoryEntry: nsISHEntry | null;
    readonly secureBrowserUI: nsISecureBrowserUI | null;
    readonly sessionHistory: nsISHistory | null;
    targetTopLevelLinkClicksToBlank: boolean;
    readonly topChromeWindow: WindowProxy | null;
    touchEventsOverride: TouchEventsOverride;
    readonly webProgress: nsIWebProgress | null;
    clearRestoreState(): void;
    fixupAndLoadURIString(aURI: string, aOptions?: LoadURIOptions): void;
    getWindowGlobals(): WindowGlobalParent[];
    goBack(aCancelContentJSEpoch?: number, aRequireUserInteraction?: boolean, aUserActivation?: boolean): void;
    goForward(aCancelContentJSEpoch?: number, aRequireUserInteraction?: boolean, aUserActivation?: boolean): void;
    goToIndex(aIndex: number, aCancelContentJSEpoch?: number, aUserActivation?: boolean): void;
    loadURI(aURI: URI, aOptions?: LoadURIOptions): void;
    notifyMediaMutedChanged(muted: boolean): void;
    notifyStartDelayedAutoplayMedia(): void;
    print(aPrintSettings: nsIPrintSettings): Promise<void>;
    reload(aReloadFlags: number): void;
    resetScalingZoom(): void;
    startApzAutoscroll(aAnchorX: number, aAnchorY: number, aScrollId: number, aPresShellId: number): boolean;
    stop(aStopFlags: number): void;
    stopApzAutoscroll(aScrollId: number, aPresShellId: number): void;
}

declare var CanonicalBrowsingContext: {
    prototype: CanonicalBrowsingContext;
    new(): CanonicalBrowsingContext;
    isInstance: IsInstance<CanonicalBrowsingContext>;
    countSiteOrigins(roots: BrowsingContext[]): number;
};

interface CanvasCaptureMediaStream extends MediaStream {
    readonly canvas: HTMLCanvasElement;
    requestFrame(): void;
    addEventListener<K extends keyof MediaStreamEventMap>(type: K, listener: (this: CanvasCaptureMediaStream, ev: MediaStreamEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaStreamEventMap>(type: K, listener: (this: CanvasCaptureMediaStream, ev: MediaStreamEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var CanvasCaptureMediaStream: {
    prototype: CanvasCaptureMediaStream;
    new(): CanvasCaptureMediaStream;
    isInstance: IsInstance<CanvasCaptureMediaStream>;
};

interface CanvasCompositing {
    globalAlpha: number;
    globalCompositeOperation: string;
}

interface CanvasDrawImage {
    drawImage(image: CanvasImageSource, dx: number, dy: number): void;
    drawImage(image: CanvasImageSource, dx: number, dy: number, dw: number, dh: number): void;
    drawImage(image: CanvasImageSource, sx: number, sy: number, sw: number, sh: number, dx: number, dy: number, dw: number, dh: number): void;
}

interface CanvasDrawPath {
    beginPath(): void;
    clip(winding?: CanvasWindingRule): void;
    clip(path: Path2D, winding?: CanvasWindingRule): void;
    fill(winding?: CanvasWindingRule): void;
    fill(path: Path2D, winding?: CanvasWindingRule): void;
    isPointInPath(x: number, y: number, winding?: CanvasWindingRule): boolean;
    isPointInPath(path: Path2D, x: number, y: number, winding?: CanvasWindingRule): boolean;
    isPointInStroke(x: number, y: number): boolean;
    isPointInStroke(path: Path2D, x: number, y: number): boolean;
    stroke(): void;
    stroke(path: Path2D): void;
}

interface CanvasFillStrokeStyles {
    fillStyle: string | CanvasGradient | CanvasPattern;
    strokeStyle: string | CanvasGradient | CanvasPattern;
    createConicGradient(angle: number, cx: number, cy: number): CanvasGradient;
    createLinearGradient(x0: number, y0: number, x1: number, y1: number): CanvasGradient;
    createPattern(image: CanvasImageSource, repetition: string | null): CanvasPattern | null;
    createRadialGradient(x0: number, y0: number, r0: number, x1: number, y1: number, r1: number): CanvasGradient;
}

interface CanvasFilters {
    filter: string;
}

interface CanvasGradient {
    addColorStop(offset: number, color: string): void;
}

declare var CanvasGradient: {
    prototype: CanvasGradient;
    new(): CanvasGradient;
    isInstance: IsInstance<CanvasGradient>;
};

interface CanvasImageData {
    createImageData(sw: number, sh: number): ImageData;
    createImageData(imagedata: ImageData): ImageData;
    getImageData(sx: number, sy: number, sw: number, sh: number): ImageData;
    putImageData(imagedata: ImageData, dx: number, dy: number): void;
    putImageData(imagedata: ImageData, dx: number, dy: number, dirtyX: number, dirtyY: number, dirtyWidth: number, dirtyHeight: number): void;
}

interface CanvasImageSmoothing {
    imageSmoothingEnabled: boolean;
}

interface CanvasPathDrawingStyles {
    lineCap: CanvasLineCap;
    lineDashOffset: number;
    lineJoin: CanvasLineJoin;
    lineWidth: number;
    miterLimit: number;
    getLineDash(): number[];
    setLineDash(segments: number[]): void;
}

interface CanvasPathMethods {
    arc(x: number, y: number, radius: number, startAngle: number, endAngle: number, anticlockwise?: boolean): void;
    arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): void;
    bezierCurveTo(cp1x: number, cp1y: number, cp2x: number, cp2y: number, x: number, y: number): void;
    closePath(): void;
    ellipse(x: number, y: number, radiusX: number, radiusY: number, rotation: number, startAngle: number, endAngle: number, anticlockwise?: boolean): void;
    lineTo(x: number, y: number): void;
    moveTo(x: number, y: number): void;
    quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void;
    rect(x: number, y: number, w: number, h: number): void;
    roundRect(x: number, y: number, w: number, h: number, radii?: number | DOMPointInit | (number | DOMPointInit)[]): void;
}

interface CanvasPattern {
    setTransform(matrix?: DOMMatrix2DInit): void;
}

declare var CanvasPattern: {
    prototype: CanvasPattern;
    new(): CanvasPattern;
    isInstance: IsInstance<CanvasPattern>;
};

interface CanvasRect {
    clearRect(x: number, y: number, w: number, h: number): void;
    fillRect(x: number, y: number, w: number, h: number): void;
    strokeRect(x: number, y: number, w: number, h: number): void;
}

interface CanvasRenderingContext2D extends CanvasCompositing, CanvasDrawImage, CanvasDrawPath, CanvasFillStrokeStyles, CanvasFilters, CanvasImageData, CanvasImageSmoothing, CanvasPathDrawingStyles, CanvasPathMethods, CanvasRect, CanvasShadowStyles, CanvasState, CanvasText, CanvasTextDrawingStyles, CanvasTransform, CanvasUserInterface {
    readonly canvas: HTMLCanvasElement | null;
    demote(): void;
    drawWindow(window: Window, x: number, y: number, w: number, h: number, bgColor: string, flags?: number): void;
    getContextAttributes(): CanvasRenderingContext2DSettings;
    readonly DRAWWINDOW_DRAW_CARET: 0x01;
    readonly DRAWWINDOW_DO_NOT_FLUSH: 0x02;
    readonly DRAWWINDOW_DRAW_VIEW: 0x04;
    readonly DRAWWINDOW_USE_WIDGET_LAYERS: 0x08;
    readonly DRAWWINDOW_ASYNC_DECODE_IMAGES: 0x10;
}

declare var CanvasRenderingContext2D: {
    prototype: CanvasRenderingContext2D;
    new(): CanvasRenderingContext2D;
    readonly DRAWWINDOW_DRAW_CARET: 0x01;
    readonly DRAWWINDOW_DO_NOT_FLUSH: 0x02;
    readonly DRAWWINDOW_DRAW_VIEW: 0x04;
    readonly DRAWWINDOW_USE_WIDGET_LAYERS: 0x08;
    readonly DRAWWINDOW_ASYNC_DECODE_IMAGES: 0x10;
    isInstance: IsInstance<CanvasRenderingContext2D>;
};

interface CanvasShadowStyles {
    shadowBlur: number;
    shadowColor: string;
    shadowOffsetX: number;
    shadowOffsetY: number;
}

interface CanvasState {
    isContextLost(): boolean;
    reset(): void;
    restore(): void;
    save(): void;
}

interface CanvasText {
    fillText(text: string, x: number, y: number, maxWidth?: number): void;
    measureText(text: string): TextMetrics;
    strokeText(text: string, x: number, y: number, maxWidth?: number): void;
}

interface CanvasTextDrawingStyles {
    direction: CanvasDirection;
    font: string;
    fontKerning: CanvasFontKerning;
    fontStretch: CanvasFontStretch;
    fontVariantCaps: CanvasFontVariantCaps;
    letterSpacing: string;
    textAlign: CanvasTextAlign;
    textBaseline: CanvasTextBaseline;
    textRendering: CanvasTextRendering;
    wordSpacing: string;
}

interface CanvasTransform {
    getTransform(): DOMMatrix;
    resetTransform(): void;
    rotate(angle: number): void;
    scale(x: number, y: number): void;
    setTransform(a: number, b: number, c: number, d: number, e: number, f: number): void;
    setTransform(transform?: DOMMatrix2DInit): void;
    transform(a: number, b: number, c: number, d: number, e: number, f: number): void;
    translate(x: number, y: number): void;
}

interface CanvasUserInterface {
    drawFocusIfNeeded(element: Element): void;
}

interface CaretPosition {
    readonly offset: number;
    readonly offsetNode: Node | null;
    getClientRect(): DOMRect | null;
}

declare var CaretPosition: {
    prototype: CaretPosition;
    new(): CaretPosition;
    isInstance: IsInstance<CaretPosition>;
};

interface CaretStateChangedEvent extends Event {
    readonly boundingClientRect: DOMRectReadOnly | null;
    readonly caretVisible: boolean;
    readonly caretVisuallyVisible: boolean;
    readonly clientX: number;
    readonly clientY: number;
    readonly collapsed: boolean;
    readonly reason: CaretChangedReason;
    readonly selectedTextContent: string;
    readonly selectionEditable: boolean;
    readonly selectionVisible: boolean;
}

declare var CaretStateChangedEvent: {
    prototype: CaretStateChangedEvent;
    new(type: string, eventInit?: CaretStateChangedEventInit): CaretStateChangedEvent;
    isInstance: IsInstance<CaretStateChangedEvent>;
};

interface ChannelMergerNode extends AudioNode {
}

declare var ChannelMergerNode: {
    prototype: ChannelMergerNode;
    new(context: BaseAudioContext, options?: ChannelMergerOptions): ChannelMergerNode;
    isInstance: IsInstance<ChannelMergerNode>;
};

interface ChannelSplitterNode extends AudioNode {
}

declare var ChannelSplitterNode: {
    prototype: ChannelSplitterNode;
    new(context: BaseAudioContext, options?: ChannelSplitterOptions): ChannelSplitterNode;
    isInstance: IsInstance<ChannelSplitterNode>;
};

interface ChannelWrapperEventMap {
    "error": Event;
    "start": Event;
    "stop": Event;
}

interface ChannelWrapper extends EventTarget {
    readonly browserElement: nsISupports | null;
    readonly canModify: boolean;
    channel: MozChannel | null;
    contentType: string;
    readonly documentURI: URI | null;
    readonly documentURL: string | null;
    readonly errorString: string | null;
    readonly finalURI: URI | null;
    readonly finalURL: string;
    readonly frameAncestors: MozFrameAncestorInfo[] | null;
    readonly frameId: number;
    readonly id: number;
    readonly isServiceWorkerScript: boolean;
    readonly isSystemLoad: boolean;
    readonly loadInfo: LoadInfo | null;
    readonly method: string;
    onerror: ((this: ChannelWrapper, ev: Event) => any) | null;
    onstart: ((this: ChannelWrapper, ev: Event) => any) | null;
    onstop: ((this: ChannelWrapper, ev: Event) => any) | null;
    readonly originURI: URI | null;
    readonly originURL: string | null;
    readonly parentFrameId: number;
    readonly proxyInfo: MozProxyInfo | null;
    readonly remoteAddress: string | null;
    readonly requestSize: number;
    readonly responseSize: number;
    readonly statusCode: number;
    readonly statusLine: string;
    readonly suspended: boolean;
    readonly thirdParty: boolean;
    readonly type: MozContentPolicyType;
    readonly urlClassification: MozUrlClassification | null;
    cancel(result: number, reason?: number): void;
    errorCheck(): void;
    getRequestHeader(header: string): string | null;
    getRequestHeaders(): MozHTTPHeader[];
    getResponseHeaders(): MozHTTPHeader[];
    matches(filter?: MozRequestFilter, extension?: WebExtensionPolicy | null, options?: MozRequestMatchOptions): boolean;
    redirectTo(url: URI): void;
    registerTraceableChannel(extension: WebExtensionPolicy, remoteTab: RemoteTab | null): void;
    resume(): void;
    setRequestHeader(header: string, value: string, merge?: boolean): void;
    setResponseHeader(header: string, value: string, merge?: boolean): void;
    suspend(profileMarkerText: string): void;
    upgradeToSecure(): void;
    addEventListener<K extends keyof ChannelWrapperEventMap>(type: K, listener: (this: ChannelWrapper, ev: ChannelWrapperEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ChannelWrapperEventMap>(type: K, listener: (this: ChannelWrapper, ev: ChannelWrapperEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ChannelWrapper: {
    prototype: ChannelWrapper;
    new(): ChannelWrapper;
    isInstance: IsInstance<ChannelWrapper>;
    get(channel: MozChannel): ChannelWrapper;
    getRegisteredChannel(aChannelId: number, extension: WebExtensionPolicy, remoteTab: RemoteTab | null): ChannelWrapper | null;
};

interface CharacterData extends Node, ChildNode, NonDocumentTypeChildNode {
    data: string;
    readonly length: number;
    appendData(data: string): void;
    deleteData(offset: number, count: number): void;
    insertData(offset: number, data: string): void;
    replaceData(offset: number, count: number, data: string): void;
    substringData(offset: number, count: number): string;
}

declare var CharacterData: {
    prototype: CharacterData;
    new(): CharacterData;
    isInstance: IsInstance<CharacterData>;
};

interface CheckerboardReportService {
    flushActiveReports(): void;
    getReports(): CheckerboardReport[];
    isRecordingEnabled(): boolean;
    setRecordingEnabled(aEnabled: boolean): void;
}

declare var CheckerboardReportService: {
    prototype: CheckerboardReportService;
    new(): CheckerboardReportService;
    isInstance: IsInstance<CheckerboardReportService>;
};

interface ChildNode {
    after(...nodes: (Node | string)[]): void;
    before(...nodes: (Node | string)[]): void;
    remove(): void;
    replaceWith(...nodes: (Node | string)[]): void;
}

interface ChildProcessMessageManager extends SyncMessageSender {
}

declare var ChildProcessMessageManager: {
    prototype: ChildProcessMessageManager;
    new(): ChildProcessMessageManager;
    isInstance: IsInstance<ChildProcessMessageManager>;
};

interface ChildSHistory {
    readonly count: number;
    readonly index: number;
    readonly legacySHistory: nsISHistory;
    canGo(aOffset: number): boolean;
    go(aOffset: number, aRequireUserInteraction?: boolean, aUserActivation?: boolean): void;
    reload(aReloadFlags: number): void;
}

declare var ChildSHistory: {
    prototype: ChildSHistory;
    new(): ChildSHistory;
    isInstance: IsInstance<ChildSHistory>;
};

interface ChromeMessageBroadcaster extends MessageBroadcaster, FrameScriptLoader {
}

declare var ChromeMessageBroadcaster: {
    prototype: ChromeMessageBroadcaster;
    new(): ChromeMessageBroadcaster;
    isInstance: IsInstance<ChromeMessageBroadcaster>;
};

interface ChromeMessageSender extends MessageSender, FrameScriptLoader {
}

declare var ChromeMessageSender: {
    prototype: ChromeMessageSender;
    new(): ChromeMessageSender;
    isInstance: IsInstance<ChromeMessageSender>;
};

interface ChromeNodeList extends NodeList {
    append(aNode: Node): void;
    remove(aNode: Node): void;
}

declare var ChromeNodeList: {
    prototype: ChromeNodeList;
    new(): ChromeNodeList;
    isInstance: IsInstance<ChromeNodeList>;
};

interface ChromeWorker extends Worker {
    addEventListener<K extends keyof WorkerEventMap>(type: K, listener: (this: ChromeWorker, ev: WorkerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WorkerEventMap>(type: K, listener: (this: ChromeWorker, ev: WorkerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ChromeWorker: {
    prototype: ChromeWorker;
    new(scriptURL: string | URL, options?: WorkerOptions): ChromeWorker;
    isInstance: IsInstance<ChromeWorker>;
};

/** Available only in secure contexts. */
interface Clipboard extends EventTarget {
    read(): Promise<ClipboardItems>;
    readText(): Promise<string>;
    write(data: ClipboardItems): Promise<void>;
    writeText(data: string): Promise<void>;
}

declare var Clipboard: {
    prototype: Clipboard;
    new(): Clipboard;
    isInstance: IsInstance<Clipboard>;
};

interface ClipboardEvent extends Event {
    readonly clipboardData: DataTransfer | null;
}

declare var ClipboardEvent: {
    prototype: ClipboardEvent;
    new(type: string, eventInitDict?: ClipboardEventInit): ClipboardEvent;
    isInstance: IsInstance<ClipboardEvent>;
};

/** Available only in secure contexts. */
interface ClipboardItem {
    readonly presentationStyle: PresentationStyle;
    readonly types: string[];
    getType(type: string): Promise<Blob>;
}

declare var ClipboardItem: {
    prototype: ClipboardItem;
    new(items: Record<string, ClipboardItemDataType | PromiseLike<ClipboardItemDataType>>, options?: ClipboardItemOptions): ClipboardItem;
    isInstance: IsInstance<ClipboardItem>;
    supports(type: string): boolean;
};

interface ClonedErrorHolder {
}

declare var ClonedErrorHolder: {
    prototype: ClonedErrorHolder;
    new(aError: any): ClonedErrorHolder;
    isInstance: IsInstance<ClonedErrorHolder>;
};

interface CloseEvent extends Event {
    readonly code: number;
    readonly reason: string;
    readonly wasClean: boolean;
}

declare var CloseEvent: {
    prototype: CloseEvent;
    new(type: string, eventInitDict?: CloseEventInit): CloseEvent;
    isInstance: IsInstance<CloseEvent>;
};

interface CommandEvent extends Event {
    readonly command: string | null;
}

declare var CommandEvent: {
    prototype: CommandEvent;
    new(): CommandEvent;
    isInstance: IsInstance<CommandEvent>;
};

interface Comment extends CharacterData {
}

declare var Comment: {
    prototype: Comment;
    new(data?: string): Comment;
    isInstance: IsInstance<Comment>;
};

interface CompositionEvent extends UIEvent {
    readonly data: string | null;
    readonly locale: string;
    readonly ranges: TextClause[];
    initCompositionEvent(typeArg: string, canBubbleArg?: boolean, cancelableArg?: boolean, viewArg?: Window | null, dataArg?: string | null, localeArg?: string): void;
}

declare var CompositionEvent: {
    prototype: CompositionEvent;
    new(type: string, eventInitDict?: CompositionEventInit): CompositionEvent;
    isInstance: IsInstance<CompositionEvent>;
};

interface CompressionStream extends GenericTransformStream {
}

declare var CompressionStream: {
    prototype: CompressionStream;
    new(format: CompressionFormat): CompressionStream;
    isInstance: IsInstance<CompressionStream>;
};

interface ConsoleInstance {
    assert(condition?: boolean, ...data: any[]): void;
    clear(): void;
    count(label?: string): void;
    countReset(label?: string): void;
    debug(...data: any[]): void;
    dir(...data: any[]): void;
    dirxml(...data: any[]): void;
    error(...data: any[]): void;
    exception(...data: any[]): void;
    group(...data: any[]): void;
    groupCollapsed(...data: any[]): void;
    groupEnd(): void;
    info(...data: any[]): void;
    log(...data: any[]): void;
    profile(...data: any[]): void;
    profileEnd(...data: any[]): void;
    reportForServiceWorkerScope(scope: string, message: string, filename: string, lineNumber: number, columnNumber: number, level: ConsoleLevel): void;
    shouldLog(level: ConsoleLogLevel): boolean;
    table(...data: any[]): void;
    time(label?: string): void;
    timeEnd(label?: string): void;
    timeLog(label?: string, ...data: any[]): void;
    timeStamp(data?: any): void;
    trace(...data: any[]): void;
    warn(...data: any[]): void;
}

declare var ConsoleInstance: {
    prototype: ConsoleInstance;
    new(): ConsoleInstance;
    isInstance: IsInstance<ConsoleInstance>;
};

interface ConstantSourceNode extends AudioScheduledSourceNode {
    readonly offset: AudioParam;
    addEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: ConstantSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: ConstantSourceNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ConstantSourceNode: {
    prototype: ConstantSourceNode;
    new(context: BaseAudioContext, options?: ConstantSourceOptions): ConstantSourceNode;
    isInstance: IsInstance<ConstantSourceNode>;
};

interface ContentFrameMessageManager extends EventTarget, MessageListenerManagerMixin, MessageManagerGlobal, MessageSenderMixin, SyncMessageSenderMixin {
    readonly content: WindowProxy | null;
    readonly docShell: nsIDocShell | null;
    readonly tabEventTarget: nsIEventTarget | null;
}

declare var ContentFrameMessageManager: {
    prototype: ContentFrameMessageManager;
    new(): ContentFrameMessageManager;
    isInstance: IsInstance<ContentFrameMessageManager>;
};

interface ContentProcessMessageManager extends MessageListenerManagerMixin, MessageManagerGlobal, MessageSenderMixin, SyncMessageSenderMixin {
    readonly initialProcessData: any;
    readonly sharedData: MozSharedMap | null;
}

declare var ContentProcessMessageManager: {
    prototype: ContentProcessMessageManager;
    new(): ContentProcessMessageManager;
    isInstance: IsInstance<ContentProcessMessageManager>;
};

interface ContentSecurityPolicy {
}

interface ContentVisibilityAutoStateChangeEvent extends Event {
    readonly skipped: boolean;
}

declare var ContentVisibilityAutoStateChangeEvent: {
    prototype: ContentVisibilityAutoStateChangeEvent;
    new(type: string, eventInitDict?: ContentVisibilityAutoStateChangeEventInit): ContentVisibilityAutoStateChangeEvent;
    isInstance: IsInstance<ContentVisibilityAutoStateChangeEvent>;
};

interface ConvolverNode extends AudioNode, AudioNodePassThrough {
    buffer: AudioBuffer | null;
    normalize: boolean;
}

declare var ConvolverNode: {
    prototype: ConvolverNode;
    new(context: BaseAudioContext, options?: ConvolverOptions): ConvolverNode;
    isInstance: IsInstance<ConvolverNode>;
};

interface Cookie {
}

interface CountQueuingStrategy {
    readonly highWaterMark: number;
    readonly size: Function;
}

declare var CountQueuingStrategy: {
    prototype: CountQueuingStrategy;
    new(init: QueuingStrategyInit): CountQueuingStrategy;
    isInstance: IsInstance<CountQueuingStrategy>;
};

interface CreateOfferRequest {
    readonly callID: string;
    readonly innerWindowID: number;
    readonly isSecure: boolean;
    readonly windowID: number;
}

declare var CreateOfferRequest: {
    prototype: CreateOfferRequest;
    new(): CreateOfferRequest;
    isInstance: IsInstance<CreateOfferRequest>;
};

/** Available only in secure contexts. */
interface Credential {
    readonly id: string;
    readonly type: string;
}

declare var Credential: {
    prototype: Credential;
    new(): Credential;
    isInstance: IsInstance<Credential>;
};

/** Available only in secure contexts. */
interface CredentialsContainer {
    create(options?: CredentialCreationOptions): Promise<Credential | null>;
    get(options?: CredentialRequestOptions): Promise<Credential | null>;
    preventSilentAccess(): Promise<void>;
    store(credential: Credential): Promise<Credential>;
}

declare var CredentialsContainer: {
    prototype: CredentialsContainer;
    new(): CredentialsContainer;
    isInstance: IsInstance<CredentialsContainer>;
};

interface Crypto {
    /** Available only in secure contexts. */
    readonly subtle: SubtleCrypto;
    getRandomValues(array: ArrayBufferView): ArrayBufferView;
    /** Available only in secure contexts. */
    randomUUID(): string;
}

declare var Crypto: {
    prototype: Crypto;
    new(): Crypto;
    isInstance: IsInstance<Crypto>;
};

/** Available only in secure contexts. */
interface CryptoKey {
    readonly algorithm: any;
    readonly extractable: boolean;
    readonly type: KeyType;
    readonly usages: KeyUsage[];
}

declare var CryptoKey: {
    prototype: CryptoKey;
    new(): CryptoKey;
    isInstance: IsInstance<CryptoKey>;
};

interface CustomElementRegistry {
    define(name: string, constructor: CustomElementConstructor, options?: ElementDefinitionOptions): void;
    get(name: string): CustomElementConstructor | undefined;
    getName(constructor: CustomElementConstructor): string | null;
    setElementCreationCallback(name: string, callback: CustomElementCreationCallback): void;
    upgrade(root: Node): void;
    whenDefined(name: string): Promise<CustomElementConstructor>;
}

declare var CustomElementRegistry: {
    prototype: CustomElementRegistry;
    new(): CustomElementRegistry;
    isInstance: IsInstance<CustomElementRegistry>;
};

interface CustomEvent extends Event {
    readonly detail: any;
    initCustomEvent(type: string, canBubble?: boolean, cancelable?: boolean, detail?: any): void;
}

declare var CustomEvent: {
    prototype: CustomEvent;
    new(type: string, eventInitDict?: CustomEventInit): CustomEvent;
    isInstance: IsInstance<CustomEvent>;
};

interface CustomStateSet {
    forEach(callbackfn: (value: string, key: string, parent: CustomStateSet) => void, thisArg?: any): void;
}

declare var CustomStateSet: {
    prototype: CustomStateSet;
    new(): CustomStateSet;
    isInstance: IsInstance<CustomStateSet>;
};

interface DOMApplication {
}

interface DOMException extends ExceptionMembers {
    readonly code: number;
    readonly message: string;
    readonly name: string;
    readonly INDEX_SIZE_ERR: 1;
    readonly DOMSTRING_SIZE_ERR: 2;
    readonly HIERARCHY_REQUEST_ERR: 3;
    readonly WRONG_DOCUMENT_ERR: 4;
    readonly INVALID_CHARACTER_ERR: 5;
    readonly NO_DATA_ALLOWED_ERR: 6;
    readonly NO_MODIFICATION_ALLOWED_ERR: 7;
    readonly NOT_FOUND_ERR: 8;
    readonly NOT_SUPPORTED_ERR: 9;
    readonly INUSE_ATTRIBUTE_ERR: 10;
    readonly INVALID_STATE_ERR: 11;
    readonly SYNTAX_ERR: 12;
    readonly INVALID_MODIFICATION_ERR: 13;
    readonly NAMESPACE_ERR: 14;
    readonly INVALID_ACCESS_ERR: 15;
    readonly VALIDATION_ERR: 16;
    readonly TYPE_MISMATCH_ERR: 17;
    readonly SECURITY_ERR: 18;
    readonly NETWORK_ERR: 19;
    readonly ABORT_ERR: 20;
    readonly URL_MISMATCH_ERR: 21;
    readonly QUOTA_EXCEEDED_ERR: 22;
    readonly TIMEOUT_ERR: 23;
    readonly INVALID_NODE_TYPE_ERR: 24;
    readonly DATA_CLONE_ERR: 25;
}

declare var DOMException: {
    prototype: DOMException;
    new(message?: string, name?: string): DOMException;
    readonly INDEX_SIZE_ERR: 1;
    readonly DOMSTRING_SIZE_ERR: 2;
    readonly HIERARCHY_REQUEST_ERR: 3;
    readonly WRONG_DOCUMENT_ERR: 4;
    readonly INVALID_CHARACTER_ERR: 5;
    readonly NO_DATA_ALLOWED_ERR: 6;
    readonly NO_MODIFICATION_ALLOWED_ERR: 7;
    readonly NOT_FOUND_ERR: 8;
    readonly NOT_SUPPORTED_ERR: 9;
    readonly INUSE_ATTRIBUTE_ERR: 10;
    readonly INVALID_STATE_ERR: 11;
    readonly SYNTAX_ERR: 12;
    readonly INVALID_MODIFICATION_ERR: 13;
    readonly NAMESPACE_ERR: 14;
    readonly INVALID_ACCESS_ERR: 15;
    readonly VALIDATION_ERR: 16;
    readonly TYPE_MISMATCH_ERR: 17;
    readonly SECURITY_ERR: 18;
    readonly NETWORK_ERR: 19;
    readonly ABORT_ERR: 20;
    readonly URL_MISMATCH_ERR: 21;
    readonly QUOTA_EXCEEDED_ERR: 22;
    readonly TIMEOUT_ERR: 23;
    readonly INVALID_NODE_TYPE_ERR: 24;
    readonly DATA_CLONE_ERR: 25;
    isInstance: IsInstance<DOMException>;
};

interface DOMImplementation {
    createDocument(namespace: string | null, qualifiedName: string | null, doctype?: DocumentType | null): Document;
    createDocumentType(qualifiedName: string, publicId: string, systemId: string): DocumentType;
    createHTMLDocument(title?: string): Document;
    hasFeature(): boolean;
}

declare var DOMImplementation: {
    prototype: DOMImplementation;
    new(): DOMImplementation;
    isInstance: IsInstance<DOMImplementation>;
};

interface DOMLocalization extends Localization {
    connectRoot(aElement: Node): void;
    disconnectRoot(aElement: Node): void;
    getAttributes(aElement: Element): L10nIdArgs;
    pauseObserving(): void;
    resumeObserving(): void;
    setArgs(aElement: Element, aArgs?: any): void;
    setAttributes(aElement: Element, aId: string, aArgs?: any): void;
    translateElements(aElements: Element[]): Promise<void>;
    translateFragment(aNode: Node): Promise<any>;
    translateRoots(): Promise<void>;
}

declare var DOMLocalization: {
    prototype: DOMLocalization;
    new(aResourceIds: L10nResourceId[], aSync?: boolean, aRegistry?: L10nRegistry, aLocales?: string[]): DOMLocalization;
    isInstance: IsInstance<DOMLocalization>;
};

interface DOMMatrix extends DOMMatrixReadOnly {
    a: number;
    b: number;
    c: number;
    d: number;
    e: number;
    f: number;
    m11: number;
    m12: number;
    m13: number;
    m14: number;
    m21: number;
    m22: number;
    m23: number;
    m24: number;
    m31: number;
    m32: number;
    m33: number;
    m34: number;
    m41: number;
    m42: number;
    m43: number;
    m44: number;
    invertSelf(): DOMMatrix;
    multiplySelf(other?: DOMMatrixInit): DOMMatrix;
    preMultiplySelf(other?: DOMMatrixInit): DOMMatrix;
    rotateAxisAngleSelf(x?: number, y?: number, z?: number, angle?: number): DOMMatrix;
    rotateFromVectorSelf(x?: number, y?: number): DOMMatrix;
    rotateSelf(rotX?: number, rotY?: number, rotZ?: number): DOMMatrix;
    scale3dSelf(scale?: number, originX?: number, originY?: number, originZ?: number): DOMMatrix;
    scaleSelf(scaleX?: number, scaleY?: number, scaleZ?: number, originX?: number, originY?: number, originZ?: number): DOMMatrix;
    setMatrixValue(transformList: string): DOMMatrix;
    skewXSelf(sx?: number): DOMMatrix;
    skewYSelf(sy?: number): DOMMatrix;
    translateSelf(tx?: number, ty?: number, tz?: number): DOMMatrix;
}

declare var DOMMatrix: {
    prototype: DOMMatrix;
    new(init?: string | number[] | DOMMatrixReadOnly): DOMMatrix;
    isInstance: IsInstance<DOMMatrix>;
    fromFloat32Array(array32: Float32Array): DOMMatrix;
    fromFloat64Array(array64: Float64Array): DOMMatrix;
    fromMatrix(other?: DOMMatrixInit): DOMMatrix;
};

type WebKitCSSMatrix = DOMMatrix;
declare var WebKitCSSMatrix: typeof DOMMatrix;

interface DOMMatrixReadOnly {
    readonly a: number;
    readonly b: number;
    readonly c: number;
    readonly d: number;
    readonly e: number;
    readonly f: number;
    readonly is2D: boolean;
    readonly isIdentity: boolean;
    readonly m11: number;
    readonly m12: number;
    readonly m13: number;
    readonly m14: number;
    readonly m21: number;
    readonly m22: number;
    readonly m23: number;
    readonly m24: number;
    readonly m31: number;
    readonly m32: number;
    readonly m33: number;
    readonly m34: number;
    readonly m41: number;
    readonly m42: number;
    readonly m43: number;
    readonly m44: number;
    flipX(): DOMMatrix;
    flipY(): DOMMatrix;
    inverse(): DOMMatrix;
    multiply(other?: DOMMatrixInit): DOMMatrix;
    rotate(rotX?: number, rotY?: number, rotZ?: number): DOMMatrix;
    rotateAxisAngle(x?: number, y?: number, z?: number, angle?: number): DOMMatrix;
    rotateFromVector(x?: number, y?: number): DOMMatrix;
    scale(scaleX?: number, scaleY?: number, scaleZ?: number, originX?: number, originY?: number, originZ?: number): DOMMatrix;
    scale3d(scale?: number, originX?: number, originY?: number, originZ?: number): DOMMatrix;
    scaleNonUniform(scaleX?: number, scaleY?: number): DOMMatrix;
    skewX(sx?: number): DOMMatrix;
    skewY(sy?: number): DOMMatrix;
    toFloat32Array(): Float32Array;
    toFloat64Array(): Float64Array;
    toJSON(): any;
    transformPoint(point?: DOMPointInit): DOMPoint;
    translate(tx?: number, ty?: number, tz?: number): DOMMatrix;
    toString(): string;
}

declare var DOMMatrixReadOnly: {
    prototype: DOMMatrixReadOnly;
    new(init?: string | number[] | DOMMatrixReadOnly): DOMMatrixReadOnly;
    isInstance: IsInstance<DOMMatrixReadOnly>;
    fromFloat32Array(array32: Float32Array): DOMMatrixReadOnly;
    fromFloat64Array(array64: Float64Array): DOMMatrixReadOnly;
    fromMatrix(other?: DOMMatrixInit): DOMMatrixReadOnly;
};

interface DOMParser {
    forceEnableDTD(): void;
    forceEnableXULXBL(): void;
    parseFromBuffer(buf: number[], type: SupportedType): Document;
    parseFromBuffer(buf: Uint8Array, type: SupportedType): Document;
    parseFromSafeString(str: string, type: SupportedType): Document;
    parseFromStream(stream: InputStream, charset: string | null, contentLength: number, type: SupportedType): Document;
    parseFromString(str: string, type: SupportedType): Document;
}

declare var DOMParser: {
    prototype: DOMParser;
    new(): DOMParser;
    isInstance: IsInstance<DOMParser>;
};

interface DOMPoint extends DOMPointReadOnly {
    w: number;
    x: number;
    y: number;
    z: number;
}

declare var DOMPoint: {
    prototype: DOMPoint;
    new(x?: number, y?: number, z?: number, w?: number): DOMPoint;
    isInstance: IsInstance<DOMPoint>;
    fromPoint(other?: DOMPointInit): DOMPoint;
};

interface DOMPointReadOnly {
    readonly w: number;
    readonly x: number;
    readonly y: number;
    readonly z: number;
    matrixTransform(matrix?: DOMMatrixInit): DOMPoint;
    toJSON(): any;
}

declare var DOMPointReadOnly: {
    prototype: DOMPointReadOnly;
    new(x?: number, y?: number, z?: number, w?: number): DOMPointReadOnly;
    isInstance: IsInstance<DOMPointReadOnly>;
    fromPoint(other?: DOMPointInit): DOMPointReadOnly;
};

interface DOMQuad {
    readonly p1: DOMPoint;
    readonly p2: DOMPoint;
    readonly p3: DOMPoint;
    readonly p4: DOMPoint;
    getBounds(): DOMRectReadOnly;
    toJSON(): any;
}

declare var DOMQuad: {
    prototype: DOMQuad;
    new(p1?: DOMPointInit, p2?: DOMPointInit, p3?: DOMPointInit, p4?: DOMPointInit): DOMQuad;
    new(rect: DOMRectReadOnly): DOMQuad;
    isInstance: IsInstance<DOMQuad>;
    fromQuad(other?: DOMQuadInit): DOMQuad;
    fromRect(other?: DOMRectInit): DOMQuad;
};

interface DOMRect extends DOMRectReadOnly {
    height: number;
    width: number;
    x: number;
    y: number;
}

declare var DOMRect: {
    prototype: DOMRect;
    new(x?: number, y?: number, width?: number, height?: number): DOMRect;
    isInstance: IsInstance<DOMRect>;
    fromRect(other?: DOMRectInit): DOMRect;
};

interface DOMRectList {
    readonly length: number;
    item(index: number): DOMRect | null;
    [index: number]: DOMRect;
}

declare var DOMRectList: {
    prototype: DOMRectList;
    new(): DOMRectList;
    isInstance: IsInstance<DOMRectList>;
};

interface DOMRectReadOnly {
    readonly bottom: number;
    readonly height: number;
    readonly left: number;
    readonly right: number;
    readonly top: number;
    readonly width: number;
    readonly x: number;
    readonly y: number;
    toJSON(): any;
}

declare var DOMRectReadOnly: {
    prototype: DOMRectReadOnly;
    new(x?: number, y?: number, width?: number, height?: number): DOMRectReadOnly;
    isInstance: IsInstance<DOMRectReadOnly>;
    fromRect(other?: DOMRectInit): DOMRectReadOnly;
};

interface DOMStringList {
    readonly length: number;
    contains(string: string): boolean;
    item(index: number): string | null;
    [index: number]: string;
}

declare var DOMStringList: {
    prototype: DOMStringList;
    new(): DOMStringList;
    isInstance: IsInstance<DOMStringList>;
};

interface DOMStringMap {
}

declare var DOMStringMap: {
    prototype: DOMStringMap;
    new(): DOMStringMap;
    isInstance: IsInstance<DOMStringMap>;
};

interface DOMTokenList {
    readonly length: number;
    value: string;
    toString(): string;
    add(...tokens: string[]): void;
    contains(token: string): boolean;
    item(index: number): string | null;
    remove(...tokens: string[]): void;
    replace(token: string, newToken: string): boolean;
    supports(token: string): boolean;
    toggle(token: string, force?: boolean): boolean;
    forEach(callbackfn: (value: string | null, key: number, parent: DOMTokenList) => void, thisArg?: any): void;
    [index: number]: string;
}

declare var DOMTokenList: {
    prototype: DOMTokenList;
    new(): DOMTokenList;
    isInstance: IsInstance<DOMTokenList>;
};

interface DataTransfer {
    dropEffect: string;
    effectAllowed: string;
    readonly files: FileList | null;
    readonly items: DataTransferItemList;
    readonly mozCSP: ContentSecurityPolicy | null;
    mozCursor: string;
    readonly mozItemCount: number;
    mozShowFailAnimation: boolean;
    readonly mozSourceNode: Node | null;
    readonly mozTriggeringPrincipalURISpec: string;
    readonly mozUserCancelled: boolean;
    readonly sourceTopWindowContext: WindowContext | null;
    readonly types: string[];
    addElement(element: Element): void;
    clearData(format?: string): void;
    getData(format: string): string;
    mozClearDataAt(format: string, index: number): void;
    mozCloneForEvent(event: string): DataTransfer;
    mozGetDataAt(format: string, index: number): any;
    mozSetDataAt(format: string, data: any, index: number): void;
    mozTypesAt(index: number): DOMStringList;
    setData(format: string, data: string): void;
    setDragImage(image: Element, x: number, y: number): void;
    updateDragImage(image: Element, x: number, y: number): void;
}

declare var DataTransfer: {
    prototype: DataTransfer;
    new(): DataTransfer;
    isInstance: IsInstance<DataTransfer>;
};

interface DataTransferItem {
    readonly kind: string;
    readonly type: string;
    getAsFile(): File | null;
    getAsString(callback: FunctionStringCallback | null): void;
    webkitGetAsEntry(): FileSystemEntry | null;
}

declare var DataTransferItem: {
    prototype: DataTransferItem;
    new(): DataTransferItem;
    isInstance: IsInstance<DataTransferItem>;
};

interface DataTransferItemList {
    readonly length: number;
    add(data: string, type: string): DataTransferItem | null;
    add(data: File): DataTransferItem | null;
    clear(): void;
    remove(index: number): void;
    [index: number]: DataTransferItem;
}

declare var DataTransferItemList: {
    prototype: DataTransferItemList;
    new(): DataTransferItemList;
    isInstance: IsInstance<DataTransferItemList>;
};

interface DebuggerNotification {
    readonly global: any;
    readonly type: DebuggerNotificationType;
}

declare var DebuggerNotification: {
    prototype: DebuggerNotification;
    new(): DebuggerNotification;
    isInstance: IsInstance<DebuggerNotification>;
};

interface DebuggerNotificationObserver {
    addListener(handler: DebuggerNotificationCallback): boolean;
    connect(global: any): boolean;
    disconnect(global: any): boolean;
    removeListener(handler: DebuggerNotificationCallback): boolean;
}

declare var DebuggerNotificationObserver: {
    prototype: DebuggerNotificationObserver;
    new(): DebuggerNotificationObserver;
    isInstance: IsInstance<DebuggerNotificationObserver>;
};

interface DecompressionStream extends GenericTransformStream {
}

declare var DecompressionStream: {
    prototype: DecompressionStream;
    new(format: CompressionFormat): DecompressionStream;
    isInstance: IsInstance<DecompressionStream>;
};

interface DelayNode extends AudioNode, AudioNodePassThrough {
    readonly delayTime: AudioParam;
}

declare var DelayNode: {
    prototype: DelayNode;
    new(context: BaseAudioContext, options?: DelayOptions): DelayNode;
    isInstance: IsInstance<DelayNode>;
};

interface DeprecationReportBody extends ReportBody {
    readonly anticipatedRemoval: DOMTimeStamp | null;
    readonly columnNumber: number | null;
    readonly id: string;
    readonly lineNumber: number | null;
    readonly message: string;
    readonly sourceFile: string | null;
}

declare var DeprecationReportBody: {
    prototype: DeprecationReportBody;
    new(): DeprecationReportBody;
    isInstance: IsInstance<DeprecationReportBody>;
};

interface DeviceAcceleration {
    readonly x: number | null;
    readonly y: number | null;
    readonly z: number | null;
}

interface DeviceLightEvent extends Event {
    readonly value: number;
}

declare var DeviceLightEvent: {
    prototype: DeviceLightEvent;
    new(type: string, eventInitDict?: DeviceLightEventInit): DeviceLightEvent;
    isInstance: IsInstance<DeviceLightEvent>;
};

interface DeviceMotionEvent extends Event {
    readonly acceleration: DeviceAcceleration | null;
    readonly accelerationIncludingGravity: DeviceAcceleration | null;
    readonly interval: number | null;
    readonly rotationRate: DeviceRotationRate | null;
    initDeviceMotionEvent(type: string, canBubble?: boolean, cancelable?: boolean, acceleration?: DeviceAccelerationInit, accelerationIncludingGravity?: DeviceAccelerationInit, rotationRate?: DeviceRotationRateInit, interval?: number | null): void;
}

declare var DeviceMotionEvent: {
    prototype: DeviceMotionEvent;
    new(type: string, eventInitDict?: DeviceMotionEventInit): DeviceMotionEvent;
    isInstance: IsInstance<DeviceMotionEvent>;
};

interface DeviceOrientationEvent extends Event {
    readonly absolute: boolean;
    readonly alpha: number | null;
    readonly beta: number | null;
    readonly gamma: number | null;
    initDeviceOrientationEvent(type: string, canBubble?: boolean, cancelable?: boolean, alpha?: number | null, beta?: number | null, gamma?: number | null, absolute?: boolean): void;
}

declare var DeviceOrientationEvent: {
    prototype: DeviceOrientationEvent;
    new(type: string, eventInitDict?: DeviceOrientationEventInit): DeviceOrientationEvent;
    isInstance: IsInstance<DeviceOrientationEvent>;
};

interface DeviceRotationRate {
    readonly alpha: number | null;
    readonly beta: number | null;
    readonly gamma: number | null;
}

interface Directory {
    readonly name: string;
    readonly path: string;
    getFiles(recursiveFlag?: boolean): Promise<File[]>;
    getFilesAndDirectories(): Promise<(File | Directory)[]>;
}

declare var Directory: {
    prototype: Directory;
    new(path: string): Directory;
    isInstance: IsInstance<Directory>;
};

interface DocumentEventMap extends GlobalEventHandlersEventMap, OnErrorEventHandlerForNodesEventMap, TouchEventHandlersEventMap {
    "afterscriptexecute": Event;
    "beforescriptexecute": Event;
    "fullscreenchange": Event;
    "fullscreenerror": Event;
    "pointerlockchange": Event;
    "pointerlockerror": Event;
    "readystatechange": Event;
    "visibilitychange": Event;
}

interface Document extends Node, DocumentOrShadowRoot, FontFaceSource, GeometryUtils, GlobalEventHandlers, NonElementParentNode, OnErrorEventHandlerForNodes, ParentNode, TouchEventHandlers, XPathEvaluatorMixin {
    readonly URL: string;
    alinkColor: string;
    readonly all: HTMLAllCollection;
    readonly anchors: HTMLCollection;
    readonly applets: HTMLCollection;
    bgColor: string;
    readonly blockedNodeByClassifierCount: number;
    readonly blockedNodesByClassifier: NodeList;
    body: HTMLElement | null;
    readonly characterSet: string;
    readonly charset: string;
    readonly commandDispatcher: XULCommandDispatcher | null;
    readonly compatMode: string;
    readonly contentLanguage: string;
    readonly contentType: string;
    cookie: string;
    readonly cookieJarSettings: nsICookieJarSettings;
    readonly csp: ContentSecurityPolicy | null;
    readonly cspJSON: string;
    readonly currentScript: Element | null;
    readonly defaultView: WindowProxy | null;
    designMode: string;
    devToolsAnonymousAndShadowEventsEnabled: boolean;
    devToolsWatchingDOMMutations: boolean;
    dir: string;
    readonly doctype: DocumentType | null;
    readonly documentElement: Element | null;
    readonly documentLoadGroup: nsILoadGroup | null;
    readonly documentReadyForIdle: Promise<undefined>;
    readonly documentURI: string;
    readonly documentURIObject: URI | null;
    domain: string;
    readonly effectiveStoragePrincipal: Principal;
    readonly embeds: HTMLCollection;
    readonly featurePolicy: FeaturePolicy;
    fgColor: string;
    readonly forms: HTMLCollection;
    readonly fragmentDirective: FragmentDirective;
    readonly fullscreen: boolean;
    readonly fullscreenEnabled: boolean;
    readonly hasBeenUserGestureActivated: boolean;
    readonly hasPendingL10nMutations: boolean;
    readonly hasValidTransientUserGestureActivation: boolean;
    readonly head: HTMLHeadElement | null;
    readonly hidden: boolean;
    readonly images: HTMLCollection;
    readonly implementation: DOMImplementation;
    readonly inputEncoding: string;
    readonly isInitialDocument: boolean;
    readonly isSrcdocDocument: boolean;
    readonly l10n: DocumentL10n | null;
    readonly lastModified: string;
    readonly lastStyleSheetSet: string | null;
    readonly lastUserGestureTimeStamp: DOMHighResTimeStamp;
    linkColor: string;
    readonly links: HTMLCollection;
    readonly loadedFromPrototype: boolean;
    readonly location: Location | null;
    readonly mozDocumentURIIfNotForErrorPages: URI | null;
    readonly mozFullScreen: boolean;
    readonly mozFullScreenEnabled: boolean;
    readonly mozSyntheticDocument: boolean;
    onafterscriptexecute: ((this: Document, ev: Event) => any) | null;
    onbeforescriptexecute: ((this: Document, ev: Event) => any) | null;
    onfullscreenchange: ((this: Document, ev: Event) => any) | null;
    onfullscreenerror: ((this: Document, ev: Event) => any) | null;
    onpointerlockchange: ((this: Document, ev: Event) => any) | null;
    onpointerlockerror: ((this: Document, ev: Event) => any) | null;
    onreadystatechange: ((this: Document, ev: Event) => any) | null;
    onvisibilitychange: ((this: Document, ev: Event) => any) | null;
    readonly partitionedPrincipal: Principal;
    readonly permDelegateHandler: nsIPermissionDelegateHandler;
    readonly plugins: HTMLCollection;
    readonly preferredStyleSheetSet: string | null;
    readonly readyState: string;
    readonly referrer: string;
    readonly referrerInfo: nsIReferrerInfo;
    readonly referrerPolicy: ReferrerPolicy;
    readonly rootElement: SVGSVGElement | null;
    readonly sandboxFlagsAsString: string | null;
    readonly scripts: HTMLCollection;
    readonly scrollingElement: Element | null;
    selectedStyleSheetSet: string | null;
    styleSheetChangeEventsEnabled: boolean;
    readonly styleSheetSets: DOMStringList;
    readonly timeline: DocumentTimeline;
    title: string;
    readonly userHasInteracted: boolean;
    readonly visibilityState: VisibilityState;
    vlinkColor: string;
    addCertException(isTemporary: boolean): Promise<any>;
    adoptNode(node: Node): Node;
    blockParsing(promise: any, options?: BlockParsingOptions): Promise<any>;
    blockUnblockOnload(block: boolean): void;
    captureEvents(): void;
    caretPositionFromPoint(x: number, y: number, options?: CaretPositionFromPointOptions): CaretPosition | null;
    clear(): void;
    clearUserGestureActivation(): void;
    close(): void;
    completeStorageAccessRequestFromSite(serializedSite: string): Promise<void>;
    consumeTransientUserGestureActivation(): boolean;
    createAttribute(name: string): Attr;
    createAttributeNS(namespace: string | null, name: string): Attr;
    createCDATASection(data: string): CDATASection;
    createComment(data: string): Comment;
    createDocumentFragment(): DocumentFragment;
    createElement<K extends keyof HTMLElementTagNameMap>(tagName: K, options?: ElementCreationOptions): HTMLElementTagNameMap[K];
    /** @deprecated */
    createElement<K extends keyof HTMLElementDeprecatedTagNameMap>(tagName: K, options?: ElementCreationOptions): HTMLElementDeprecatedTagNameMap[K];
    createElement(tagName: string, options?: ElementCreationOptions): HTMLElement;
    createElementNS(namespace: string | null, qualifiedName: string, options?: string | ElementCreationOptions): Element;
    createEvent(eventInterface: "AddonEvent"): AddonEvent;
    createEvent(eventInterface: "AnimationEvent"): AnimationEvent;
    createEvent(eventInterface: "AnimationPlaybackEvent"): AnimationPlaybackEvent;
    createEvent(eventInterface: "AudioProcessingEvent"): AudioProcessingEvent;
    createEvent(eventInterface: "BeforeUnloadEvent"): BeforeUnloadEvent;
    createEvent(eventInterface: "BlobEvent"): BlobEvent;
    createEvent(eventInterface: "CSSCustomPropertyRegisteredEvent"): CSSCustomPropertyRegisteredEvent;
    createEvent(eventInterface: "CaretStateChangedEvent"): CaretStateChangedEvent;
    createEvent(eventInterface: "ClipboardEvent"): ClipboardEvent;
    createEvent(eventInterface: "CloseEvent"): CloseEvent;
    createEvent(eventInterface: "CommandEvent"): CommandEvent;
    createEvent(eventInterface: "CompositionEvent"): CompositionEvent;
    createEvent(eventInterface: "ContentVisibilityAutoStateChangeEvent"): ContentVisibilityAutoStateChangeEvent;
    createEvent(eventInterface: "CustomEvent"): CustomEvent;
    createEvent(eventInterface: "DeviceLightEvent"): DeviceLightEvent;
    createEvent(eventInterface: "DeviceMotionEvent"): DeviceMotionEvent;
    createEvent(eventInterface: "DeviceOrientationEvent"): DeviceOrientationEvent;
    createEvent(eventInterface: "DragEvent"): DragEvent;
    createEvent(eventInterface: "ErrorEvent"): ErrorEvent;
    createEvent(eventInterface: "FocusEvent"): FocusEvent;
    createEvent(eventInterface: "FontFaceSetLoadEvent"): FontFaceSetLoadEvent;
    createEvent(eventInterface: "FormDataEvent"): FormDataEvent;
    createEvent(eventInterface: "FrameCrashedEvent"): FrameCrashedEvent;
    createEvent(eventInterface: "GPUUncapturedErrorEvent"): GPUUncapturedErrorEvent;
    createEvent(eventInterface: "GamepadAxisMoveEvent"): GamepadAxisMoveEvent;
    createEvent(eventInterface: "GamepadButtonEvent"): GamepadButtonEvent;
    createEvent(eventInterface: "GamepadEvent"): GamepadEvent;
    createEvent(eventInterface: "HashChangeEvent"): HashChangeEvent;
    createEvent(eventInterface: "IDBVersionChangeEvent"): IDBVersionChangeEvent;
    createEvent(eventInterface: "ImageCaptureErrorEvent"): ImageCaptureErrorEvent;
    createEvent(eventInterface: "InputEvent"): InputEvent;
    createEvent(eventInterface: "InvokeEvent"): InvokeEvent;
    createEvent(eventInterface: "KeyboardEvent"): KeyboardEvent;
    createEvent(eventInterface: "MIDIConnectionEvent"): MIDIConnectionEvent;
    createEvent(eventInterface: "MIDIMessageEvent"): MIDIMessageEvent;
    createEvent(eventInterface: "MediaEncryptedEvent"): MediaEncryptedEvent;
    createEvent(eventInterface: "MediaKeyMessageEvent"): MediaKeyMessageEvent;
    createEvent(eventInterface: "MediaQueryListEvent"): MediaQueryListEvent;
    createEvent(eventInterface: "MediaRecorderErrorEvent"): MediaRecorderErrorEvent;
    createEvent(eventInterface: "MediaStreamEvent"): MediaStreamEvent;
    createEvent(eventInterface: "MediaStreamTrackEvent"): MediaStreamTrackEvent;
    createEvent(eventInterface: "MerchantValidationEvent"): MerchantValidationEvent;
    createEvent(eventInterface: "MessageEvent"): MessageEvent;
    createEvent(eventInterface: "MouseEvent"): MouseEvent;
    createEvent(eventInterface: "MouseEvents"): MouseEvent;
    createEvent(eventInterface: "MouseScrollEvent"): MouseScrollEvent;
    createEvent(eventInterface: "MozSharedMapChangeEvent"): MozSharedMapChangeEvent;
    createEvent(eventInterface: "MutationEvent"): MutationEvent;
    createEvent(eventInterface: "MutationEvents"): MutationEvent;
    createEvent(eventInterface: "NavigateEvent"): NavigateEvent;
    createEvent(eventInterface: "NavigationCurrentEntryChangeEvent"): NavigationCurrentEntryChangeEvent;
    createEvent(eventInterface: "NotifyPaintEvent"): NotifyPaintEvent;
    createEvent(eventInterface: "OfflineAudioCompletionEvent"): OfflineAudioCompletionEvent;
    createEvent(eventInterface: "PageTransitionEvent"): PageTransitionEvent;
    createEvent(eventInterface: "PaymentMethodChangeEvent"): PaymentMethodChangeEvent;
    createEvent(eventInterface: "PaymentRequestUpdateEvent"): PaymentRequestUpdateEvent;
    createEvent(eventInterface: "PerformanceEntryEvent"): PerformanceEntryEvent;
    createEvent(eventInterface: "PluginCrashedEvent"): PluginCrashedEvent;
    createEvent(eventInterface: "PointerEvent"): PointerEvent;
    createEvent(eventInterface: "PopStateEvent"): PopStateEvent;
    createEvent(eventInterface: "PopupBlockedEvent"): PopupBlockedEvent;
    createEvent(eventInterface: "PopupPositionedEvent"): PopupPositionedEvent;
    createEvent(eventInterface: "PositionStateEvent"): PositionStateEvent;
    createEvent(eventInterface: "ProgressEvent"): ProgressEvent;
    createEvent(eventInterface: "PromiseRejectionEvent"): PromiseRejectionEvent;
    createEvent(eventInterface: "RTCDTMFToneChangeEvent"): RTCDTMFToneChangeEvent;
    createEvent(eventInterface: "RTCDataChannelEvent"): RTCDataChannelEvent;
    createEvent(eventInterface: "RTCPeerConnectionIceEvent"): RTCPeerConnectionIceEvent;
    createEvent(eventInterface: "RTCTrackEvent"): RTCTrackEvent;
    createEvent(eventInterface: "ScrollAreaEvent"): ScrollAreaEvent;
    createEvent(eventInterface: "ScrollViewChangeEvent"): ScrollViewChangeEvent;
    createEvent(eventInterface: "SecurityPolicyViolationEvent"): SecurityPolicyViolationEvent;
    createEvent(eventInterface: "SimpleGestureEvent"): SimpleGestureEvent;
    createEvent(eventInterface: "SpeechRecognitionEvent"): SpeechRecognitionEvent;
    createEvent(eventInterface: "SpeechSynthesisErrorEvent"): SpeechSynthesisErrorEvent;
    createEvent(eventInterface: "SpeechSynthesisEvent"): SpeechSynthesisEvent;
    createEvent(eventInterface: "StorageEvent"): StorageEvent;
    createEvent(eventInterface: "StreamFilterDataEvent"): StreamFilterDataEvent;
    createEvent(eventInterface: "StyleSheetApplicableStateChangeEvent"): StyleSheetApplicableStateChangeEvent;
    createEvent(eventInterface: "StyleSheetRemovedEvent"): StyleSheetRemovedEvent;
    createEvent(eventInterface: "SubmitEvent"): SubmitEvent;
    createEvent(eventInterface: "TCPServerSocketEvent"): TCPServerSocketEvent;
    createEvent(eventInterface: "TCPSocketErrorEvent"): TCPSocketErrorEvent;
    createEvent(eventInterface: "TCPSocketEvent"): TCPSocketEvent;
    createEvent(eventInterface: "TaskPriorityChangeEvent"): TaskPriorityChangeEvent;
    createEvent(eventInterface: "TextEvent"): TextEvent;
    createEvent(eventInterface: "TimeEvent"): TimeEvent;
    createEvent(eventInterface: "ToggleEvent"): ToggleEvent;
    createEvent(eventInterface: "TouchEvent"): TouchEvent;
    createEvent(eventInterface: "TrackEvent"): TrackEvent;
    createEvent(eventInterface: "TransitionEvent"): TransitionEvent;
    createEvent(eventInterface: "UDPMessageEvent"): UDPMessageEvent;
    createEvent(eventInterface: "UIEvent"): UIEvent;
    createEvent(eventInterface: "UIEvents"): UIEvent;
    createEvent(eventInterface: "UserProximityEvent"): UserProximityEvent;
    createEvent(eventInterface: "VRDisplayEvent"): VRDisplayEvent;
    createEvent(eventInterface: "WebGLContextEvent"): WebGLContextEvent;
    createEvent(eventInterface: "WheelEvent"): WheelEvent;
    createEvent(eventInterface: "XRInputSourceEvent"): XRInputSourceEvent;
    createEvent(eventInterface: "XRInputSourcesChangeEvent"): XRInputSourcesChangeEvent;
    createEvent(eventInterface: "XRReferenceSpaceEvent"): XRReferenceSpaceEvent;
    createEvent(eventInterface: "XRSessionEvent"): XRSessionEvent;
    createEvent(eventInterface: "XULCommandEvent"): XULCommandEvent;
    createEvent(eventInterface: string): Event;
    createNodeIterator(root: Node, whatToShow?: number, filter?: NodeFilter | null): NodeIterator;
    createProcessingInstruction(target: string, data: string): ProcessingInstruction;
    createRange(): Range;
    createTextNode(data: string): Text;
    createTouch(view?: Window | null, target?: EventTarget | null, identifier?: number, pageX?: number, pageY?: number, screenX?: number, screenY?: number, clientX?: number, clientY?: number, radiusX?: number, radiusY?: number, rotationAngle?: number, force?: number): Touch;
    createTouchList(touch: Touch, ...touches: Touch[]): TouchList;
    createTouchList(): TouchList;
    createTouchList(touches: Touch[]): TouchList;
    createTreeWalker(root: Node, whatToShow?: number, filter?: NodeFilter | null): TreeWalker;
    createXULElement(localName: string, options?: string | ElementCreationOptions): Element;
    enableStyleSheetsForSet(name: string | null): void;
    execCommand(commandId: string, showUI?: boolean, value?: string): boolean;
    exitFullscreen(): Promise<void>;
    exitPointerLock(): void;
    getConnectedShadowRoots(): ShadowRoot[];
    getElementsByClassName(classNames: string): HTMLCollection;
    getElementsByName(elementName: string): NodeList;
    getElementsByTagName<K extends keyof HTMLElementTagNameMap>(localName: K): HTMLCollectionOf<HTMLElementTagNameMap[K]>;
    getElementsByTagName<K extends keyof SVGElementTagNameMap>(localName: K): HTMLCollectionOf<SVGElementTagNameMap[K]>;
    getElementsByTagName<K extends keyof MathMLElementTagNameMap>(localName: K): HTMLCollectionOf<MathMLElementTagNameMap[K]>;
    /** @deprecated */
    getElementsByTagName<K extends keyof HTMLElementDeprecatedTagNameMap>(localName: K): HTMLCollectionOf<HTMLElementDeprecatedTagNameMap[K]>;
    getElementsByTagName(localName: string): HTMLCollectionOf<Element>;
    getElementsByTagNameNS(namespace: string | null, localName: string): HTMLCollection;
    getFailedCertSecurityInfo(): FailedCertSecurityInfo;
    getNetErrorInfo(): NetErrorInfo;
    getSelection(): Selection | null;
    getWireframe(aIncludeNodes?: boolean): Wireframe | null;
    hasFocus(): boolean;
    hasStorageAccess(): Promise<boolean>;
    importNode(node: Node, deep?: boolean): Node;
    insertAnonymousContent(aForce?: boolean): AnonymousContent;
    isActive(): boolean;
    mozCancelFullScreen(): Promise<void>;
    mozSetImageElement(aImageElementId: string, aImageElement: Element | null): void;
    notifyUserGestureActivation(): void;
    open(unused1?: string, unused2?: string): Document;
    open(url: string | URL, name: string, features: string): WindowProxy | null;
    queryCommandEnabled(commandId: string): boolean;
    queryCommandIndeterm(commandId: string): boolean;
    queryCommandState(commandId: string): boolean;
    queryCommandSupported(commandId: string): boolean;
    queryCommandValue(commandId: string): string;
    releaseCapture(): void;
    releaseEvents(): void;
    reloadWithHttpsOnlyException(): void;
    removeAnonymousContent(aContent: AnonymousContent): void;
    requestStorageAccess(): Promise<void>;
    requestStorageAccessForOrigin(thirdPartyOrigin: string, requireUserInteraction?: boolean): Promise<void>;
    requestStorageAccessUnderSite(serializedSite: string): Promise<void>;
    setKeyPressEventModel(aKeyPressEventModel: number): void;
    setNotifyFetchSuccess(aShouldNotify: boolean): void;
    setNotifyFormOrPasswordRemoved(aShouldNotify: boolean): void;
    setSuppressedEventListener(aListener: EventListener | null): void;
    userInteractionForTesting(): void;
    write(...text: string[]): void;
    writeln(...text: string[]): void;
    readonly KEYPRESS_EVENT_MODEL_DEFAULT: 0;
    readonly KEYPRESS_EVENT_MODEL_SPLIT: 1;
    readonly KEYPRESS_EVENT_MODEL_CONFLATED: 2;
    addEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: Document, ev: DocumentEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: Document, ev: DocumentEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Document: {
    prototype: Document;
    new(): Document;
    readonly KEYPRESS_EVENT_MODEL_DEFAULT: 0;
    readonly KEYPRESS_EVENT_MODEL_SPLIT: 1;
    readonly KEYPRESS_EVENT_MODEL_CONFLATED: 2;
    isInstance: IsInstance<Document>;
    parseHTMLUnsafe(html: string): Document;
};

interface DocumentFragment extends Node, NonElementParentNode, ParentNode {
}

declare var DocumentFragment: {
    prototype: DocumentFragment;
    new(): DocumentFragment;
    isInstance: IsInstance<DocumentFragment>;
};

interface DocumentL10n extends DOMLocalization {
    readonly ready: Promise<any>;
    connectRoot(aElement: Node, aTranslate?: boolean): void;
}

interface DocumentOrShadowRoot {
    readonly activeElement: Element | null;
    adoptedStyleSheets: CSSStyleSheet[];
    readonly fullscreenElement: Element | null;
    readonly mozFullScreenElement: Element | null;
    readonly pointerLockElement: Element | null;
    readonly styleSheets: StyleSheetList;
    elementFromPoint(x: number, y: number): Element | null;
    elementsFromPoint(x: number, y: number): Element[];
    getAnimations(): Animation[];
    nodeFromPoint(x: number, y: number): Node | null;
    nodesFromPoint(x: number, y: number): Node[];
}

interface DocumentTimeline extends AnimationTimeline {
}

declare var DocumentTimeline: {
    prototype: DocumentTimeline;
    new(options?: DocumentTimelineOptions): DocumentTimeline;
    isInstance: IsInstance<DocumentTimeline>;
};

interface DocumentType extends Node, ChildNode {
    readonly name: string;
    readonly publicId: string;
    readonly systemId: string;
}

declare var DocumentType: {
    prototype: DocumentType;
    new(): DocumentType;
    isInstance: IsInstance<DocumentType>;
};

interface DominatorTree {
    readonly root: NodeId;
    getImmediateDominator(node: NodeId): NodeId | null;
    getImmediatelyDominated(node: NodeId): NodeId[] | null;
    getRetainedSize(node: NodeId): NodeSize | null;
}

declare var DominatorTree: {
    prototype: DominatorTree;
    new(): DominatorTree;
    isInstance: IsInstance<DominatorTree>;
};

interface DragEvent extends MouseEvent {
    readonly dataTransfer: DataTransfer | null;
    initDragEvent(type: string, canBubble?: boolean, cancelable?: boolean, aView?: Window | null, aDetail?: number, aScreenX?: number, aScreenY?: number, aClientX?: number, aClientY?: number, aCtrlKey?: boolean, aAltKey?: boolean, aShiftKey?: boolean, aMetaKey?: boolean, aButton?: number, aRelatedTarget?: EventTarget | null, aDataTransfer?: DataTransfer | null): void;
}

declare var DragEvent: {
    prototype: DragEvent;
    new(type: string, eventInitDict?: DragEventInit): DragEvent;
    isInstance: IsInstance<DragEvent>;
};

interface DynamicsCompressorNode extends AudioNode, AudioNodePassThrough {
    readonly attack: AudioParam;
    readonly knee: AudioParam;
    readonly ratio: AudioParam;
    readonly reduction: number;
    readonly release: AudioParam;
    readonly threshold: AudioParam;
}

declare var DynamicsCompressorNode: {
    prototype: DynamicsCompressorNode;
    new(context: BaseAudioContext, options?: DynamicsCompressorOptions): DynamicsCompressorNode;
    isInstance: IsInstance<DynamicsCompressorNode>;
};

interface EXT_blend_minmax {
    readonly MIN_EXT: 0x8007;
    readonly MAX_EXT: 0x8008;
}

interface EXT_color_buffer_float {
}

interface EXT_color_buffer_half_float {
    readonly RGBA16F_EXT: 0x881A;
    readonly RGB16F_EXT: 0x881B;
    readonly FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT: 0x8211;
    readonly UNSIGNED_NORMALIZED_EXT: 0x8C17;
}

interface EXT_depth_clamp {
    readonly DEPTH_CLAMP_EXT: 0x864F;
}

interface EXT_disjoint_timer_query {
    beginQueryEXT(target: GLenum, query: WebGLQuery): void;
    createQueryEXT(): WebGLQuery;
    deleteQueryEXT(query: WebGLQuery | null): void;
    endQueryEXT(target: GLenum): void;
    getQueryEXT(target: GLenum, pname: GLenum): any;
    getQueryObjectEXT(query: WebGLQuery, pname: GLenum): any;
    isQueryEXT(query: WebGLQuery | null): boolean;
    queryCounterEXT(query: WebGLQuery, target: GLenum): void;
    readonly QUERY_COUNTER_BITS_EXT: 0x8864;
    readonly CURRENT_QUERY_EXT: 0x8865;
    readonly QUERY_RESULT_EXT: 0x8866;
    readonly QUERY_RESULT_AVAILABLE_EXT: 0x8867;
    readonly TIME_ELAPSED_EXT: 0x88BF;
    readonly TIMESTAMP_EXT: 0x8E28;
    readonly GPU_DISJOINT_EXT: 0x8FBB;
}

interface EXT_float_blend {
}

interface EXT_frag_depth {
}

interface EXT_sRGB {
    readonly SRGB_EXT: 0x8C40;
    readonly SRGB_ALPHA_EXT: 0x8C42;
    readonly SRGB8_ALPHA8_EXT: 0x8C43;
    readonly FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: 0x8210;
}

interface EXT_shader_texture_lod {
}

interface EXT_texture_compression_bptc {
    readonly COMPRESSED_RGBA_BPTC_UNORM_EXT: 0x8E8C;
    readonly COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT: 0x8E8D;
    readonly COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT: 0x8E8E;
    readonly COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT: 0x8E8F;
}

interface EXT_texture_compression_rgtc {
    readonly COMPRESSED_RED_RGTC1_EXT: 0x8DBB;
    readonly COMPRESSED_SIGNED_RED_RGTC1_EXT: 0x8DBC;
    readonly COMPRESSED_RED_GREEN_RGTC2_EXT: 0x8DBD;
    readonly COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT: 0x8DBE;
}

interface EXT_texture_filter_anisotropic {
    readonly TEXTURE_MAX_ANISOTROPY_EXT: 0x84FE;
    readonly MAX_TEXTURE_MAX_ANISOTROPY_EXT: 0x84FF;
}

interface EXT_texture_norm16 {
    readonly R16_EXT: 0x822A;
    readonly RG16_EXT: 0x822C;
    readonly RGB16_EXT: 0x8054;
    readonly RGBA16_EXT: 0x805B;
    readonly R16_SNORM_EXT: 0x8F98;
    readonly RG16_SNORM_EXT: 0x8F99;
    readonly RGB16_SNORM_EXT: 0x8F9A;
    readonly RGBA16_SNORM_EXT: 0x8F9B;
}

interface ElementEventMap {
    "fullscreenchange": Event;
    "fullscreenerror": Event;
}

interface Element extends Node, ARIAMixin, Animatable, ChildNode, GeometryUtils, NonDocumentTypeChildNode, ParentNode {
    readonly assignedSlot: HTMLSlotElement | null;
    readonly attributes: NamedNodeMap;
    readonly classList: DOMTokenList;
    className: string;
    readonly clientHeight: number;
    readonly clientHeightDouble: number;
    readonly clientLeft: number;
    readonly clientTop: number;
    readonly clientWidth: number;
    readonly clientWidthDouble: number;
    readonly currentCSSZoom: number;
    readonly firstLineBoxBSize: number;
    readonly fontSizeInflation: number;
    readonly hasVisibleScrollbars: boolean;
    id: string;
    readonly implementedPseudoElement: string | null;
    innerHTML: string;
    readonly localName: string;
    readonly namespaceURI: string | null;
    onfullscreenchange: ((this: Element, ev: Event) => any) | null;
    onfullscreenerror: ((this: Element, ev: Event) => any) | null;
    readonly openOrClosedAssignedSlot: HTMLSlotElement | null;
    readonly openOrClosedShadowRoot: ShadowRoot | null;
    outerHTML: string;
    readonly part: DOMTokenList;
    readonly prefix: string | null;
    readonly screen: nsIScreen | null;
    readonly screenX: number;
    readonly screenY: number;
    readonly scrollHeight: number;
    scrollLeft: number;
    readonly scrollLeftMax: number;
    readonly scrollLeftMin: number;
    scrollTop: number;
    readonly scrollTopMax: number;
    readonly scrollTopMin: number;
    readonly scrollWidth: number;
    readonly shadowRoot: ShadowRoot | null;
    slot: string;
    readonly tagName: string;
    attachShadow(shadowRootInitDict: ShadowRootInit): ShadowRoot;
    checkVisibility(options?: CheckVisibilityOptions): boolean;
    closest(selector: string): Element | null;
    getAsFlexContainer(): Flex | null;
    getAttribute(name: string): string | null;
    getAttributeNS(namespace: string | null, localName: string): string | null;
    getAttributeNames(): string[];
    getAttributeNode(name: string): Attr | null;
    getAttributeNodeNS(namespaceURI: string | null, localName: string): Attr | null;
    getBoundingClientRect(): DOMRect;
    getClientRects(): DOMRectList;
    getElementsByClassName(classNames: string): HTMLCollection;
    getElementsByTagName<K extends keyof HTMLElementTagNameMap>(localName: K): HTMLCollectionOf<HTMLElementTagNameMap[K]>;
    getElementsByTagName<K extends keyof SVGElementTagNameMap>(localName: K): HTMLCollectionOf<SVGElementTagNameMap[K]>;
    getElementsByTagName<K extends keyof MathMLElementTagNameMap>(localName: K): HTMLCollectionOf<MathMLElementTagNameMap[K]>;
    /** @deprecated */
    getElementsByTagName<K extends keyof HTMLElementDeprecatedTagNameMap>(localName: K): HTMLCollectionOf<HTMLElementDeprecatedTagNameMap[K]>;
    getElementsByTagName(localName: string): HTMLCollectionOf<Element>;
    getElementsByTagNameNS(namespace: string | null, localName: string): HTMLCollection;
    getElementsWithGrid(): Element[];
    getGridFragments(): Grid[];
    getHTML(options?: GetHTMLOptions): string;
    getTransformToAncestor(ancestor: Element): DOMMatrixReadOnly;
    getTransformToParent(): DOMMatrixReadOnly;
    getTransformToViewport(): DOMMatrixReadOnly;
    hasAttribute(name: string): boolean;
    hasAttributeNS(namespace: string | null, localName: string): boolean;
    hasAttributes(): boolean;
    hasGridFragments(): boolean;
    hasPointerCapture(pointerId: number): boolean;
    insertAdjacentElement(where: string, element: Element): Element | null;
    insertAdjacentHTML(position: string, text: string): void;
    insertAdjacentText(where: string, data: string): void;
    matches(selector: string): boolean;
    mozMatchesSelector(selector: string): boolean;
    mozRequestFullScreen(): Promise<void>;
    mozScrollSnap(): void;
    releaseCapture(): void;
    releasePointerCapture(pointerId: number): void;
    removeAttribute(name: string): void;
    removeAttributeNS(namespace: string | null, localName: string): void;
    removeAttributeNode(oldAttr: Attr): Attr | null;
    requestFullscreen(): Promise<void>;
    requestPointerLock(): void;
    scroll(x: number, y: number): void;
    scroll(options?: ScrollToOptions): void;
    scrollBy(x: number, y: number): void;
    scrollBy(options?: ScrollToOptions): void;
    scrollIntoView(arg?: boolean | ScrollIntoViewOptions): void;
    scrollTo(x: number, y: number): void;
    scrollTo(options?: ScrollToOptions): void;
    setAttribute(name: string, value: string): void;
    setAttributeDevtools(name: string, value: string): void;
    setAttributeDevtoolsNS(namespace: string | null, name: string, value: string): void;
    setAttributeNS(namespace: string | null, name: string, value: string): void;
    setAttributeNode(newAttr: Attr): Attr | null;
    setAttributeNodeNS(newAttr: Attr): Attr | null;
    setCapture(retargetToElement?: boolean): void;
    setCaptureAlways(retargetToElement?: boolean): void;
    /** Available only in secure contexts. */
    setHTML(aInnerHTML: string, options?: SetHTMLOptions): void;
    setHTMLUnsafe(html: string): void;
    setPointerCapture(pointerId: number): void;
    toggleAttribute(name: string, force?: boolean): boolean;
    webkitMatchesSelector(selector: string): boolean;
    addEventListener<K extends keyof ElementEventMap>(type: K, listener: (this: Element, ev: ElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ElementEventMap>(type: K, listener: (this: Element, ev: ElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Element: {
    prototype: Element;
    new(): Element;
    isInstance: IsInstance<Element>;
};

interface ElementCSSInlineStyle {
    readonly style: CSSStyleDeclaration;
}

interface ElementInternals extends ARIAMixin {
    readonly form: HTMLFormElement | null;
    readonly labels: NodeList;
    readonly shadowRoot: ShadowRoot | null;
    readonly states: CustomStateSet;
    readonly validationAnchor: HTMLElement | null;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    reportValidity(): boolean;
    setFormValue(value: File | string | FormData | null, state?: File | string | FormData | null): void;
    setValidity(flags?: ValidityStateFlags, message?: string, anchor?: HTMLElement): void;
}

declare var ElementInternals: {
    prototype: ElementInternals;
    new(): ElementInternals;
    isInstance: IsInstance<ElementInternals>;
};

interface EncodedAudioChunk {
    readonly byteLength: number;
    readonly duration: number | null;
    readonly timestamp: number;
    readonly type: EncodedAudioChunkType;
    copyTo(destination: ArrayBufferView | ArrayBuffer): void;
}

declare var EncodedAudioChunk: {
    prototype: EncodedAudioChunk;
    new(init: EncodedAudioChunkInit): EncodedAudioChunk;
    isInstance: IsInstance<EncodedAudioChunk>;
};

interface EncodedVideoChunk {
    readonly byteLength: number;
    readonly duration: number | null;
    readonly timestamp: number;
    readonly type: EncodedVideoChunkType;
    copyTo(destination: ArrayBufferView | ArrayBuffer): void;
}

declare var EncodedVideoChunk: {
    prototype: EncodedVideoChunk;
    new(init: EncodedVideoChunkInit): EncodedVideoChunk;
    isInstance: IsInstance<EncodedVideoChunk>;
};

interface ErrorEvent extends Event {
    readonly colno: number;
    readonly error: any;
    readonly filename: string;
    readonly lineno: number;
    readonly message: string;
}

declare var ErrorEvent: {
    prototype: ErrorEvent;
    new(type: string, eventInitDict?: ErrorEventInit): ErrorEvent;
    isInstance: IsInstance<ErrorEvent>;
};

interface Event {
    readonly bubbles: boolean;
    cancelBubble: boolean;
    readonly cancelable: boolean;
    readonly composed: boolean;
    readonly composedTarget: EventTarget | null;
    readonly currentTarget: EventTarget | null;
    readonly defaultPrevented: boolean;
    readonly defaultPreventedByChrome: boolean;
    readonly defaultPreventedByContent: boolean;
    readonly eventPhase: number;
    readonly explicitOriginalTarget: EventTarget | null;
    readonly isReplyEventFromRemoteContent: boolean;
    readonly isSynthesized: boolean;
    readonly isTrusted: boolean;
    readonly isWaitingReplyFromRemoteContent: boolean;
    readonly multipleActionsPrevented: boolean;
    readonly originalTarget: EventTarget | null;
    returnValue: boolean;
    readonly target: EventTarget | null;
    readonly timeStamp: DOMHighResTimeStamp;
    readonly type: string;
    composedPath(): EventTarget[];
    initEvent(type: string, bubbles?: boolean, cancelable?: boolean): void;
    preventDefault(): void;
    preventMultipleActions(): void;
    requestReplyFromRemoteContent(): void;
    stopImmediatePropagation(): void;
    stopPropagation(): void;
    readonly NONE: 0;
    readonly CAPTURING_PHASE: 1;
    readonly AT_TARGET: 2;
    readonly BUBBLING_PHASE: 3;
    readonly ALT_MASK: 0x00000001;
    readonly CONTROL_MASK: 0x00000002;
    readonly SHIFT_MASK: 0x00000004;
    readonly META_MASK: 0x00000008;
}

declare var Event: {
    prototype: Event;
    new(type: string, eventInitDict?: EventInit): Event;
    readonly NONE: 0;
    readonly CAPTURING_PHASE: 1;
    readonly AT_TARGET: 2;
    readonly BUBBLING_PHASE: 3;
    readonly ALT_MASK: 0x00000001;
    readonly CONTROL_MASK: 0x00000002;
    readonly SHIFT_MASK: 0x00000004;
    readonly META_MASK: 0x00000008;
    isInstance: IsInstance<Event>;
};

interface EventCallbackDebuggerNotification extends CallbackDebuggerNotification {
    readonly event: Event;
    readonly targetType: EventCallbackDebuggerNotificationType;
}

declare var EventCallbackDebuggerNotification: {
    prototype: EventCallbackDebuggerNotification;
    new(): EventCallbackDebuggerNotification;
    isInstance: IsInstance<EventCallbackDebuggerNotification>;
};

interface EventCounts {
    forEach(callbackfn: (value: number, key: string, parent: EventCounts) => void, thisArg?: any): void;
}

declare var EventCounts: {
    prototype: EventCounts;
    new(): EventCounts;
    isInstance: IsInstance<EventCounts>;
};

interface EventHandler {
}

interface EventListenerOrEventListenerObject {
}

interface EventSourceEventMap {
    "error": Event;
    "message": Event;
    "open": Event;
}

interface EventSource extends EventTarget {
    onerror: ((this: EventSource, ev: Event) => any) | null;
    onmessage: ((this: EventSource, ev: Event) => any) | null;
    onopen: ((this: EventSource, ev: Event) => any) | null;
    readonly readyState: number;
    readonly url: string;
    readonly withCredentials: boolean;
    close(): void;
    readonly CONNECTING: 0;
    readonly OPEN: 1;
    readonly CLOSED: 2;
    addEventListener<K extends keyof EventSourceEventMap>(type: K, listener: (this: EventSource, ev: EventSourceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: (this: EventSource, event: MessageEvent) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof EventSourceEventMap>(type: K, listener: (this: EventSource, ev: EventSourceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: (this: EventSource, event: MessageEvent) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var EventSource: {
    prototype: EventSource;
    new(url: string | URL, eventSourceInitDict?: EventSourceInit): EventSource;
    readonly CONNECTING: 0;
    readonly OPEN: 1;
    readonly CLOSED: 2;
    isInstance: IsInstance<EventSource>;
};

interface EventTarget {
    readonly ownerGlobal: WindowProxy | null;
    addEventListener(type: string, listener: EventListener | null, options?: AddEventListenerOptions | boolean, wantsUntrusted?: boolean | null): void;
    dispatchEvent(event: Event): boolean;
    getEventHandler(type: string): EventHandler;
    removeEventListener(type: string, listener: EventListener | null, options?: EventListenerOptions | boolean): void;
    setEventHandler(type: string, handler: EventHandler): void;
}

declare var EventTarget: {
    prototype: EventTarget;
    new(): EventTarget;
    isInstance: IsInstance<EventTarget>;
};

interface Exception extends ExceptionMembers {
    readonly message: string;
    readonly name: string;
    toString(): string;
}

interface ExceptionMembers {
    readonly columnNumber: number;
    readonly data: nsISupports | null;
    readonly filename: string;
    readonly lineNumber: number;
    readonly location: StackFrame | null;
    readonly result: number;
    readonly stack: string;
}

interface External {
    AddSearchProvider(): void;
    IsSearchProviderInstalled(): void;
}

interface FeaturePolicy {
    allowedFeatures(): string[];
    allowsFeature(feature: string, origin?: string): boolean;
    features(): string[];
    getAllowlistForFeature(feature: string): string[];
}

interface FeaturePolicyViolationReportBody extends ReportBody {
    readonly columnNumber: number | null;
    readonly disposition: string;
    readonly featureId: string;
    readonly lineNumber: number | null;
    readonly sourceFile: string | null;
}

declare var FeaturePolicyViolationReportBody: {
    prototype: FeaturePolicyViolationReportBody;
    new(): FeaturePolicyViolationReportBody;
    isInstance: IsInstance<FeaturePolicyViolationReportBody>;
};

interface FetchObserverEventMap {
    "requestprogress": Event;
    "responseprogress": Event;
    "statechange": Event;
}

interface FetchObserver extends EventTarget {
    onrequestprogress: ((this: FetchObserver, ev: Event) => any) | null;
    onresponseprogress: ((this: FetchObserver, ev: Event) => any) | null;
    onstatechange: ((this: FetchObserver, ev: Event) => any) | null;
    readonly state: FetchState;
    addEventListener<K extends keyof FetchObserverEventMap>(type: K, listener: (this: FetchObserver, ev: FetchObserverEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof FetchObserverEventMap>(type: K, listener: (this: FetchObserver, ev: FetchObserverEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var FetchObserver: {
    prototype: FetchObserver;
    new(): FetchObserver;
    isInstance: IsInstance<FetchObserver>;
};

interface File extends Blob {
    readonly lastModified: number;
    readonly mozFullPath: string;
    readonly name: string;
    readonly webkitRelativePath: string;
}

declare var File: {
    prototype: File;
    new(fileBits: BlobPart[], fileName: string, options?: FilePropertyBag): File;
    isInstance: IsInstance<File>;
    createFromFileName(fileName: string, options?: ChromeFilePropertyBag): Promise<File>;
    createFromNsIFile(file: nsIFile, options?: ChromeFilePropertyBag): Promise<File>;
};

interface FileList {
    readonly length: number;
    item(index: number): File | null;
    [index: number]: File;
}

declare var FileList: {
    prototype: FileList;
    new(): FileList;
    isInstance: IsInstance<FileList>;
};

interface FileReaderEventMap {
    "abort": Event;
    "error": Event;
    "load": Event;
    "loadend": Event;
    "loadstart": Event;
    "progress": Event;
}

interface FileReader extends EventTarget {
    readonly error: DOMException | null;
    onabort: ((this: FileReader, ev: Event) => any) | null;
    onerror: ((this: FileReader, ev: Event) => any) | null;
    onload: ((this: FileReader, ev: Event) => any) | null;
    onloadend: ((this: FileReader, ev: Event) => any) | null;
    onloadstart: ((this: FileReader, ev: Event) => any) | null;
    onprogress: ((this: FileReader, ev: Event) => any) | null;
    readonly readyState: number;
    readonly result: string | ArrayBuffer | null;
    abort(): void;
    readAsArrayBuffer(blob: Blob): void;
    readAsBinaryString(filedata: Blob): void;
    readAsDataURL(blob: Blob): void;
    readAsText(blob: Blob, label?: string): void;
    readonly EMPTY: 0;
    readonly LOADING: 1;
    readonly DONE: 2;
    addEventListener<K extends keyof FileReaderEventMap>(type: K, listener: (this: FileReader, ev: FileReaderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof FileReaderEventMap>(type: K, listener: (this: FileReader, ev: FileReaderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var FileReader: {
    prototype: FileReader;
    new(): FileReader;
    readonly EMPTY: 0;
    readonly LOADING: 1;
    readonly DONE: 2;
    isInstance: IsInstance<FileReader>;
};

interface FileSystem {
    readonly name: string;
    readonly root: FileSystemDirectoryEntry;
}

declare var FileSystem: {
    prototype: FileSystem;
    new(): FileSystem;
    isInstance: IsInstance<FileSystem>;
};

interface FileSystemDirectoryEntry extends FileSystemEntry {
    createReader(): FileSystemDirectoryReader;
    getDirectory(path?: string | null, options?: FileSystemFlags, successCallback?: FileSystemEntryCallback, errorCallback?: ErrorCallback): void;
    getFile(path?: string | null, options?: FileSystemFlags, successCallback?: FileSystemEntryCallback, errorCallback?: ErrorCallback): void;
}

declare var FileSystemDirectoryEntry: {
    prototype: FileSystemDirectoryEntry;
    new(): FileSystemDirectoryEntry;
    isInstance: IsInstance<FileSystemDirectoryEntry>;
};

/** Available only in secure contexts. */
interface FileSystemDirectoryHandle extends FileSystemHandle {
    getDirectoryHandle(name: string, options?: FileSystemGetDirectoryOptions): Promise<FileSystemDirectoryHandle>;
    getFileHandle(name: string, options?: FileSystemGetFileOptions): Promise<FileSystemFileHandle>;
    removeEntry(name: string, options?: FileSystemRemoveOptions): Promise<void>;
    resolve(possibleDescendant: FileSystemHandle): Promise<string[] | null>;
}

declare var FileSystemDirectoryHandle: {
    prototype: FileSystemDirectoryHandle;
    new(): FileSystemDirectoryHandle;
    isInstance: IsInstance<FileSystemDirectoryHandle>;
};

/** Available only in secure contexts. */
interface FileSystemDirectoryIterator {
    next(): Promise<any>;
}

interface FileSystemDirectoryReader {
    readEntries(successCallback: FileSystemEntriesCallback, errorCallback?: ErrorCallback): void;
}

declare var FileSystemDirectoryReader: {
    prototype: FileSystemDirectoryReader;
    new(): FileSystemDirectoryReader;
    isInstance: IsInstance<FileSystemDirectoryReader>;
};

interface FileSystemEntry {
    readonly filesystem: FileSystem;
    readonly fullPath: string;
    readonly isDirectory: boolean;
    readonly isFile: boolean;
    readonly name: string;
    getParent(successCallback?: FileSystemEntryCallback, errorCallback?: ErrorCallback): void;
}

declare var FileSystemEntry: {
    prototype: FileSystemEntry;
    new(): FileSystemEntry;
    isInstance: IsInstance<FileSystemEntry>;
};

interface FileSystemFileEntry extends FileSystemEntry {
    file(successCallback: FileCallback, errorCallback?: ErrorCallback): void;
}

declare var FileSystemFileEntry: {
    prototype: FileSystemFileEntry;
    new(): FileSystemFileEntry;
    isInstance: IsInstance<FileSystemFileEntry>;
};

/** Available only in secure contexts. */
interface FileSystemFileHandle extends FileSystemHandle {
    createWritable(options?: FileSystemCreateWritableOptions): Promise<FileSystemWritableFileStream>;
    getFile(): Promise<File>;
}

declare var FileSystemFileHandle: {
    prototype: FileSystemFileHandle;
    new(): FileSystemFileHandle;
    isInstance: IsInstance<FileSystemFileHandle>;
};

/** Available only in secure contexts. */
interface FileSystemHandle {
    readonly kind: FileSystemHandleKind;
    readonly name: string;
    isSameEntry(other: FileSystemHandle): Promise<boolean>;
    move(name: string): Promise<void>;
    move(parent: FileSystemDirectoryHandle): Promise<void>;
    move(parent: FileSystemDirectoryHandle, name: string): Promise<void>;
}

declare var FileSystemHandle: {
    prototype: FileSystemHandle;
    new(): FileSystemHandle;
    isInstance: IsInstance<FileSystemHandle>;
};

/** Available only in secure contexts. */
interface FileSystemWritableFileStream extends WritableStream {
    seek(position: number): Promise<void>;
    truncate(size: number): Promise<void>;
    write(data: FileSystemWriteChunkType): Promise<void>;
}

declare var FileSystemWritableFileStream: {
    prototype: FileSystemWritableFileStream;
    new(): FileSystemWritableFileStream;
    isInstance: IsInstance<FileSystemWritableFileStream>;
};

interface Flex {
    readonly crossAxisDirection: FlexPhysicalDirection;
    readonly mainAxisDirection: FlexPhysicalDirection;
    getLines(): FlexLineValues[];
}

declare var Flex: {
    prototype: Flex;
    new(): Flex;
    isInstance: IsInstance<Flex>;
};

interface FlexItemValues {
    readonly clampState: FlexItemClampState;
    readonly crossMaxSize: number;
    readonly crossMinSize: number;
    readonly frameRect: DOMRectReadOnly;
    readonly mainBaseSize: number;
    readonly mainDeltaSize: number;
    readonly mainMaxSize: number;
    readonly mainMinSize: number;
    readonly node: Node | null;
}

declare var FlexItemValues: {
    prototype: FlexItemValues;
    new(): FlexItemValues;
    isInstance: IsInstance<FlexItemValues>;
};

interface FlexLineValues {
    readonly crossSize: number;
    readonly crossStart: number;
    readonly firstBaselineOffset: number;
    readonly growthState: FlexLineGrowthState;
    readonly lastBaselineOffset: number;
    getItems(): FlexItemValues[];
}

declare var FlexLineValues: {
    prototype: FlexLineValues;
    new(): FlexLineValues;
    isInstance: IsInstance<FlexLineValues>;
};

interface FluentBundle {
    readonly locales: string[];
    addResource(aResource: FluentResource, aOptions?: FluentBundleAddResourceOptions): void;
    formatPattern(pattern: FluentPattern, aArgs?: L10nArgs | null, aErrors?: any): string;
    getMessage(id: string): FluentMessage | null;
    hasMessage(id: string): boolean;
}

declare var FluentBundle: {
    prototype: FluentBundle;
    new(aLocales: string | string[], aOptions?: FluentBundleOptions): FluentBundle;
    isInstance: IsInstance<FluentBundle>;
};

interface FluentBundleAsyncIterator {
    next(): Promise<FluentBundleIteratorResult>;
    values(): FluentBundleAsyncIterator;
}

interface FluentBundleIterator {
    next(): FluentBundleIteratorResult;
    values(): FluentBundleIterator;
}

interface FluentPattern {
}

declare var FluentPattern: {
    prototype: FluentPattern;
    new(): FluentPattern;
    isInstance: IsInstance<FluentPattern>;
};

interface FluentResource {
    textElements(): FluentTextElementItem[];
}

declare var FluentResource: {
    prototype: FluentResource;
    new(source: string): FluentResource;
    isInstance: IsInstance<FluentResource>;
};

interface FocusEvent extends UIEvent {
    readonly relatedTarget: EventTarget | null;
}

declare var FocusEvent: {
    prototype: FocusEvent;
    new(typeArg: string, focusEventInitDict?: FocusEventInit): FocusEvent;
    isInstance: IsInstance<FocusEvent>;
};

interface FontFace {
    ascentOverride: string;
    descentOverride: string;
    display: string;
    family: string;
    featureSettings: string;
    lineGapOverride: string;
    readonly loaded: Promise<FontFace>;
    sizeAdjust: string;
    readonly status: FontFaceLoadStatus;
    stretch: string;
    style: string;
    unicodeRange: string;
    variant: string;
    variationSettings: string;
    weight: string;
    load(): Promise<FontFace>;
}

declare var FontFace: {
    prototype: FontFace;
    new(family: string, source: string | BinaryData, descriptors?: FontFaceDescriptors): FontFace;
    isInstance: IsInstance<FontFace>;
};

interface FontFaceSetEventMap {
    "loading": Event;
    "loadingdone": Event;
    "loadingerror": Event;
}

interface FontFaceSet extends EventTarget {
    onloading: ((this: FontFaceSet, ev: Event) => any) | null;
    onloadingdone: ((this: FontFaceSet, ev: Event) => any) | null;
    onloadingerror: ((this: FontFaceSet, ev: Event) => any) | null;
    readonly ready: Promise<undefined>;
    readonly size: number;
    readonly status: FontFaceSetLoadStatus;
    add(font: FontFace): void;
    check(font: string, text?: string): boolean;
    clear(): void;
    delete(font: FontFace): boolean;
    entries(): FontFaceSetIterator;
    forEach(cb: FontFaceSetForEachCallback, thisArg?: any): void;
    has(font: FontFace): boolean;
    load(font: string, text?: string): Promise<FontFace[]>;
    values(): FontFaceSetIterator;
    addEventListener<K extends keyof FontFaceSetEventMap>(type: K, listener: (this: FontFaceSet, ev: FontFaceSetEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof FontFaceSetEventMap>(type: K, listener: (this: FontFaceSet, ev: FontFaceSetEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var FontFaceSet: {
    prototype: FontFaceSet;
    new(): FontFaceSet;
    isInstance: IsInstance<FontFaceSet>;
};

interface FontFaceSetIterator {
    next(): FontFaceSetIteratorResult;
}

interface FontFaceSetLoadEvent extends Event {
    readonly fontfaces: FontFace[];
}

declare var FontFaceSetLoadEvent: {
    prototype: FontFaceSetLoadEvent;
    new(type: string, eventInitDict?: FontFaceSetLoadEventInit): FontFaceSetLoadEvent;
    isInstance: IsInstance<FontFaceSetLoadEvent>;
};

interface FontFaceSource {
    readonly fonts: FontFaceSet;
}

interface FormData {
    append(name: string, value: Blob, filename?: string): void;
    append(name: string, value: string): void;
    delete(name: string): void;
    get(name: string): FormDataEntryValue | null;
    getAll(name: string): FormDataEntryValue[];
    has(name: string): boolean;
    set(name: string, value: Blob, filename?: string): void;
    set(name: string, value: string): void;
    forEach(callbackfn: (value: FormDataEntryValue, key: string, parent: FormData) => void, thisArg?: any): void;
}

declare var FormData: {
    prototype: FormData;
    new(form?: HTMLFormElement, submitter?: HTMLElement | null): FormData;
    isInstance: IsInstance<FormData>;
};

interface FormDataEvent extends Event {
    readonly formData: FormData;
}

declare var FormDataEvent: {
    prototype: FormDataEvent;
    new(type: string, eventInitDict?: FormDataEventInit): FormDataEvent;
    isInstance: IsInstance<FormDataEvent>;
};

interface FragmentDirective {
}

declare var FragmentDirective: {
    prototype: FragmentDirective;
    new(): FragmentDirective;
    isInstance: IsInstance<FragmentDirective>;
};

interface FrameCrashedEvent extends Event {
    readonly browsingContextId: number;
    readonly childID: number;
    readonly isTopFrame: boolean;
}

declare var FrameCrashedEvent: {
    prototype: FrameCrashedEvent;
    new(type: string, eventInitDict?: FrameCrashedEventInit): FrameCrashedEvent;
    isInstance: IsInstance<FrameCrashedEvent>;
};

interface FrameLoader extends WebBrowserPersistable {
    readonly browsingContext: BrowsingContext | null;
    readonly childID: number;
    readonly depthTooGreat: boolean;
    readonly docShell: nsIDocShell | null;
    readonly isDead: boolean;
    readonly isRemoteFrame: boolean;
    readonly lazyHeight: number;
    readonly lazyWidth: number;
    readonly loadContext: LoadContext | null;
    readonly messageManager: MessageSender | null;
    readonly ownerElement: Element | null;
    readonly remoteTab: RemoteTab | null;
    exitPrintPreview(): void;
    printPreview(aPrintSettings: nsIPrintSettings, aSourceBrowsingContext: BrowsingContext | null): Promise<number>;
    requestEpochUpdate(aEpoch: number): void;
    requestSHistoryUpdate(): void;
    requestTabStateFlush(): Promise<void>;
    requestUpdatePosition(): void;
}

declare var FrameLoader: {
    prototype: FrameLoader;
    new(): FrameLoader;
    isInstance: IsInstance<FrameLoader>;
};

interface FrameScriptLoader {
    getDelayedFrameScripts(): any[][];
    loadFrameScript(url: string, allowDelayedLoad: boolean, runInGlobalScope?: boolean): void;
    removeDelayedFrameScript(url: string): void;
}

/** Available only in secure contexts. */
interface GPU {
    getPreferredCanvasFormat(): GPUTextureFormat;
    requestAdapter(options?: GPURequestAdapterOptions): Promise<GPUAdapter | null>;
}

declare var GPU: {
    prototype: GPU;
    new(): GPU;
    isInstance: IsInstance<GPU>;
};

/** Available only in secure contexts. */
interface GPUAdapter {
    readonly features: GPUSupportedFeatures;
    readonly isFallbackAdapter: boolean;
    readonly limits: GPUSupportedLimits;
    requestAdapterInfo(unmaskHints?: string[]): Promise<GPUAdapterInfo>;
    requestDevice(descriptor?: GPUDeviceDescriptor): Promise<GPUDevice>;
}

declare var GPUAdapter: {
    prototype: GPUAdapter;
    new(): GPUAdapter;
    isInstance: IsInstance<GPUAdapter>;
};

/** Available only in secure contexts. */
interface GPUAdapterInfo {
    readonly architecture: string;
    readonly description: string;
    readonly device: string;
    readonly vendor: string;
    readonly wgpuBackend: string;
    readonly wgpuDevice: number;
    readonly wgpuDeviceType: string;
    readonly wgpuDriver: string;
    readonly wgpuDriverInfo: string;
    readonly wgpuName: string;
    readonly wgpuVendor: number;
}

declare var GPUAdapterInfo: {
    prototype: GPUAdapterInfo;
    new(): GPUAdapterInfo;
    isInstance: IsInstance<GPUAdapterInfo>;
};

/** Available only in secure contexts. */
interface GPUBindGroup extends GPUObjectBase {
}

declare var GPUBindGroup: {
    prototype: GPUBindGroup;
    new(): GPUBindGroup;
    isInstance: IsInstance<GPUBindGroup>;
};

/** Available only in secure contexts. */
interface GPUBindGroupLayout extends GPUObjectBase {
}

declare var GPUBindGroupLayout: {
    prototype: GPUBindGroupLayout;
    new(): GPUBindGroupLayout;
    isInstance: IsInstance<GPUBindGroupLayout>;
};

interface GPUBindingCommandsMixin {
    setBindGroup(index: GPUIndex32, bindGroup: GPUBindGroup, dynamicOffsets?: GPUBufferDynamicOffset[]): void;
}

/** Available only in secure contexts. */
interface GPUBuffer extends GPUObjectBase {
    readonly mapState: GPUBufferMapState;
    readonly size: GPUSize64Out;
    readonly usage: GPUFlagsConstant;
    destroy(): void;
    getMappedRange(offset?: GPUSize64, size?: GPUSize64): ArrayBuffer;
    mapAsync(mode: GPUMapModeFlags, offset?: GPUSize64, size?: GPUSize64): Promise<void>;
    unmap(): void;
}

declare var GPUBuffer: {
    prototype: GPUBuffer;
    new(): GPUBuffer;
    isInstance: IsInstance<GPUBuffer>;
};

/** Available only in secure contexts. */
interface GPUCanvasContext {
    readonly canvas: HTMLCanvasElement | OffscreenCanvas;
    configure(configuration: GPUCanvasConfiguration): void;
    getCurrentTexture(): GPUTexture;
    unconfigure(): void;
}

declare var GPUCanvasContext: {
    prototype: GPUCanvasContext;
    new(): GPUCanvasContext;
    isInstance: IsInstance<GPUCanvasContext>;
};

/** Available only in secure contexts. */
interface GPUCommandBuffer extends GPUObjectBase {
}

declare var GPUCommandBuffer: {
    prototype: GPUCommandBuffer;
    new(): GPUCommandBuffer;
    isInstance: IsInstance<GPUCommandBuffer>;
};

/** Available only in secure contexts. */
interface GPUCommandEncoder extends GPUDebugCommandsMixin, GPUObjectBase {
    beginComputePass(descriptor?: GPUComputePassDescriptor): GPUComputePassEncoder;
    beginRenderPass(descriptor: GPURenderPassDescriptor): GPURenderPassEncoder;
    clearBuffer(buffer: GPUBuffer, offset?: GPUSize64, size?: GPUSize64): void;
    copyBufferToBuffer(source: GPUBuffer, sourceOffset: GPUSize64, destination: GPUBuffer, destinationOffset: GPUSize64, size: GPUSize64): void;
    copyBufferToTexture(source: GPUImageCopyBuffer, destination: GPUImageCopyTexture, copySize: GPUExtent3D): void;
    copyTextureToBuffer(source: GPUImageCopyTexture, destination: GPUImageCopyBuffer, copySize: GPUExtent3D): void;
    copyTextureToTexture(source: GPUImageCopyTexture, destination: GPUImageCopyTexture, copySize: GPUExtent3D): void;
    finish(descriptor?: GPUCommandBufferDescriptor): GPUCommandBuffer;
}

declare var GPUCommandEncoder: {
    prototype: GPUCommandEncoder;
    new(): GPUCommandEncoder;
    isInstance: IsInstance<GPUCommandEncoder>;
};

/** Available only in secure contexts. */
interface GPUCompilationInfo {
    readonly messages: GPUCompilationMessage[];
}

declare var GPUCompilationInfo: {
    prototype: GPUCompilationInfo;
    new(): GPUCompilationInfo;
    isInstance: IsInstance<GPUCompilationInfo>;
};

/** Available only in secure contexts. */
interface GPUCompilationMessage {
    readonly length: number;
    readonly lineNum: number;
    readonly linePos: number;
    readonly message: string;
    readonly offset: number;
    readonly type: GPUCompilationMessageType;
}

declare var GPUCompilationMessage: {
    prototype: GPUCompilationMessage;
    new(): GPUCompilationMessage;
    isInstance: IsInstance<GPUCompilationMessage>;
};

/** Available only in secure contexts. */
interface GPUComputePassEncoder extends GPUBindingCommandsMixin, GPUDebugCommandsMixin, GPUObjectBase {
    dispatchWorkgroups(workgroupCountX: GPUSize32, workgroupCountY?: GPUSize32, workgroupCountZ?: GPUSize32): void;
    dispatchWorkgroupsIndirect(indirectBuffer: GPUBuffer, indirectOffset: GPUSize64): void;
    end(): void;
    setPipeline(pipeline: GPUComputePipeline): void;
}

declare var GPUComputePassEncoder: {
    prototype: GPUComputePassEncoder;
    new(): GPUComputePassEncoder;
    isInstance: IsInstance<GPUComputePassEncoder>;
};

/** Available only in secure contexts. */
interface GPUComputePipeline extends GPUObjectBase, GPUPipelineBase {
}

declare var GPUComputePipeline: {
    prototype: GPUComputePipeline;
    new(): GPUComputePipeline;
    isInstance: IsInstance<GPUComputePipeline>;
};

interface GPUDebugCommandsMixin {
    insertDebugMarker(markerLabel: string): void;
    popDebugGroup(): void;
    pushDebugGroup(groupLabel: string): void;
}

interface GPUDeviceEventMap {
    "uncapturederror": Event;
}

/** Available only in secure contexts. */
interface GPUDevice extends EventTarget, GPUObjectBase {
    readonly features: GPUSupportedFeatures;
    readonly limits: GPUSupportedLimits;
    readonly lost: Promise<GPUDeviceLostInfo>;
    onuncapturederror: ((this: GPUDevice, ev: Event) => any) | null;
    readonly queue: GPUQueue;
    createBindGroup(descriptor: GPUBindGroupDescriptor): GPUBindGroup;
    createBindGroupLayout(descriptor: GPUBindGroupLayoutDescriptor): GPUBindGroupLayout;
    createBuffer(descriptor: GPUBufferDescriptor): GPUBuffer;
    createCommandEncoder(descriptor?: GPUCommandEncoderDescriptor): GPUCommandEncoder;
    createComputePipeline(descriptor: GPUComputePipelineDescriptor): GPUComputePipeline;
    createComputePipelineAsync(descriptor: GPUComputePipelineDescriptor): Promise<GPUComputePipeline>;
    createPipelineLayout(descriptor: GPUPipelineLayoutDescriptor): GPUPipelineLayout;
    createRenderBundleEncoder(descriptor: GPURenderBundleEncoderDescriptor): GPURenderBundleEncoder;
    createRenderPipeline(descriptor: GPURenderPipelineDescriptor): GPURenderPipeline;
    createRenderPipelineAsync(descriptor: GPURenderPipelineDescriptor): Promise<GPURenderPipeline>;
    createSampler(descriptor?: GPUSamplerDescriptor): GPUSampler;
    createShaderModule(descriptor: GPUShaderModuleDescriptor): GPUShaderModule;
    createTexture(descriptor: GPUTextureDescriptor): GPUTexture;
    destroy(): void;
    popErrorScope(): Promise<GPUError | null>;
    pushErrorScope(filter: GPUErrorFilter): void;
    addEventListener<K extends keyof GPUDeviceEventMap>(type: K, listener: (this: GPUDevice, ev: GPUDeviceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof GPUDeviceEventMap>(type: K, listener: (this: GPUDevice, ev: GPUDeviceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var GPUDevice: {
    prototype: GPUDevice;
    new(): GPUDevice;
    isInstance: IsInstance<GPUDevice>;
};

/** Available only in secure contexts. */
interface GPUDeviceLostInfo {
    readonly message: string;
    readonly reason: any;
}

declare var GPUDeviceLostInfo: {
    prototype: GPUDeviceLostInfo;
    new(): GPUDeviceLostInfo;
    isInstance: IsInstance<GPUDeviceLostInfo>;
};

/** Available only in secure contexts. */
interface GPUError {
    readonly message: string;
}

declare var GPUError: {
    prototype: GPUError;
    new(): GPUError;
    isInstance: IsInstance<GPUError>;
};

/** Available only in secure contexts. */
interface GPUInternalError extends GPUError {
}

declare var GPUInternalError: {
    prototype: GPUInternalError;
    new(message: string): GPUInternalError;
    isInstance: IsInstance<GPUInternalError>;
};

interface GPUObjectBase {
    label: string | null;
}

/** Available only in secure contexts. */
interface GPUOutOfMemoryError extends GPUError {
}

declare var GPUOutOfMemoryError: {
    prototype: GPUOutOfMemoryError;
    new(message: string): GPUOutOfMemoryError;
    isInstance: IsInstance<GPUOutOfMemoryError>;
};

interface GPUPipelineBase {
    getBindGroupLayout(index: number): GPUBindGroupLayout;
}

/** Available only in secure contexts. */
interface GPUPipelineLayout extends GPUObjectBase {
}

declare var GPUPipelineLayout: {
    prototype: GPUPipelineLayout;
    new(): GPUPipelineLayout;
    isInstance: IsInstance<GPUPipelineLayout>;
};

/** Available only in secure contexts. */
interface GPUQuerySet extends GPUObjectBase {
    destroy(): void;
}

declare var GPUQuerySet: {
    prototype: GPUQuerySet;
    new(): GPUQuerySet;
    isInstance: IsInstance<GPUQuerySet>;
};

/** Available only in secure contexts. */
interface GPUQueue extends GPUObjectBase {
    copyExternalImageToTexture(source: GPUImageCopyExternalImage, destination: GPUImageCopyTextureTagged, copySize: GPUExtent3D): void;
    onSubmittedWorkDone(): Promise<void>;
    submit(buffers: GPUCommandBuffer[]): void;
    writeBuffer(buffer: GPUBuffer, bufferOffset: GPUSize64, data: BufferSource, dataOffset?: GPUSize64, size?: GPUSize64): void;
    writeTexture(destination: GPUImageCopyTexture, data: BufferSource, dataLayout: GPUImageDataLayout, size: GPUExtent3D): void;
}

declare var GPUQueue: {
    prototype: GPUQueue;
    new(): GPUQueue;
    isInstance: IsInstance<GPUQueue>;
};

/** Available only in secure contexts. */
interface GPURenderBundle extends GPUObjectBase {
}

declare var GPURenderBundle: {
    prototype: GPURenderBundle;
    new(): GPURenderBundle;
    isInstance: IsInstance<GPURenderBundle>;
};

/** Available only in secure contexts. */
interface GPURenderBundleEncoder extends GPUBindingCommandsMixin, GPUDebugCommandsMixin, GPUObjectBase, GPURenderCommandsMixin {
    finish(descriptor?: GPURenderBundleDescriptor): GPURenderBundle;
}

declare var GPURenderBundleEncoder: {
    prototype: GPURenderBundleEncoder;
    new(): GPURenderBundleEncoder;
    isInstance: IsInstance<GPURenderBundleEncoder>;
};

interface GPURenderCommandsMixin {
    draw(vertexCount: GPUSize32, instanceCount?: GPUSize32, firstVertex?: GPUSize32, firstInstance?: GPUSize32): void;
    drawIndexed(indexCount: GPUSize32, instanceCount?: GPUSize32, firstIndex?: GPUSize32, baseVertex?: GPUSignedOffset32, firstInstance?: GPUSize32): void;
    drawIndexedIndirect(indirectBuffer: GPUBuffer, indirectOffset: GPUSize64): void;
    drawIndirect(indirectBuffer: GPUBuffer, indirectOffset: GPUSize64): void;
    setIndexBuffer(buffer: GPUBuffer, indexFormat: GPUIndexFormat, offset?: GPUSize64, size?: GPUSize64): void;
    setPipeline(pipeline: GPURenderPipeline): void;
    setVertexBuffer(slot: GPUIndex32, buffer: GPUBuffer, offset?: GPUSize64, size?: GPUSize64): void;
}

/** Available only in secure contexts. */
interface GPURenderPassEncoder extends GPUBindingCommandsMixin, GPUDebugCommandsMixin, GPUObjectBase, GPURenderCommandsMixin {
    end(): void;
    executeBundles(bundles: GPURenderBundle[]): void;
    setBlendConstant(color: GPUColor): void;
    setScissorRect(x: GPUIntegerCoordinate, y: GPUIntegerCoordinate, width: GPUIntegerCoordinate, height: GPUIntegerCoordinate): void;
    setStencilReference(reference: GPUStencilValue): void;
    setViewport(x: number, y: number, width: number, height: number, minDepth: number, maxDepth: number): void;
}

declare var GPURenderPassEncoder: {
    prototype: GPURenderPassEncoder;
    new(): GPURenderPassEncoder;
    isInstance: IsInstance<GPURenderPassEncoder>;
};

/** Available only in secure contexts. */
interface GPURenderPipeline extends GPUObjectBase, GPUPipelineBase {
}

declare var GPURenderPipeline: {
    prototype: GPURenderPipeline;
    new(): GPURenderPipeline;
    isInstance: IsInstance<GPURenderPipeline>;
};

/** Available only in secure contexts. */
interface GPUSampler extends GPUObjectBase {
}

declare var GPUSampler: {
    prototype: GPUSampler;
    new(): GPUSampler;
    isInstance: IsInstance<GPUSampler>;
};

/** Available only in secure contexts. */
interface GPUShaderModule extends GPUObjectBase {
    getCompilationInfo(): Promise<GPUCompilationInfo>;
}

declare var GPUShaderModule: {
    prototype: GPUShaderModule;
    new(): GPUShaderModule;
    isInstance: IsInstance<GPUShaderModule>;
};

/** Available only in secure contexts. */
interface GPUSupportedFeatures {
    forEach(callbackfn: (value: string, key: string, parent: GPUSupportedFeatures) => void, thisArg?: any): void;
}

declare var GPUSupportedFeatures: {
    prototype: GPUSupportedFeatures;
    new(): GPUSupportedFeatures;
    isInstance: IsInstance<GPUSupportedFeatures>;
};

/** Available only in secure contexts. */
interface GPUSupportedLimits {
    readonly maxBindGroups: number;
    readonly maxBindGroupsPlusVertexBuffers: number;
    readonly maxBindingsPerBindGroup: number;
    readonly maxBufferSize: number;
    readonly maxColorAttachmentBytesPerSample: number;
    readonly maxColorAttachments: number;
    readonly maxComputeInvocationsPerWorkgroup: number;
    readonly maxComputeWorkgroupSizeX: number;
    readonly maxComputeWorkgroupSizeY: number;
    readonly maxComputeWorkgroupSizeZ: number;
    readonly maxComputeWorkgroupStorageSize: number;
    readonly maxComputeWorkgroupsPerDimension: number;
    readonly maxDynamicStorageBuffersPerPipelineLayout: number;
    readonly maxDynamicUniformBuffersPerPipelineLayout: number;
    readonly maxInterStageShaderComponents: number;
    readonly maxInterStageShaderVariables: number;
    readonly maxSampledTexturesPerShaderStage: number;
    readonly maxSamplersPerShaderStage: number;
    readonly maxStorageBufferBindingSize: number;
    readonly maxStorageBuffersPerShaderStage: number;
    readonly maxStorageTexturesPerShaderStage: number;
    readonly maxTextureArrayLayers: number;
    readonly maxTextureDimension1D: number;
    readonly maxTextureDimension2D: number;
    readonly maxTextureDimension3D: number;
    readonly maxUniformBufferBindingSize: number;
    readonly maxUniformBuffersPerShaderStage: number;
    readonly maxVertexAttributes: number;
    readonly maxVertexBufferArrayStride: number;
    readonly maxVertexBuffers: number;
    readonly minStorageBufferOffsetAlignment: number;
    readonly minUniformBufferOffsetAlignment: number;
}

declare var GPUSupportedLimits: {
    prototype: GPUSupportedLimits;
    new(): GPUSupportedLimits;
    isInstance: IsInstance<GPUSupportedLimits>;
};

/** Available only in secure contexts. */
interface GPUTexture extends GPUObjectBase {
    readonly depthOrArrayLayers: GPUIntegerCoordinateOut;
    readonly dimension: GPUTextureDimension;
    readonly format: GPUTextureFormat;
    readonly height: GPUIntegerCoordinateOut;
    readonly mipLevelCount: GPUIntegerCoordinateOut;
    readonly sampleCount: GPUSize32Out;
    readonly usage: GPUFlagsConstant;
    readonly width: GPUIntegerCoordinateOut;
    createView(descriptor?: GPUTextureViewDescriptor): GPUTextureView;
    destroy(): void;
}

declare var GPUTexture: {
    prototype: GPUTexture;
    new(): GPUTexture;
    isInstance: IsInstance<GPUTexture>;
};

/** Available only in secure contexts. */
interface GPUTextureView extends GPUObjectBase {
}

declare var GPUTextureView: {
    prototype: GPUTextureView;
    new(): GPUTextureView;
    isInstance: IsInstance<GPUTextureView>;
};

/** Available only in secure contexts. */
interface GPUUncapturedErrorEvent extends Event {
    readonly error: GPUError;
}

declare var GPUUncapturedErrorEvent: {
    prototype: GPUUncapturedErrorEvent;
    new(type: string, gpuUncapturedErrorEventInitDict: GPUUncapturedErrorEventInit): GPUUncapturedErrorEvent;
    isInstance: IsInstance<GPUUncapturedErrorEvent>;
};

/** Available only in secure contexts. */
interface GPUValidationError extends GPUError {
}

declare var GPUValidationError: {
    prototype: GPUValidationError;
    new(message: string): GPUValidationError;
    isInstance: IsInstance<GPUValidationError>;
};

interface GainNode extends AudioNode, AudioNodePassThrough {
    readonly gain: AudioParam;
}

declare var GainNode: {
    prototype: GainNode;
    new(context: BaseAudioContext, options?: GainOptions): GainNode;
    isInstance: IsInstance<GainNode>;
};

interface Gamepad {
    readonly axes: number[];
    readonly buttons: GamepadButton[];
    readonly connected: boolean;
    readonly displayId: number;
    readonly hand: GamepadHand;
    readonly hapticActuators: GamepadHapticActuator[];
    readonly id: string;
    readonly index: number;
    readonly lightIndicators: GamepadLightIndicator[];
    readonly mapping: GamepadMappingType;
    readonly pose: GamepadPose | null;
    readonly timestamp: DOMHighResTimeStamp;
    readonly touchEvents: GamepadTouch[];
}

declare var Gamepad: {
    prototype: Gamepad;
    new(): Gamepad;
    isInstance: IsInstance<Gamepad>;
};

interface GamepadAxisMoveEvent extends GamepadEvent {
    readonly axis: number;
    readonly value: number;
}

declare var GamepadAxisMoveEvent: {
    prototype: GamepadAxisMoveEvent;
    new(type: string, eventInitDict?: GamepadAxisMoveEventInit): GamepadAxisMoveEvent;
    isInstance: IsInstance<GamepadAxisMoveEvent>;
};

interface GamepadButton {
    readonly pressed: boolean;
    readonly touched: boolean;
    readonly value: number;
}

declare var GamepadButton: {
    prototype: GamepadButton;
    new(): GamepadButton;
    isInstance: IsInstance<GamepadButton>;
};

interface GamepadButtonEvent extends GamepadEvent {
    readonly button: number;
}

declare var GamepadButtonEvent: {
    prototype: GamepadButtonEvent;
    new(type: string, eventInitDict?: GamepadButtonEventInit): GamepadButtonEvent;
    isInstance: IsInstance<GamepadButtonEvent>;
};

interface GamepadEvent extends Event {
    readonly gamepad: Gamepad | null;
}

declare var GamepadEvent: {
    prototype: GamepadEvent;
    new(type: string, eventInitDict?: GamepadEventInit): GamepadEvent;
    isInstance: IsInstance<GamepadEvent>;
};

interface GamepadHapticActuator {
    readonly type: GamepadHapticActuatorType;
    pulse(value: number, duration: number): Promise<boolean>;
}

declare var GamepadHapticActuator: {
    prototype: GamepadHapticActuator;
    new(): GamepadHapticActuator;
    isInstance: IsInstance<GamepadHapticActuator>;
};

interface GamepadLightIndicator {
    readonly type: GamepadLightIndicatorType;
    setColor(color: GamepadLightColor): Promise<boolean>;
}

declare var GamepadLightIndicator: {
    prototype: GamepadLightIndicator;
    new(): GamepadLightIndicator;
    isInstance: IsInstance<GamepadLightIndicator>;
};

interface GamepadPose {
    readonly angularAcceleration: Float32Array | null;
    readonly angularVelocity: Float32Array | null;
    readonly hasOrientation: boolean;
    readonly hasPosition: boolean;
    readonly linearAcceleration: Float32Array | null;
    readonly linearVelocity: Float32Array | null;
    readonly orientation: Float32Array | null;
    readonly position: Float32Array | null;
}

declare var GamepadPose: {
    prototype: GamepadPose;
    new(): GamepadPose;
    isInstance: IsInstance<GamepadPose>;
};

interface GamepadServiceTest {
    readonly leftHand: GamepadHand;
    readonly noHand: GamepadHand;
    readonly noMapping: GamepadMappingType;
    readonly rightHand: GamepadHand;
    readonly standardMapping: GamepadMappingType;
    addGamepad(id: string, mapping: GamepadMappingType, hand: GamepadHand, numButtons: number, numAxes: number, numHaptics: number, numLightIndicator: number, numTouchEvents: number): Promise<number>;
    newAxisMoveEvent(index: number, axis: number, value: number): Promise<number>;
    newButtonEvent(index: number, button: number, pressed: boolean, touched: boolean): Promise<number>;
    newButtonValueEvent(index: number, button: number, pressed: boolean, touched: boolean, value: number): Promise<number>;
    newPoseMove(index: number, orient: Float32Array | null, pos: Float32Array | null, angVelocity: Float32Array | null, angAcceleration: Float32Array | null, linVelocity: Float32Array | null, linAcceleration: Float32Array | null): Promise<number>;
    newTouch(index: number, aTouchArrayIndex: number, touchId: number, surfaceId: number, position: Float32Array, surfaceDimension: Float32Array | null): Promise<number>;
    removeGamepad(index: number): Promise<number>;
}

declare var GamepadServiceTest: {
    prototype: GamepadServiceTest;
    new(): GamepadServiceTest;
    isInstance: IsInstance<GamepadServiceTest>;
};

interface GamepadTouch {
    readonly position: Float32Array;
    readonly surfaceDimensions: Uint32Array | null;
    readonly surfaceId: number;
    readonly touchId: number;
}

declare var GamepadTouch: {
    prototype: GamepadTouch;
    new(): GamepadTouch;
    isInstance: IsInstance<GamepadTouch>;
};

interface GenericTransformStream {
    readonly readable: ReadableStream;
    readonly writable: WritableStream;
}

interface Geolocation {
    clearWatch(watchId: number): void;
    getCurrentPosition(successCallback: PositionCallback, errorCallback?: PositionErrorCallback | null, options?: PositionOptions): void;
    watchPosition(successCallback: PositionCallback, errorCallback?: PositionErrorCallback | null, options?: PositionOptions): number;
}

declare var Geolocation: {
    prototype: Geolocation;
    new(): Geolocation;
    isInstance: IsInstance<Geolocation>;
};

/** Available only in secure contexts. */
interface GeolocationCoordinates {
    readonly accuracy: number;
    readonly altitude: number | null;
    readonly altitudeAccuracy: number | null;
    readonly heading: number | null;
    readonly latitude: number;
    readonly longitude: number;
    readonly speed: number | null;
    toJSON(): any;
}

declare var GeolocationCoordinates: {
    prototype: GeolocationCoordinates;
    new(): GeolocationCoordinates;
    isInstance: IsInstance<GeolocationCoordinates>;
};

/** Available only in secure contexts. */
interface GeolocationPosition {
    readonly coords: GeolocationCoordinates;
    readonly timestamp: EpochTimeStamp;
    toJSON(): any;
}

declare var GeolocationPosition: {
    prototype: GeolocationPosition;
    new(): GeolocationPosition;
    isInstance: IsInstance<GeolocationPosition>;
};

interface GeolocationPositionError {
    readonly code: number;
    readonly message: string;
    readonly PERMISSION_DENIED: 1;
    readonly POSITION_UNAVAILABLE: 2;
    readonly TIMEOUT: 3;
}

declare var GeolocationPositionError: {
    prototype: GeolocationPositionError;
    new(): GeolocationPositionError;
    readonly PERMISSION_DENIED: 1;
    readonly POSITION_UNAVAILABLE: 2;
    readonly TIMEOUT: 3;
    isInstance: IsInstance<GeolocationPositionError>;
};

interface GeometryUtils {
    convertPointFromNode(point: DOMPointInit, from: GeometryNode, options?: ConvertCoordinateOptions): DOMPoint;
    convertQuadFromNode(quad: DOMQuad, from: GeometryNode, options?: ConvertCoordinateOptions): DOMQuad;
    convertRectFromNode(rect: DOMRectReadOnly, from: GeometryNode, options?: ConvertCoordinateOptions): DOMQuad;
    getBoxQuads(options?: BoxQuadOptions): DOMQuad[];
    getBoxQuadsFromWindowOrigin(options?: BoxQuadOptions): DOMQuad[];
}

interface GetUserMediaRequest {
    readonly callID: string;
    readonly devices: nsIMediaDevice[];
    readonly innerWindowID: number;
    readonly isHandlingUserInput: boolean;
    readonly isSecure: boolean;
    readonly mediaSource: string;
    readonly rawID: string;
    readonly type: GetUserMediaRequestType;
    readonly windowID: number;
    getAudioOutputOptions(): AudioOutputOptions;
    getConstraints(): MediaStreamConstraints;
}

interface GleanBoolean extends GleanMetric {
    set(value: boolean): void;
    testGetValue(aPingName?: string): boolean | null;
}

declare var GleanBoolean: {
    prototype: GleanBoolean;
    new(): GleanBoolean;
    isInstance: IsInstance<GleanBoolean>;
};

interface GleanCategory {
}

declare var GleanCategory: {
    prototype: GleanCategory;
    new(): GleanCategory;
    isInstance: IsInstance<GleanCategory>;
};

interface GleanCounter extends GleanMetric {
    add(aAmount?: number): void;
    testGetValue(aPingName?: string): number | null;
}

declare var GleanCounter: {
    prototype: GleanCounter;
    new(): GleanCounter;
    isInstance: IsInstance<GleanCounter>;
};

interface GleanCustomDistribution extends GleanMetric {
    accumulateSamples(aSamples: number[]): void;
    accumulateSingleSample(aSample: number): void;
    testGetValue(aPingName?: string): GleanDistributionData | null;
}

declare var GleanCustomDistribution: {
    prototype: GleanCustomDistribution;
    new(): GleanCustomDistribution;
    isInstance: IsInstance<GleanCustomDistribution>;
};

interface GleanDatetime extends GleanMetric {
    set(aValue?: number): void;
    testGetValue(aPingName?: string): any;
}

declare var GleanDatetime: {
    prototype: GleanDatetime;
    new(): GleanDatetime;
    isInstance: IsInstance<GleanDatetime>;
};

interface GleanDenominator extends GleanMetric {
    add(aAmount?: number): void;
    testGetValue(aPingName?: string): number | null;
}

declare var GleanDenominator: {
    prototype: GleanDenominator;
    new(): GleanDenominator;
    isInstance: IsInstance<GleanDenominator>;
};

interface GleanEvent extends GleanMetric {
    record(aExtra?: Record<string, string | null>): void;
    testGetValue(aPingName?: string): GleanEventRecord[] | null;
}

declare var GleanEvent: {
    prototype: GleanEvent;
    new(): GleanEvent;
    isInstance: IsInstance<GleanEvent>;
};

interface GleanImpl {
}

declare var GleanImpl: {
    prototype: GleanImpl;
    new(): GleanImpl;
    isInstance: IsInstance<GleanImpl>;
};

interface GleanLabeled {
}

declare var GleanLabeled: {
    prototype: GleanLabeled;
    new(): GleanLabeled;
    isInstance: IsInstance<GleanLabeled>;
};

interface GleanMemoryDistribution extends GleanMetric {
    accumulate(aSample: number): void;
    testGetValue(aPingName?: string): GleanDistributionData | null;
}

declare var GleanMemoryDistribution: {
    prototype: GleanMemoryDistribution;
    new(): GleanMemoryDistribution;
    isInstance: IsInstance<GleanMemoryDistribution>;
};

interface GleanMetric {
}

declare var GleanMetric: {
    prototype: GleanMetric;
    new(): GleanMetric;
    isInstance: IsInstance<GleanMetric>;
};

interface GleanNumerator extends GleanMetric {
    addToNumerator(aAmount?: number): void;
    testGetValue(aPingName?: string): GleanRateData | null;
}

declare var GleanNumerator: {
    prototype: GleanNumerator;
    new(): GleanNumerator;
    isInstance: IsInstance<GleanNumerator>;
};

interface GleanObject extends GleanMetric {
    set(value: any): void;
    testGetValue(aPingName?: string): any;
}

declare var GleanObject: {
    prototype: GleanObject;
    new(): GleanObject;
    isInstance: IsInstance<GleanObject>;
};

interface GleanPingsImpl {
}

declare var GleanPingsImpl: {
    prototype: GleanPingsImpl;
    new(): GleanPingsImpl;
    isInstance: IsInstance<GleanPingsImpl>;
};

interface GleanQuantity extends GleanMetric {
    set(aValue: number): void;
    testGetValue(aPingName?: string): number | null;
}

declare var GleanQuantity: {
    prototype: GleanQuantity;
    new(): GleanQuantity;
    isInstance: IsInstance<GleanQuantity>;
};

interface GleanRate extends GleanMetric {
    addToDenominator(aAmount?: number): void;
    addToNumerator(aAmount?: number): void;
    testGetValue(aPingName?: string): GleanRateData | null;
}

declare var GleanRate: {
    prototype: GleanRate;
    new(): GleanRate;
    isInstance: IsInstance<GleanRate>;
};

interface GleanString extends GleanMetric {
    set(aValue: string | null): void;
    testGetValue(aPingName?: string): string | null;
}

declare var GleanString: {
    prototype: GleanString;
    new(): GleanString;
    isInstance: IsInstance<GleanString>;
};

interface GleanStringList extends GleanMetric {
    add(value: string): void;
    set(aValue: string[]): void;
    testGetValue(aPingName?: string): string[] | null;
}

declare var GleanStringList: {
    prototype: GleanStringList;
    new(): GleanStringList;
    isInstance: IsInstance<GleanStringList>;
};

interface GleanText extends GleanMetric {
    set(aValue: string): void;
    testGetValue(aPingName?: string): string | null;
}

declare var GleanText: {
    prototype: GleanText;
    new(): GleanText;
    isInstance: IsInstance<GleanText>;
};

interface GleanTimespan extends GleanMetric {
    cancel(): void;
    setRaw(aDuration: number): void;
    start(): void;
    stop(): void;
    testGetValue(aPingName?: string): number | null;
}

declare var GleanTimespan: {
    prototype: GleanTimespan;
    new(): GleanTimespan;
    isInstance: IsInstance<GleanTimespan>;
};

interface GleanTimingDistribution extends GleanMetric {
    cancel(aId: number): void;
    start(): number;
    stopAndAccumulate(aId: number): void;
    testAccumulateRawMillis(aSample: number): void;
    testGetValue(aPingName?: string): GleanDistributionData | null;
}

declare var GleanTimingDistribution: {
    prototype: GleanTimingDistribution;
    new(): GleanTimingDistribution;
    isInstance: IsInstance<GleanTimingDistribution>;
};

interface GleanUrl extends GleanMetric {
    set(aValue: string): void;
    testGetValue(aPingName?: string): string | null;
}

declare var GleanUrl: {
    prototype: GleanUrl;
    new(): GleanUrl;
    isInstance: IsInstance<GleanUrl>;
};

interface GleanUuid extends GleanMetric {
    generateAndSet(): void;
    set(aValue: string): void;
    testGetValue(aPingName?: string): string | null;
}

declare var GleanUuid: {
    prototype: GleanUuid;
    new(): GleanUuid;
    isInstance: IsInstance<GleanUuid>;
};

interface GlobalCrypto {
    readonly crypto: Crypto;
}

interface GlobalEventHandlersEventMap {
    "abort": Event;
    "animationcancel": Event;
    "animationend": Event;
    "animationiteration": Event;
    "animationstart": Event;
    "auxclick": Event;
    "beforeinput": Event;
    "beforetoggle": Event;
    "blur": Event;
    "cancel": Event;
    "canplay": Event;
    "canplaythrough": Event;
    "change": Event;
    "click": Event;
    "close": Event;
    "contentvisibilityautostatechange": Event;
    "contextlost": Event;
    "contextmenu": Event;
    "contextrestored": Event;
    "copy": Event;
    "cuechange": Event;
    "cut": Event;
    "dblclick": Event;
    "drag": Event;
    "dragend": Event;
    "dragenter": Event;
    "dragexit": Event;
    "dragleave": Event;
    "dragover": Event;
    "dragstart": Event;
    "drop": Event;
    "durationchange": Event;
    "emptied": Event;
    "ended": Event;
    "focus": Event;
    "formdata": Event;
    "gotpointercapture": Event;
    "input": Event;
    "invalid": Event;
    "keydown": Event;
    "keypress": Event;
    "keyup": Event;
    "load": Event;
    "loadeddata": Event;
    "loadedmetadata": Event;
    "loadstart": Event;
    "lostpointercapture": Event;
    "mousedown": Event;
    "mouseenter": Event;
    "mouseleave": Event;
    "mousemove": Event;
    "mouseout": Event;
    "mouseover": Event;
    "mouseup": Event;
    "mozfullscreenchange": Event;
    "mozfullscreenerror": Event;
    "paste": Event;
    "pause": Event;
    "play": Event;
    "playing": Event;
    "pointercancel": Event;
    "pointerdown": Event;
    "pointerenter": Event;
    "pointerleave": Event;
    "pointermove": Event;
    "pointerout": Event;
    "pointerover": Event;
    "pointerup": Event;
    "progress": Event;
    "ratechange": Event;
    "reset": Event;
    "resize": Event;
    "scroll": Event;
    "scrollend": Event;
    "securitypolicyviolation": Event;
    "seeked": Event;
    "seeking": Event;
    "select": Event;
    "selectionchange": Event;
    "selectstart": Event;
    "slotchange": Event;
    "stalled": Event;
    "submit": Event;
    "suspend": Event;
    "timeupdate": Event;
    "toggle": Event;
    "transitioncancel": Event;
    "transitionend": Event;
    "transitionrun": Event;
    "transitionstart": Event;
    "volumechange": Event;
    "waiting": Event;
    "webkitanimationend": Event;
    "webkitanimationiteration": Event;
    "webkitanimationstart": Event;
    "webkittransitionend": Event;
    "wheel": Event;
}

interface GlobalEventHandlers {
    onabort: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onanimationcancel: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onanimationend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onanimationiteration: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onanimationstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onauxclick: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onbeforeinput: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onbeforetoggle: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onblur: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncancel: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncanplay: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncanplaythrough: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onchange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onclick: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onclose: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncontentvisibilityautostatechange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncontextlost: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncontextmenu: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncontextrestored: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncopy: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncuechange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oncut: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondblclick: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondrag: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragenter: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragexit: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragleave: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragover: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondragstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondrop: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ondurationchange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onemptied: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onended: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onfocus: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onformdata: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ongotpointercapture: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oninput: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    oninvalid: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onkeydown: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onkeypress: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onkeyup: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onload: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onloadeddata: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onloadedmetadata: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onloadstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onlostpointercapture: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmousedown: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmouseenter: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmouseleave: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmousemove: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmouseout: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmouseover: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmouseup: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmozfullscreenchange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onmozfullscreenerror: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpaste: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpause: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onplay: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onplaying: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointercancel: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerdown: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerenter: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerleave: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointermove: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerout: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerover: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onpointerup: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onprogress: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onratechange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onreset: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onresize: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onscroll: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onscrollend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onsecuritypolicyviolation: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onseeked: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onseeking: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onselect: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onselectionchange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onselectstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onslotchange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onstalled: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onsubmit: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onsuspend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontimeupdate: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontoggle: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontransitioncancel: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontransitionend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontransitionrun: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    ontransitionstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onvolumechange: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwaiting: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwebkitanimationend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwebkitanimationiteration: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwebkitanimationstart: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwebkittransitionend: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    onwheel: ((this: GlobalEventHandlers, ev: Event) => any) | null;
    addEventListener<K extends keyof GlobalEventHandlersEventMap>(type: K, listener: (this: GlobalEventHandlers, ev: GlobalEventHandlersEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof GlobalEventHandlersEventMap>(type: K, listener: (this: GlobalEventHandlers, ev: GlobalEventHandlersEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface GlobalPrivacyControl {
    readonly globalPrivacyControl: boolean;
}

interface GlobalProcessScriptLoader {
    readonly initialProcessData: any;
    readonly sharedData: MozWritableSharedMap;
}

interface Grid {
    readonly areas: GridArea[];
    readonly cols: GridDimension;
    readonly rows: GridDimension;
}

declare var Grid: {
    prototype: Grid;
    new(): Grid;
    isInstance: IsInstance<Grid>;
};

interface GridArea {
    readonly columnEnd: number;
    readonly columnStart: number;
    readonly name: string;
    readonly rowEnd: number;
    readonly rowStart: number;
    readonly type: GridDeclaration;
}

declare var GridArea: {
    prototype: GridArea;
    new(): GridArea;
    isInstance: IsInstance<GridArea>;
};

interface GridDimension {
    readonly lines: GridLines;
    readonly tracks: GridTracks;
}

declare var GridDimension: {
    prototype: GridDimension;
    new(): GridDimension;
    isInstance: IsInstance<GridDimension>;
};

interface GridLine {
    readonly breadth: number;
    readonly names: string[];
    readonly negativeNumber: number;
    readonly number: number;
    readonly start: number;
    readonly type: GridDeclaration;
}

declare var GridLine: {
    prototype: GridLine;
    new(): GridLine;
    isInstance: IsInstance<GridLine>;
};

interface GridLines {
    readonly length: number;
    item(index: number): GridLine | null;
    [index: number]: GridLine;
}

declare var GridLines: {
    prototype: GridLines;
    new(): GridLines;
    isInstance: IsInstance<GridLines>;
};

interface GridTrack {
    readonly breadth: number;
    readonly start: number;
    readonly state: GridTrackState;
    readonly type: GridDeclaration;
}

declare var GridTrack: {
    prototype: GridTrack;
    new(): GridTrack;
    isInstance: IsInstance<GridTrack>;
};

interface GridTracks {
    readonly length: number;
    item(index: number): GridTrack | null;
    [index: number]: GridTrack;
}

declare var GridTracks: {
    prototype: GridTracks;
    new(): GridTracks;
    isInstance: IsInstance<GridTracks>;
};

interface HTMLAllCollection {
    readonly length: number;
    item(nameOrIndex?: string): HTMLCollection | Element | null;
    namedItem(name: string): HTMLCollection | Element | null;
    [index: number]: Element;
}

declare var HTMLAllCollection: {
    prototype: HTMLAllCollection;
    new(): HTMLAllCollection;
    isInstance: IsInstance<HTMLAllCollection>;
};

interface HTMLAnchorElement extends HTMLElement, HTMLHyperlinkElementUtils {
    charset: string;
    coords: string;
    download: string;
    hreflang: string;
    name: string;
    ping: string;
    referrerPolicy: string;
    rel: string;
    readonly relList: DOMTokenList;
    rev: string;
    shape: string;
    target: string;
    text: string;
    type: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLAnchorElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLAnchorElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLAnchorElement: {
    prototype: HTMLAnchorElement;
    new(): HTMLAnchorElement;
    isInstance: IsInstance<HTMLAnchorElement>;
};

interface HTMLAreaElement extends HTMLElement, HTMLHyperlinkElementUtils {
    alt: string;
    coords: string;
    download: string;
    noHref: boolean;
    ping: string;
    referrerPolicy: string;
    rel: string;
    readonly relList: DOMTokenList;
    shape: string;
    target: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLAreaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLAreaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLAreaElement: {
    prototype: HTMLAreaElement;
    new(): HTMLAreaElement;
    isInstance: IsInstance<HTMLAreaElement>;
};

interface HTMLAudioElement extends HTMLMediaElement {
    addEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLAudioElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLAudioElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLAudioElement: {
    prototype: HTMLAudioElement;
    new(): HTMLAudioElement;
    isInstance: IsInstance<HTMLAudioElement>;
};

interface HTMLBRElement extends HTMLElement {
    clear: string;
    readonly isPaddingForEmptyEditor: boolean;
    readonly isPaddingForEmptyLastLine: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLBRElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLBRElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLBRElement: {
    prototype: HTMLBRElement;
    new(): HTMLBRElement;
    isInstance: IsInstance<HTMLBRElement>;
};

interface HTMLBaseElement extends HTMLElement {
    href: string;
    target: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLBaseElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLBaseElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLBaseElement: {
    prototype: HTMLBaseElement;
    new(): HTMLBaseElement;
    isInstance: IsInstance<HTMLBaseElement>;
};

interface HTMLBodyElementEventMap extends HTMLElementEventMap, WindowEventHandlersEventMap {
}

interface HTMLBodyElement extends HTMLElement, WindowEventHandlers {
    aLink: string;
    background: string;
    bgColor: string;
    link: string;
    text: string;
    vLink: string;
    addEventListener<K extends keyof HTMLBodyElementEventMap>(type: K, listener: (this: HTMLBodyElement, ev: HTMLBodyElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLBodyElementEventMap>(type: K, listener: (this: HTMLBodyElement, ev: HTMLBodyElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLBodyElement: {
    prototype: HTMLBodyElement;
    new(): HTMLBodyElement;
    isInstance: IsInstance<HTMLBodyElement>;
};

interface HTMLButtonElement extends HTMLElement, InvokerElement, PopoverInvokerElement {
    disabled: boolean;
    readonly form: HTMLFormElement | null;
    formAction: string;
    formEnctype: string;
    formMethod: string;
    formNoValidate: boolean;
    formTarget: string;
    readonly labels: NodeList;
    name: string;
    type: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    value: string;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    reportValidity(): boolean;
    setCustomValidity(error: string): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLButtonElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLButtonElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLButtonElement: {
    prototype: HTMLButtonElement;
    new(): HTMLButtonElement;
    isInstance: IsInstance<HTMLButtonElement>;
};

interface HTMLCanvasElement extends HTMLElement {
    height: number;
    mozOpaque: boolean;
    mozPrintCallback: PrintCallback | null;
    width: number;
    captureStream(frameRate?: number): CanvasCaptureMediaStream;
    getContext(contextId: string, contextOptions?: any): nsISupports | null;
    toBlob(callback: BlobCallback, type?: string, encoderOptions?: any): void;
    toDataURL(type?: string, encoderOptions?: any): string;
    transferControlToOffscreen(): OffscreenCanvas;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLCanvasElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLCanvasElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLCanvasElement: {
    prototype: HTMLCanvasElement;
    new(): HTMLCanvasElement;
    isInstance: IsInstance<HTMLCanvasElement>;
};

interface HTMLCollectionBase {
    readonly length: number;
    item(index: number): Element | null;
    [index: number]: Element;
}

interface HTMLCollection extends HTMLCollectionBase {
    namedItem(name: string): Element | null;
}

declare var HTMLCollection: {
    prototype: HTMLCollection;
    new(): HTMLCollection;
    isInstance: IsInstance<HTMLCollection>;
};

interface HTMLDListElement extends HTMLElement {
    compact: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDListElement: {
    prototype: HTMLDListElement;
    new(): HTMLDListElement;
    isInstance: IsInstance<HTMLDListElement>;
};

interface HTMLDataElement extends HTMLElement {
    value: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDataElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDataElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDataElement: {
    prototype: HTMLDataElement;
    new(): HTMLDataElement;
    isInstance: IsInstance<HTMLDataElement>;
};

interface HTMLDataListElement extends HTMLElement {
    readonly options: HTMLCollection;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDataListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDataListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDataListElement: {
    prototype: HTMLDataListElement;
    new(): HTMLDataListElement;
    isInstance: IsInstance<HTMLDataListElement>;
};

interface HTMLDetailsElement extends HTMLElement {
    name: string;
    open: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDetailsElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDetailsElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDetailsElement: {
    prototype: HTMLDetailsElement;
    new(): HTMLDetailsElement;
    isInstance: IsInstance<HTMLDetailsElement>;
};

interface HTMLDialogElement extends HTMLElement {
    open: boolean;
    returnValue: string;
    close(returnValue?: string): void;
    show(): void;
    showModal(): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDialogElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDialogElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDialogElement: {
    prototype: HTMLDialogElement;
    new(): HTMLDialogElement;
    isInstance: IsInstance<HTMLDialogElement>;
};

interface HTMLDirectoryElement extends HTMLElement {
    compact: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDirectoryElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDirectoryElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDirectoryElement: {
    prototype: HTMLDirectoryElement;
    new(): HTMLDirectoryElement;
    isInstance: IsInstance<HTMLDirectoryElement>;
};

interface HTMLDivElement extends HTMLElement {
    align: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDivElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLDivElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLDivElement: {
    prototype: HTMLDivElement;
    new(): HTMLDivElement;
    isInstance: IsInstance<HTMLDivElement>;
};

interface HTMLDocument extends Document {
    addEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: HTMLDocument, ev: DocumentEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: HTMLDocument, ev: DocumentEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [name: string]: any;
}

declare var HTMLDocument: {
    prototype: HTMLDocument;
    new(): HTMLDocument;
    isInstance: IsInstance<HTMLDocument>;
};

interface HTMLElementEventMap extends ElementEventMap, GlobalEventHandlersEventMap, OnErrorEventHandlerForNodesEventMap, TouchEventHandlersEventMap {
}

interface HTMLElement extends Element, ElementCSSInlineStyle, GlobalEventHandlers, HTMLOrForeignElement, OnErrorEventHandlerForNodes, TouchEventHandlers {
    accessKey: string;
    readonly accessKeyLabel: string;
    autocapitalize: string;
    contentEditable: string;
    dir: string;
    draggable: boolean;
    enterKeyHint: string;
    hidden: boolean;
    inert: boolean;
    innerText: string;
    inputMode: string;
    readonly internals: ElementInternals | null;
    readonly isContentEditable: boolean;
    readonly isFormAssociatedCustomElements: boolean;
    lang: string;
    nonce: string;
    readonly offsetHeight: number;
    readonly offsetLeft: number;
    readonly offsetParent: Element | null;
    readonly offsetTop: number;
    readonly offsetWidth: number;
    outerText: string;
    popover: string | null;
    spellcheck: boolean;
    title: string;
    translate: boolean;
    attachInternals(): ElementInternals;
    click(): void;
    hidePopover(): void;
    showPopover(): void;
    togglePopover(force?: boolean): boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLElement: {
    prototype: HTMLElement;
    new(): HTMLElement;
    isInstance: IsInstance<HTMLElement>;
};

interface HTMLEmbedElement extends HTMLElement, MozFrameLoaderOwner, MozObjectLoadingContent {
    align: string;
    height: string;
    name: string;
    src: string;
    type: string;
    width: string;
    getSVGDocument(): Document | null;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLEmbedElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLEmbedElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLEmbedElement: {
    prototype: HTMLEmbedElement;
    new(): HTMLEmbedElement;
    readonly TYPE_LOADING: 0;
    readonly TYPE_DOCUMENT: 1;
    readonly TYPE_FALLBACK: 2;
    isInstance: IsInstance<HTMLEmbedElement>;
};

interface HTMLFieldSetElement extends HTMLElement {
    disabled: boolean;
    readonly elements: HTMLCollection;
    readonly form: HTMLFormElement | null;
    name: string;
    readonly type: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    reportValidity(): boolean;
    setCustomValidity(error: string): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFieldSetElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFieldSetElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLFieldSetElement: {
    prototype: HTMLFieldSetElement;
    new(): HTMLFieldSetElement;
    isInstance: IsInstance<HTMLFieldSetElement>;
};

interface HTMLFontElement extends HTMLElement {
    color: string;
    face: string;
    size: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFontElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFontElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLFontElement: {
    prototype: HTMLFontElement;
    new(): HTMLFontElement;
    isInstance: IsInstance<HTMLFontElement>;
};

interface HTMLFormControlsCollection extends HTMLCollectionBase {
    namedItem(name: string): RadioNodeList | Element | null;
}

declare var HTMLFormControlsCollection: {
    prototype: HTMLFormControlsCollection;
    new(): HTMLFormControlsCollection;
    isInstance: IsInstance<HTMLFormControlsCollection>;
};

interface HTMLFormElement extends HTMLElement {
    acceptCharset: string;
    action: string;
    autocomplete: string;
    readonly elements: HTMLFormControlsCollection;
    encoding: string;
    enctype: string;
    readonly length: number;
    method: string;
    name: string;
    noValidate: boolean;
    rel: string;
    readonly relList: DOMTokenList;
    target: string;
    checkValidity(): boolean;
    reportValidity(): boolean;
    requestSubmit(submitter?: HTMLElement | null): void;
    reset(): void;
    submit(): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFormElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFormElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: Element;
}

declare var HTMLFormElement: {
    prototype: HTMLFormElement;
    new(): HTMLFormElement;
    isInstance: IsInstance<HTMLFormElement>;
};

interface HTMLFrameElement extends HTMLElement, MozFrameLoaderOwner {
    readonly contentDocument: Document | null;
    readonly contentWindow: WindowProxy | null;
    frameBorder: string;
    longDesc: string;
    marginHeight: string;
    marginWidth: string;
    name: string;
    noResize: boolean;
    scrolling: string;
    src: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFrameElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLFrameElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLFrameElement: {
    prototype: HTMLFrameElement;
    new(): HTMLFrameElement;
    isInstance: IsInstance<HTMLFrameElement>;
};

interface HTMLFrameSetElementEventMap extends HTMLElementEventMap, WindowEventHandlersEventMap {
}

interface HTMLFrameSetElement extends HTMLElement, WindowEventHandlers {
    cols: string;
    rows: string;
    addEventListener<K extends keyof HTMLFrameSetElementEventMap>(type: K, listener: (this: HTMLFrameSetElement, ev: HTMLFrameSetElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLFrameSetElementEventMap>(type: K, listener: (this: HTMLFrameSetElement, ev: HTMLFrameSetElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLFrameSetElement: {
    prototype: HTMLFrameSetElement;
    new(): HTMLFrameSetElement;
    isInstance: IsInstance<HTMLFrameSetElement>;
};

interface HTMLHRElement extends HTMLElement {
    align: string;
    color: string;
    noShade: boolean;
    size: string;
    width: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHRElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHRElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLHRElement: {
    prototype: HTMLHRElement;
    new(): HTMLHRElement;
    isInstance: IsInstance<HTMLHRElement>;
};

interface HTMLHeadElement extends HTMLElement {
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHeadElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHeadElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLHeadElement: {
    prototype: HTMLHeadElement;
    new(): HTMLHeadElement;
    isInstance: IsInstance<HTMLHeadElement>;
};

interface HTMLHeadingElement extends HTMLElement {
    align: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHeadingElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHeadingElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLHeadingElement: {
    prototype: HTMLHeadingElement;
    new(): HTMLHeadingElement;
    isInstance: IsInstance<HTMLHeadingElement>;
};

interface HTMLHtmlElement extends HTMLElement {
    version: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHtmlElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLHtmlElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLHtmlElement: {
    prototype: HTMLHtmlElement;
    new(): HTMLHtmlElement;
    isInstance: IsInstance<HTMLHtmlElement>;
};

interface HTMLHyperlinkElementUtils {
    hash: string;
    host: string;
    hostname: string;
    href: string;
    toString(): string;
    readonly origin: string;
    password: string;
    pathname: string;
    port: string;
    protocol: string;
    search: string;
    username: string;
}

interface HTMLIFrameElement extends HTMLElement, MozFrameLoaderOwner {
    align: string;
    allow: string;
    allowFullscreen: boolean;
    readonly contentDocument: Document | null;
    readonly contentWindow: WindowProxy | null;
    readonly featurePolicy: FeaturePolicy;
    frameBorder: string;
    height: string;
    loading: string;
    longDesc: string;
    marginHeight: string;
    marginWidth: string;
    name: string;
    referrerPolicy: string;
    readonly sandbox: DOMTokenList;
    scrolling: string;
    src: string;
    srcdoc: string;
    width: string;
    getSVGDocument(): Document | null;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLIFrameElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLIFrameElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLIFrameElement: {
    prototype: HTMLIFrameElement;
    new(): HTMLIFrameElement;
    isInstance: IsInstance<HTMLIFrameElement>;
};

interface HTMLImageElement extends HTMLElement, MozImageLoadingContent {
    align: string;
    alt: string;
    border: string;
    readonly complete: boolean;
    crossOrigin: string | null;
    readonly currentSrc: string;
    decoding: string;
    fetchPriority: string;
    height: number;
    hspace: number;
    isMap: boolean;
    loading: string;
    longDesc: string;
    lowsrc: string;
    name: string;
    readonly naturalHeight: number;
    readonly naturalWidth: number;
    referrerPolicy: string;
    sizes: string;
    src: string;
    srcset: string;
    useMap: string;
    vspace: number;
    width: number;
    readonly x: number;
    readonly y: number;
    decode(): Promise<void>;
    recognizeCurrentImageText(): Promise<ImageText[]>;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLImageElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLImageElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLImageElement: {
    prototype: HTMLImageElement;
    new(): HTMLImageElement;
    readonly UNKNOWN_REQUEST: -1;
    readonly CURRENT_REQUEST: 0;
    readonly PENDING_REQUEST: 1;
    isInstance: IsInstance<HTMLImageElement>;
};

interface HTMLInputElement extends HTMLElement, InvokerElement, MozEditableElement, MozImageLoadingContent, PopoverInvokerElement {
    accept: string;
    align: string;
    alt: string;
    autocomplete: string;
    autofillState: string;
    capture: string;
    checked: boolean;
    readonly controllers: XULControllers | null;
    readonly dateTimeBoxElement: Element | null;
    defaultChecked: boolean;
    defaultValue: string;
    dirName: string;
    disabled: boolean;
    files: FileList | null;
    readonly form: HTMLFormElement | null;
    formAction: string;
    formEnctype: string;
    formMethod: string;
    formNoValidate: boolean;
    formTarget: string;
    readonly hasBeenTypePassword: boolean;
    height: number;
    indeterminate: boolean;
    readonly labels: NodeList | null;
    readonly lastInteractiveValue: string;
    readonly list: HTMLDataListElement | null;
    max: string;
    maxLength: number;
    min: string;
    minLength: number;
    multiple: boolean;
    name: string;
    pattern: string;
    placeholder: string;
    previewValue: string;
    readOnly: boolean;
    required: boolean;
    revealPassword: boolean;
    selectionDirection: string | null;
    selectionEnd: number | null;
    selectionStart: number | null;
    size: number;
    src: string;
    step: string;
    readonly textLength: number;
    type: string;
    useMap: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    value: string;
    valueAsDate: any;
    valueAsNumber: number;
    readonly webkitEntries: FileSystemEntry[];
    webkitdirectory: boolean;
    width: number;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    closeDateTimePicker(): void;
    getAutocompleteInfo(): AutocompleteInfo | null;
    getDateTimeInputBoxValue(): DateTimeValue;
    getFilesAndDirectories(): Promise<(File | Directory)[]>;
    getMaximum(): number;
    getMinimum(): number;
    getStep(): number;
    getStepBase(): number;
    mozGetFileNameArray(): string[];
    mozIsTextField(aExcludePassword: boolean): boolean;
    mozSetDirectory(directoryPath: string): void;
    mozSetDndFilesAndDirectories(list: (File | Directory)[]): void;
    mozSetFileArray(files: File[]): void;
    mozSetFileNameArray(fileNames: string[]): void;
    openDateTimePicker(initialValue?: DateTimeValue): void;
    reportValidity(): boolean;
    select(): void;
    setCustomValidity(error: string): void;
    setFocusState(aIsFocused: boolean): void;
    setRangeText(replacement: string): void;
    setRangeText(replacement: string, start: number, end: number, selectionMode?: SelectionMode): void;
    setSelectionRange(start: number, end: number, direction?: string): void;
    showPicker(): void;
    stepDown(n?: number): void;
    stepUp(n?: number): void;
    updateDateTimePicker(value?: DateTimeValue): void;
    updateValidityState(): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLInputElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLInputElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLInputElement: {
    prototype: HTMLInputElement;
    new(): HTMLInputElement;
    readonly UNKNOWN_REQUEST: -1;
    readonly CURRENT_REQUEST: 0;
    readonly PENDING_REQUEST: 1;
    isInstance: IsInstance<HTMLInputElement>;
};

interface HTMLLIElement extends HTMLElement {
    type: string;
    value: number;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLIElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLIElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLLIElement: {
    prototype: HTMLLIElement;
    new(): HTMLLIElement;
    isInstance: IsInstance<HTMLLIElement>;
};

interface HTMLLabelElement extends HTMLElement {
    readonly control: HTMLElement | null;
    readonly form: HTMLFormElement | null;
    htmlFor: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLabelElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLabelElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLLabelElement: {
    prototype: HTMLLabelElement;
    new(): HTMLLabelElement;
    isInstance: IsInstance<HTMLLabelElement>;
};

interface HTMLLegendElement extends HTMLElement {
    align: string;
    readonly form: HTMLFormElement | null;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLegendElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLegendElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLLegendElement: {
    prototype: HTMLLegendElement;
    new(): HTMLLegendElement;
    isInstance: IsInstance<HTMLLegendElement>;
};

interface HTMLLinkElement extends HTMLElement, LinkStyle {
    as: string;
    readonly blocking: DOMTokenList;
    charset: string;
    crossOrigin: string | null;
    disabled: boolean;
    fetchPriority: string;
    href: string;
    hreflang: string;
    imageSizes: string;
    imageSrcset: string;
    integrity: string;
    media: string;
    referrerPolicy: string;
    rel: string;
    readonly relList: DOMTokenList;
    rev: string;
    readonly sizes: DOMTokenList;
    target: string;
    type: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLinkElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLLinkElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLLinkElement: {
    prototype: HTMLLinkElement;
    new(): HTMLLinkElement;
    isInstance: IsInstance<HTMLLinkElement>;
};

interface HTMLMapElement extends HTMLElement {
    readonly areas: HTMLCollection;
    name: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMapElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMapElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMapElement: {
    prototype: HTMLMapElement;
    new(): HTMLMapElement;
    isInstance: IsInstance<HTMLMapElement>;
};

interface HTMLMarqueeElement extends HTMLElement {
    behavior: string;
    bgColor: string;
    direction: string;
    height: string;
    hspace: number;
    loop: number;
    scrollAmount: number;
    scrollDelay: number;
    trueSpeed: boolean;
    vspace: number;
    width: string;
    start(): void;
    stop(): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMarqueeElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMarqueeElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMarqueeElement: {
    prototype: HTMLMarqueeElement;
    new(): HTMLMarqueeElement;
    isInstance: IsInstance<HTMLMarqueeElement>;
};

interface HTMLMediaElementEventMap extends HTMLElementEventMap {
    "encrypted": Event;
    "waitingforkey": Event;
}

interface HTMLMediaElement extends HTMLElement {
    readonly allowedToPlay: boolean;
    readonly audiblePlayTime: number;
    readonly audioTracks: AudioTrackList;
    autoplay: boolean;
    readonly buffered: TimeRanges;
    readonly computedMuted: boolean;
    readonly computedVolume: number;
    controls: boolean;
    crossOrigin: string | null;
    readonly currentSrc: string;
    currentTime: number;
    defaultMuted: boolean;
    defaultPlaybackRate: number;
    readonly duration: number;
    readonly ended: boolean;
    readonly error: MediaError | null;
    readonly inaudiblePlayTime: number;
    readonly invisiblePlayTime: number;
    readonly isEncrypted: boolean;
    readonly isInViewPort: boolean;
    readonly isSuspendedByInactiveDocOrDocShell: boolean;
    readonly isVideoDecodingSuspended: boolean;
    loop: boolean;
    readonly mediaKeys: MediaKeys | null;
    mozAllowCasting: boolean;
    readonly mozAudioCaptured: boolean;
    readonly mozFragmentEnd: number;
    mozIsCasting: boolean;
    readonly mozMediaSourceObject: MediaSource | null;
    muted: boolean;
    readonly mutedPlayTime: number;
    readonly networkState: number;
    onencrypted: ((this: HTMLMediaElement, ev: Event) => any) | null;
    onwaitingforkey: ((this: HTMLMediaElement, ev: Event) => any) | null;
    readonly paused: boolean;
    playbackRate: number;
    readonly played: TimeRanges;
    preload: string;
    preservesPitch: boolean;
    readonly readyState: number;
    readonly seekable: TimeRanges;
    readonly seeking: boolean;
    /** Available only in secure contexts. */
    readonly sinkId: string;
    src: string;
    srcObject: MediaStream | null;
    readonly textTracks: TextTrackList | null;
    readonly totalAudioPlayTime: number;
    readonly totalVideoHDRPlayTime: number;
    readonly totalVideoPlayTime: number;
    readonly videoTracks: VideoTrackList;
    readonly visiblePlayTime: number;
    volume: number;
    addTextTrack(kind: TextTrackKind, label?: string, language?: string): TextTrack;
    canPlayType(type: string): string;
    fastSeek(time: number): void;
    hasSuspendTaint(): boolean;
    load(): void;
    mozCaptureStream(): MediaStream;
    mozCaptureStreamUntilEnded(): MediaStream;
    mozGetMetadata(): any;
    mozRequestDebugInfo(): Promise<HTMLMediaElementDebugInfo>;
    mozRequestDebugLog(): Promise<string>;
    pause(): void;
    play(): Promise<void>;
    seekToNextFrame(): Promise<void>;
    setAudioSinkFailedStartup(): void;
    setDecodeError(error: string): void;
    setFormatDiagnosticsReportForMimeType(mimeType: string, error: DecoderDoctorReportType): void;
    setMediaKeys(mediaKeys: MediaKeys | null): Promise<void>;
    /** Available only in secure contexts. */
    setSinkId(sinkId: string): Promise<void>;
    setVisible(aVisible: boolean): void;
    readonly NETWORK_EMPTY: 0;
    readonly NETWORK_IDLE: 1;
    readonly NETWORK_LOADING: 2;
    readonly NETWORK_NO_SOURCE: 3;
    readonly HAVE_NOTHING: 0;
    readonly HAVE_METADATA: 1;
    readonly HAVE_CURRENT_DATA: 2;
    readonly HAVE_FUTURE_DATA: 3;
    readonly HAVE_ENOUGH_DATA: 4;
    addEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLMediaElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLMediaElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMediaElement: {
    prototype: HTMLMediaElement;
    new(): HTMLMediaElement;
    readonly NETWORK_EMPTY: 0;
    readonly NETWORK_IDLE: 1;
    readonly NETWORK_LOADING: 2;
    readonly NETWORK_NO_SOURCE: 3;
    readonly HAVE_NOTHING: 0;
    readonly HAVE_METADATA: 1;
    readonly HAVE_CURRENT_DATA: 2;
    readonly HAVE_FUTURE_DATA: 3;
    readonly HAVE_ENOUGH_DATA: 4;
    isInstance: IsInstance<HTMLMediaElement>;
    mozEnableDebugLog(): void;
};

interface HTMLMenuElement extends HTMLElement {
    compact: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMenuElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMenuElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMenuElement: {
    prototype: HTMLMenuElement;
    new(): HTMLMenuElement;
    isInstance: IsInstance<HTMLMenuElement>;
};

interface HTMLMetaElement extends HTMLElement {
    content: string;
    httpEquiv: string;
    media: string;
    name: string;
    scheme: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMetaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMetaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMetaElement: {
    prototype: HTMLMetaElement;
    new(): HTMLMetaElement;
    isInstance: IsInstance<HTMLMetaElement>;
};

interface HTMLMeterElement extends HTMLElement {
    high: number;
    readonly labels: NodeList;
    low: number;
    max: number;
    min: number;
    optimum: number;
    value: number;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMeterElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLMeterElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLMeterElement: {
    prototype: HTMLMeterElement;
    new(): HTMLMeterElement;
    isInstance: IsInstance<HTMLMeterElement>;
};

interface HTMLModElement extends HTMLElement {
    cite: string;
    dateTime: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLModElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLModElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLModElement: {
    prototype: HTMLModElement;
    new(): HTMLModElement;
    isInstance: IsInstance<HTMLModElement>;
};

interface HTMLOListElement extends HTMLElement {
    compact: boolean;
    reversed: boolean;
    start: number;
    type: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLOListElement: {
    prototype: HTMLOListElement;
    new(): HTMLOListElement;
    isInstance: IsInstance<HTMLOListElement>;
};

interface HTMLObjectElement extends HTMLElement, MozFrameLoaderOwner, MozObjectLoadingContent {
    align: string;
    archive: string;
    border: string;
    code: string;
    codeBase: string;
    codeType: string;
    readonly contentDocument: Document | null;
    readonly contentWindow: WindowProxy | null;
    data: string;
    declare: boolean;
    readonly form: HTMLFormElement | null;
    height: string;
    hspace: number;
    name: string;
    standby: string;
    type: string;
    useMap: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    vspace: number;
    width: string;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    getSVGDocument(): Document | null;
    reportValidity(): boolean;
    setCustomValidity(error: string): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLObjectElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLObjectElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLObjectElement: {
    prototype: HTMLObjectElement;
    new(): HTMLObjectElement;
    readonly TYPE_LOADING: 0;
    readonly TYPE_DOCUMENT: 1;
    readonly TYPE_FALLBACK: 2;
    isInstance: IsInstance<HTMLObjectElement>;
};

interface HTMLOptGroupElement extends HTMLElement {
    disabled: boolean;
    label: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOptGroupElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOptGroupElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLOptGroupElement: {
    prototype: HTMLOptGroupElement;
    new(): HTMLOptGroupElement;
    isInstance: IsInstance<HTMLOptGroupElement>;
};

interface HTMLOptionElement extends HTMLElement {
    defaultSelected: boolean;
    disabled: boolean;
    readonly form: HTMLFormElement | null;
    readonly index: number;
    label: string;
    selected: boolean;
    text: string;
    value: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOptionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOptionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLOptionElement: {
    prototype: HTMLOptionElement;
    new(): HTMLOptionElement;
    isInstance: IsInstance<HTMLOptionElement>;
};

interface HTMLOptionsCollection extends HTMLCollectionBase {
    length: number;
    selectedIndex: number;
    add(element: HTMLOptionElement | HTMLOptGroupElement, before?: HTMLElement | number | null): void;
    remove(index: number): void;
}

declare var HTMLOptionsCollection: {
    prototype: HTMLOptionsCollection;
    new(): HTMLOptionsCollection;
    isInstance: IsInstance<HTMLOptionsCollection>;
};

interface HTMLOrForeignElement {
    autofocus: boolean;
    readonly dataset: DOMStringMap;
    tabIndex: number;
    blur(): void;
    focus(options?: FocusOptions): void;
}

interface HTMLOutputElement extends HTMLElement {
    defaultValue: string;
    readonly form: HTMLFormElement | null;
    readonly htmlFor: DOMTokenList;
    readonly labels: NodeList;
    name: string;
    readonly type: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    value: string;
    readonly willValidate: boolean;
    checkValidity(): boolean;
    reportValidity(): boolean;
    setCustomValidity(error: string): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOutputElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLOutputElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLOutputElement: {
    prototype: HTMLOutputElement;
    new(): HTMLOutputElement;
    isInstance: IsInstance<HTMLOutputElement>;
};

interface HTMLParagraphElement extends HTMLElement {
    align: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLParagraphElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLParagraphElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLParagraphElement: {
    prototype: HTMLParagraphElement;
    new(): HTMLParagraphElement;
    isInstance: IsInstance<HTMLParagraphElement>;
};

interface HTMLParamElement extends HTMLElement {
    name: string;
    type: string;
    value: string;
    valueType: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLParamElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLParamElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLParamElement: {
    prototype: HTMLParamElement;
    new(): HTMLParamElement;
    isInstance: IsInstance<HTMLParamElement>;
};

interface HTMLPictureElement extends HTMLElement {
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLPictureElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLPictureElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLPictureElement: {
    prototype: HTMLPictureElement;
    new(): HTMLPictureElement;
    isInstance: IsInstance<HTMLPictureElement>;
};

interface HTMLPreElement extends HTMLElement {
    width: number;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLPreElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLPreElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLPreElement: {
    prototype: HTMLPreElement;
    new(): HTMLPreElement;
    isInstance: IsInstance<HTMLPreElement>;
};

interface HTMLProgressElement extends HTMLElement {
    readonly labels: NodeList;
    max: number;
    readonly position: number;
    value: number;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLProgressElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLProgressElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLProgressElement: {
    prototype: HTMLProgressElement;
    new(): HTMLProgressElement;
    isInstance: IsInstance<HTMLProgressElement>;
};

interface HTMLQuoteElement extends HTMLElement {
    cite: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLQuoteElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLQuoteElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLQuoteElement: {
    prototype: HTMLQuoteElement;
    new(): HTMLQuoteElement;
    isInstance: IsInstance<HTMLQuoteElement>;
};

interface HTMLScriptElement extends HTMLElement {
    async: boolean;
    readonly blocking: DOMTokenList;
    charset: string;
    crossOrigin: string | null;
    defer: boolean;
    event: string;
    fetchPriority: string;
    htmlFor: string;
    integrity: string;
    noModule: boolean;
    referrerPolicy: string;
    src: string;
    text: string;
    type: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLScriptElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLScriptElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLScriptElement: {
    prototype: HTMLScriptElement;
    new(): HTMLScriptElement;
    isInstance: IsInstance<HTMLScriptElement>;
    supports(type: string): boolean;
};

interface HTMLSelectElement extends HTMLElement {
    autocomplete: string;
    autofillState: string;
    disabled: boolean;
    readonly form: HTMLFormElement | null;
    readonly isCombobox: boolean;
    readonly labels: NodeList;
    length: number;
    multiple: boolean;
    name: string;
    openInParentProcess: boolean;
    readonly options: HTMLOptionsCollection;
    previewValue: string;
    required: boolean;
    selectedIndex: number;
    readonly selectedOptions: HTMLCollection;
    size: number;
    readonly type: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    value: string;
    readonly willValidate: boolean;
    add(element: HTMLOptionElement | HTMLOptGroupElement, before?: HTMLElement | number | null): void;
    checkValidity(): boolean;
    getAutocompleteInfo(): AutocompleteInfo;
    item(index: number): Element | null;
    namedItem(name: string): HTMLOptionElement | null;
    remove(index: number): void;
    remove(): void;
    reportValidity(): boolean;
    setCustomValidity(error: string): void;
    showPicker(): void;
    userFinishedInteracting(changed: boolean): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSelectElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSelectElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: Element;
}

declare var HTMLSelectElement: {
    prototype: HTMLSelectElement;
    new(): HTMLSelectElement;
    isInstance: IsInstance<HTMLSelectElement>;
};

interface HTMLSlotElement extends HTMLElement {
    name: string;
    assign(...nodes: (Element | Text)[]): void;
    assignedElements(options?: AssignedNodesOptions): Element[];
    assignedNodes(options?: AssignedNodesOptions): Node[];
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSlotElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSlotElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLSlotElement: {
    prototype: HTMLSlotElement;
    new(): HTMLSlotElement;
    isInstance: IsInstance<HTMLSlotElement>;
};

interface HTMLSourceElement extends HTMLElement {
    height: number;
    media: string;
    sizes: string;
    src: string;
    srcset: string;
    type: string;
    width: number;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSourceElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSourceElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLSourceElement: {
    prototype: HTMLSourceElement;
    new(): HTMLSourceElement;
    isInstance: IsInstance<HTMLSourceElement>;
};

interface HTMLSpanElement extends HTMLElement {
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSpanElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLSpanElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLSpanElement: {
    prototype: HTMLSpanElement;
    new(): HTMLSpanElement;
    isInstance: IsInstance<HTMLSpanElement>;
};

interface HTMLStyleElement extends HTMLElement, LinkStyle {
    readonly blocking: DOMTokenList;
    disabled: boolean;
    media: string;
    type: string;
    setDevtoolsAsTriggeringPrincipal(): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLStyleElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLStyleElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLStyleElement: {
    prototype: HTMLStyleElement;
    new(): HTMLStyleElement;
    isInstance: IsInstance<HTMLStyleElement>;
};

interface HTMLTableCaptionElement extends HTMLElement {
    align: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableCaptionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableCaptionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableCaptionElement: {
    prototype: HTMLTableCaptionElement;
    new(): HTMLTableCaptionElement;
    isInstance: IsInstance<HTMLTableCaptionElement>;
};

interface HTMLTableCellElement extends HTMLElement {
    abbr: string;
    align: string;
    axis: string;
    bgColor: string;
    readonly cellIndex: number;
    ch: string;
    chOff: string;
    colSpan: number;
    headers: string;
    height: string;
    noWrap: boolean;
    rowSpan: number;
    scope: string;
    vAlign: string;
    width: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableCellElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableCellElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableCellElement: {
    prototype: HTMLTableCellElement;
    new(): HTMLTableCellElement;
    isInstance: IsInstance<HTMLTableCellElement>;
};

interface HTMLTableColElement extends HTMLElement {
    align: string;
    ch: string;
    chOff: string;
    span: number;
    vAlign: string;
    width: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableColElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableColElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableColElement: {
    prototype: HTMLTableColElement;
    new(): HTMLTableColElement;
    isInstance: IsInstance<HTMLTableColElement>;
};

interface HTMLTableElement extends HTMLElement {
    align: string;
    bgColor: string;
    border: string;
    caption: HTMLTableCaptionElement | null;
    cellPadding: string;
    cellSpacing: string;
    frame: string;
    readonly rows: HTMLCollection;
    rules: string;
    summary: string;
    readonly tBodies: HTMLCollection;
    tFoot: HTMLTableSectionElement | null;
    tHead: HTMLTableSectionElement | null;
    width: string;
    createCaption(): HTMLElement;
    createTBody(): HTMLElement;
    createTFoot(): HTMLElement;
    createTHead(): HTMLElement;
    deleteCaption(): void;
    deleteRow(index: number): void;
    deleteTFoot(): void;
    deleteTHead(): void;
    insertRow(index?: number): HTMLElement;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableElement: {
    prototype: HTMLTableElement;
    new(): HTMLTableElement;
    isInstance: IsInstance<HTMLTableElement>;
};

interface HTMLTableRowElement extends HTMLElement {
    align: string;
    bgColor: string;
    readonly cells: HTMLCollection;
    ch: string;
    chOff: string;
    readonly rowIndex: number;
    readonly sectionRowIndex: number;
    vAlign: string;
    deleteCell(index: number): void;
    insertCell(index?: number): HTMLElement;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableRowElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableRowElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableRowElement: {
    prototype: HTMLTableRowElement;
    new(): HTMLTableRowElement;
    isInstance: IsInstance<HTMLTableRowElement>;
};

interface HTMLTableSectionElement extends HTMLElement {
    align: string;
    ch: string;
    chOff: string;
    readonly rows: HTMLCollection;
    vAlign: string;
    deleteRow(index: number): void;
    insertRow(index?: number): HTMLElement;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableSectionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTableSectionElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTableSectionElement: {
    prototype: HTMLTableSectionElement;
    new(): HTMLTableSectionElement;
    isInstance: IsInstance<HTMLTableSectionElement>;
};

interface HTMLTemplateElement extends HTMLElement {
    readonly content: DocumentFragment;
    shadowRootClonable: boolean;
    shadowRootDelegatesFocus: boolean;
    shadowRootMode: string;
    shadowRootSerializable: boolean;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTemplateElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTemplateElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTemplateElement: {
    prototype: HTMLTemplateElement;
    new(): HTMLTemplateElement;
    isInstance: IsInstance<HTMLTemplateElement>;
};

interface HTMLTextAreaElement extends HTMLElement, MozEditableElement {
    autocomplete: string;
    cols: number;
    readonly controllers: XULControllers;
    defaultValue: string;
    dirName: string;
    disabled: boolean;
    readonly form: HTMLFormElement | null;
    readonly labels: NodeList;
    maxLength: number;
    minLength: number;
    name: string;
    placeholder: string;
    previewValue: string;
    readOnly: boolean;
    required: boolean;
    rows: number;
    selectionDirection: string | null;
    selectionEnd: number | null;
    selectionStart: number | null;
    readonly textLength: number;
    readonly type: string;
    readonly validationMessage: string;
    readonly validity: ValidityState;
    value: string;
    readonly willValidate: boolean;
    wrap: string;
    checkValidity(): boolean;
    reportValidity(): boolean;
    select(): void;
    setCustomValidity(error: string): void;
    setRangeText(replacement: string): void;
    setRangeText(replacement: string, start: number, end: number, selectionMode?: SelectionMode): void;
    setSelectionRange(start: number, end: number, direction?: string): void;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTextAreaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTextAreaElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTextAreaElement: {
    prototype: HTMLTextAreaElement;
    new(): HTMLTextAreaElement;
    isInstance: IsInstance<HTMLTextAreaElement>;
};

interface HTMLTimeElement extends HTMLElement {
    dateTime: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTimeElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTimeElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTimeElement: {
    prototype: HTMLTimeElement;
    new(): HTMLTimeElement;
    isInstance: IsInstance<HTMLTimeElement>;
};

interface HTMLTitleElement extends HTMLElement {
    text: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTitleElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTitleElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTitleElement: {
    prototype: HTMLTitleElement;
    new(): HTMLTitleElement;
    isInstance: IsInstance<HTMLTitleElement>;
};

interface HTMLTrackElement extends HTMLElement {
    default: boolean;
    kind: string;
    label: string;
    readonly readyState: number;
    src: string;
    srclang: string;
    readonly track: TextTrack | null;
    readonly NONE: 0;
    readonly LOADING: 1;
    readonly LOADED: 2;
    readonly ERROR: 3;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTrackElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLTrackElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLTrackElement: {
    prototype: HTMLTrackElement;
    new(): HTMLTrackElement;
    readonly NONE: 0;
    readonly LOADING: 1;
    readonly LOADED: 2;
    readonly ERROR: 3;
    isInstance: IsInstance<HTMLTrackElement>;
};

interface HTMLUListElement extends HTMLElement {
    compact: boolean;
    type: string;
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLUListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLUListElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLUListElement: {
    prototype: HTMLUListElement;
    new(): HTMLUListElement;
    isInstance: IsInstance<HTMLUListElement>;
};

interface HTMLUnknownElement extends HTMLElement {
    addEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLUnknownElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(type: K, listener: (this: HTMLUnknownElement, ev: HTMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLUnknownElement: {
    prototype: HTMLUnknownElement;
    new(): HTMLUnknownElement;
    isInstance: IsInstance<HTMLUnknownElement>;
};

interface HTMLVideoElement extends HTMLMediaElement {
    disablePictureInPicture: boolean;
    height: number;
    readonly isCloningElementVisually: boolean;
    readonly mozDecodedFrames: number;
    readonly mozFrameDelay: number;
    readonly mozHasAudio: boolean;
    readonly mozPaintedFrames: number;
    readonly mozParsedFrames: number;
    readonly mozPresentedFrames: number;
    poster: string;
    readonly videoHeight: number;
    readonly videoWidth: number;
    width: number;
    cancelVideoFrameCallback(handle: number): void;
    cloneElementVisually(target: HTMLVideoElement): Promise<void>;
    getVideoPlaybackQuality(): VideoPlaybackQuality;
    requestVideoFrameCallback(callback: VideoFrameRequestCallback): number;
    stopCloningElementVisually(): void;
    addEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLVideoElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof HTMLMediaElementEventMap>(type: K, listener: (this: HTMLVideoElement, ev: HTMLMediaElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var HTMLVideoElement: {
    prototype: HTMLVideoElement;
    new(): HTMLVideoElement;
    isInstance: IsInstance<HTMLVideoElement>;
};

interface HashChangeEvent extends Event {
    readonly newURL: string;
    readonly oldURL: string;
}

declare var HashChangeEvent: {
    prototype: HashChangeEvent;
    new(type: string, eventInitDict?: HashChangeEventInit): HashChangeEvent;
    isInstance: IsInstance<HashChangeEvent>;
};

interface Headers {
    guard: HeadersGuardEnum;
    append(name: string, value: string): void;
    delete(name: string): void;
    get(name: string): string | null;
    getSetCookie(): string[];
    has(name: string): boolean;
    set(name: string, value: string): void;
    forEach(callbackfn: (value: string, key: string, parent: Headers) => void, thisArg?: any): void;
}

declare var Headers: {
    prototype: Headers;
    new(init?: HeadersInit): Headers;
    isInstance: IsInstance<Headers>;
};

interface HeapSnapshot {
    readonly creationTime: number | null;
    computeDominatorTree(): DominatorTree;
    computeShortestPaths(start: NodeId, targets: NodeId[], maxNumPaths: number): any;
    describeNode(breakdown: any, node: NodeId): any;
    takeCensus(options: any): any;
}

declare var HeapSnapshot: {
    prototype: HeapSnapshot;
    new(): HeapSnapshot;
    isInstance: IsInstance<HeapSnapshot>;
};

interface Highlight {
    priority: number;
    type: HighlightType;
    forEach(callbackfn: (value: AbstractRange, key: AbstractRange, parent: Highlight) => void, thisArg?: any): void;
}

declare var Highlight: {
    prototype: Highlight;
    new(...initialRanges: AbstractRange[]): Highlight;
    isInstance: IsInstance<Highlight>;
};

interface HighlightRegistry {
    forEach(callbackfn: (value: Highlight, key: string, parent: HighlightRegistry) => void, thisArg?: any): void;
}

declare var HighlightRegistry: {
    prototype: HighlightRegistry;
    new(): HighlightRegistry;
    isInstance: IsInstance<HighlightRegistry>;
};

interface History {
    readonly length: number;
    scrollRestoration: ScrollRestoration;
    readonly state: any;
    back(): void;
    forward(): void;
    go(delta?: number): void;
    pushState(data: any, title: string, url?: string | null): void;
    replaceState(data: any, title: string, url?: string | null): void;
}

declare var History: {
    prototype: History;
    new(): History;
    isInstance: IsInstance<History>;
};

interface IDBCursor {
    readonly direction: IDBCursorDirection;
    readonly key: any;
    readonly primaryKey: any;
    readonly request: IDBRequest;
    readonly source: IDBObjectStore | IDBIndex;
    advance(count: number): void;
    continue(key?: any): void;
    continuePrimaryKey(key: any, primaryKey: any): void;
    delete(): IDBRequest;
    update(value: any): IDBRequest;
}

declare var IDBCursor: {
    prototype: IDBCursor;
    new(): IDBCursor;
    isInstance: IsInstance<IDBCursor>;
};

interface IDBCursorWithValue extends IDBCursor {
    readonly value: any;
}

declare var IDBCursorWithValue: {
    prototype: IDBCursorWithValue;
    new(): IDBCursorWithValue;
    isInstance: IsInstance<IDBCursorWithValue>;
};

interface IDBDatabaseEventMap {
    "abort": Event;
    "close": Event;
    "error": Event;
    "versionchange": Event;
}

interface IDBDatabase extends EventTarget {
    readonly name: string;
    readonly objectStoreNames: DOMStringList;
    onabort: ((this: IDBDatabase, ev: Event) => any) | null;
    onclose: ((this: IDBDatabase, ev: Event) => any) | null;
    onerror: ((this: IDBDatabase, ev: Event) => any) | null;
    onversionchange: ((this: IDBDatabase, ev: Event) => any) | null;
    readonly version: number;
    close(): void;
    createObjectStore(name: string, options?: IDBObjectStoreParameters): IDBObjectStore;
    deleteObjectStore(name: string): void;
    transaction(storeNames: string | string[], mode?: IDBTransactionMode, options?: IDBTransactionOptions): IDBTransaction;
    addEventListener<K extends keyof IDBDatabaseEventMap>(type: K, listener: (this: IDBDatabase, ev: IDBDatabaseEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof IDBDatabaseEventMap>(type: K, listener: (this: IDBDatabase, ev: IDBDatabaseEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var IDBDatabase: {
    prototype: IDBDatabase;
    new(): IDBDatabase;
    isInstance: IsInstance<IDBDatabase>;
};

interface IDBFactory {
    cmp(first: any, second: any): number;
    databases(): Promise<IDBDatabaseInfo[]>;
    deleteDatabase(name: string): IDBOpenDBRequest;
    deleteForPrincipal(principal: Principal, name: string, options?: IDBOpenDBOptions): IDBOpenDBRequest;
    open(name: string, version?: number): IDBOpenDBRequest;
    openForPrincipal(principal: Principal, name: string, version: number): IDBOpenDBRequest;
    openForPrincipal(principal: Principal, name: string, options?: IDBOpenDBOptions): IDBOpenDBRequest;
}

declare var IDBFactory: {
    prototype: IDBFactory;
    new(): IDBFactory;
    isInstance: IsInstance<IDBFactory>;
};

interface IDBIndex {
    readonly isAutoLocale: boolean;
    readonly keyPath: any;
    readonly locale: string | null;
    readonly multiEntry: boolean;
    name: string;
    readonly objectStore: IDBObjectStore;
    readonly unique: boolean;
    count(query?: any): IDBRequest;
    get(query: any): IDBRequest;
    getAll(query?: any, count?: number): IDBRequest;
    getAllKeys(query?: any, count?: number): IDBRequest;
    getKey(query: any): IDBRequest;
    openCursor(query?: any, direction?: IDBCursorDirection): IDBRequest;
    openKeyCursor(query?: any, direction?: IDBCursorDirection): IDBRequest;
}

declare var IDBIndex: {
    prototype: IDBIndex;
    new(): IDBIndex;
    isInstance: IsInstance<IDBIndex>;
};

interface IDBKeyRange {
    readonly lower: any;
    readonly lowerOpen: boolean;
    readonly upper: any;
    readonly upperOpen: boolean;
    includes(key: any): boolean;
}

declare var IDBKeyRange: {
    prototype: IDBKeyRange;
    new(): IDBKeyRange;
    isInstance: IsInstance<IDBKeyRange>;
    bound(lower: any, upper: any, lowerOpen?: boolean, upperOpen?: boolean): IDBKeyRange;
    lowerBound(lower: any, open?: boolean): IDBKeyRange;
    only(value: any): IDBKeyRange;
    upperBound(upper: any, open?: boolean): IDBKeyRange;
};

interface IDBObjectStore {
    readonly autoIncrement: boolean;
    readonly indexNames: DOMStringList;
    readonly keyPath: any;
    name: string;
    readonly transaction: IDBTransaction;
    add(value: any, key?: any): IDBRequest;
    clear(): IDBRequest;
    count(key?: any): IDBRequest;
    createIndex(name: string, keyPath: string | string[], optionalParameters?: IDBIndexParameters): IDBIndex;
    delete(key: any): IDBRequest;
    deleteIndex(indexName: string): void;
    get(key: any): IDBRequest;
    getAll(query?: any, count?: number): IDBRequest;
    getAllKeys(query?: any, count?: number): IDBRequest;
    getKey(key: any): IDBRequest;
    index(name: string): IDBIndex;
    openCursor(range?: any, direction?: IDBCursorDirection): IDBRequest;
    openKeyCursor(query?: any, direction?: IDBCursorDirection): IDBRequest;
    put(value: any, key?: any): IDBRequest;
}

declare var IDBObjectStore: {
    prototype: IDBObjectStore;
    new(): IDBObjectStore;
    isInstance: IsInstance<IDBObjectStore>;
};

interface IDBOpenDBRequestEventMap extends IDBRequestEventMap {
    "blocked": Event;
    "upgradeneeded": Event;
}

interface IDBOpenDBRequest extends IDBRequest {
    onblocked: ((this: IDBOpenDBRequest, ev: Event) => any) | null;
    onupgradeneeded: ((this: IDBOpenDBRequest, ev: Event) => any) | null;
    addEventListener<K extends keyof IDBOpenDBRequestEventMap>(type: K, listener: (this: IDBOpenDBRequest, ev: IDBOpenDBRequestEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof IDBOpenDBRequestEventMap>(type: K, listener: (this: IDBOpenDBRequest, ev: IDBOpenDBRequestEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var IDBOpenDBRequest: {
    prototype: IDBOpenDBRequest;
    new(): IDBOpenDBRequest;
    isInstance: IsInstance<IDBOpenDBRequest>;
};

interface IDBRequestEventMap {
    "error": Event;
    "success": Event;
}

interface IDBRequest extends EventTarget {
    readonly error: DOMException | null;
    onerror: ((this: IDBRequest, ev: Event) => any) | null;
    onsuccess: ((this: IDBRequest, ev: Event) => any) | null;
    readonly readyState: IDBRequestReadyState;
    readonly result: any;
    readonly source: IDBObjectStore | IDBIndex | IDBCursor | null;
    readonly transaction: IDBTransaction | null;
    addEventListener<K extends keyof IDBRequestEventMap>(type: K, listener: (this: IDBRequest, ev: IDBRequestEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof IDBRequestEventMap>(type: K, listener: (this: IDBRequest, ev: IDBRequestEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var IDBRequest: {
    prototype: IDBRequest;
    new(): IDBRequest;
    isInstance: IsInstance<IDBRequest>;
};

interface IDBTransactionEventMap {
    "abort": Event;
    "complete": Event;
    "error": Event;
}

interface IDBTransaction extends EventTarget {
    readonly db: IDBDatabase;
    readonly durability: IDBTransactionDurability;
    readonly error: DOMException | null;
    readonly mode: IDBTransactionMode;
    readonly objectStoreNames: DOMStringList;
    onabort: ((this: IDBTransaction, ev: Event) => any) | null;
    oncomplete: ((this: IDBTransaction, ev: Event) => any) | null;
    onerror: ((this: IDBTransaction, ev: Event) => any) | null;
    abort(): void;
    commit(): void;
    objectStore(name: string): IDBObjectStore;
    addEventListener<K extends keyof IDBTransactionEventMap>(type: K, listener: (this: IDBTransaction, ev: IDBTransactionEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof IDBTransactionEventMap>(type: K, listener: (this: IDBTransaction, ev: IDBTransactionEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var IDBTransaction: {
    prototype: IDBTransaction;
    new(): IDBTransaction;
    isInstance: IsInstance<IDBTransaction>;
};

interface IDBVersionChangeEvent extends Event {
    readonly newVersion: number | null;
    readonly oldVersion: number;
}

declare var IDBVersionChangeEvent: {
    prototype: IDBVersionChangeEvent;
    new(type: string, eventInitDict?: IDBVersionChangeEventInit): IDBVersionChangeEvent;
    isInstance: IsInstance<IDBVersionChangeEvent>;
};

interface IIRFilterNode extends AudioNode, AudioNodePassThrough {
    getFrequencyResponse(frequencyHz: Float32Array, magResponse: Float32Array, phaseResponse: Float32Array): void;
}

declare var IIRFilterNode: {
    prototype: IIRFilterNode;
    new(context: BaseAudioContext, options: IIRFilterOptions): IIRFilterNode;
    isInstance: IsInstance<IIRFilterNode>;
};

/** Available only in secure contexts. */
interface IdentityCredential extends Credential {
    readonly origin: string;
    readonly token: string | null;
}

declare var IdentityCredential: {
    prototype: IdentityCredential;
    new(init: IdentityCredentialInit): IdentityCredential;
    isInstance: IsInstance<IdentityCredential>;
};

interface IdleDeadline {
    readonly didTimeout: boolean;
    timeRemaining(): DOMHighResTimeStamp;
}

declare var IdleDeadline: {
    prototype: IdleDeadline;
    new(): IdleDeadline;
    isInstance: IsInstance<IdleDeadline>;
};

interface ImageBitmap {
    readonly height: number;
    readonly width: number;
    close(): void;
}

declare var ImageBitmap: {
    prototype: ImageBitmap;
    new(): ImageBitmap;
    isInstance: IsInstance<ImageBitmap>;
};

interface ImageBitmapRenderingContext {
    readonly canvas: CanvasSource | null;
    transferFromImageBitmap(bitmap: ImageBitmap | null): void;
    transferImageBitmap(bitmap: ImageBitmap): void;
}

declare var ImageBitmapRenderingContext: {
    prototype: ImageBitmapRenderingContext;
    new(): ImageBitmapRenderingContext;
    isInstance: IsInstance<ImageBitmapRenderingContext>;
};

interface ImageCaptureEventMap {
    "error": Event;
    "photo": Event;
}

interface ImageCapture extends EventTarget {
    onerror: ((this: ImageCapture, ev: Event) => any) | null;
    onphoto: ((this: ImageCapture, ev: Event) => any) | null;
    readonly videoStreamTrack: MediaStreamTrack;
    takePhoto(): void;
    addEventListener<K extends keyof ImageCaptureEventMap>(type: K, listener: (this: ImageCapture, ev: ImageCaptureEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ImageCaptureEventMap>(type: K, listener: (this: ImageCapture, ev: ImageCaptureEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ImageCapture: {
    prototype: ImageCapture;
    new(track: MediaStreamTrack): ImageCapture;
    isInstance: IsInstance<ImageCapture>;
};

interface ImageCaptureError {
    readonly code: number;
    readonly message: string;
    readonly FRAME_GRAB_ERROR: 1;
    readonly SETTINGS_ERROR: 2;
    readonly PHOTO_ERROR: 3;
    readonly ERROR_UNKNOWN: 4;
}

interface ImageCaptureErrorEvent extends Event {
    readonly imageCaptureError: ImageCaptureError | null;
}

declare var ImageCaptureErrorEvent: {
    prototype: ImageCaptureErrorEvent;
    new(type: string, imageCaptureErrorInitDict?: ImageCaptureErrorEventInit): ImageCaptureErrorEvent;
    isInstance: IsInstance<ImageCaptureErrorEvent>;
};

interface ImageData {
    readonly data: Uint8ClampedArray;
    readonly height: number;
    readonly width: number;
}

declare var ImageData: {
    prototype: ImageData;
    new(sw: number, sh: number): ImageData;
    new(data: Uint8ClampedArray, sw: number, sh?: number): ImageData;
    isInstance: IsInstance<ImageData>;
};

/** Available only in secure contexts. */
interface ImageDecoder {
    readonly complete: boolean;
    readonly completed: Promise<undefined>;
    readonly tracks: ImageTrackList;
    readonly type: string;
    close(): void;
    decode(options?: ImageDecodeOptions): Promise<ImageDecodeResult>;
    reset(): void;
}

declare var ImageDecoder: {
    prototype: ImageDecoder;
    new(init: ImageDecoderInit): ImageDecoder;
    isInstance: IsInstance<ImageDecoder>;
    isTypeSupported(type: string): Promise<boolean>;
};

interface ImageDocument extends HTMLDocument {
    readonly imageIsOverflowing: boolean;
    readonly imageIsResized: boolean;
    restoreImage(): void;
    shrinkToFit(): void;
    addEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: ImageDocument, ev: DocumentEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: ImageDocument, ev: DocumentEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ImageDocument: {
    prototype: ImageDocument;
    new(): ImageDocument;
    isInstance: IsInstance<ImageDocument>;
};

/** Available only in secure contexts. */
interface ImageTrack {
    readonly animated: boolean;
    readonly frameCount: number;
    readonly repetitionCount: number;
    selected: boolean;
}

declare var ImageTrack: {
    prototype: ImageTrack;
    new(): ImageTrack;
    isInstance: IsInstance<ImageTrack>;
};

/** Available only in secure contexts. */
interface ImageTrackList {
    readonly length: number;
    readonly ready: Promise<undefined>;
    readonly selectedIndex: number;
    readonly selectedTrack: ImageTrack | null;
    [index: number]: ImageTrack;
}

declare var ImageTrackList: {
    prototype: ImageTrackList;
    new(): ImageTrackList;
    isInstance: IsInstance<ImageTrackList>;
};

interface InputEvent extends UIEvent {
    readonly data: string | null;
    readonly dataTransfer: DataTransfer | null;
    readonly inputType: string;
    readonly isComposing: boolean;
    getTargetRanges(): StaticRange[];
}

declare var InputEvent: {
    prototype: InputEvent;
    new(type: string, eventInitDict?: InputEventInit): InputEvent;
    isInstance: IsInstance<InputEvent>;
};

interface InputStream {
}

interface InspectorCSSParser {
    readonly columnNumber: number;
    readonly lineNumber: number;
    nextToken(): InspectorCSSToken | null;
}

declare var InspectorCSSParser: {
    prototype: InspectorCSSParser;
    new(text: string): InspectorCSSParser;
    isInstance: IsInstance<InspectorCSSParser>;
};

interface InspectorFontFace {
    readonly CSSFamilyName: string;
    readonly CSSGeneric: string;
    readonly URI: string;
    readonly format: string;
    readonly fromFontGroup: boolean;
    readonly fromLanguagePrefs: boolean;
    readonly fromSystemFallback: boolean;
    readonly localName: string;
    readonly metadata: string;
    readonly name: string;
    readonly ranges: Range[];
    readonly rule: CSSFontFaceRule | null;
    readonly srcIndex: number;
    getFeatures(): InspectorFontFeature[];
    getVariationAxes(): InspectorVariationAxis[];
    getVariationInstances(): InspectorVariationInstance[];
}

declare var InspectorFontFace: {
    prototype: InspectorFontFace;
    new(): InspectorFontFace;
    isInstance: IsInstance<InspectorFontFace>;
};

interface InstallTriggerImpl {
    enabled(): boolean;
    install(installs: Record<string, string | InstallTriggerData>, callback?: InstallTriggerCallback): boolean;
    installChrome(type: number, url: string, skin: string): boolean;
    startSoftwareUpdate(url: string, flags?: number): boolean;
    updateEnabled(): boolean;
    readonly SKIN: 1;
    readonly LOCALE: 2;
    readonly CONTENT: 4;
    readonly PACKAGE: 7;
}

declare var InstallTriggerImpl: {
    prototype: InstallTriggerImpl;
    new(): InstallTriggerImpl;
    readonly SKIN: 1;
    readonly LOCALE: 2;
    readonly CONTENT: 4;
    readonly PACKAGE: 7;
    isInstance: IsInstance<InstallTriggerImpl>;
};

interface IntersectionObserver {
    readonly root: Node | null;
    readonly rootMargin: string;
    readonly thresholds: number[];
    disconnect(): void;
    observe(target: Element): void;
    takeRecords(): IntersectionObserverEntry[];
    unobserve(target: Element): void;
}

declare var IntersectionObserver: {
    prototype: IntersectionObserver;
    new(intersectionCallback: IntersectionCallback, options?: IntersectionObserverInit): IntersectionObserver;
    isInstance: IsInstance<IntersectionObserver>;
};

interface IntersectionObserverEntry {
    readonly boundingClientRect: DOMRectReadOnly;
    readonly intersectionRatio: number;
    readonly intersectionRect: DOMRectReadOnly;
    readonly isIntersecting: boolean;
    readonly rootBounds: DOMRectReadOnly | null;
    readonly target: Element;
    readonly time: DOMHighResTimeStamp;
}

declare var IntersectionObserverEntry: {
    prototype: IntersectionObserverEntry;
    new(): IntersectionObserverEntry;
    isInstance: IsInstance<IntersectionObserverEntry>;
};

interface IntlUtils {
    getDisplayNames(locales: string[], options?: DisplayNameOptions): DisplayNameResult;
    isAppLocaleRTL(): boolean;
}

interface InvokeEvent extends Event {
    readonly action: string;
    readonly invoker: Element | null;
}

declare var InvokeEvent: {
    prototype: InvokeEvent;
    new(type: string, eventInitDict?: InvokeEventInit): InvokeEvent;
    isInstance: IsInstance<InvokeEvent>;
};

interface InvokerElement {
    invokeAction: string;
    invokeTargetElement: Element | null;
}


interface JSActor {
    readonly name: string;
    sendAsyncMessage(messageName: string, obj?: any, transferables?: any): void;
    sendQuery(messageName: string, obj?: any): Promise<any>;
}

interface JSProcessActorChild extends JSActor {
    readonly manager: nsIDOMProcessChild;
}

declare var JSProcessActorChild: {
    prototype: JSProcessActorChild;
    new(): JSProcessActorChild;
    isInstance: IsInstance<JSProcessActorChild>;
};

interface JSProcessActorParent extends JSActor {
    readonly manager: nsIDOMProcessParent;
}

declare var JSProcessActorParent: {
    prototype: JSProcessActorParent;
    new(): JSProcessActorParent;
    isInstance: IsInstance<JSProcessActorParent>;
};

interface JSString {
}

interface JSWindowActorChild extends JSActor {
    readonly browsingContext: BrowsingContext | null;
    readonly contentWindow: WindowProxy | null;
    readonly docShell: nsIDocShell | null;
    readonly document: Document | null;
    readonly manager: WindowGlobalChild | null;
    readonly windowContext: WindowContext | null;
}

declare var JSWindowActorChild: {
    prototype: JSWindowActorChild;
    new(): JSWindowActorChild;
    isInstance: IsInstance<JSWindowActorChild>;
};

interface JSWindowActorParent extends JSActor {
    readonly browsingContext: CanonicalBrowsingContext | null;
    readonly manager: WindowGlobalParent | null;
    readonly windowContext: WindowContext | null;
}

declare var JSWindowActorParent: {
    prototype: JSWindowActorParent;
    new(): JSWindowActorParent;
    isInstance: IsInstance<JSWindowActorParent>;
};

interface KeyEvent extends KeyEventMixin {
}

declare var KeyEvent: {
    prototype: KeyEvent;
    new(): KeyEvent;
    readonly DOM_VK_CANCEL: 0x03;
    readonly DOM_VK_HELP: 0x06;
    readonly DOM_VK_BACK_SPACE: 0x08;
    readonly DOM_VK_TAB: 0x09;
    readonly DOM_VK_CLEAR: 0x0C;
    readonly DOM_VK_RETURN: 0x0D;
    readonly DOM_VK_SHIFT: 0x10;
    readonly DOM_VK_CONTROL: 0x11;
    readonly DOM_VK_ALT: 0x12;
    readonly DOM_VK_PAUSE: 0x13;
    readonly DOM_VK_CAPS_LOCK: 0x14;
    readonly DOM_VK_KANA: 0x15;
    readonly DOM_VK_HANGUL: 0x15;
    readonly DOM_VK_EISU: 0x16;
    readonly DOM_VK_JUNJA: 0x17;
    readonly DOM_VK_FINAL: 0x18;
    readonly DOM_VK_HANJA: 0x19;
    readonly DOM_VK_KANJI: 0x19;
    readonly DOM_VK_ESCAPE: 0x1B;
    readonly DOM_VK_CONVERT: 0x1C;
    readonly DOM_VK_NONCONVERT: 0x1D;
    readonly DOM_VK_ACCEPT: 0x1E;
    readonly DOM_VK_MODECHANGE: 0x1F;
    readonly DOM_VK_SPACE: 0x20;
    readonly DOM_VK_PAGE_UP: 0x21;
    readonly DOM_VK_PAGE_DOWN: 0x22;
    readonly DOM_VK_END: 0x23;
    readonly DOM_VK_HOME: 0x24;
    readonly DOM_VK_LEFT: 0x25;
    readonly DOM_VK_UP: 0x26;
    readonly DOM_VK_RIGHT: 0x27;
    readonly DOM_VK_DOWN: 0x28;
    readonly DOM_VK_SELECT: 0x29;
    readonly DOM_VK_PRINT: 0x2A;
    readonly DOM_VK_EXECUTE: 0x2B;
    readonly DOM_VK_PRINTSCREEN: 0x2C;
    readonly DOM_VK_INSERT: 0x2D;
    readonly DOM_VK_DELETE: 0x2E;
    readonly DOM_VK_0: 0x30;
    readonly DOM_VK_1: 0x31;
    readonly DOM_VK_2: 0x32;
    readonly DOM_VK_3: 0x33;
    readonly DOM_VK_4: 0x34;
    readonly DOM_VK_5: 0x35;
    readonly DOM_VK_6: 0x36;
    readonly DOM_VK_7: 0x37;
    readonly DOM_VK_8: 0x38;
    readonly DOM_VK_9: 0x39;
    readonly DOM_VK_COLON: 0x3A;
    readonly DOM_VK_SEMICOLON: 0x3B;
    readonly DOM_VK_LESS_THAN: 0x3C;
    readonly DOM_VK_EQUALS: 0x3D;
    readonly DOM_VK_GREATER_THAN: 0x3E;
    readonly DOM_VK_QUESTION_MARK: 0x3F;
    readonly DOM_VK_AT: 0x40;
    readonly DOM_VK_A: 0x41;
    readonly DOM_VK_B: 0x42;
    readonly DOM_VK_C: 0x43;
    readonly DOM_VK_D: 0x44;
    readonly DOM_VK_E: 0x45;
    readonly DOM_VK_F: 0x46;
    readonly DOM_VK_G: 0x47;
    readonly DOM_VK_H: 0x48;
    readonly DOM_VK_I: 0x49;
    readonly DOM_VK_J: 0x4A;
    readonly DOM_VK_K: 0x4B;
    readonly DOM_VK_L: 0x4C;
    readonly DOM_VK_M: 0x4D;
    readonly DOM_VK_N: 0x4E;
    readonly DOM_VK_O: 0x4F;
    readonly DOM_VK_P: 0x50;
    readonly DOM_VK_Q: 0x51;
    readonly DOM_VK_R: 0x52;
    readonly DOM_VK_S: 0x53;
    readonly DOM_VK_T: 0x54;
    readonly DOM_VK_U: 0x55;
    readonly DOM_VK_V: 0x56;
    readonly DOM_VK_W: 0x57;
    readonly DOM_VK_X: 0x58;
    readonly DOM_VK_Y: 0x59;
    readonly DOM_VK_Z: 0x5A;
    readonly DOM_VK_WIN: 0x5B;
    readonly DOM_VK_CONTEXT_MENU: 0x5D;
    readonly DOM_VK_SLEEP: 0x5F;
    readonly DOM_VK_NUMPAD0: 0x60;
    readonly DOM_VK_NUMPAD1: 0x61;
    readonly DOM_VK_NUMPAD2: 0x62;
    readonly DOM_VK_NUMPAD3: 0x63;
    readonly DOM_VK_NUMPAD4: 0x64;
    readonly DOM_VK_NUMPAD5: 0x65;
    readonly DOM_VK_NUMPAD6: 0x66;
    readonly DOM_VK_NUMPAD7: 0x67;
    readonly DOM_VK_NUMPAD8: 0x68;
    readonly DOM_VK_NUMPAD9: 0x69;
    readonly DOM_VK_MULTIPLY: 0x6A;
    readonly DOM_VK_ADD: 0x6B;
    readonly DOM_VK_SEPARATOR: 0x6C;
    readonly DOM_VK_SUBTRACT: 0x6D;
    readonly DOM_VK_DECIMAL: 0x6E;
    readonly DOM_VK_DIVIDE: 0x6F;
    readonly DOM_VK_F1: 0x70;
    readonly DOM_VK_F2: 0x71;
    readonly DOM_VK_F3: 0x72;
    readonly DOM_VK_F4: 0x73;
    readonly DOM_VK_F5: 0x74;
    readonly DOM_VK_F6: 0x75;
    readonly DOM_VK_F7: 0x76;
    readonly DOM_VK_F8: 0x77;
    readonly DOM_VK_F9: 0x78;
    readonly DOM_VK_F10: 0x79;
    readonly DOM_VK_F11: 0x7A;
    readonly DOM_VK_F12: 0x7B;
    readonly DOM_VK_F13: 0x7C;
    readonly DOM_VK_F14: 0x7D;
    readonly DOM_VK_F15: 0x7E;
    readonly DOM_VK_F16: 0x7F;
    readonly DOM_VK_F17: 0x80;
    readonly DOM_VK_F18: 0x81;
    readonly DOM_VK_F19: 0x82;
    readonly DOM_VK_F20: 0x83;
    readonly DOM_VK_F21: 0x84;
    readonly DOM_VK_F22: 0x85;
    readonly DOM_VK_F23: 0x86;
    readonly DOM_VK_F24: 0x87;
    readonly DOM_VK_NUM_LOCK: 0x90;
    readonly DOM_VK_SCROLL_LOCK: 0x91;
    readonly DOM_VK_WIN_OEM_FJ_JISHO: 0x92;
    readonly DOM_VK_WIN_OEM_FJ_MASSHOU: 0x93;
    readonly DOM_VK_WIN_OEM_FJ_TOUROKU: 0x94;
    readonly DOM_VK_WIN_OEM_FJ_LOYA: 0x95;
    readonly DOM_VK_WIN_OEM_FJ_ROYA: 0x96;
    readonly DOM_VK_CIRCUMFLEX: 0xA0;
    readonly DOM_VK_EXCLAMATION: 0xA1;
    readonly DOM_VK_DOUBLE_QUOTE: 0xA2;
    readonly DOM_VK_HASH: 0xA3;
    readonly DOM_VK_DOLLAR: 0xA4;
    readonly DOM_VK_PERCENT: 0xA5;
    readonly DOM_VK_AMPERSAND: 0xA6;
    readonly DOM_VK_UNDERSCORE: 0xA7;
    readonly DOM_VK_OPEN_PAREN: 0xA8;
    readonly DOM_VK_CLOSE_PAREN: 0xA9;
    readonly DOM_VK_ASTERISK: 0xAA;
    readonly DOM_VK_PLUS: 0xAB;
    readonly DOM_VK_PIPE: 0xAC;
    readonly DOM_VK_HYPHEN_MINUS: 0xAD;
    readonly DOM_VK_OPEN_CURLY_BRACKET: 0xAE;
    readonly DOM_VK_CLOSE_CURLY_BRACKET: 0xAF;
    readonly DOM_VK_TILDE: 0xB0;
    readonly DOM_VK_VOLUME_MUTE: 0xB5;
    readonly DOM_VK_VOLUME_DOWN: 0xB6;
    readonly DOM_VK_VOLUME_UP: 0xB7;
    readonly DOM_VK_COMMA: 0xBC;
    readonly DOM_VK_PERIOD: 0xBE;
    readonly DOM_VK_SLASH: 0xBF;
    readonly DOM_VK_BACK_QUOTE: 0xC0;
    readonly DOM_VK_OPEN_BRACKET: 0xDB;
    readonly DOM_VK_BACK_SLASH: 0xDC;
    readonly DOM_VK_CLOSE_BRACKET: 0xDD;
    readonly DOM_VK_QUOTE: 0xDE;
    readonly DOM_VK_META: 0xE0;
    readonly DOM_VK_ALTGR: 0xE1;
    readonly DOM_VK_WIN_ICO_HELP: 0xE3;
    readonly DOM_VK_WIN_ICO_00: 0xE4;
    readonly DOM_VK_PROCESSKEY: 0xE5;
    readonly DOM_VK_WIN_ICO_CLEAR: 0xE6;
    readonly DOM_VK_WIN_OEM_RESET: 0xE9;
    readonly DOM_VK_WIN_OEM_JUMP: 0xEA;
    readonly DOM_VK_WIN_OEM_PA1: 0xEB;
    readonly DOM_VK_WIN_OEM_PA2: 0xEC;
    readonly DOM_VK_WIN_OEM_PA3: 0xED;
    readonly DOM_VK_WIN_OEM_WSCTRL: 0xEE;
    readonly DOM_VK_WIN_OEM_CUSEL: 0xEF;
    readonly DOM_VK_WIN_OEM_ATTN: 0xF0;
    readonly DOM_VK_WIN_OEM_FINISH: 0xF1;
    readonly DOM_VK_WIN_OEM_COPY: 0xF2;
    readonly DOM_VK_WIN_OEM_AUTO: 0xF3;
    readonly DOM_VK_WIN_OEM_ENLW: 0xF4;
    readonly DOM_VK_WIN_OEM_BACKTAB: 0xF5;
    readonly DOM_VK_ATTN: 0xF6;
    readonly DOM_VK_CRSEL: 0xF7;
    readonly DOM_VK_EXSEL: 0xF8;
    readonly DOM_VK_EREOF: 0xF9;
    readonly DOM_VK_PLAY: 0xFA;
    readonly DOM_VK_ZOOM: 0xFB;
    readonly DOM_VK_PA1: 0xFD;
    readonly DOM_VK_WIN_OEM_CLEAR: 0xFE;
    isInstance: IsInstance<KeyEvent>;
};

interface KeyEventMixin {
    initKeyEvent(type: string, canBubble?: boolean, cancelable?: boolean, view?: Window | null, ctrlKey?: boolean, altKey?: boolean, shiftKey?: boolean, metaKey?: boolean, keyCode?: number, charCode?: number): void;
    readonly DOM_VK_CANCEL: 0x03;
    readonly DOM_VK_HELP: 0x06;
    readonly DOM_VK_BACK_SPACE: 0x08;
    readonly DOM_VK_TAB: 0x09;
    readonly DOM_VK_CLEAR: 0x0C;
    readonly DOM_VK_RETURN: 0x0D;
    readonly DOM_VK_SHIFT: 0x10;
    readonly DOM_VK_CONTROL: 0x11;
    readonly DOM_VK_ALT: 0x12;
    readonly DOM_VK_PAUSE: 0x13;
    readonly DOM_VK_CAPS_LOCK: 0x14;
    readonly DOM_VK_KANA: 0x15;
    readonly DOM_VK_HANGUL: 0x15;
    readonly DOM_VK_EISU: 0x16;
    readonly DOM_VK_JUNJA: 0x17;
    readonly DOM_VK_FINAL: 0x18;
    readonly DOM_VK_HANJA: 0x19;
    readonly DOM_VK_KANJI: 0x19;
    readonly DOM_VK_ESCAPE: 0x1B;
    readonly DOM_VK_CONVERT: 0x1C;
    readonly DOM_VK_NONCONVERT: 0x1D;
    readonly DOM_VK_ACCEPT: 0x1E;
    readonly DOM_VK_MODECHANGE: 0x1F;
    readonly DOM_VK_SPACE: 0x20;
    readonly DOM_VK_PAGE_UP: 0x21;
    readonly DOM_VK_PAGE_DOWN: 0x22;
    readonly DOM_VK_END: 0x23;
    readonly DOM_VK_HOME: 0x24;
    readonly DOM_VK_LEFT: 0x25;
    readonly DOM_VK_UP: 0x26;
    readonly DOM_VK_RIGHT: 0x27;
    readonly DOM_VK_DOWN: 0x28;
    readonly DOM_VK_SELECT: 0x29;
    readonly DOM_VK_PRINT: 0x2A;
    readonly DOM_VK_EXECUTE: 0x2B;
    readonly DOM_VK_PRINTSCREEN: 0x2C;
    readonly DOM_VK_INSERT: 0x2D;
    readonly DOM_VK_DELETE: 0x2E;
    readonly DOM_VK_0: 0x30;
    readonly DOM_VK_1: 0x31;
    readonly DOM_VK_2: 0x32;
    readonly DOM_VK_3: 0x33;
    readonly DOM_VK_4: 0x34;
    readonly DOM_VK_5: 0x35;
    readonly DOM_VK_6: 0x36;
    readonly DOM_VK_7: 0x37;
    readonly DOM_VK_8: 0x38;
    readonly DOM_VK_9: 0x39;
    readonly DOM_VK_COLON: 0x3A;
    readonly DOM_VK_SEMICOLON: 0x3B;
    readonly DOM_VK_LESS_THAN: 0x3C;
    readonly DOM_VK_EQUALS: 0x3D;
    readonly DOM_VK_GREATER_THAN: 0x3E;
    readonly DOM_VK_QUESTION_MARK: 0x3F;
    readonly DOM_VK_AT: 0x40;
    readonly DOM_VK_A: 0x41;
    readonly DOM_VK_B: 0x42;
    readonly DOM_VK_C: 0x43;
    readonly DOM_VK_D: 0x44;
    readonly DOM_VK_E: 0x45;
    readonly DOM_VK_F: 0x46;
    readonly DOM_VK_G: 0x47;
    readonly DOM_VK_H: 0x48;
    readonly DOM_VK_I: 0x49;
    readonly DOM_VK_J: 0x4A;
    readonly DOM_VK_K: 0x4B;
    readonly DOM_VK_L: 0x4C;
    readonly DOM_VK_M: 0x4D;
    readonly DOM_VK_N: 0x4E;
    readonly DOM_VK_O: 0x4F;
    readonly DOM_VK_P: 0x50;
    readonly DOM_VK_Q: 0x51;
    readonly DOM_VK_R: 0x52;
    readonly DOM_VK_S: 0x53;
    readonly DOM_VK_T: 0x54;
    readonly DOM_VK_U: 0x55;
    readonly DOM_VK_V: 0x56;
    readonly DOM_VK_W: 0x57;
    readonly DOM_VK_X: 0x58;
    readonly DOM_VK_Y: 0x59;
    readonly DOM_VK_Z: 0x5A;
    readonly DOM_VK_WIN: 0x5B;
    readonly DOM_VK_CONTEXT_MENU: 0x5D;
    readonly DOM_VK_SLEEP: 0x5F;
    readonly DOM_VK_NUMPAD0: 0x60;
    readonly DOM_VK_NUMPAD1: 0x61;
    readonly DOM_VK_NUMPAD2: 0x62;
    readonly DOM_VK_NUMPAD3: 0x63;
    readonly DOM_VK_NUMPAD4: 0x64;
    readonly DOM_VK_NUMPAD5: 0x65;
    readonly DOM_VK_NUMPAD6: 0x66;
    readonly DOM_VK_NUMPAD7: 0x67;
    readonly DOM_VK_NUMPAD8: 0x68;
    readonly DOM_VK_NUMPAD9: 0x69;
    readonly DOM_VK_MULTIPLY: 0x6A;
    readonly DOM_VK_ADD: 0x6B;
    readonly DOM_VK_SEPARATOR: 0x6C;
    readonly DOM_VK_SUBTRACT: 0x6D;
    readonly DOM_VK_DECIMAL: 0x6E;
    readonly DOM_VK_DIVIDE: 0x6F;
    readonly DOM_VK_F1: 0x70;
    readonly DOM_VK_F2: 0x71;
    readonly DOM_VK_F3: 0x72;
    readonly DOM_VK_F4: 0x73;
    readonly DOM_VK_F5: 0x74;
    readonly DOM_VK_F6: 0x75;
    readonly DOM_VK_F7: 0x76;
    readonly DOM_VK_F8: 0x77;
    readonly DOM_VK_F9: 0x78;
    readonly DOM_VK_F10: 0x79;
    readonly DOM_VK_F11: 0x7A;
    readonly DOM_VK_F12: 0x7B;
    readonly DOM_VK_F13: 0x7C;
    readonly DOM_VK_F14: 0x7D;
    readonly DOM_VK_F15: 0x7E;
    readonly DOM_VK_F16: 0x7F;
    readonly DOM_VK_F17: 0x80;
    readonly DOM_VK_F18: 0x81;
    readonly DOM_VK_F19: 0x82;
    readonly DOM_VK_F20: 0x83;
    readonly DOM_VK_F21: 0x84;
    readonly DOM_VK_F22: 0x85;
    readonly DOM_VK_F23: 0x86;
    readonly DOM_VK_F24: 0x87;
    readonly DOM_VK_NUM_LOCK: 0x90;
    readonly DOM_VK_SCROLL_LOCK: 0x91;
    readonly DOM_VK_WIN_OEM_FJ_JISHO: 0x92;
    readonly DOM_VK_WIN_OEM_FJ_MASSHOU: 0x93;
    readonly DOM_VK_WIN_OEM_FJ_TOUROKU: 0x94;
    readonly DOM_VK_WIN_OEM_FJ_LOYA: 0x95;
    readonly DOM_VK_WIN_OEM_FJ_ROYA: 0x96;
    readonly DOM_VK_CIRCUMFLEX: 0xA0;
    readonly DOM_VK_EXCLAMATION: 0xA1;
    readonly DOM_VK_DOUBLE_QUOTE: 0xA2;
    readonly DOM_VK_HASH: 0xA3;
    readonly DOM_VK_DOLLAR: 0xA4;
    readonly DOM_VK_PERCENT: 0xA5;
    readonly DOM_VK_AMPERSAND: 0xA6;
    readonly DOM_VK_UNDERSCORE: 0xA7;
    readonly DOM_VK_OPEN_PAREN: 0xA8;
    readonly DOM_VK_CLOSE_PAREN: 0xA9;
    readonly DOM_VK_ASTERISK: 0xAA;
    readonly DOM_VK_PLUS: 0xAB;
    readonly DOM_VK_PIPE: 0xAC;
    readonly DOM_VK_HYPHEN_MINUS: 0xAD;
    readonly DOM_VK_OPEN_CURLY_BRACKET: 0xAE;
    readonly DOM_VK_CLOSE_CURLY_BRACKET: 0xAF;
    readonly DOM_VK_TILDE: 0xB0;
    readonly DOM_VK_VOLUME_MUTE: 0xB5;
    readonly DOM_VK_VOLUME_DOWN: 0xB6;
    readonly DOM_VK_VOLUME_UP: 0xB7;
    readonly DOM_VK_COMMA: 0xBC;
    readonly DOM_VK_PERIOD: 0xBE;
    readonly DOM_VK_SLASH: 0xBF;
    readonly DOM_VK_BACK_QUOTE: 0xC0;
    readonly DOM_VK_OPEN_BRACKET: 0xDB;
    readonly DOM_VK_BACK_SLASH: 0xDC;
    readonly DOM_VK_CLOSE_BRACKET: 0xDD;
    readonly DOM_VK_QUOTE: 0xDE;
    readonly DOM_VK_META: 0xE0;
    readonly DOM_VK_ALTGR: 0xE1;
    readonly DOM_VK_WIN_ICO_HELP: 0xE3;
    readonly DOM_VK_WIN_ICO_00: 0xE4;
    readonly DOM_VK_PROCESSKEY: 0xE5;
    readonly DOM_VK_WIN_ICO_CLEAR: 0xE6;
    readonly DOM_VK_WIN_OEM_RESET: 0xE9;
    readonly DOM_VK_WIN_OEM_JUMP: 0xEA;
    readonly DOM_VK_WIN_OEM_PA1: 0xEB;
    readonly DOM_VK_WIN_OEM_PA2: 0xEC;
    readonly DOM_VK_WIN_OEM_PA3: 0xED;
    readonly DOM_VK_WIN_OEM_WSCTRL: 0xEE;
    readonly DOM_VK_WIN_OEM_CUSEL: 0xEF;
    readonly DOM_VK_WIN_OEM_ATTN: 0xF0;
    readonly DOM_VK_WIN_OEM_FINISH: 0xF1;
    readonly DOM_VK_WIN_OEM_COPY: 0xF2;
    readonly DOM_VK_WIN_OEM_AUTO: 0xF3;
    readonly DOM_VK_WIN_OEM_ENLW: 0xF4;
    readonly DOM_VK_WIN_OEM_BACKTAB: 0xF5;
    readonly DOM_VK_ATTN: 0xF6;
    readonly DOM_VK_CRSEL: 0xF7;
    readonly DOM_VK_EXSEL: 0xF8;
    readonly DOM_VK_EREOF: 0xF9;
    readonly DOM_VK_PLAY: 0xFA;
    readonly DOM_VK_ZOOM: 0xFB;
    readonly DOM_VK_PA1: 0xFD;
    readonly DOM_VK_WIN_OEM_CLEAR: 0xFE;
}

interface KeyboardEvent extends UIEvent, KeyEventMixin {
    readonly altKey: boolean;
    readonly charCode: number;
    readonly code: string;
    readonly ctrlKey: boolean;
    readonly initDict: KeyboardEventInit;
    readonly isComposing: boolean;
    readonly key: string;
    readonly keyCode: number;
    readonly location: number;
    readonly metaKey: boolean;
    readonly repeat: boolean;
    readonly shiftKey: boolean;
    getModifierState(key: string): boolean;
    initKeyboardEvent(typeArg: string, bubblesArg?: boolean, cancelableArg?: boolean, viewArg?: Window | null, keyArg?: string, locationArg?: number, ctrlKey?: boolean, altKey?: boolean, shiftKey?: boolean, metaKey?: boolean): void;
    readonly DOM_KEY_LOCATION_STANDARD: 0x00;
    readonly DOM_KEY_LOCATION_LEFT: 0x01;
    readonly DOM_KEY_LOCATION_RIGHT: 0x02;
    readonly DOM_KEY_LOCATION_NUMPAD: 0x03;
}

declare var KeyboardEvent: {
    prototype: KeyboardEvent;
    new(typeArg: string, keyboardEventInitDict?: KeyboardEventInit): KeyboardEvent;
    readonly DOM_KEY_LOCATION_STANDARD: 0x00;
    readonly DOM_KEY_LOCATION_LEFT: 0x01;
    readonly DOM_KEY_LOCATION_RIGHT: 0x02;
    readonly DOM_KEY_LOCATION_NUMPAD: 0x03;
    readonly DOM_VK_CANCEL: 0x03;
    readonly DOM_VK_HELP: 0x06;
    readonly DOM_VK_BACK_SPACE: 0x08;
    readonly DOM_VK_TAB: 0x09;
    readonly DOM_VK_CLEAR: 0x0C;
    readonly DOM_VK_RETURN: 0x0D;
    readonly DOM_VK_SHIFT: 0x10;
    readonly DOM_VK_CONTROL: 0x11;
    readonly DOM_VK_ALT: 0x12;
    readonly DOM_VK_PAUSE: 0x13;
    readonly DOM_VK_CAPS_LOCK: 0x14;
    readonly DOM_VK_KANA: 0x15;
    readonly DOM_VK_HANGUL: 0x15;
    readonly DOM_VK_EISU: 0x16;
    readonly DOM_VK_JUNJA: 0x17;
    readonly DOM_VK_FINAL: 0x18;
    readonly DOM_VK_HANJA: 0x19;
    readonly DOM_VK_KANJI: 0x19;
    readonly DOM_VK_ESCAPE: 0x1B;
    readonly DOM_VK_CONVERT: 0x1C;
    readonly DOM_VK_NONCONVERT: 0x1D;
    readonly DOM_VK_ACCEPT: 0x1E;
    readonly DOM_VK_MODECHANGE: 0x1F;
    readonly DOM_VK_SPACE: 0x20;
    readonly DOM_VK_PAGE_UP: 0x21;
    readonly DOM_VK_PAGE_DOWN: 0x22;
    readonly DOM_VK_END: 0x23;
    readonly DOM_VK_HOME: 0x24;
    readonly DOM_VK_LEFT: 0x25;
    readonly DOM_VK_UP: 0x26;
    readonly DOM_VK_RIGHT: 0x27;
    readonly DOM_VK_DOWN: 0x28;
    readonly DOM_VK_SELECT: 0x29;
    readonly DOM_VK_PRINT: 0x2A;
    readonly DOM_VK_EXECUTE: 0x2B;
    readonly DOM_VK_PRINTSCREEN: 0x2C;
    readonly DOM_VK_INSERT: 0x2D;
    readonly DOM_VK_DELETE: 0x2E;
    readonly DOM_VK_0: 0x30;
    readonly DOM_VK_1: 0x31;
    readonly DOM_VK_2: 0x32;
    readonly DOM_VK_3: 0x33;
    readonly DOM_VK_4: 0x34;
    readonly DOM_VK_5: 0x35;
    readonly DOM_VK_6: 0x36;
    readonly DOM_VK_7: 0x37;
    readonly DOM_VK_8: 0x38;
    readonly DOM_VK_9: 0x39;
    readonly DOM_VK_COLON: 0x3A;
    readonly DOM_VK_SEMICOLON: 0x3B;
    readonly DOM_VK_LESS_THAN: 0x3C;
    readonly DOM_VK_EQUALS: 0x3D;
    readonly DOM_VK_GREATER_THAN: 0x3E;
    readonly DOM_VK_QUESTION_MARK: 0x3F;
    readonly DOM_VK_AT: 0x40;
    readonly DOM_VK_A: 0x41;
    readonly DOM_VK_B: 0x42;
    readonly DOM_VK_C: 0x43;
    readonly DOM_VK_D: 0x44;
    readonly DOM_VK_E: 0x45;
    readonly DOM_VK_F: 0x46;
    readonly DOM_VK_G: 0x47;
    readonly DOM_VK_H: 0x48;
    readonly DOM_VK_I: 0x49;
    readonly DOM_VK_J: 0x4A;
    readonly DOM_VK_K: 0x4B;
    readonly DOM_VK_L: 0x4C;
    readonly DOM_VK_M: 0x4D;
    readonly DOM_VK_N: 0x4E;
    readonly DOM_VK_O: 0x4F;
    readonly DOM_VK_P: 0x50;
    readonly DOM_VK_Q: 0x51;
    readonly DOM_VK_R: 0x52;
    readonly DOM_VK_S: 0x53;
    readonly DOM_VK_T: 0x54;
    readonly DOM_VK_U: 0x55;
    readonly DOM_VK_V: 0x56;
    readonly DOM_VK_W: 0x57;
    readonly DOM_VK_X: 0x58;
    readonly DOM_VK_Y: 0x59;
    readonly DOM_VK_Z: 0x5A;
    readonly DOM_VK_WIN: 0x5B;
    readonly DOM_VK_CONTEXT_MENU: 0x5D;
    readonly DOM_VK_SLEEP: 0x5F;
    readonly DOM_VK_NUMPAD0: 0x60;
    readonly DOM_VK_NUMPAD1: 0x61;
    readonly DOM_VK_NUMPAD2: 0x62;
    readonly DOM_VK_NUMPAD3: 0x63;
    readonly DOM_VK_NUMPAD4: 0x64;
    readonly DOM_VK_NUMPAD5: 0x65;
    readonly DOM_VK_NUMPAD6: 0x66;
    readonly DOM_VK_NUMPAD7: 0x67;
    readonly DOM_VK_NUMPAD8: 0x68;
    readonly DOM_VK_NUMPAD9: 0x69;
    readonly DOM_VK_MULTIPLY: 0x6A;
    readonly DOM_VK_ADD: 0x6B;
    readonly DOM_VK_SEPARATOR: 0x6C;
    readonly DOM_VK_SUBTRACT: 0x6D;
    readonly DOM_VK_DECIMAL: 0x6E;
    readonly DOM_VK_DIVIDE: 0x6F;
    readonly DOM_VK_F1: 0x70;
    readonly DOM_VK_F2: 0x71;
    readonly DOM_VK_F3: 0x72;
    readonly DOM_VK_F4: 0x73;
    readonly DOM_VK_F5: 0x74;
    readonly DOM_VK_F6: 0x75;
    readonly DOM_VK_F7: 0x76;
    readonly DOM_VK_F8: 0x77;
    readonly DOM_VK_F9: 0x78;
    readonly DOM_VK_F10: 0x79;
    readonly DOM_VK_F11: 0x7A;
    readonly DOM_VK_F12: 0x7B;
    readonly DOM_VK_F13: 0x7C;
    readonly DOM_VK_F14: 0x7D;
    readonly DOM_VK_F15: 0x7E;
    readonly DOM_VK_F16: 0x7F;
    readonly DOM_VK_F17: 0x80;
    readonly DOM_VK_F18: 0x81;
    readonly DOM_VK_F19: 0x82;
    readonly DOM_VK_F20: 0x83;
    readonly DOM_VK_F21: 0x84;
    readonly DOM_VK_F22: 0x85;
    readonly DOM_VK_F23: 0x86;
    readonly DOM_VK_F24: 0x87;
    readonly DOM_VK_NUM_LOCK: 0x90;
    readonly DOM_VK_SCROLL_LOCK: 0x91;
    readonly DOM_VK_WIN_OEM_FJ_JISHO: 0x92;
    readonly DOM_VK_WIN_OEM_FJ_MASSHOU: 0x93;
    readonly DOM_VK_WIN_OEM_FJ_TOUROKU: 0x94;
    readonly DOM_VK_WIN_OEM_FJ_LOYA: 0x95;
    readonly DOM_VK_WIN_OEM_FJ_ROYA: 0x96;
    readonly DOM_VK_CIRCUMFLEX: 0xA0;
    readonly DOM_VK_EXCLAMATION: 0xA1;
    readonly DOM_VK_DOUBLE_QUOTE: 0xA2;
    readonly DOM_VK_HASH: 0xA3;
    readonly DOM_VK_DOLLAR: 0xA4;
    readonly DOM_VK_PERCENT: 0xA5;
    readonly DOM_VK_AMPERSAND: 0xA6;
    readonly DOM_VK_UNDERSCORE: 0xA7;
    readonly DOM_VK_OPEN_PAREN: 0xA8;
    readonly DOM_VK_CLOSE_PAREN: 0xA9;
    readonly DOM_VK_ASTERISK: 0xAA;
    readonly DOM_VK_PLUS: 0xAB;
    readonly DOM_VK_PIPE: 0xAC;
    readonly DOM_VK_HYPHEN_MINUS: 0xAD;
    readonly DOM_VK_OPEN_CURLY_BRACKET: 0xAE;
    readonly DOM_VK_CLOSE_CURLY_BRACKET: 0xAF;
    readonly DOM_VK_TILDE: 0xB0;
    readonly DOM_VK_VOLUME_MUTE: 0xB5;
    readonly DOM_VK_VOLUME_DOWN: 0xB6;
    readonly DOM_VK_VOLUME_UP: 0xB7;
    readonly DOM_VK_COMMA: 0xBC;
    readonly DOM_VK_PERIOD: 0xBE;
    readonly DOM_VK_SLASH: 0xBF;
    readonly DOM_VK_BACK_QUOTE: 0xC0;
    readonly DOM_VK_OPEN_BRACKET: 0xDB;
    readonly DOM_VK_BACK_SLASH: 0xDC;
    readonly DOM_VK_CLOSE_BRACKET: 0xDD;
    readonly DOM_VK_QUOTE: 0xDE;
    readonly DOM_VK_META: 0xE0;
    readonly DOM_VK_ALTGR: 0xE1;
    readonly DOM_VK_WIN_ICO_HELP: 0xE3;
    readonly DOM_VK_WIN_ICO_00: 0xE4;
    readonly DOM_VK_PROCESSKEY: 0xE5;
    readonly DOM_VK_WIN_ICO_CLEAR: 0xE6;
    readonly DOM_VK_WIN_OEM_RESET: 0xE9;
    readonly DOM_VK_WIN_OEM_JUMP: 0xEA;
    readonly DOM_VK_WIN_OEM_PA1: 0xEB;
    readonly DOM_VK_WIN_OEM_PA2: 0xEC;
    readonly DOM_VK_WIN_OEM_PA3: 0xED;
    readonly DOM_VK_WIN_OEM_WSCTRL: 0xEE;
    readonly DOM_VK_WIN_OEM_CUSEL: 0xEF;
    readonly DOM_VK_WIN_OEM_ATTN: 0xF0;
    readonly DOM_VK_WIN_OEM_FINISH: 0xF1;
    readonly DOM_VK_WIN_OEM_COPY: 0xF2;
    readonly DOM_VK_WIN_OEM_AUTO: 0xF3;
    readonly DOM_VK_WIN_OEM_ENLW: 0xF4;
    readonly DOM_VK_WIN_OEM_BACKTAB: 0xF5;
    readonly DOM_VK_ATTN: 0xF6;
    readonly DOM_VK_CRSEL: 0xF7;
    readonly DOM_VK_EXSEL: 0xF8;
    readonly DOM_VK_EREOF: 0xF9;
    readonly DOM_VK_PLAY: 0xFA;
    readonly DOM_VK_ZOOM: 0xFB;
    readonly DOM_VK_PA1: 0xFD;
    readonly DOM_VK_WIN_OEM_CLEAR: 0xFE;
    isInstance: IsInstance<KeyboardEvent>;
};

interface KeyframeEffect extends AnimationEffect {
    composite: CompositeOperation;
    iterationComposite: IterationCompositeOperation;
    pseudoElement: string | null;
    target: Element | null;
    getKeyframes(): any[];
    getProperties(): AnimationPropertyDetails[];
    setKeyframes(keyframes: any): void;
}

declare var KeyframeEffect: {
    prototype: KeyframeEffect;
    new(target: Element | null, keyframes: any, options?: number | KeyframeEffectOptions): KeyframeEffect;
    new(source: KeyframeEffect): KeyframeEffect;
    isInstance: IsInstance<KeyframeEffect>;
};

interface L10nFileSource {
    readonly index: string[] | null;
    readonly locales: string[];
    readonly metaSource: string;
    readonly name: string;
    readonly prePath: string;
    fetchFile(locale: string, path: string): Promise<FluentResource | null>;
    fetchFileSync(locale: string, path: string): FluentResource | null;
    hasFile(locale: string, path: string): L10nFileSourceHasFileStatus;
}

declare var L10nFileSource: {
    prototype: L10nFileSource;
    new(name: string, metaSource: string, locales: string[], prePath: string, options?: FileSourceOptions, index?: string[]): L10nFileSource;
    isInstance: IsInstance<L10nFileSource>;
    createMock(name: string, metasource: string, locales: string[], prePath: string, fs: L10nFileSourceMockFile[]): L10nFileSource;
};

interface L10nRegistry {
    clearSources(): void;
    generateBundles(aLocales: string[], aResourceIds: L10nResourceId[]): FluentBundleAsyncIterator;
    generateBundlesSync(aLocales: string[], aResourceIds: L10nResourceId[]): FluentBundleIterator;
    getAvailableLocales(): string[];
    getSource(aName: string): L10nFileSource | null;
    getSourceNames(): string[];
    hasSource(aName: string): boolean;
    registerSources(aSources: L10nFileSource[]): void;
    removeSources(aSources: string[]): void;
    updateSources(aSources: L10nFileSource[]): void;
}

declare var L10nRegistry: {
    prototype: L10nRegistry;
    new(aOptions?: L10nRegistryOptions): L10nRegistry;
    isInstance: IsInstance<L10nRegistry>;
    getInstance(): L10nRegistry;
};

interface LargestContentfulPaint extends PerformanceEntry {
    readonly element: Element | null;
    readonly id: string;
    readonly loadTime: DOMHighResTimeStamp;
    readonly renderTime: DOMHighResTimeStamp;
    readonly size: number;
    readonly url: string;
    toJSON(): any;
}

declare var LargestContentfulPaint: {
    prototype: LargestContentfulPaint;
    new(): LargestContentfulPaint;
    isInstance: IsInstance<LargestContentfulPaint>;
};

interface LegacyMozTCPSocket {
    listen(port: number, options?: ServerSocketOptions, backlog?: number): TCPServerSocket;
    open(host: string, port: number, options?: SocketOptions): TCPSocket;
}

interface LinkStyle {
    readonly sheet: StyleSheet | null;
}

interface LoadContext {
}

interface LoadContextMixin {
    readonly associatedWindow: WindowProxy | null;
    readonly isContent: boolean;
    readonly originAttributes: any;
    readonly topFrameElement: Element | null;
    readonly topWindow: WindowProxy | null;
    usePrivateBrowsing: boolean;
    readonly useRemoteSubframes: boolean;
    readonly useRemoteTabs: boolean;
    useTrackingProtection: boolean;
}

interface LoadInfo {
}

interface Localization {
    addResourceIds(aResourceIds: L10nResourceId[]): void;
    formatMessages(aKeys: L10nKey[]): Promise<(L10nMessage | null)[]>;
    formatMessagesSync(aKeys: L10nKey[]): (L10nMessage | null)[];
    formatValue(aId: string, aArgs?: L10nArgs): Promise<string | null>;
    formatValueSync(aId: string, aArgs?: L10nArgs): string | null;
    formatValues(aKeys: L10nKey[]): Promise<(string | null)[]>;
    formatValuesSync(aKeys: L10nKey[]): (string | null)[];
    removeResourceIds(aResourceIds: L10nResourceId[]): number;
    setAsync(): void;
}

declare var Localization: {
    prototype: Localization;
    new(aResourceIds: L10nResourceId[], aSync?: boolean, aRegistry?: L10nRegistry, aLocales?: string[]): Localization;
    isInstance: IsInstance<Localization>;
};

interface Location {
    hash: string;
    host: string;
    hostname: string;
    href: string;
    toString(): string;
    readonly origin: string;
    pathname: string;
    port: string;
    protocol: string;
    search: string;
    assign(url: string | URL): void;
    reload(forceget?: boolean): void;
    replace(url: string | URL): void;
}

declare var Location: {
    prototype: Location;
    new(): Location;
    isInstance: IsInstance<Location>;
};

/** Available only in secure contexts. */
interface Lock {
    readonly mode: LockMode;
    readonly name: string;
}

declare var Lock: {
    prototype: Lock;
    new(): Lock;
    isInstance: IsInstance<Lock>;
};

/** Available only in secure contexts. */
interface LockManager {
    query(): Promise<LockManagerSnapshot>;
    request(name: string, callback: LockGrantedCallback): Promise<any>;
    request(name: string, options: LockOptions, callback: LockGrantedCallback): Promise<any>;
}

declare var LockManager: {
    prototype: LockManager;
    new(): LockManager;
    isInstance: IsInstance<LockManager>;
};

interface MIDIAccessEventMap {
    "statechange": Event;
}

/** Available only in secure contexts. */
interface MIDIAccess extends EventTarget {
    readonly inputs: MIDIInputMap;
    onstatechange: ((this: MIDIAccess, ev: Event) => any) | null;
    readonly outputs: MIDIOutputMap;
    readonly sysexEnabled: boolean;
    addEventListener<K extends keyof MIDIAccessEventMap>(type: K, listener: (this: MIDIAccess, ev: MIDIAccessEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MIDIAccessEventMap>(type: K, listener: (this: MIDIAccess, ev: MIDIAccessEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MIDIAccess: {
    prototype: MIDIAccess;
    new(): MIDIAccess;
    isInstance: IsInstance<MIDIAccess>;
};

/** Available only in secure contexts. */
interface MIDIConnectionEvent extends Event {
    readonly port: MIDIPort | null;
}

declare var MIDIConnectionEvent: {
    prototype: MIDIConnectionEvent;
    new(type: string, eventInitDict?: MIDIConnectionEventInit): MIDIConnectionEvent;
    isInstance: IsInstance<MIDIConnectionEvent>;
};

interface MIDIInputEventMap extends MIDIPortEventMap {
    "midimessage": Event;
}

/** Available only in secure contexts. */
interface MIDIInput extends MIDIPort {
    onmidimessage: ((this: MIDIInput, ev: Event) => any) | null;
    addEventListener<K extends keyof MIDIInputEventMap>(type: K, listener: (this: MIDIInput, ev: MIDIInputEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MIDIInputEventMap>(type: K, listener: (this: MIDIInput, ev: MIDIInputEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MIDIInput: {
    prototype: MIDIInput;
    new(): MIDIInput;
    isInstance: IsInstance<MIDIInput>;
};

/** Available only in secure contexts. */
interface MIDIInputMap {
    forEach(callbackfn: (value: MIDIInput, key: string, parent: MIDIInputMap) => void, thisArg?: any): void;
}

declare var MIDIInputMap: {
    prototype: MIDIInputMap;
    new(): MIDIInputMap;
    isInstance: IsInstance<MIDIInputMap>;
};

/** Available only in secure contexts. */
interface MIDIMessageEvent extends Event {
    readonly data: Uint8Array;
}

declare var MIDIMessageEvent: {
    prototype: MIDIMessageEvent;
    new(type: string, eventInitDict?: MIDIMessageEventInit): MIDIMessageEvent;
    isInstance: IsInstance<MIDIMessageEvent>;
};

/** Available only in secure contexts. */
interface MIDIOutput extends MIDIPort {
    clear(): void;
    send(data: number[], timestamp?: DOMHighResTimeStamp): void;
    addEventListener<K extends keyof MIDIPortEventMap>(type: K, listener: (this: MIDIOutput, ev: MIDIPortEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MIDIPortEventMap>(type: K, listener: (this: MIDIOutput, ev: MIDIPortEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MIDIOutput: {
    prototype: MIDIOutput;
    new(): MIDIOutput;
    isInstance: IsInstance<MIDIOutput>;
};

/** Available only in secure contexts. */
interface MIDIOutputMap {
    forEach(callbackfn: (value: MIDIOutput, key: string, parent: MIDIOutputMap) => void, thisArg?: any): void;
}

declare var MIDIOutputMap: {
    prototype: MIDIOutputMap;
    new(): MIDIOutputMap;
    isInstance: IsInstance<MIDIOutputMap>;
};

interface MIDIPortEventMap {
    "statechange": Event;
}

/** Available only in secure contexts. */
interface MIDIPort extends EventTarget {
    readonly connection: MIDIPortConnectionState;
    readonly id: string;
    readonly manufacturer: string | null;
    readonly name: string | null;
    onstatechange: ((this: MIDIPort, ev: Event) => any) | null;
    readonly state: MIDIPortDeviceState;
    readonly type: MIDIPortType;
    readonly version: string | null;
    close(): Promise<MIDIPort>;
    open(): Promise<MIDIPort>;
    addEventListener<K extends keyof MIDIPortEventMap>(type: K, listener: (this: MIDIPort, ev: MIDIPortEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MIDIPortEventMap>(type: K, listener: (this: MIDIPort, ev: MIDIPortEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MIDIPort: {
    prototype: MIDIPort;
    new(): MIDIPort;
    isInstance: IsInstance<MIDIPort>;
};

interface MOZ_debug {
    getParameter(pname: GLenum): any;
    readonly EXTENSIONS: 0x1F03;
    readonly WSI_INFO: 0x10000;
    readonly UNPACK_REQUIRE_FASTPATH: 0x10001;
    readonly DOES_INDEX_VALIDATION: 0x10002;
}

interface MatchGlob {
    readonly glob: string;
    matches(string: string): boolean;
}

declare var MatchGlob: {
    prototype: MatchGlob;
    new(glob: string, allowQuestion?: boolean): MatchGlob;
    isInstance: IsInstance<MatchGlob>;
};

interface MatchPattern {
    readonly matchesAllWebUrls: boolean;
    readonly pattern: string;
    matches(uri: URI, explicit?: boolean): boolean;
    matches(url: string, explicit?: boolean): boolean;
    matchesCookie(cookie: Cookie): boolean;
    overlaps(pattern: MatchPattern): boolean;
    subsumes(pattern: MatchPattern): boolean;
    subsumesDomain(pattern: MatchPattern): boolean;
}

declare var MatchPattern: {
    prototype: MatchPattern;
    new(pattern: string, options?: MatchPatternOptions): MatchPattern;
    isInstance: IsInstance<MatchPattern>;
};

interface MatchPatternSet {
    readonly matchesAllWebUrls: boolean;
    readonly patterns: MatchPattern[];
    matches(uri: URI, explicit?: boolean): boolean;
    matches(url: string, explicit?: boolean): boolean;
    matchesCookie(cookie: Cookie): boolean;
    overlaps(pattern: MatchPattern): boolean;
    overlaps(patternSet: MatchPatternSet): boolean;
    overlapsAll(patternSet: MatchPatternSet): boolean;
    subsumes(pattern: MatchPattern): boolean;
    subsumesDomain(pattern: MatchPattern): boolean;
}

declare var MatchPatternSet: {
    prototype: MatchPatternSet;
    new(patterns: (string | MatchPattern)[], options?: MatchPatternOptions): MatchPatternSet;
    isInstance: IsInstance<MatchPatternSet>;
};

interface MathMLElementEventMap extends ElementEventMap, GlobalEventHandlersEventMap, OnErrorEventHandlerForNodesEventMap, TouchEventHandlersEventMap {
}

interface MathMLElement extends Element, ElementCSSInlineStyle, GlobalEventHandlers, HTMLOrForeignElement, OnErrorEventHandlerForNodes, TouchEventHandlers {
    addEventListener<K extends keyof MathMLElementEventMap>(type: K, listener: (this: MathMLElement, ev: MathMLElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MathMLElementEventMap>(type: K, listener: (this: MathMLElement, ev: MathMLElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MathMLElement: {
    prototype: MathMLElement;
    new(): MathMLElement;
    isInstance: IsInstance<MathMLElement>;
};

interface MediaCapabilities {
    decodingInfo(configuration: MediaDecodingConfiguration): Promise<MediaCapabilitiesDecodingInfo>;
    encodingInfo(configuration: MediaEncodingConfiguration): Promise<MediaCapabilitiesInfo>;
}

declare var MediaCapabilities: {
    prototype: MediaCapabilities;
    new(): MediaCapabilities;
    isInstance: IsInstance<MediaCapabilities>;
};

interface MediaControllerEventMap {
    "activated": Event;
    "deactivated": Event;
    "metadatachange": Event;
    "playbackstatechange": Event;
    "positionstatechange": Event;
    "supportedkeyschange": Event;
}

interface MediaController extends EventTarget {
    readonly id: number;
    readonly isActive: boolean;
    readonly isAudible: boolean;
    readonly isPlaying: boolean;
    onactivated: ((this: MediaController, ev: Event) => any) | null;
    ondeactivated: ((this: MediaController, ev: Event) => any) | null;
    onmetadatachange: ((this: MediaController, ev: Event) => any) | null;
    onplaybackstatechange: ((this: MediaController, ev: Event) => any) | null;
    onpositionstatechange: ((this: MediaController, ev: Event) => any) | null;
    onsupportedkeyschange: ((this: MediaController, ev: Event) => any) | null;
    readonly playbackState: MediaSessionPlaybackState;
    readonly supportedKeys: MediaControlKey[];
    focus(): void;
    getMetadata(): MediaMetadataInit;
    nextTrack(): void;
    pause(): void;
    play(): void;
    prevTrack(): void;
    seekBackward(): void;
    seekForward(): void;
    seekTo(seekTime: number, fastSeek?: boolean): void;
    skipAd(): void;
    stop(): void;
    addEventListener<K extends keyof MediaControllerEventMap>(type: K, listener: (this: MediaController, ev: MediaControllerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaControllerEventMap>(type: K, listener: (this: MediaController, ev: MediaControllerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaController: {
    prototype: MediaController;
    new(): MediaController;
    isInstance: IsInstance<MediaController>;
};

interface MediaDeviceInfo {
    readonly deviceId: string;
    readonly groupId: string;
    readonly kind: MediaDeviceKind;
    readonly label: string;
    toJSON(): any;
}

declare var MediaDeviceInfo: {
    prototype: MediaDeviceInfo;
    new(): MediaDeviceInfo;
    isInstance: IsInstance<MediaDeviceInfo>;
};

interface MediaDevicesEventMap {
    "devicechange": Event;
}

interface MediaDevices extends EventTarget {
    ondevicechange: ((this: MediaDevices, ev: Event) => any) | null;
    enumerateDevices(): Promise<MediaDeviceInfo[]>;
    /** Available only in secure contexts. */
    getDisplayMedia(constraints?: DisplayMediaStreamConstraints): Promise<MediaStream>;
    getSupportedConstraints(): MediaTrackSupportedConstraints;
    getUserMedia(constraints?: MediaStreamConstraints): Promise<MediaStream>;
    /** Available only in secure contexts. */
    selectAudioOutput(options?: AudioOutputOptions): Promise<MediaDeviceInfo>;
    addEventListener<K extends keyof MediaDevicesEventMap>(type: K, listener: (this: MediaDevices, ev: MediaDevicesEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaDevicesEventMap>(type: K, listener: (this: MediaDevices, ev: MediaDevicesEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaDevices: {
    prototype: MediaDevices;
    new(): MediaDevices;
    isInstance: IsInstance<MediaDevices>;
};

interface MediaElementAudioSourceNode extends AudioNode, AudioNodePassThrough {
    readonly mediaElement: HTMLMediaElement;
}

declare var MediaElementAudioSourceNode: {
    prototype: MediaElementAudioSourceNode;
    new(context: AudioContext, options: MediaElementAudioSourceOptions): MediaElementAudioSourceNode;
    isInstance: IsInstance<MediaElementAudioSourceNode>;
};

interface MediaEncryptedEvent extends Event {
    readonly initData: ArrayBuffer | null;
    readonly initDataType: string;
}

declare var MediaEncryptedEvent: {
    prototype: MediaEncryptedEvent;
    new(type: string, eventInitDict?: MediaKeyNeededEventInit): MediaEncryptedEvent;
    isInstance: IsInstance<MediaEncryptedEvent>;
};

interface MediaError {
    readonly code: number;
    readonly message: string;
    readonly MEDIA_ERR_ABORTED: 1;
    readonly MEDIA_ERR_NETWORK: 2;
    readonly MEDIA_ERR_DECODE: 3;
    readonly MEDIA_ERR_SRC_NOT_SUPPORTED: 4;
}

declare var MediaError: {
    prototype: MediaError;
    new(): MediaError;
    readonly MEDIA_ERR_ABORTED: 1;
    readonly MEDIA_ERR_NETWORK: 2;
    readonly MEDIA_ERR_DECODE: 3;
    readonly MEDIA_ERR_SRC_NOT_SUPPORTED: 4;
    isInstance: IsInstance<MediaError>;
};

interface MediaKeyError extends Event {
    readonly systemCode: number;
}

declare var MediaKeyError: {
    prototype: MediaKeyError;
    new(): MediaKeyError;
    isInstance: IsInstance<MediaKeyError>;
};

interface MediaKeyMessageEvent extends Event {
    readonly message: ArrayBuffer;
    readonly messageType: MediaKeyMessageType;
}

declare var MediaKeyMessageEvent: {
    prototype: MediaKeyMessageEvent;
    new(type: string, eventInitDict: MediaKeyMessageEventInit): MediaKeyMessageEvent;
    isInstance: IsInstance<MediaKeyMessageEvent>;
};

interface MediaKeySessionEventMap {
    "keystatuseschange": Event;
    "message": Event;
}

interface MediaKeySession extends EventTarget {
    readonly closed: Promise<undefined>;
    readonly error: MediaKeyError | null;
    readonly expiration: number;
    readonly keyStatuses: MediaKeyStatusMap;
    onkeystatuseschange: ((this: MediaKeySession, ev: Event) => any) | null;
    onmessage: ((this: MediaKeySession, ev: Event) => any) | null;
    readonly sessionId: string;
    close(): Promise<void>;
    generateRequest(initDataType: string, initData: BufferSource): Promise<void>;
    load(sessionId: string): Promise<boolean>;
    remove(): Promise<void>;
    update(response: BufferSource): Promise<void>;
    addEventListener<K extends keyof MediaKeySessionEventMap>(type: K, listener: (this: MediaKeySession, ev: MediaKeySessionEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaKeySessionEventMap>(type: K, listener: (this: MediaKeySession, ev: MediaKeySessionEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaKeySession: {
    prototype: MediaKeySession;
    new(): MediaKeySession;
    isInstance: IsInstance<MediaKeySession>;
};

interface MediaKeyStatusMap {
    readonly size: number;
    get(keyId: BufferSource): MediaKeyStatus | undefined;
    has(keyId: BufferSource): boolean;
    forEach(callbackfn: (value: MediaKeyStatus, key: ArrayBuffer, parent: MediaKeyStatusMap) => void, thisArg?: any): void;
}

declare var MediaKeyStatusMap: {
    prototype: MediaKeyStatusMap;
    new(): MediaKeyStatusMap;
    isInstance: IsInstance<MediaKeyStatusMap>;
};

interface MediaKeySystemAccess {
    readonly keySystem: string;
    createMediaKeys(): Promise<MediaKeys>;
    getConfiguration(): MediaKeySystemConfiguration;
}

declare var MediaKeySystemAccess: {
    prototype: MediaKeySystemAccess;
    new(): MediaKeySystemAccess;
    isInstance: IsInstance<MediaKeySystemAccess>;
};

interface MediaKeys {
    readonly keySystem: string;
    createSession(sessionType?: MediaKeySessionType): MediaKeySession;
    getStatusForPolicy(policy?: MediaKeysPolicy): Promise<MediaKeyStatus>;
    setServerCertificate(serverCertificate: BufferSource): Promise<void>;
}

declare var MediaKeys: {
    prototype: MediaKeys;
    new(): MediaKeys;
    isInstance: IsInstance<MediaKeys>;
};

interface MediaList {
    readonly length: number;
    mediaText: string;
    toString(): string;
    appendMedium(newMedium: string): void;
    deleteMedium(oldMedium: string): void;
    item(index: number): string | null;
    [index: number]: string;
}

declare var MediaList: {
    prototype: MediaList;
    new(): MediaList;
    isInstance: IsInstance<MediaList>;
};

interface MediaMetadata {
    album: string;
    artist: string;
    artwork: any[];
    title: string;
}

declare var MediaMetadata: {
    prototype: MediaMetadata;
    new(init?: MediaMetadataInit): MediaMetadata;
    isInstance: IsInstance<MediaMetadata>;
};

interface MediaQueryListEventMap {
    "change": Event;
}

interface MediaQueryList extends EventTarget {
    readonly matches: boolean;
    readonly media: string;
    onchange: ((this: MediaQueryList, ev: Event) => any) | null;
    addListener(listener: EventListener | null): void;
    removeListener(listener: EventListener | null): void;
    addEventListener<K extends keyof MediaQueryListEventMap>(type: K, listener: (this: MediaQueryList, ev: MediaQueryListEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaQueryListEventMap>(type: K, listener: (this: MediaQueryList, ev: MediaQueryListEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaQueryList: {
    prototype: MediaQueryList;
    new(): MediaQueryList;
    isInstance: IsInstance<MediaQueryList>;
};

interface MediaQueryListEvent extends Event {
    readonly matches: boolean;
    readonly media: string;
}

declare var MediaQueryListEvent: {
    prototype: MediaQueryListEvent;
    new(type: string, eventInitDict?: MediaQueryListEventInit): MediaQueryListEvent;
    isInstance: IsInstance<MediaQueryListEvent>;
};

interface MediaRecorderEventMap {
    "dataavailable": Event;
    "error": Event;
    "pause": Event;
    "resume": Event;
    "start": Event;
    "stop": Event;
}

interface MediaRecorder extends EventTarget {
    readonly audioBitsPerSecond: number;
    readonly mimeType: string;
    ondataavailable: ((this: MediaRecorder, ev: Event) => any) | null;
    onerror: ((this: MediaRecorder, ev: Event) => any) | null;
    onpause: ((this: MediaRecorder, ev: Event) => any) | null;
    onresume: ((this: MediaRecorder, ev: Event) => any) | null;
    onstart: ((this: MediaRecorder, ev: Event) => any) | null;
    onstop: ((this: MediaRecorder, ev: Event) => any) | null;
    readonly state: RecordingState;
    readonly stream: MediaStream;
    readonly videoBitsPerSecond: number;
    pause(): void;
    requestData(): void;
    resume(): void;
    start(timeslice?: number): void;
    stop(): void;
    addEventListener<K extends keyof MediaRecorderEventMap>(type: K, listener: (this: MediaRecorder, ev: MediaRecorderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaRecorderEventMap>(type: K, listener: (this: MediaRecorder, ev: MediaRecorderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaRecorder: {
    prototype: MediaRecorder;
    new(stream: MediaStream, options?: MediaRecorderOptions): MediaRecorder;
    new(node: AudioNode, output?: number, options?: MediaRecorderOptions): MediaRecorder;
    isInstance: IsInstance<MediaRecorder>;
    isTypeSupported(type: string): boolean;
};

interface MediaRecorderErrorEvent extends Event {
    readonly error: DOMException;
}

declare var MediaRecorderErrorEvent: {
    prototype: MediaRecorderErrorEvent;
    new(type: string, eventInitDict: MediaRecorderErrorEventInit): MediaRecorderErrorEvent;
    isInstance: IsInstance<MediaRecorderErrorEvent>;
};

interface MediaSession {
    metadata: MediaMetadata | null;
    playbackState: MediaSessionPlaybackState;
    notifyHandler(details: MediaSessionActionDetails): void;
    setActionHandler(action: MediaSessionAction, handler: MediaSessionActionHandler | null): void;
    setPositionState(state?: MediaPositionState): void;
}

declare var MediaSession: {
    prototype: MediaSession;
    new(): MediaSession;
    isInstance: IsInstance<MediaSession>;
};

interface MediaSourceEventMap {
    "sourceclose": Event;
    "sourceended": Event;
    "sourceopen": Event;
}

interface MediaSource extends EventTarget {
    readonly activeSourceBuffers: SourceBufferList;
    duration: number;
    onsourceclose: ((this: MediaSource, ev: Event) => any) | null;
    onsourceended: ((this: MediaSource, ev: Event) => any) | null;
    onsourceopen: ((this: MediaSource, ev: Event) => any) | null;
    readonly readyState: MediaSourceReadyState;
    readonly sourceBuffers: SourceBufferList;
    addSourceBuffer(type: string): SourceBuffer;
    clearLiveSeekableRange(): void;
    endOfStream(error?: MediaSourceEndOfStreamError): void;
    mozDebugReaderData(): Promise<MediaSourceDecoderDebugInfo>;
    removeSourceBuffer(sourceBuffer: SourceBuffer): void;
    setLiveSeekableRange(start: number, end: number): void;
    addEventListener<K extends keyof MediaSourceEventMap>(type: K, listener: (this: MediaSource, ev: MediaSourceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaSourceEventMap>(type: K, listener: (this: MediaSource, ev: MediaSourceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaSource: {
    prototype: MediaSource;
    new(): MediaSource;
    isInstance: IsInstance<MediaSource>;
    isTypeSupported(type: string): boolean;
};

interface MediaStreamEventMap {
    "addtrack": Event;
    "removetrack": Event;
}

interface MediaStream extends EventTarget {
    readonly active: boolean;
    readonly id: string;
    onaddtrack: ((this: MediaStream, ev: Event) => any) | null;
    onremovetrack: ((this: MediaStream, ev: Event) => any) | null;
    addTrack(track: MediaStreamTrack): void;
    assignId(id: string): void;
    clone(): MediaStream;
    getAudioTracks(): MediaStreamTrack[];
    getTrackById(trackId: string): MediaStreamTrack | null;
    getTracks(): MediaStreamTrack[];
    getVideoTracks(): MediaStreamTrack[];
    removeTrack(track: MediaStreamTrack): void;
    addEventListener<K extends keyof MediaStreamEventMap>(type: K, listener: (this: MediaStream, ev: MediaStreamEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaStreamEventMap>(type: K, listener: (this: MediaStream, ev: MediaStreamEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaStream: {
    prototype: MediaStream;
    new(): MediaStream;
    new(stream: MediaStream): MediaStream;
    new(tracks: MediaStreamTrack[]): MediaStream;
    isInstance: IsInstance<MediaStream>;
    countUnderlyingStreams(): Promise<number>;
};

interface MediaStreamAudioDestinationNode extends AudioNode {
    readonly stream: MediaStream;
}

declare var MediaStreamAudioDestinationNode: {
    prototype: MediaStreamAudioDestinationNode;
    new(context: AudioContext, options?: AudioNodeOptions): MediaStreamAudioDestinationNode;
    isInstance: IsInstance<MediaStreamAudioDestinationNode>;
};

interface MediaStreamAudioSourceNode extends AudioNode, AudioNodePassThrough {
    readonly mediaStream: MediaStream;
}

declare var MediaStreamAudioSourceNode: {
    prototype: MediaStreamAudioSourceNode;
    new(context: AudioContext, options: MediaStreamAudioSourceOptions): MediaStreamAudioSourceNode;
    isInstance: IsInstance<MediaStreamAudioSourceNode>;
};

interface MediaStreamError {
    readonly constraint: string | null;
    readonly message: string | null;
    readonly name: string;
}

interface MediaStreamEvent extends Event {
    readonly stream: MediaStream | null;
}

declare var MediaStreamEvent: {
    prototype: MediaStreamEvent;
    new(type: string, eventInitDict?: MediaStreamEventInit): MediaStreamEvent;
    isInstance: IsInstance<MediaStreamEvent>;
};

interface MediaStreamTrackEventMap {
    "ended": Event;
    "mute": Event;
    "unmute": Event;
}

interface MediaStreamTrack extends EventTarget {
    enabled: boolean;
    readonly id: string;
    readonly kind: string;
    readonly label: string;
    readonly muted: boolean;
    onended: ((this: MediaStreamTrack, ev: Event) => any) | null;
    onmute: ((this: MediaStreamTrack, ev: Event) => any) | null;
    onunmute: ((this: MediaStreamTrack, ev: Event) => any) | null;
    readonly readyState: MediaStreamTrackState;
    applyConstraints(constraints?: MediaTrackConstraints): Promise<void>;
    clone(): MediaStreamTrack;
    getConstraints(): MediaTrackConstraints;
    getSettings(): MediaTrackSettings;
    stop(): void;
    addEventListener<K extends keyof MediaStreamTrackEventMap>(type: K, listener: (this: MediaStreamTrack, ev: MediaStreamTrackEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MediaStreamTrackEventMap>(type: K, listener: (this: MediaStreamTrack, ev: MediaStreamTrackEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MediaStreamTrack: {
    prototype: MediaStreamTrack;
    new(): MediaStreamTrack;
    isInstance: IsInstance<MediaStreamTrack>;
};

interface MediaStreamTrackAudioSourceNode extends AudioNode, AudioNodePassThrough {
}

declare var MediaStreamTrackAudioSourceNode: {
    prototype: MediaStreamTrackAudioSourceNode;
    new(context: AudioContext, options: MediaStreamTrackAudioSourceOptions): MediaStreamTrackAudioSourceNode;
    isInstance: IsInstance<MediaStreamTrackAudioSourceNode>;
};

interface MediaStreamTrackEvent extends Event {
    readonly track: MediaStreamTrack;
}

declare var MediaStreamTrackEvent: {
    prototype: MediaStreamTrackEvent;
    new(type: string, eventInitDict: MediaStreamTrackEventInit): MediaStreamTrackEvent;
    isInstance: IsInstance<MediaStreamTrackEvent>;
};

/** Available only in secure contexts. */
interface MerchantValidationEvent extends Event {
    readonly methodName: string;
    readonly validationURL: string;
    complete(merchantSessionPromise: any): void;
}

declare var MerchantValidationEvent: {
    prototype: MerchantValidationEvent;
    new(type: string, eventInitDict?: MerchantValidationEventInit): MerchantValidationEvent;
    isInstance: IsInstance<MerchantValidationEvent>;
};

interface MessageBroadcaster extends MessageListenerManager {
    readonly childCount: number;
    broadcastAsyncMessage(messageName?: string | null, obj?: any): void;
    getChildAt(aIndex: number): MessageListenerManager | null;
    releaseCachedProcesses(): void;
}

declare var MessageBroadcaster: {
    prototype: MessageBroadcaster;
    new(): MessageBroadcaster;
    isInstance: IsInstance<MessageBroadcaster>;
};

interface MessageChannel {
    readonly port1: MessagePort;
    readonly port2: MessagePort;
}

declare var MessageChannel: {
    prototype: MessageChannel;
    new(): MessageChannel;
    isInstance: IsInstance<MessageChannel>;
};

interface MessageEvent extends Event {
    readonly data: any;
    readonly lastEventId: string;
    readonly origin: string;
    readonly ports: MessagePort[];
    readonly source: MessageEventSource | null;
    initMessageEvent(type: string, bubbles?: boolean, cancelable?: boolean, data?: any, origin?: string, lastEventId?: string, source?: MessageEventSource | null, ports?: MessagePort[]): void;
}

declare var MessageEvent: {
    prototype: MessageEvent;
    new(type: string, eventInitDict?: MessageEventInit): MessageEvent;
    isInstance: IsInstance<MessageEvent>;
};

interface MessageListenerManager extends MessageListenerManagerMixin {
}

declare var MessageListenerManager: {
    prototype: MessageListenerManager;
    new(): MessageListenerManager;
    isInstance: IsInstance<MessageListenerManager>;
};

interface MessageListenerManagerMixin {
    addMessageListener(messageName: string, listener: MessageListener, listenWhenClosed?: boolean): void;
    addWeakMessageListener(messageName: string, listener: MessageListener): void;
    removeMessageListener(messageName: string, listener: MessageListener): void;
    removeWeakMessageListener(messageName: string, listener: MessageListener): void;
}

interface MessageManagerGlobal {
    atob(asciiString: string): string;
    btoa(base64Data: string): string;
    dump(str: string): void;
}

interface MessagePortEventMap {
    "message": Event;
    "messageerror": Event;
}

interface MessagePort extends EventTarget {
    onmessage: ((this: MessagePort, ev: Event) => any) | null;
    onmessageerror: ((this: MessagePort, ev: Event) => any) | null;
    close(): void;
    postMessage(message: any, transferable: any[]): void;
    postMessage(message: any, options?: StructuredSerializeOptions): void;
    start(): void;
    addEventListener<K extends keyof MessagePortEventMap>(type: K, listener: (this: MessagePort, ev: MessagePortEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof MessagePortEventMap>(type: K, listener: (this: MessagePort, ev: MessagePortEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var MessagePort: {
    prototype: MessagePort;
    new(): MessagePort;
    isInstance: IsInstance<MessagePort>;
};

interface MessageSender extends MessageListenerManager, MessageSenderMixin {
}

declare var MessageSender: {
    prototype: MessageSender;
    new(): MessageSender;
    isInstance: IsInstance<MessageSender>;
};

interface MessageSenderMixin {
    readonly processMessageManager: MessageSender | null;
    readonly remoteType: string;
    sendAsyncMessage(messageName?: string | null, obj?: any, transfers?: any): void;
}

interface MimeType {
    readonly description: string;
    readonly enabledPlugin: Plugin;
    readonly suffixes: string;
    readonly type: string;
}

declare var MimeType: {
    prototype: MimeType;
    new(): MimeType;
    isInstance: IsInstance<MimeType>;
};

interface MimeTypeArray {
    readonly length: number;
    item(index: number): MimeType | null;
    namedItem(name: string): MimeType | null;
    [index: number]: MimeType;
}

declare var MimeTypeArray: {
    prototype: MimeTypeArray;
    new(): MimeTypeArray;
    isInstance: IsInstance<MimeTypeArray>;
};

interface MouseEvent extends UIEvent {
    readonly altKey: boolean;
    readonly button: number;
    readonly buttons: number;
    readonly clientX: number;
    readonly clientY: number;
    readonly ctrlKey: boolean;
    readonly inputSource: number;
    readonly metaKey: boolean;
    readonly movementX: number;
    readonly movementY: number;
    readonly mozInputSource: number;
    readonly mozPressure: number;
    readonly offsetX: number;
    readonly offsetY: number;
    readonly pageX: number;
    readonly pageY: number;
    readonly relatedTarget: EventTarget | null;
    readonly screen: nsIScreen | null;
    readonly screenX: number;
    readonly screenY: number;
    readonly shiftKey: boolean;
    readonly x: number;
    readonly y: number;
    clickEventPrevented(): boolean;
    getModifierState(keyArg: string): boolean;
    initMouseEvent(typeArg: string, canBubbleArg?: boolean, cancelableArg?: boolean, viewArg?: Window | null, detailArg?: number, screenXArg?: number, screenYArg?: number, clientXArg?: number, clientYArg?: number, ctrlKeyArg?: boolean, altKeyArg?: boolean, shiftKeyArg?: boolean, metaKeyArg?: boolean, buttonArg?: number, relatedTargetArg?: EventTarget | null): void;
    initNSMouseEvent(typeArg: string, canBubbleArg?: boolean, cancelableArg?: boolean, viewArg?: Window | null, detailArg?: number, screenXArg?: number, screenYArg?: number, clientXArg?: number, clientYArg?: number, ctrlKeyArg?: boolean, altKeyArg?: boolean, shiftKeyArg?: boolean, metaKeyArg?: boolean, buttonArg?: number, relatedTargetArg?: EventTarget | null, pressure?: number, inputSourceArg?: number): void;
    preventClickEvent(): void;
    readonly MOZ_SOURCE_UNKNOWN: 0;
    readonly MOZ_SOURCE_MOUSE: 1;
    readonly MOZ_SOURCE_PEN: 2;
    readonly MOZ_SOURCE_ERASER: 3;
    readonly MOZ_SOURCE_CURSOR: 4;
    readonly MOZ_SOURCE_TOUCH: 5;
    readonly MOZ_SOURCE_KEYBOARD: 6;
}

declare var MouseEvent: {
    prototype: MouseEvent;
    new(typeArg: string, mouseEventInitDict?: MouseEventInit): MouseEvent;
    readonly MOZ_SOURCE_UNKNOWN: 0;
    readonly MOZ_SOURCE_MOUSE: 1;
    readonly MOZ_SOURCE_PEN: 2;
    readonly MOZ_SOURCE_ERASER: 3;
    readonly MOZ_SOURCE_CURSOR: 4;
    readonly MOZ_SOURCE_TOUCH: 5;
    readonly MOZ_SOURCE_KEYBOARD: 6;
    isInstance: IsInstance<MouseEvent>;
};

interface MouseScrollEvent extends MouseEvent {
    readonly axis: number;
    initMouseScrollEvent(type: string, canBubble?: boolean, cancelable?: boolean, view?: Window | null, detail?: number, screenX?: number, screenY?: number, clientX?: number, clientY?: number, ctrlKey?: boolean, altKey?: boolean, shiftKey?: boolean, metaKey?: boolean, button?: number, relatedTarget?: EventTarget | null, axis?: number): void;
    readonly HORIZONTAL_AXIS: 1;
    readonly VERTICAL_AXIS: 2;
}

declare var MouseScrollEvent: {
    prototype: MouseScrollEvent;
    new(): MouseScrollEvent;
    readonly HORIZONTAL_AXIS: 1;
    readonly VERTICAL_AXIS: 2;
    isInstance: IsInstance<MouseScrollEvent>;
};

interface MozCanvasPrintState {
    readonly context: nsISupports;
    done(): void;
}

declare var MozCanvasPrintState: {
    prototype: MozCanvasPrintState;
    new(): MozCanvasPrintState;
    isInstance: IsInstance<MozCanvasPrintState>;
};

interface MozChannel {
}

interface MozDocumentMatcher {
    readonly allFrames: boolean;
    readonly checkPermissions: boolean;
    readonly excludeMatches: MatchPatternSet | null;
    readonly extension: WebExtensionPolicy | null;
    readonly frameID: number | null;
    readonly matchAboutBlank: boolean;
    readonly matchOriginAsFallback: boolean;
    readonly matches: MatchPatternSet;
    readonly originAttributesPatterns: any;
    matchesURI(uri: URI): boolean;
    matchesWindowGlobal(windowGlobal: WindowGlobalChild, ignorePermissions?: boolean): boolean;
}

declare var MozDocumentMatcher: {
    prototype: MozDocumentMatcher;
    new(options: MozDocumentMatcherInit): MozDocumentMatcher;
    isInstance: IsInstance<MozDocumentMatcher>;
};

interface MozDocumentObserver {
    disconnect(): void;
    observe(matchers: MozDocumentMatcher[]): void;
}

declare var MozDocumentObserver: {
    prototype: MozDocumentObserver;
    new(callbacks: MozDocumentCallback): MozDocumentObserver;
    isInstance: IsInstance<MozDocumentObserver>;
};

interface MozEditableElement {
    readonly editor: nsIEditor | null;
    readonly hasEditor: boolean;
    readonly isInputEventTarget: boolean;
    setUserInput(input: string): void;
}

interface MozFrameLoaderOwner {
    readonly browsingContext: BrowsingContext | null;
    readonly frameLoader: FrameLoader | null;
    changeRemoteness(aOptions: RemotenessOptions): void;
    swapFrameLoaders(aOtherLoaderOwner: XULFrameElement): void;
    swapFrameLoaders(aOtherLoaderOwner: HTMLIFrameElement): void;
}

interface MozImageLoadingContent {
    readonly currentRequestFinalURI: URI | null;
    readonly currentURI: URI | null;
    loadingEnabled: boolean;
    addObserver(aObserver: imgINotificationObserver): void;
    forceReload(aNotify?: boolean): void;
    getRequest(aRequestType: number): imgIRequest | null;
    getRequestType(aRequest: imgIRequest): number;
    removeObserver(aObserver: imgINotificationObserver): void;
    readonly UNKNOWN_REQUEST: -1;
    readonly CURRENT_REQUEST: 0;
    readonly PENDING_REQUEST: 1;
}

interface MozObjectLoadingContent {
    readonly actualType: string;
    readonly displayedType: number;
    readonly srcURI: URI | null;
    readonly TYPE_LOADING: 0;
    readonly TYPE_DOCUMENT: 1;
    readonly TYPE_FALLBACK: 2;
}

interface MozObserver {
}

interface MozQueryInterface {
}

declare var MozQueryInterface: {
    prototype: MozQueryInterface;
    new(): MozQueryInterface;
    isInstance: IsInstance<MozQueryInterface>;
};

interface MozSharedMap extends EventTarget {
    get(name: string): StructuredClonable;
    has(name: string): boolean;
    forEach(callbackfn: (value: StructuredClonable, key: string, parent: MozSharedMap) => void, thisArg?: any): void;
}

declare var MozSharedMap: {
    prototype: MozSharedMap;
    new(): MozSharedMap;
    isInstance: IsInstance<MozSharedMap>;
};

interface MozSharedMapChangeEvent extends Event {
    readonly changedKeys: string[];
}

declare var MozSharedMapChangeEvent: {
    prototype: MozSharedMapChangeEvent;
    new(): MozSharedMapChangeEvent;
    isInstance: IsInstance<MozSharedMapChangeEvent>;
};

interface MozStorageAsyncStatementParams {
    readonly length: number;
    [index: number]: any;
    [name: string]: any;
}

declare var MozStorageAsyncStatementParams: {
    prototype: MozStorageAsyncStatementParams;
    new(): MozStorageAsyncStatementParams;
    isInstance: IsInstance<MozStorageAsyncStatementParams>;
};

interface MozStorageStatementParams {
    readonly length: number;
    [index: number]: any;
    [name: string]: any;
}

declare var MozStorageStatementParams: {
    prototype: MozStorageStatementParams;
    new(): MozStorageStatementParams;
    isInstance: IsInstance<MozStorageStatementParams>;
};

interface MozStorageStatementRow {
    [name: string]: any;
}

declare var MozStorageStatementRow: {
    prototype: MozStorageStatementRow;
    new(): MozStorageStatementRow;
    isInstance: IsInstance<MozStorageStatementRow>;
};

interface MozTreeView {
}

interface MozWritableSharedMap extends MozSharedMap {
    delete(name: string): void;
    flush(): void;
    set(name: string, value: StructuredClonable): void;
}

declare var MozWritableSharedMap: {
    prototype: MozWritableSharedMap;
    new(): MozWritableSharedMap;
    isInstance: IsInstance<MozWritableSharedMap>;
};

interface MutationEvent extends Event {
    readonly attrChange: number;
    readonly attrName: string;
    readonly newValue: string;
    readonly prevValue: string;
    readonly relatedNode: Node | null;
    initMutationEvent(type: string, canBubble?: boolean, cancelable?: boolean, relatedNode?: Node | null, prevValue?: string, newValue?: string, attrName?: string, attrChange?: number): void;
    readonly MODIFICATION: 1;
    readonly ADDITION: 2;
    readonly REMOVAL: 3;
}

declare var MutationEvent: {
    prototype: MutationEvent;
    new(): MutationEvent;
    readonly MODIFICATION: 1;
    readonly ADDITION: 2;
    readonly REMOVAL: 3;
    isInstance: IsInstance<MutationEvent>;
};

interface MutationObserver {
    mergeAttributeRecords: boolean;
    readonly mutationCallback: MutationCallback;
    disconnect(): void;
    getObservingInfo(): (MutationObservingInfo | null)[];
    observe(target: Node, options?: MutationObserverInit): void;
    takeRecords(): MutationRecord[];
}

declare var MutationObserver: {
    prototype: MutationObserver;
    new(mutationCallback: MutationCallback): MutationObserver;
    isInstance: IsInstance<MutationObserver>;
};

interface MutationRecord {
    readonly addedAnimations: Animation[];
    readonly addedNodes: NodeList;
    readonly attributeName: string | null;
    readonly attributeNamespace: string | null;
    readonly changedAnimations: Animation[];
    readonly nextSibling: Node | null;
    readonly oldValue: string | null;
    readonly previousSibling: Node | null;
    readonly removedAnimations: Animation[];
    readonly removedNodes: NodeList;
    readonly target: Node | null;
    readonly type: string;
}

declare var MutationRecord: {
    prototype: MutationRecord;
    new(): MutationRecord;
    isInstance: IsInstance<MutationRecord>;
};

interface NamedNodeMap {
    readonly length: number;
    getNamedItem(name: string): Attr | null;
    getNamedItemNS(namespaceURI: string | null, localName: string): Attr | null;
    item(index: number): Attr | null;
    removeNamedItem(name: string): Attr;
    removeNamedItemNS(namespaceURI: string | null, localName: string): Attr;
    setNamedItem(arg: Attr): Attr | null;
    setNamedItemNS(arg: Attr): Attr | null;
    [index: number]: Attr;
}

declare var NamedNodeMap: {
    prototype: NamedNodeMap;
    new(): NamedNodeMap;
    isInstance: IsInstance<NamedNodeMap>;
};

interface NavigateEvent extends Event {
    readonly canIntercept: boolean;
    readonly destination: NavigationDestination;
    readonly downloadRequest: string | null;
    readonly formData: FormData | null;
    readonly hasUAVisualTransition: boolean;
    readonly hashChange: boolean;
    readonly info: any;
    readonly navigationType: NavigationType;
    readonly signal: AbortSignal;
    readonly userInitiated: boolean;
    intercept(options?: NavigationInterceptOptions): void;
    scroll(): void;
}

declare var NavigateEvent: {
    prototype: NavigateEvent;
    new(type: string, eventInitDict: NavigateEventInit): NavigateEvent;
    isInstance: IsInstance<NavigateEvent>;
};

interface NavigationEventMap {
    "currententrychange": Event;
    "navigate": Event;
    "navigateerror": Event;
    "navigatesuccess": Event;
}

interface Navigation extends EventTarget {
    readonly activation: NavigationActivation | null;
    readonly canGoBack: boolean;
    readonly canGoForward: boolean;
    readonly currentEntry: NavigationHistoryEntry | null;
    oncurrententrychange: ((this: Navigation, ev: Event) => any) | null;
    onnavigate: ((this: Navigation, ev: Event) => any) | null;
    onnavigateerror: ((this: Navigation, ev: Event) => any) | null;
    onnavigatesuccess: ((this: Navigation, ev: Event) => any) | null;
    readonly transition: NavigationTransition | null;
    back(options?: NavigationOptions): NavigationResult;
    entries(): NavigationHistoryEntry[];
    forward(options?: NavigationOptions): NavigationResult;
    navigate(url: string | URL, options?: NavigationNavigateOptions): NavigationResult;
    reload(options?: NavigationReloadOptions): NavigationResult;
    traverseTo(key: string, options?: NavigationOptions): NavigationResult;
    updateCurrentEntry(options: NavigationUpdateCurrentEntryOptions): void;
    addEventListener<K extends keyof NavigationEventMap>(type: K, listener: (this: Navigation, ev: NavigationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof NavigationEventMap>(type: K, listener: (this: Navigation, ev: NavigationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Navigation: {
    prototype: Navigation;
    new(): Navigation;
    isInstance: IsInstance<Navigation>;
};

interface NavigationActivation {
    readonly entry: NavigationHistoryEntry;
    readonly from: NavigationHistoryEntry | null;
    readonly navigationType: NavigationType;
}

declare var NavigationActivation: {
    prototype: NavigationActivation;
    new(): NavigationActivation;
    isInstance: IsInstance<NavigationActivation>;
};

interface NavigationCurrentEntryChangeEvent extends Event {
    readonly from: NavigationHistoryEntry;
    readonly navigationType: NavigationType | null;
}

declare var NavigationCurrentEntryChangeEvent: {
    prototype: NavigationCurrentEntryChangeEvent;
    new(type: string, eventInitDict: NavigationCurrentEntryChangeEventInit): NavigationCurrentEntryChangeEvent;
    isInstance: IsInstance<NavigationCurrentEntryChangeEvent>;
};

interface NavigationDestination {
    readonly id: string;
    readonly index: number;
    readonly key: string;
    readonly sameDocument: boolean;
    readonly url: string;
    getState(): any;
}

declare var NavigationDestination: {
    prototype: NavigationDestination;
    new(): NavigationDestination;
    isInstance: IsInstance<NavigationDestination>;
};

interface NavigationHistoryEntryEventMap {
    "dispose": Event;
}

interface NavigationHistoryEntry extends EventTarget {
    readonly id: string;
    readonly index: number;
    readonly key: string;
    ondispose: ((this: NavigationHistoryEntry, ev: Event) => any) | null;
    readonly sameDocument: boolean;
    readonly url: string | null;
    getState(): any;
    addEventListener<K extends keyof NavigationHistoryEntryEventMap>(type: K, listener: (this: NavigationHistoryEntry, ev: NavigationHistoryEntryEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof NavigationHistoryEntryEventMap>(type: K, listener: (this: NavigationHistoryEntry, ev: NavigationHistoryEntryEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var NavigationHistoryEntry: {
    prototype: NavigationHistoryEntry;
    new(): NavigationHistoryEntry;
    isInstance: IsInstance<NavigationHistoryEntry>;
};

/** Available only in secure contexts. */
interface NavigationPreloadManager {
    disable(): Promise<void>;
    enable(): Promise<void>;
    getState(): Promise<NavigationPreloadState>;
    setHeaderValue(value: string): Promise<void>;
}

declare var NavigationPreloadManager: {
    prototype: NavigationPreloadManager;
    new(): NavigationPreloadManager;
    isInstance: IsInstance<NavigationPreloadManager>;
};

interface NavigationTransition {
    readonly finished: Promise<undefined>;
    readonly from: NavigationHistoryEntry;
    readonly navigationType: NavigationType;
}

declare var NavigationTransition: {
    prototype: NavigationTransition;
    new(): NavigationTransition;
    isInstance: IsInstance<NavigationTransition>;
};

interface Navigator extends GlobalPrivacyControl, NavigatorAutomationInformation, NavigatorConcurrentHardware, NavigatorContentUtils, NavigatorGPU, NavigatorGeolocation, NavigatorID, NavigatorLanguage, NavigatorLocks, NavigatorOnLine, NavigatorStorage {
    /** Available only in secure contexts. */
    readonly activeVRDisplays: VRDisplay[];
    readonly buildID: string;
    /** Available only in secure contexts. */
    readonly clipboard: Clipboard;
    readonly connection: NetworkInformation;
    readonly cookieEnabled: boolean;
    /** Available only in secure contexts. */
    readonly credentials: CredentialsContainer;
    readonly doNotTrack: string;
    readonly isWebVRContentDetected: boolean;
    readonly isWebVRContentPresenting: boolean;
    readonly maxTouchPoints: number;
    readonly mediaCapabilities: MediaCapabilities;
    readonly mediaDevices: MediaDevices;
    readonly mediaSession: MediaSession;
    readonly mimeTypes: MimeTypeArray;
    readonly mozAddonManager: AddonManager;
    readonly mozTCPSocket: LegacyMozTCPSocket;
    readonly oscpu: string;
    readonly pdfViewerEnabled: boolean;
    readonly permissions: Permissions;
    readonly plugins: PluginArray;
    readonly privateAttribution: PrivateAttribution;
    readonly productSub: string;
    readonly serviceWorker: ServiceWorkerContainer;
    readonly testTrialGatedAttribute: boolean;
    readonly userActivation: UserActivation;
    readonly vendor: string;
    readonly vendorSub: string;
    readonly wakeLock: WakeLock;
    /** Available only in secure contexts. */
    readonly xr: XRSystem;
    /** Available only in secure contexts. */
    canShare(data?: ShareData): boolean;
    getAutoplayPolicy(type: AutoplayPolicyMediaType): AutoplayPolicy;
    getAutoplayPolicy(element: HTMLMediaElement): AutoplayPolicy;
    getAutoplayPolicy(context: AudioContext): AutoplayPolicy;
    getBattery(): Promise<BatteryManager>;
    getGamepads(): (Gamepad | null)[];
    /** Available only in secure contexts. */
    getVRDisplays(): Promise<VRDisplay[]>;
    javaEnabled(): boolean;
    mozGetUserMedia(constraints: MediaStreamConstraints, successCallback: NavigatorUserMediaSuccessCallback, errorCallback: NavigatorUserMediaErrorCallback): void;
    requestAllGamepads(): Promise<Gamepad[]>;
    requestGamepadServiceTest(): GamepadServiceTest;
    requestMIDIAccess(options?: MIDIOptions): Promise<MIDIAccess>;
    requestMediaKeySystemAccess(keySystem: string, supportedConfigurations: MediaKeySystemConfiguration[]): Promise<MediaKeySystemAccess>;
    requestVRPresentation(display: VRDisplay): void;
    requestVRServiceTest(): VRServiceTest;
    sendBeacon(url: string, data?: BodyInit | null): boolean;
    setVibrationPermission(permitted: boolean, persistent?: boolean): void;
    /** Available only in secure contexts. */
    share(data?: ShareData): Promise<void>;
    vibrate(duration: number): boolean;
    vibrate(pattern: number[]): boolean;
}

declare var Navigator: {
    prototype: Navigator;
    new(): Navigator;
    isInstance: IsInstance<Navigator>;
};

interface NavigatorAutomationInformation {
    readonly webdriver: boolean;
}

interface NavigatorConcurrentHardware {
    readonly hardwareConcurrency: number;
}

interface NavigatorContentUtils {
    checkProtocolHandlerAllowed(scheme: string, handlerURI: URI, documentURI: URI): void;
    /** Available only in secure contexts. */
    registerProtocolHandler(scheme: string, url: string): void;
}

interface NavigatorGPU {
    /** Available only in secure contexts. */
    readonly gpu: GPU;
}

interface NavigatorGeolocation {
    readonly geolocation: Geolocation;
}

interface NavigatorID {
    readonly appCodeName: string;
    readonly appName: string;
    readonly appVersion: string;
    readonly platform: string;
    readonly product: string;
    readonly userAgent: string;
    taintEnabled(): boolean;
}

interface NavigatorLanguage {
    readonly language: string | null;
    readonly languages: string[];
}

/** Available only in secure contexts. */
interface NavigatorLocks {
    readonly locks: LockManager;
}

interface NavigatorOnLine {
    readonly onLine: boolean;
}

/** Available only in secure contexts. */
interface NavigatorStorage {
    readonly storage: StorageManager;
}

interface NetworkInformationEventMap {
    "typechange": Event;
}

interface NetworkInformation extends EventTarget {
    ontypechange: ((this: NetworkInformation, ev: Event) => any) | null;
    readonly type: ConnectionType;
    addEventListener<K extends keyof NetworkInformationEventMap>(type: K, listener: (this: NetworkInformation, ev: NetworkInformationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof NetworkInformationEventMap>(type: K, listener: (this: NetworkInformation, ev: NetworkInformationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var NetworkInformation: {
    prototype: NetworkInformation;
    new(): NetworkInformation;
    isInstance: IsInstance<NetworkInformation>;
};

interface Node extends EventTarget {
    readonly accessibleNode: AccessibleNode | null;
    readonly baseURI: string | null;
    readonly baseURIObject: URI | null;
    readonly childNodes: NodeList;
    readonly containingShadowRoot: ShadowRoot | null;
    readonly firstChild: Node | null;
    readonly flattenedTreeParentNode: Node | null;
    readonly isConnected: boolean;
    readonly isNativeAnonymous: boolean;
    readonly lastChild: Node | null;
    readonly nextSibling: Node | null;
    readonly nodeName: string;
    readonly nodePrincipal: Principal;
    readonly nodeType: number;
    nodeValue: string | null;
    readonly ownerDocument: Document | null;
    readonly parentElement: Element | null;
    readonly parentFlexElement: Element | null;
    readonly parentNode: Node | null;
    readonly previousSibling: Node | null;
    textContent: string | null;
    appendChild(node: Node): Node;
    cloneNode(deep?: boolean): Node;
    compareDocumentPosition(other: Node): number;
    contains(other: Node | null): boolean;
    generateXPath(): string;
    getRootNode(options?: GetRootNodeOptions): Node;
    hasChildNodes(): boolean;
    insertBefore(node: Node, child: Node | null): Node;
    isDefaultNamespace(namespace: string | null): boolean;
    isEqualNode(node: Node | null): boolean;
    isSameNode(node: Node | null): boolean;
    lookupNamespaceURI(prefix: string | null): string | null;
    lookupPrefix(namespace: string | null): string | null;
    normalize(): void;
    removeChild(child: Node): Node;
    replaceChild(node: Node, child: Node): Node;
    readonly ELEMENT_NODE: 1;
    readonly ATTRIBUTE_NODE: 2;
    readonly TEXT_NODE: 3;
    readonly CDATA_SECTION_NODE: 4;
    readonly ENTITY_REFERENCE_NODE: 5;
    readonly ENTITY_NODE: 6;
    readonly PROCESSING_INSTRUCTION_NODE: 7;
    readonly COMMENT_NODE: 8;
    readonly DOCUMENT_NODE: 9;
    readonly DOCUMENT_TYPE_NODE: 10;
    readonly DOCUMENT_FRAGMENT_NODE: 11;
    readonly NOTATION_NODE: 12;
    readonly DOCUMENT_POSITION_DISCONNECTED: 0x01;
    readonly DOCUMENT_POSITION_PRECEDING: 0x02;
    readonly DOCUMENT_POSITION_FOLLOWING: 0x04;
    readonly DOCUMENT_POSITION_CONTAINS: 0x08;
    readonly DOCUMENT_POSITION_CONTAINED_BY: 0x10;
    readonly DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC: 0x20;
}

declare var Node: {
    prototype: Node;
    new(): Node;
    readonly ELEMENT_NODE: 1;
    readonly ATTRIBUTE_NODE: 2;
    readonly TEXT_NODE: 3;
    readonly CDATA_SECTION_NODE: 4;
    readonly ENTITY_REFERENCE_NODE: 5;
    readonly ENTITY_NODE: 6;
    readonly PROCESSING_INSTRUCTION_NODE: 7;
    readonly COMMENT_NODE: 8;
    readonly DOCUMENT_NODE: 9;
    readonly DOCUMENT_TYPE_NODE: 10;
    readonly DOCUMENT_FRAGMENT_NODE: 11;
    readonly NOTATION_NODE: 12;
    readonly DOCUMENT_POSITION_DISCONNECTED: 0x01;
    readonly DOCUMENT_POSITION_PRECEDING: 0x02;
    readonly DOCUMENT_POSITION_FOLLOWING: 0x04;
    readonly DOCUMENT_POSITION_CONTAINS: 0x08;
    readonly DOCUMENT_POSITION_CONTAINED_BY: 0x10;
    readonly DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC: 0x20;
    isInstance: IsInstance<Node>;
};

interface NodeIterator {
    readonly filter: NodeFilter | null;
    readonly pointerBeforeReferenceNode: boolean;
    readonly referenceNode: Node | null;
    readonly root: Node;
    readonly whatToShow: number;
    detach(): void;
    nextNode(): Node | null;
    previousNode(): Node | null;
}

declare var NodeIterator: {
    prototype: NodeIterator;
    new(): NodeIterator;
    isInstance: IsInstance<NodeIterator>;
};

interface NodeList {
    readonly length: number;
    item(index: number): Node | null;
    forEach(callbackfn: (value: Node | null, key: number, parent: NodeList) => void, thisArg?: any): void;
    [index: number]: Node;
}

declare var NodeList: {
    prototype: NodeList;
    new(): NodeList;
    isInstance: IsInstance<NodeList>;
};

interface NonDocumentTypeChildNode {
    readonly nextElementSibling: Element | null;
    readonly previousElementSibling: Element | null;
}

interface NonElementParentNode {
    getElementById(elementId: string): Element | null;
}

interface NotificationEventMap {
    "click": Event;
    "close": Event;
    "error": Event;
    "show": Event;
}

interface Notification extends EventTarget {
    readonly body: string | null;
    readonly data: any;
    readonly dir: NotificationDirection;
    readonly icon: string | null;
    readonly lang: string | null;
    onclick: ((this: Notification, ev: Event) => any) | null;
    onclose: ((this: Notification, ev: Event) => any) | null;
    onerror: ((this: Notification, ev: Event) => any) | null;
    onshow: ((this: Notification, ev: Event) => any) | null;
    readonly requireInteraction: boolean;
    readonly silent: boolean;
    readonly tag: string | null;
    readonly title: string;
    readonly vibrate: number[];
    close(): void;
    addEventListener<K extends keyof NotificationEventMap>(type: K, listener: (this: Notification, ev: NotificationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof NotificationEventMap>(type: K, listener: (this: Notification, ev: NotificationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Notification: {
    prototype: Notification;
    new(title: string, options?: NotificationOptions): Notification;
    isInstance: IsInstance<Notification>;
    readonly permission: NotificationPermission;
    requestPermission(permissionCallback?: NotificationPermissionCallback): Promise<NotificationPermission>;
};

interface NotifyPaintEvent extends Event {
    readonly boundingClientRect: DOMRect;
    readonly clientRects: DOMRectList;
    readonly paintRequests: PaintRequestList;
    readonly paintTimeStamp: DOMHighResTimeStamp;
    readonly transactionId: number;
}

declare var NotifyPaintEvent: {
    prototype: NotifyPaintEvent;
    new(): NotifyPaintEvent;
    isInstance: IsInstance<NotifyPaintEvent>;
};

interface OES_draw_buffers_indexed {
    blendEquationSeparateiOES(buf: GLuint, modeRGB: GLenum, modeAlpha: GLenum): void;
    blendEquationiOES(buf: GLuint, mode: GLenum): void;
    blendFuncSeparateiOES(buf: GLuint, srcRGB: GLenum, dstRGB: GLenum, srcAlpha: GLenum, dstAlpha: GLenum): void;
    blendFunciOES(buf: GLuint, src: GLenum, dst: GLenum): void;
    colorMaskiOES(buf: GLuint, r: GLboolean, g: GLboolean, b: GLboolean, a: GLboolean): void;
    disableiOES(target: GLenum, index: GLuint): void;
    enableiOES(target: GLenum, index: GLuint): void;
}

interface OES_element_index_uint {
}

interface OES_fbo_render_mipmap {
}

interface OES_standard_derivatives {
    readonly FRAGMENT_SHADER_DERIVATIVE_HINT_OES: 0x8B8B;
}

interface OES_texture_float {
}

interface OES_texture_float_linear {
}

interface OES_texture_half_float {
    readonly HALF_FLOAT_OES: 0x8D61;
}

interface OES_texture_half_float_linear {
}

interface OES_vertex_array_object {
    bindVertexArrayOES(arrayObject: WebGLVertexArrayObject | null): void;
    createVertexArrayOES(): WebGLVertexArrayObject;
    deleteVertexArrayOES(arrayObject: WebGLVertexArrayObject | null): void;
    isVertexArrayOES(arrayObject: WebGLVertexArrayObject | null): GLboolean;
    readonly VERTEX_ARRAY_BINDING_OES: 0x85B5;
}

interface OVR_multiview2 {
    framebufferTextureMultiviewOVR(target: GLenum, attachment: GLenum, texture: WebGLTexture | null, level: GLint, baseViewIndex: GLint, numViews: GLsizei): void;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR: 0x9630;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR: 0x9632;
    readonly MAX_VIEWS_OVR: 0x9631;
    readonly FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR: 0x9633;
}

interface OfflineAudioCompletionEvent extends Event {
    readonly renderedBuffer: AudioBuffer;
}

declare var OfflineAudioCompletionEvent: {
    prototype: OfflineAudioCompletionEvent;
    new(type: string, eventInitDict: OfflineAudioCompletionEventInit): OfflineAudioCompletionEvent;
    isInstance: IsInstance<OfflineAudioCompletionEvent>;
};

interface OfflineAudioContextEventMap extends BaseAudioContextEventMap {
    "complete": Event;
}

interface OfflineAudioContext extends BaseAudioContext {
    readonly length: number;
    oncomplete: ((this: OfflineAudioContext, ev: Event) => any) | null;
    startRendering(): Promise<AudioBuffer>;
    addEventListener<K extends keyof OfflineAudioContextEventMap>(type: K, listener: (this: OfflineAudioContext, ev: OfflineAudioContextEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof OfflineAudioContextEventMap>(type: K, listener: (this: OfflineAudioContext, ev: OfflineAudioContextEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var OfflineAudioContext: {
    prototype: OfflineAudioContext;
    new(contextOptions: OfflineAudioContextOptions): OfflineAudioContext;
    new(numberOfChannels: number, length: number, sampleRate: number): OfflineAudioContext;
    isInstance: IsInstance<OfflineAudioContext>;
};

interface OffscreenCanvasEventMap {
    "contextlost": Event;
    "contextrestored": Event;
}

interface OffscreenCanvas extends EventTarget {
    height: number;
    oncontextlost: ((this: OffscreenCanvas, ev: Event) => any) | null;
    oncontextrestored: ((this: OffscreenCanvas, ev: Event) => any) | null;
    width: number;
    convertToBlob(options?: ImageEncodeOptions): Promise<Blob>;
    getContext(contextId: OffscreenRenderingContextId, contextOptions?: any): OffscreenRenderingContext | null;
    toBlob(type?: string, encoderOptions?: any): Promise<Blob>;
    transferToImageBitmap(): ImageBitmap;
    addEventListener<K extends keyof OffscreenCanvasEventMap>(type: K, listener: (this: OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof OffscreenCanvasEventMap>(type: K, listener: (this: OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var OffscreenCanvas: {
    prototype: OffscreenCanvas;
    new(width: number, height: number): OffscreenCanvas;
    isInstance: IsInstance<OffscreenCanvas>;
};

interface OffscreenCanvasRenderingContext2D extends CanvasCompositing, CanvasDrawImage, CanvasDrawPath, CanvasFillStrokeStyles, CanvasFilters, CanvasImageData, CanvasImageSmoothing, CanvasPathDrawingStyles, CanvasPathMethods, CanvasRect, CanvasShadowStyles, CanvasState, CanvasText, CanvasTextDrawingStyles, CanvasTransform {
    readonly canvas: OffscreenCanvas;
    commit(): void;
}

declare var OffscreenCanvasRenderingContext2D: {
    prototype: OffscreenCanvasRenderingContext2D;
    new(): OffscreenCanvasRenderingContext2D;
    isInstance: IsInstance<OffscreenCanvasRenderingContext2D>;
};

interface OnErrorEventHandlerForNodesEventMap {
    "error": Event;
}

interface OnErrorEventHandlerForNodes {
    onerror: ((this: OnErrorEventHandlerForNodes, ev: Event) => any) | null;
    addEventListener<K extends keyof OnErrorEventHandlerForNodesEventMap>(type: K, listener: (this: OnErrorEventHandlerForNodes, ev: OnErrorEventHandlerForNodesEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof OnErrorEventHandlerForNodesEventMap>(type: K, listener: (this: OnErrorEventHandlerForNodes, ev: OnErrorEventHandlerForNodesEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface OnErrorEventHandlerForWindowEventMap {
    "error": Event;
}

interface OnErrorEventHandlerForWindow {
    onerror: ((this: OnErrorEventHandlerForWindow, ev: Event) => any) | null;
    addEventListener<K extends keyof OnErrorEventHandlerForWindowEventMap>(type: K, listener: (this: OnErrorEventHandlerForWindow, ev: OnErrorEventHandlerForWindowEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof OnErrorEventHandlerForWindowEventMap>(type: K, listener: (this: OnErrorEventHandlerForWindow, ev: OnErrorEventHandlerForWindowEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface OscillatorNode extends AudioScheduledSourceNode, AudioNodePassThrough {
    readonly detune: AudioParam;
    readonly frequency: AudioParam;
    type: OscillatorType;
    setPeriodicWave(periodicWave: PeriodicWave): void;
    addEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: OscillatorNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AudioScheduledSourceNodeEventMap>(type: K, listener: (this: OscillatorNode, ev: AudioScheduledSourceNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var OscillatorNode: {
    prototype: OscillatorNode;
    new(context: BaseAudioContext, options?: OscillatorOptions): OscillatorNode;
    isInstance: IsInstance<OscillatorNode>;
};

interface OutputStream {
}

interface PageTransitionEvent extends Event {
    readonly inFrameSwap: boolean;
    readonly persisted: boolean;
}

declare var PageTransitionEvent: {
    prototype: PageTransitionEvent;
    new(type: string, eventInitDict?: PageTransitionEventInit): PageTransitionEvent;
    isInstance: IsInstance<PageTransitionEvent>;
};

interface PaintRequest {
    readonly clientRect: DOMRect;
    readonly reason: string;
}

declare var PaintRequest: {
    prototype: PaintRequest;
    new(): PaintRequest;
    isInstance: IsInstance<PaintRequest>;
};

interface PaintRequestList {
    readonly length: number;
    item(index: number): PaintRequest | null;
    [index: number]: PaintRequest;
}

declare var PaintRequestList: {
    prototype: PaintRequestList;
    new(): PaintRequestList;
    isInstance: IsInstance<PaintRequestList>;
};

interface PannerNode extends AudioNode, AudioNodePassThrough {
    coneInnerAngle: number;
    coneOuterAngle: number;
    coneOuterGain: number;
    distanceModel: DistanceModelType;
    maxDistance: number;
    readonly orientationX: AudioParam;
    readonly orientationY: AudioParam;
    readonly orientationZ: AudioParam;
    panningModel: PanningModelType;
    readonly positionX: AudioParam;
    readonly positionY: AudioParam;
    readonly positionZ: AudioParam;
    refDistance: number;
    rolloffFactor: number;
    setOrientation(x: number, y: number, z: number): void;
    setPosition(x: number, y: number, z: number): void;
}

declare var PannerNode: {
    prototype: PannerNode;
    new(context: BaseAudioContext, options?: PannerOptions): PannerNode;
    isInstance: IsInstance<PannerNode>;
};

interface ParentNode {
    readonly childElementCount: number;
    readonly children: HTMLCollection;
    readonly firstElementChild: Element | null;
    readonly lastElementChild: Element | null;
    append(...nodes: (Node | string)[]): void;
    getElementsByAttribute(name: string, value: string | null): HTMLCollection;
    getElementsByAttributeNS(namespaceURI: string | null, name: string, value: string | null): HTMLCollection;
    prepend(...nodes: (Node | string)[]): void;
    querySelector<K extends keyof HTMLElementTagNameMap>(selectors: K): HTMLElementTagNameMap[K] | null;
    querySelector<K extends keyof SVGElementTagNameMap>(selectors: K): SVGElementTagNameMap[K] | null;
    querySelector<K extends keyof MathMLElementTagNameMap>(selectors: K): MathMLElementTagNameMap[K] | null;
    /** @deprecated */
    querySelector<K extends keyof HTMLElementDeprecatedTagNameMap>(selectors: K): HTMLElementDeprecatedTagNameMap[K] | null;
    querySelector<E extends Element = Element>(selectors: string): E | null;
    querySelectorAll<K extends keyof HTMLElementTagNameMap>(selectors: K): NodeListOf<HTMLElementTagNameMap[K]>;
    querySelectorAll<K extends keyof SVGElementTagNameMap>(selectors: K): NodeListOf<SVGElementTagNameMap[K]>;
    querySelectorAll<K extends keyof MathMLElementTagNameMap>(selectors: K): NodeListOf<MathMLElementTagNameMap[K]>;
    /** @deprecated */
    querySelectorAll<K extends keyof HTMLElementDeprecatedTagNameMap>(selectors: K): NodeListOf<HTMLElementDeprecatedTagNameMap[K]>;
    querySelectorAll<E extends Element = Element>(selectors: string): NodeListOf<E>;
    replaceChildren(...nodes: (Node | string)[]): void;
}

interface ParentProcessMessageManager extends MessageBroadcaster, GlobalProcessScriptLoader, ProcessScriptLoader {
}

declare var ParentProcessMessageManager: {
    prototype: ParentProcessMessageManager;
    new(): ParentProcessMessageManager;
    isInstance: IsInstance<ParentProcessMessageManager>;
};

interface Path2D extends CanvasPathMethods {
    addPath(path: Path2D, transform?: DOMMatrix2DInit): void;
}

declare var Path2D: {
    prototype: Path2D;
    new(): Path2D;
    new(other: Path2D): Path2D;
    new(pathString: string): Path2D;
    isInstance: IsInstance<Path2D>;
};

/** Available only in secure contexts. */
interface PaymentAddress {
    readonly addressLine: string[];
    readonly city: string;
    readonly country: string;
    readonly dependentLocality: string;
    readonly organization: string;
    readonly phone: string;
    readonly postalCode: string;
    readonly recipient: string;
    readonly region: string;
    readonly regionCode: string;
    readonly sortingCode: string;
    toJSON(): any;
}

declare var PaymentAddress: {
    prototype: PaymentAddress;
    new(): PaymentAddress;
    isInstance: IsInstance<PaymentAddress>;
};

/** Available only in secure contexts. */
interface PaymentMethodChangeEvent extends PaymentRequestUpdateEvent {
    readonly methodDetails: any;
    readonly methodName: string;
}

declare var PaymentMethodChangeEvent: {
    prototype: PaymentMethodChangeEvent;
    new(type: string, eventInitDict?: PaymentMethodChangeEventInit): PaymentMethodChangeEvent;
    isInstance: IsInstance<PaymentMethodChangeEvent>;
};

interface PaymentRequestEventMap {
    "merchantvalidation": Event;
    "paymentmethodchange": Event;
    "shippingaddresschange": Event;
    "shippingoptionchange": Event;
}

/** Available only in secure contexts. */
interface PaymentRequest extends EventTarget {
    readonly id: string;
    onmerchantvalidation: ((this: PaymentRequest, ev: Event) => any) | null;
    onpaymentmethodchange: ((this: PaymentRequest, ev: Event) => any) | null;
    onshippingaddresschange: ((this: PaymentRequest, ev: Event) => any) | null;
    onshippingoptionchange: ((this: PaymentRequest, ev: Event) => any) | null;
    readonly shippingAddress: PaymentAddress | null;
    readonly shippingOption: string | null;
    readonly shippingType: PaymentShippingType | null;
    abort(): Promise<void>;
    canMakePayment(): Promise<boolean>;
    show(detailsPromise?: PaymentDetailsUpdate | PromiseLike<PaymentDetailsUpdate>): Promise<PaymentResponse>;
    addEventListener<K extends keyof PaymentRequestEventMap>(type: K, listener: (this: PaymentRequest, ev: PaymentRequestEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof PaymentRequestEventMap>(type: K, listener: (this: PaymentRequest, ev: PaymentRequestEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var PaymentRequest: {
    prototype: PaymentRequest;
    new(methodData: PaymentMethodData[], details: PaymentDetailsInit, options?: PaymentOptions): PaymentRequest;
    isInstance: IsInstance<PaymentRequest>;
};

/** Available only in secure contexts. */
interface PaymentRequestUpdateEvent extends Event {
    updateWith(detailsPromise: PaymentDetailsUpdate | PromiseLike<PaymentDetailsUpdate>): void;
}

declare var PaymentRequestUpdateEvent: {
    prototype: PaymentRequestUpdateEvent;
    new(type: string, eventInitDict?: PaymentRequestUpdateEventInit): PaymentRequestUpdateEvent;
    isInstance: IsInstance<PaymentRequestUpdateEvent>;
};

interface PaymentResponseEventMap {
    "payerdetailchange": Event;
}

/** Available only in secure contexts. */
interface PaymentResponse extends EventTarget {
    readonly details: any;
    readonly methodName: string;
    onpayerdetailchange: ((this: PaymentResponse, ev: Event) => any) | null;
    readonly payerEmail: string | null;
    readonly payerName: string | null;
    readonly payerPhone: string | null;
    readonly requestId: string;
    readonly shippingAddress: PaymentAddress | null;
    readonly shippingOption: string | null;
    complete(result?: PaymentComplete): Promise<void>;
    retry(errorFields?: PaymentValidationErrors): Promise<void>;
    toJSON(): any;
    addEventListener<K extends keyof PaymentResponseEventMap>(type: K, listener: (this: PaymentResponse, ev: PaymentResponseEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof PaymentResponseEventMap>(type: K, listener: (this: PaymentResponse, ev: PaymentResponseEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var PaymentResponse: {
    prototype: PaymentResponse;
    new(): PaymentResponse;
    isInstance: IsInstance<PaymentResponse>;
};

interface PeerConnectionImpl {
    certificate: RTCCertificate;
    readonly connectionState: RTCPeerConnectionState;
    readonly currentLocalDescription: string;
    readonly currentOfferer: boolean | null;
    readonly currentRemoteDescription: string;
    readonly duplicateFingerprintQuirk: boolean;
    readonly fingerprint: string;
    readonly iceConnectionState: RTCIceConnectionState;
    readonly iceGatheringState: RTCIceGatheringState;
    id: string;
    peerIdentity: string;
    readonly pendingLocalDescription: string;
    readonly pendingOfferer: boolean | null;
    readonly pendingRemoteDescription: string;
    readonly privacyRequested: boolean;
    readonly sctp: RTCSctpTransport | null;
    readonly signalingState: RTCSignalingState;
    addIceCandidate(candidate: string, mid: string, ufrag: string, level: number | null): void;
    addTransceiver(init: RTCRtpTransceiverInit, kind: string, sendTrack: MediaStreamTrack | null, addTrackMagic: boolean): RTCRtpTransceiver;
    chain(op: ChainedOperation): Promise<any>;
    close(): void;
    closeStreams(): void;
    createAnswer(): void;
    createDataChannel(label: string, protocol: string, type: number, ordered: boolean, maxTime: number, maxNum: number, externalNegotiated: boolean, stream: number): RTCDataChannel;
    createOffer(options?: RTCOfferOptions): void;
    createdSender(sender: RTCRtpSender): boolean;
    disablePacketDump(level: number, type: mozPacketDumpType, sending: boolean): void;
    enablePacketDump(level: number, type: mozPacketDumpType, sending: boolean): void;
    getRemoteStreams(): MediaStream[];
    getStats(selector: MediaStreamTrack | null): Promise<RTCStatsReport>;
    getTransceivers(): RTCRtpTransceiver[];
    initialize(observer: PeerConnectionObserver, window: Window): void;
    onSetDescriptionError(): void;
    onSetDescriptionSuccess(type: RTCSdpType, remote: boolean): Promise<void>;
    pluginCrash(pluginId: number, name: string): boolean;
    restartIce(): void;
    restartIceNoRenegotiationNeeded(): void;
    setConfiguration(config?: RTCConfiguration): void;
    setLocalDescription(action: number, sdp: string): void;
    setRemoteDescription(action: number, sdp: string): void;
    updateNegotiationNeeded(): void;
}

declare var PeerConnectionImpl: {
    prototype: PeerConnectionImpl;
    new(): PeerConnectionImpl;
    isInstance: IsInstance<PeerConnectionImpl>;
};

interface PeerConnectionObserver {
    fireNegotiationNeededEvent(): void;
    fireStreamEvent(stream: MediaStream): void;
    fireTrackEvent(receiver: RTCRtpReceiver, streams: MediaStream[]): void;
    notifyDataChannel(channel: RTCDataChannel): void;
    onAddIceCandidateError(error: PCErrorData): void;
    onAddIceCandidateSuccess(): void;
    onCreateAnswerError(error: PCErrorData): void;
    onCreateAnswerSuccess(answer: string): void;
    onCreateOfferError(error: PCErrorData): void;
    onCreateOfferSuccess(offer: string): void;
    onIceCandidate(level: number, mid: string, candidate: string, ufrag: string): void;
    onPacket(level: number, type: mozPacketDumpType, sending: boolean, packet: ArrayBuffer): void;
    onSetDescriptionError(error: PCErrorData): void;
    onSetDescriptionSuccess(): void;
    onStateChange(state: PCObserverStateType): void;
}

declare var PeerConnectionObserver: {
    prototype: PeerConnectionObserver;
    new(domPC: RTCPeerConnection): PeerConnectionObserver;
    isInstance: IsInstance<PeerConnectionObserver>;
};

interface PerformanceEventMap {
    "resourcetimingbufferfull": Event;
}

interface Performance extends EventTarget {
    readonly eventCounts: EventCounts;
    readonly mozMemory: any;
    readonly navigation: PerformanceNavigation;
    onresourcetimingbufferfull: ((this: Performance, ev: Event) => any) | null;
    readonly timeOrigin: DOMHighResTimeStamp;
    readonly timing: PerformanceTiming;
    clearMarks(markName?: string): void;
    clearMeasures(measureName?: string): void;
    clearResourceTimings(): void;
    getEntries(): PerformanceEntryList;
    getEntriesByName(name: string, entryType?: string): PerformanceEntryList;
    getEntriesByType(entryType: string): PerformanceEntryList;
    mark(markName: string, markOptions?: PerformanceMarkOptions): PerformanceMark;
    measure(measureName: string, startOrMeasureOptions?: string | PerformanceMeasureOptions, endMark?: string): PerformanceMeasure;
    now(): DOMHighResTimeStamp;
    setResourceTimingBufferSize(maxSize: number): void;
    toJSON(): any;
    addEventListener<K extends keyof PerformanceEventMap>(type: K, listener: (this: Performance, ev: PerformanceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof PerformanceEventMap>(type: K, listener: (this: Performance, ev: PerformanceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Performance: {
    prototype: Performance;
    new(): Performance;
    isInstance: IsInstance<Performance>;
};

interface PerformanceEntry {
    readonly duration: DOMHighResTimeStamp;
    readonly entryType: string;
    readonly name: string;
    readonly startTime: DOMHighResTimeStamp;
    toJSON(): any;
}

declare var PerformanceEntry: {
    prototype: PerformanceEntry;
    new(): PerformanceEntry;
    isInstance: IsInstance<PerformanceEntry>;
};

interface PerformanceEntryEvent extends Event {
    readonly duration: DOMHighResTimeStamp;
    readonly entryType: string;
    readonly epoch: number;
    readonly name: string;
    readonly origin: string;
    readonly startTime: DOMHighResTimeStamp;
}

declare var PerformanceEntryEvent: {
    prototype: PerformanceEntryEvent;
    new(type: string, eventInitDict?: PerformanceEntryEventInit): PerformanceEntryEvent;
    isInstance: IsInstance<PerformanceEntryEvent>;
};

interface PerformanceEventTiming extends PerformanceEntry {
    readonly cancelable: boolean;
    readonly processingEnd: DOMHighResTimeStamp;
    readonly processingStart: DOMHighResTimeStamp;
    readonly target: Node | null;
    toJSON(): any;
}

declare var PerformanceEventTiming: {
    prototype: PerformanceEventTiming;
    new(): PerformanceEventTiming;
    isInstance: IsInstance<PerformanceEventTiming>;
};

interface PerformanceMark extends PerformanceEntry {
    readonly detail: any;
}

declare var PerformanceMark: {
    prototype: PerformanceMark;
    new(markName: string, markOptions?: PerformanceMarkOptions): PerformanceMark;
    isInstance: IsInstance<PerformanceMark>;
};

interface PerformanceMeasure extends PerformanceEntry {
    readonly detail: any;
}

declare var PerformanceMeasure: {
    prototype: PerformanceMeasure;
    new(): PerformanceMeasure;
    isInstance: IsInstance<PerformanceMeasure>;
};

interface PerformanceNavigation {
    readonly redirectCount: number;
    readonly type: number;
    toJSON(): any;
    readonly TYPE_NAVIGATE: 0;
    readonly TYPE_RELOAD: 1;
    readonly TYPE_BACK_FORWARD: 2;
    readonly TYPE_RESERVED: 255;
}

declare var PerformanceNavigation: {
    prototype: PerformanceNavigation;
    new(): PerformanceNavigation;
    readonly TYPE_NAVIGATE: 0;
    readonly TYPE_RELOAD: 1;
    readonly TYPE_BACK_FORWARD: 2;
    readonly TYPE_RESERVED: 255;
    isInstance: IsInstance<PerformanceNavigation>;
};

interface PerformanceNavigationTiming extends PerformanceResourceTiming {
    readonly domComplete: DOMHighResTimeStamp;
    readonly domContentLoadedEventEnd: DOMHighResTimeStamp;
    readonly domContentLoadedEventStart: DOMHighResTimeStamp;
    readonly domInteractive: DOMHighResTimeStamp;
    readonly loadEventEnd: DOMHighResTimeStamp;
    readonly loadEventStart: DOMHighResTimeStamp;
    readonly redirectCount: number;
    readonly type: NavigationTimingType;
    readonly unloadEventEnd: DOMHighResTimeStamp;
    readonly unloadEventStart: DOMHighResTimeStamp;
    toJSON(): any;
}

declare var PerformanceNavigationTiming: {
    prototype: PerformanceNavigationTiming;
    new(): PerformanceNavigationTiming;
    isInstance: IsInstance<PerformanceNavigationTiming>;
};

interface PerformanceObserver {
    disconnect(): void;
    observe(options?: PerformanceObserverInit): void;
    takeRecords(): PerformanceEntryList;
}

declare var PerformanceObserver: {
    prototype: PerformanceObserver;
    new(callback: PerformanceObserverCallback): PerformanceObserver;
    isInstance: IsInstance<PerformanceObserver>;
    readonly supportedEntryTypes: any;
};

interface PerformanceObserverEntryList {
    getEntries(filter?: PerformanceEntryFilterOptions): PerformanceEntryList;
    getEntriesByName(name: string, entryType?: string): PerformanceEntryList;
    getEntriesByType(entryType: string): PerformanceEntryList;
}

declare var PerformanceObserverEntryList: {
    prototype: PerformanceObserverEntryList;
    new(): PerformanceObserverEntryList;
    isInstance: IsInstance<PerformanceObserverEntryList>;
};

interface PerformancePaintTiming extends PerformanceEntry {
}

declare var PerformancePaintTiming: {
    prototype: PerformancePaintTiming;
    new(): PerformancePaintTiming;
    isInstance: IsInstance<PerformancePaintTiming>;
};

interface PerformanceResourceTiming extends PerformanceEntry {
    readonly connectEnd: DOMHighResTimeStamp;
    readonly connectStart: DOMHighResTimeStamp;
    readonly contentType: string;
    readonly decodedBodySize: number;
    readonly domainLookupEnd: DOMHighResTimeStamp;
    readonly domainLookupStart: DOMHighResTimeStamp;
    readonly encodedBodySize: number;
    readonly fetchStart: DOMHighResTimeStamp;
    readonly initiatorType: string;
    readonly nextHopProtocol: string;
    readonly redirectEnd: DOMHighResTimeStamp;
    readonly redirectStart: DOMHighResTimeStamp;
    readonly renderBlockingStatus: RenderBlockingStatusType;
    readonly requestStart: DOMHighResTimeStamp;
    readonly responseEnd: DOMHighResTimeStamp;
    readonly responseStart: DOMHighResTimeStamp;
    readonly responseStatus: number;
    readonly secureConnectionStart: DOMHighResTimeStamp;
    /** Available only in secure contexts. */
    readonly serverTiming: PerformanceServerTiming[];
    readonly transferSize: number;
    readonly workerStart: DOMHighResTimeStamp;
    toJSON(): any;
}

declare var PerformanceResourceTiming: {
    prototype: PerformanceResourceTiming;
    new(): PerformanceResourceTiming;
    isInstance: IsInstance<PerformanceResourceTiming>;
};

/** Available only in secure contexts. */
interface PerformanceServerTiming {
    readonly description: string;
    readonly duration: DOMHighResTimeStamp;
    readonly name: string;
    toJSON(): any;
}

declare var PerformanceServerTiming: {
    prototype: PerformanceServerTiming;
    new(): PerformanceServerTiming;
    isInstance: IsInstance<PerformanceServerTiming>;
};

interface PerformanceTiming {
    readonly connectEnd: number;
    readonly connectStart: number;
    readonly domComplete: number;
    readonly domContentLoadedEventEnd: number;
    readonly domContentLoadedEventStart: number;
    readonly domInteractive: number;
    readonly domLoading: number;
    readonly domainLookupEnd: number;
    readonly domainLookupStart: number;
    readonly fetchStart: number;
    readonly loadEventEnd: number;
    readonly loadEventStart: number;
    readonly navigationStart: number;
    readonly redirectEnd: number;
    readonly redirectStart: number;
    readonly requestStart: number;
    readonly responseEnd: number;
    readonly responseStart: number;
    readonly secureConnectionStart: number;
    readonly timeToContentfulPaint: number;
    readonly timeToDOMContentFlushed: number;
    readonly timeToFirstInteractive: number;
    readonly timeToNonBlankPaint: number;
    readonly unloadEventEnd: number;
    readonly unloadEventStart: number;
    toJSON(): any;
}

declare var PerformanceTiming: {
    prototype: PerformanceTiming;
    new(): PerformanceTiming;
    isInstance: IsInstance<PerformanceTiming>;
};

interface PeriodicWave {
}

declare var PeriodicWave: {
    prototype: PeriodicWave;
    new(context: BaseAudioContext, options?: PeriodicWaveOptions): PeriodicWave;
    isInstance: IsInstance<PeriodicWave>;
};

interface PermissionStatusEventMap {
    "change": Event;
}

interface PermissionStatus extends EventTarget {
    readonly name: PermissionName;
    onchange: ((this: PermissionStatus, ev: Event) => any) | null;
    readonly state: PermissionState;
    readonly type: string;
    addEventListener<K extends keyof PermissionStatusEventMap>(type: K, listener: (this: PermissionStatus, ev: PermissionStatusEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof PermissionStatusEventMap>(type: K, listener: (this: PermissionStatus, ev: PermissionStatusEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var PermissionStatus: {
    prototype: PermissionStatus;
    new(): PermissionStatus;
    isInstance: IsInstance<PermissionStatus>;
};

interface Permissions {
    parseSetParameters(parameters: PermissionSetParameters): PermissionStatus;
    query(permission: any): Promise<PermissionStatus>;
}

declare var Permissions: {
    prototype: Permissions;
    new(): Permissions;
    isInstance: IsInstance<Permissions>;
};

interface PlacesBookmark extends PlacesEvent {
    readonly guid: string;
    readonly id: number;
    readonly isTagging: boolean;
    readonly itemType: number;
    readonly parentGuid: string;
    readonly parentId: number;
    readonly source: number;
    readonly url: string;
}

declare var PlacesBookmark: {
    prototype: PlacesBookmark;
    new(): PlacesBookmark;
    isInstance: IsInstance<PlacesBookmark>;
};

interface PlacesBookmarkAddition extends PlacesBookmark {
    readonly dateAdded: number;
    readonly frecency: number;
    readonly hidden: boolean;
    readonly index: number;
    readonly lastVisitDate: number | null;
    readonly tags: string;
    readonly targetFolderGuid: string;
    readonly targetFolderItemId: number;
    readonly targetFolderTitle: string;
    readonly title: string;
    readonly visitCount: number;
}

declare var PlacesBookmarkAddition: {
    prototype: PlacesBookmarkAddition;
    new(initDict: PlacesBookmarkAdditionInit): PlacesBookmarkAddition;
    isInstance: IsInstance<PlacesBookmarkAddition>;
};

interface PlacesBookmarkChanged extends PlacesBookmark {
    readonly lastModified: number;
}

declare var PlacesBookmarkChanged: {
    prototype: PlacesBookmarkChanged;
    new(): PlacesBookmarkChanged;
    isInstance: IsInstance<PlacesBookmarkChanged>;
};

interface PlacesBookmarkGuid extends PlacesBookmarkChanged {
}

declare var PlacesBookmarkGuid: {
    prototype: PlacesBookmarkGuid;
    new(initDict: PlacesBookmarkGuidInit): PlacesBookmarkGuid;
    isInstance: IsInstance<PlacesBookmarkGuid>;
};

interface PlacesBookmarkKeyword extends PlacesBookmarkChanged {
    readonly keyword: string;
}

declare var PlacesBookmarkKeyword: {
    prototype: PlacesBookmarkKeyword;
    new(initDict: PlacesBookmarkKeywordInit): PlacesBookmarkKeyword;
    isInstance: IsInstance<PlacesBookmarkKeyword>;
};

interface PlacesBookmarkMoved extends PlacesBookmark {
    readonly dateAdded: number;
    readonly frecency: number;
    readonly hidden: boolean;
    readonly index: number;
    readonly lastVisitDate: number | null;
    readonly oldIndex: number;
    readonly oldParentGuid: string;
    readonly tags: string;
    readonly title: string;
    readonly visitCount: number;
}

declare var PlacesBookmarkMoved: {
    prototype: PlacesBookmarkMoved;
    new(initDict: PlacesBookmarkMovedInit): PlacesBookmarkMoved;
    isInstance: IsInstance<PlacesBookmarkMoved>;
};

interface PlacesBookmarkRemoved extends PlacesBookmark {
    readonly index: number;
    readonly isDescendantRemoval: boolean;
    readonly title: string;
}

declare var PlacesBookmarkRemoved: {
    prototype: PlacesBookmarkRemoved;
    new(initDict: PlacesBookmarkRemovedInit): PlacesBookmarkRemoved;
    isInstance: IsInstance<PlacesBookmarkRemoved>;
};

interface PlacesBookmarkTags extends PlacesBookmarkChanged {
    readonly tags: string[];
}

declare var PlacesBookmarkTags: {
    prototype: PlacesBookmarkTags;
    new(initDict: PlacesBookmarkTagsInit): PlacesBookmarkTags;
    isInstance: IsInstance<PlacesBookmarkTags>;
};

interface PlacesBookmarkTime extends PlacesBookmarkChanged {
    readonly dateAdded: number;
}

declare var PlacesBookmarkTime: {
    prototype: PlacesBookmarkTime;
    new(initDict: PlacesBookmarkTimeInit): PlacesBookmarkTime;
    isInstance: IsInstance<PlacesBookmarkTime>;
};

interface PlacesBookmarkTitle extends PlacesBookmarkChanged {
    readonly title: string;
}

declare var PlacesBookmarkTitle: {
    prototype: PlacesBookmarkTitle;
    new(initDict: PlacesBookmarkTitleInit): PlacesBookmarkTitle;
    isInstance: IsInstance<PlacesBookmarkTitle>;
};

interface PlacesBookmarkUrl extends PlacesBookmarkChanged {
}

declare var PlacesBookmarkUrl: {
    prototype: PlacesBookmarkUrl;
    new(initDict: PlacesBookmarkUrlInit): PlacesBookmarkUrl;
    isInstance: IsInstance<PlacesBookmarkUrl>;
};

interface PlacesEvent {
    readonly type: PlacesEventType;
}

declare var PlacesEvent: {
    prototype: PlacesEvent;
    new(): PlacesEvent;
    isInstance: IsInstance<PlacesEvent>;
};

interface PlacesEventCounts {
    forEach(callbackfn: (value: number, key: string, parent: PlacesEventCounts) => void, thisArg?: any): void;
}

declare var PlacesEventCounts: {
    prototype: PlacesEventCounts;
    new(): PlacesEventCounts;
    isInstance: IsInstance<PlacesEventCounts>;
};

interface PlacesFavicon extends PlacesEvent {
    readonly faviconUrl: string;
    readonly pageGuid: string;
    readonly url: string;
}

declare var PlacesFavicon: {
    prototype: PlacesFavicon;
    new(initDict: PlacesFaviconInit): PlacesFavicon;
    isInstance: IsInstance<PlacesFavicon>;
};

interface PlacesHistoryCleared extends PlacesEvent {
}

declare var PlacesHistoryCleared: {
    prototype: PlacesHistoryCleared;
    new(): PlacesHistoryCleared;
    isInstance: IsInstance<PlacesHistoryCleared>;
};

interface PlacesPurgeCaches extends PlacesEvent {
}

declare var PlacesPurgeCaches: {
    prototype: PlacesPurgeCaches;
    new(): PlacesPurgeCaches;
    isInstance: IsInstance<PlacesPurgeCaches>;
};

interface PlacesRanking extends PlacesEvent {
}

declare var PlacesRanking: {
    prototype: PlacesRanking;
    new(): PlacesRanking;
    isInstance: IsInstance<PlacesRanking>;
};

interface PlacesVisit extends PlacesEvent {
    readonly frecency: number;
    readonly hidden: boolean;
    readonly lastKnownTitle: string | null;
    readonly pageGuid: string;
    readonly referringVisitId: number;
    readonly transitionType: number;
    readonly typedCount: number;
    readonly url: string;
    readonly visitCount: number;
    readonly visitId: number;
    readonly visitTime: number;
}

declare var PlacesVisit: {
    prototype: PlacesVisit;
    new(): PlacesVisit;
    isInstance: IsInstance<PlacesVisit>;
};

interface PlacesVisitRemoved extends PlacesEvent {
    readonly isPartialVisistsRemoval: boolean;
    readonly isRemovedFromStore: boolean;
    readonly pageGuid: string;
    readonly reason: number;
    readonly transitionType: number;
    readonly url: string;
    readonly REASON_DELETED: 0;
    readonly REASON_EXPIRED: 1;
}

declare var PlacesVisitRemoved: {
    prototype: PlacesVisitRemoved;
    new(initDict: PlacesVisitRemovedInit): PlacesVisitRemoved;
    readonly REASON_DELETED: 0;
    readonly REASON_EXPIRED: 1;
    isInstance: IsInstance<PlacesVisitRemoved>;
};

interface PlacesVisitTitle extends PlacesEvent {
    readonly pageGuid: string;
    readonly title: string;
    readonly url: string;
}

declare var PlacesVisitTitle: {
    prototype: PlacesVisitTitle;
    new(initDict: PlacesVisitTitleInit): PlacesVisitTitle;
    isInstance: IsInstance<PlacesVisitTitle>;
};

interface PlacesWeakCallbackWrapper {
}

declare var PlacesWeakCallbackWrapper: {
    prototype: PlacesWeakCallbackWrapper;
    new(callback: PlacesEventCallback): PlacesWeakCallbackWrapper;
    isInstance: IsInstance<PlacesWeakCallbackWrapper>;
};

interface Plugin {
    readonly description: string;
    readonly filename: string;
    readonly length: number;
    readonly name: string;
    item(index: number): MimeType | null;
    namedItem(name: string): MimeType | null;
    [index: number]: MimeType;
}

declare var Plugin: {
    prototype: Plugin;
    new(): Plugin;
    isInstance: IsInstance<Plugin>;
};

interface PluginArray {
    readonly length: number;
    item(index: number): Plugin | null;
    namedItem(name: string): Plugin | null;
    refresh(): void;
    [index: number]: Plugin;
}

declare var PluginArray: {
    prototype: PluginArray;
    new(): PluginArray;
    isInstance: IsInstance<PluginArray>;
};

interface PluginCrashedEvent extends Event {
    readonly gmpPlugin: boolean;
    readonly pluginDumpID: string;
    readonly pluginFilename: string | null;
    readonly pluginID: number;
    readonly pluginName: string;
    readonly submittedCrashReport: boolean;
}

declare var PluginCrashedEvent: {
    prototype: PluginCrashedEvent;
    new(type: string, eventInitDict?: PluginCrashedEventInit): PluginCrashedEvent;
    isInstance: IsInstance<PluginCrashedEvent>;
};

interface PointerEvent extends MouseEvent {
    readonly height: number;
    readonly isPrimary: boolean;
    readonly pointerId: number;
    readonly pointerType: string;
    readonly pressure: number;
    readonly tangentialPressure: number;
    readonly tiltX: number;
    readonly tiltY: number;
    readonly twist: number;
    readonly width: number;
    getCoalescedEvents(): PointerEvent[];
    getPredictedEvents(): PointerEvent[];
}

declare var PointerEvent: {
    prototype: PointerEvent;
    new(type: string, eventInitDict?: PointerEventInit): PointerEvent;
    isInstance: IsInstance<PointerEvent>;
};

interface PopStateEvent extends Event {
    readonly state: any;
}

declare var PopStateEvent: {
    prototype: PopStateEvent;
    new(type: string, eventInitDict?: PopStateEventInit): PopStateEvent;
    isInstance: IsInstance<PopStateEvent>;
};

interface PopoverInvokerElement {
    popoverTargetAction: string;
    popoverTargetElement: Element | null;
}

interface PopupBlockedEvent extends Event {
    readonly popupWindowFeatures: string | null;
    readonly popupWindowName: string | null;
    readonly popupWindowURI: URI | null;
    readonly requestingWindow: Window | null;
}

declare var PopupBlockedEvent: {
    prototype: PopupBlockedEvent;
    new(type: string, eventInitDict?: PopupBlockedEventInit): PopupBlockedEvent;
    isInstance: IsInstance<PopupBlockedEvent>;
};

interface PopupPositionedEvent extends Event {
    readonly alignmentOffset: number;
    readonly alignmentPosition: string;
    readonly isAnchored: boolean;
    readonly popupAlignment: string;
}

declare var PopupPositionedEvent: {
    prototype: PopupPositionedEvent;
    new(type: string, init?: PopupPositionedEventInit): PopupPositionedEvent;
    isInstance: IsInstance<PopupPositionedEvent>;
};

interface PositionStateEvent extends Event {
    readonly duration: number;
    readonly playbackRate: number;
    readonly position: number;
}

declare var PositionStateEvent: {
    prototype: PositionStateEvent;
    new(type: string, eventInitDict?: PositionStateEventInit): PositionStateEvent;
    isInstance: IsInstance<PositionStateEvent>;
};

interface PrecompiledScript {
    readonly hasReturnValue: boolean;
    readonly url: string;
    executeInGlobal(global: any, options?: ExecuteInGlobalOptions): any;
}

declare var PrecompiledScript: {
    prototype: PrecompiledScript;
    new(): PrecompiledScript;
    isInstance: IsInstance<PrecompiledScript>;
};

interface Principal {
}

/** Available only in secure contexts. */
interface PrivateAttribution {
    measureConversion(options: PrivateAttributionConversionOptions): void;
    saveImpression(options: PrivateAttributionImpressionOptions): void;
}

declare var PrivateAttribution: {
    prototype: PrivateAttribution;
    new(): PrivateAttribution;
    isInstance: IsInstance<PrivateAttribution>;
};

interface ProcessMessageManager extends MessageSender, ProcessScriptLoader {
    readonly isInProcess: boolean;
    readonly osPid: number;
}

declare var ProcessMessageManager: {
    prototype: ProcessMessageManager;
    new(): ProcessMessageManager;
    isInstance: IsInstance<ProcessMessageManager>;
};

interface ProcessScriptLoader {
    getDelayedProcessScripts(): any[][];
    loadProcessScript(url: string, allowDelayedLoad: boolean): void;
    removeDelayedProcessScript(url: string): void;
}

interface ProcessingInstruction extends CharacterData, LinkStyle {
    readonly target: string;
}

declare var ProcessingInstruction: {
    prototype: ProcessingInstruction;
    new(): ProcessingInstruction;
    isInstance: IsInstance<ProcessingInstruction>;
};

interface ProgressEvent extends Event {
    readonly lengthComputable: boolean;
    readonly loaded: number;
    readonly total: number;
}

declare var ProgressEvent: {
    prototype: ProgressEvent;
    new(type: string, eventInitDict?: ProgressEventInit): ProgressEvent;
    isInstance: IsInstance<ProgressEvent>;
};

interface PromiseNativeHandler {
}

interface PromiseRejectionEvent extends Event {
    readonly promise: Promise<any>;
    readonly reason: any;
}

declare var PromiseRejectionEvent: {
    prototype: PromiseRejectionEvent;
    new(type: string, eventInitDict: PromiseRejectionEventInit): PromiseRejectionEvent;
    isInstance: IsInstance<PromiseRejectionEvent>;
};

/** Available only in secure contexts. */
interface PublicKeyCredential extends Credential {
    readonly authenticatorAttachment: string | null;
    readonly rawId: ArrayBuffer;
    readonly response: AuthenticatorResponse;
    getClientExtensionResults(): AuthenticationExtensionsClientOutputs;
    toJSON(): any;
}

declare var PublicKeyCredential: {
    prototype: PublicKeyCredential;
    new(): PublicKeyCredential;
    isInstance: IsInstance<PublicKeyCredential>;
    isConditionalMediationAvailable(): Promise<boolean>;
    isUserVerifyingPlatformAuthenticatorAvailable(): Promise<boolean>;
    parseCreationOptionsFromJSON(options: PublicKeyCredentialCreationOptionsJSON): PublicKeyCredentialCreationOptions;
    parseRequestOptionsFromJSON(options: PublicKeyCredentialRequestOptionsJSON): PublicKeyCredentialRequestOptions;
};

interface PushManager {
    getSubscription(): Promise<PushSubscription | null>;
    permissionState(options?: PushSubscriptionOptionsInit): Promise<PermissionState>;
    subscribe(options?: PushSubscriptionOptionsInit): Promise<PushSubscription>;
}

declare var PushManager: {
    prototype: PushManager;
    new(scope: string): PushManager;
    isInstance: IsInstance<PushManager>;
};

interface PushManagerImpl {
    getSubscription(): Promise<PushSubscription | null>;
    permissionState(options?: PushSubscriptionOptionsInit): Promise<PermissionState>;
    subscribe(options?: PushSubscriptionOptionsInit): Promise<PushSubscription>;
}

declare var PushManagerImpl: {
    prototype: PushManagerImpl;
    new(scope: string): PushManagerImpl;
    isInstance: IsInstance<PushManagerImpl>;
};

interface PushSubscription {
    readonly endpoint: string;
    readonly expirationTime: EpochTimeStamp | null;
    readonly options: PushSubscriptionOptions;
    getKey(name: PushEncryptionKeyName): ArrayBuffer | null;
    toJSON(): PushSubscriptionJSON;
    unsubscribe(): Promise<boolean>;
}

declare var PushSubscription: {
    prototype: PushSubscription;
    new(initDict: PushSubscriptionInit): PushSubscription;
    isInstance: IsInstance<PushSubscription>;
};

interface PushSubscriptionOptions {
    readonly applicationServerKey: ArrayBuffer | null;
}

declare var PushSubscriptionOptions: {
    prototype: PushSubscriptionOptions;
    new(): PushSubscriptionOptions;
    isInstance: IsInstance<PushSubscriptionOptions>;
};

interface RTCCertificate {
    readonly expires: DOMTimeStamp;
}

declare var RTCCertificate: {
    prototype: RTCCertificate;
    new(): RTCCertificate;
    isInstance: IsInstance<RTCCertificate>;
};

interface RTCDTMFSenderEventMap {
    "tonechange": Event;
}

interface RTCDTMFSender extends EventTarget {
    readonly canInsertDTMF: boolean;
    ontonechange: ((this: RTCDTMFSender, ev: Event) => any) | null;
    readonly toneBuffer: string;
    insertDTMF(tones: string, duration?: number, interToneGap?: number): void;
    addEventListener<K extends keyof RTCDTMFSenderEventMap>(type: K, listener: (this: RTCDTMFSender, ev: RTCDTMFSenderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCDTMFSenderEventMap>(type: K, listener: (this: RTCDTMFSender, ev: RTCDTMFSenderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCDTMFSender: {
    prototype: RTCDTMFSender;
    new(): RTCDTMFSender;
    isInstance: IsInstance<RTCDTMFSender>;
};

interface RTCDTMFToneChangeEvent extends Event {
    readonly tone: string;
}

declare var RTCDTMFToneChangeEvent: {
    prototype: RTCDTMFToneChangeEvent;
    new(type: string, eventInitDict?: RTCDTMFToneChangeEventInit): RTCDTMFToneChangeEvent;
    isInstance: IsInstance<RTCDTMFToneChangeEvent>;
};

interface RTCDataChannelEventMap {
    "bufferedamountlow": Event;
    "close": Event;
    "error": Event;
    "message": Event;
    "open": Event;
}

interface RTCDataChannel extends EventTarget {
    binaryType: RTCDataChannelType;
    readonly bufferedAmount: number;
    bufferedAmountLowThreshold: number;
    readonly id: number | null;
    readonly label: string;
    readonly maxPacketLifeTime: number | null;
    readonly maxRetransmits: number | null;
    readonly negotiated: boolean;
    onbufferedamountlow: ((this: RTCDataChannel, ev: Event) => any) | null;
    onclose: ((this: RTCDataChannel, ev: Event) => any) | null;
    onerror: ((this: RTCDataChannel, ev: Event) => any) | null;
    onmessage: ((this: RTCDataChannel, ev: Event) => any) | null;
    onopen: ((this: RTCDataChannel, ev: Event) => any) | null;
    readonly ordered: boolean;
    readonly protocol: string;
    readonly readyState: RTCDataChannelState;
    readonly reliable: boolean;
    close(): void;
    send(data: string): void;
    send(data: Blob): void;
    send(data: ArrayBuffer): void;
    send(data: ArrayBufferView): void;
    addEventListener<K extends keyof RTCDataChannelEventMap>(type: K, listener: (this: RTCDataChannel, ev: RTCDataChannelEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCDataChannelEventMap>(type: K, listener: (this: RTCDataChannel, ev: RTCDataChannelEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCDataChannel: {
    prototype: RTCDataChannel;
    new(): RTCDataChannel;
    isInstance: IsInstance<RTCDataChannel>;
};

interface RTCDataChannelEvent extends Event {
    readonly channel: RTCDataChannel;
}

declare var RTCDataChannelEvent: {
    prototype: RTCDataChannelEvent;
    new(type: string, eventInitDict: RTCDataChannelEventInit): RTCDataChannelEvent;
    isInstance: IsInstance<RTCDataChannelEvent>;
};

interface RTCDtlsTransportEventMap {
    "statechange": Event;
}

interface RTCDtlsTransport extends EventTarget {
    readonly iceTransport: RTCIceTransport;
    onstatechange: ((this: RTCDtlsTransport, ev: Event) => any) | null;
    readonly state: RTCDtlsTransportState;
    addEventListener<K extends keyof RTCDtlsTransportEventMap>(type: K, listener: (this: RTCDtlsTransport, ev: RTCDtlsTransportEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCDtlsTransportEventMap>(type: K, listener: (this: RTCDtlsTransport, ev: RTCDtlsTransportEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCDtlsTransport: {
    prototype: RTCDtlsTransport;
    new(): RTCDtlsTransport;
    isInstance: IsInstance<RTCDtlsTransport>;
};

interface RTCEncodedAudioFrame {
    data: ArrayBuffer;
    readonly timestamp: number;
    getMetadata(): RTCEncodedAudioFrameMetadata;
}

declare var RTCEncodedAudioFrame: {
    prototype: RTCEncodedAudioFrame;
    new(): RTCEncodedAudioFrame;
    isInstance: IsInstance<RTCEncodedAudioFrame>;
};

interface RTCEncodedVideoFrame {
    data: ArrayBuffer;
    readonly timestamp: number;
    readonly type: RTCEncodedVideoFrameType;
    getMetadata(): RTCEncodedVideoFrameMetadata;
}

declare var RTCEncodedVideoFrame: {
    prototype: RTCEncodedVideoFrame;
    new(): RTCEncodedVideoFrame;
    isInstance: IsInstance<RTCEncodedVideoFrame>;
};

interface RTCIceCandidate {
    readonly address: string | null;
    readonly candidate: string;
    readonly component: RTCIceComponent | null;
    readonly foundation: string | null;
    readonly port: number | null;
    readonly priority: number | null;
    readonly protocol: RTCIceProtocol | null;
    readonly relatedAddress: string | null;
    readonly relatedPort: number | null;
    readonly sdpMLineIndex: number | null;
    readonly sdpMid: string | null;
    readonly tcpType: RTCIceTcpCandidateType | null;
    readonly type: RTCIceCandidateType | null;
    readonly usernameFragment: string | null;
    toJSON(): RTCIceCandidateInit;
}

declare var RTCIceCandidate: {
    prototype: RTCIceCandidate;
    new(candidateInitDict?: RTCIceCandidateInit): RTCIceCandidate;
    isInstance: IsInstance<RTCIceCandidate>;
};

interface RTCIceTransportEventMap {
    "gatheringstatechange": Event;
    "statechange": Event;
}

interface RTCIceTransport extends EventTarget {
    readonly gatheringState: RTCIceGathererState;
    ongatheringstatechange: ((this: RTCIceTransport, ev: Event) => any) | null;
    onstatechange: ((this: RTCIceTransport, ev: Event) => any) | null;
    readonly state: RTCIceTransportState;
    addEventListener<K extends keyof RTCIceTransportEventMap>(type: K, listener: (this: RTCIceTransport, ev: RTCIceTransportEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCIceTransportEventMap>(type: K, listener: (this: RTCIceTransport, ev: RTCIceTransportEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCIceTransport: {
    prototype: RTCIceTransport;
    new(): RTCIceTransport;
    isInstance: IsInstance<RTCIceTransport>;
};

interface RTCIdentityProviderRegistrar {
    readonly hasIdp: boolean;
    generateAssertion(contents: string, origin: string, options?: RTCIdentityProviderOptions): Promise<RTCIdentityAssertionResult>;
    register(idp: RTCIdentityProvider): void;
    validateAssertion(assertion: string, origin: string): Promise<RTCIdentityValidationResult>;
}

interface RTCPeerConnectionEventMap {
    "addstream": Event;
    "addtrack": Event;
    "connectionstatechange": Event;
    "datachannel": Event;
    "icecandidate": Event;
    "iceconnectionstatechange": Event;
    "icegatheringstatechange": Event;
    "negotiationneeded": Event;
    "signalingstatechange": Event;
    "track": Event;
}

interface RTCPeerConnection extends EventTarget {
    readonly canTrickleIceCandidates: boolean | null;
    readonly connectionState: RTCPeerConnectionState;
    readonly currentLocalDescription: RTCSessionDescription | null;
    readonly currentRemoteDescription: RTCSessionDescription | null;
    readonly iceConnectionState: RTCIceConnectionState;
    readonly iceGatheringState: RTCIceGatheringState;
    id: string;
    readonly idpLoginUrl: string | null;
    readonly localDescription: RTCSessionDescription | null;
    onaddstream: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onaddtrack: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onconnectionstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
    ondatachannel: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onicecandidate: ((this: RTCPeerConnection, ev: Event) => any) | null;
    oniceconnectionstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onicegatheringstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onnegotiationneeded: ((this: RTCPeerConnection, ev: Event) => any) | null;
    onsignalingstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
    ontrack: ((this: RTCPeerConnection, ev: Event) => any) | null;
    readonly peerIdentity: Promise<RTCIdentityAssertion>;
    readonly pendingLocalDescription: RTCSessionDescription | null;
    readonly pendingRemoteDescription: RTCSessionDescription | null;
    readonly remoteDescription: RTCSessionDescription | null;
    readonly sctp: RTCSctpTransport | null;
    readonly signalingState: RTCSignalingState;
    addIceCandidate(candidate: RTCIceCandidateInit, successCallback: VoidFunction, failureCallback: RTCPeerConnectionErrorCallback): Promise<void>;
    addStream(stream: MediaStream): void;
    addTrack(track: MediaStreamTrack, ...streams: MediaStream[]): RTCRtpSender;
    addTransceiver(trackOrKind: MediaStreamTrack | string, init?: RTCRtpTransceiverInit): RTCRtpTransceiver;
    close(): void;
    createAnswer(successCallback: RTCSessionDescriptionCallback, failureCallback: RTCPeerConnectionErrorCallback): Promise<void>;
    createDataChannel(label: string, dataChannelDict?: RTCDataChannelInit): RTCDataChannel;
    createOffer(successCallback: RTCSessionDescriptionCallback, failureCallback: RTCPeerConnectionErrorCallback, options?: RTCOfferOptions): Promise<void>;
    getConfiguration(): RTCConfiguration;
    getIdentityAssertion(): Promise<string>;
    getLocalStreams(): MediaStream[];
    getReceivers(): RTCRtpReceiver[];
    getRemoteStreams(): MediaStream[];
    getSenders(): RTCRtpSender[];
    getStats(selector?: MediaStreamTrack | null): Promise<RTCStatsReport>;
    getTransceivers(): RTCRtpTransceiver[];
    mozDisablePacketDump(level: number, type: mozPacketDumpType, sending: boolean): void;
    mozEnablePacketDump(level: number, type: mozPacketDumpType, sending: boolean): void;
    mozSetPacketCallback(callback: mozPacketCallback): void;
    removeTrack(sender: RTCRtpSender): void;
    restartIce(): void;
    setConfiguration(configuration?: RTCConfiguration): void;
    setIdentityProvider(provider: string, options?: RTCIdentityProviderOptions): void;
    setLocalDescription(description: RTCLocalSessionDescriptionInit, successCallback: VoidFunction, failureCallback: RTCPeerConnectionErrorCallback): Promise<void>;
    setRemoteDescription(description: RTCSessionDescriptionInit, successCallback: VoidFunction, failureCallback: RTCPeerConnectionErrorCallback): Promise<void>;
    addEventListener<K extends keyof RTCPeerConnectionEventMap>(type: K, listener: (this: RTCPeerConnection, ev: RTCPeerConnectionEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCPeerConnectionEventMap>(type: K, listener: (this: RTCPeerConnection, ev: RTCPeerConnectionEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCPeerConnection: {
    prototype: RTCPeerConnection;
    new(configuration?: RTCConfiguration): RTCPeerConnection;
    isInstance: IsInstance<RTCPeerConnection>;
    generateCertificate(keygenAlgorithm: AlgorithmIdentifier): Promise<RTCCertificate>;
};

interface RTCPeerConnectionIceEvent extends Event {
    readonly candidate: RTCIceCandidate | null;
}

declare var RTCPeerConnectionIceEvent: {
    prototype: RTCPeerConnectionIceEvent;
    new(type: string, eventInitDict?: RTCPeerConnectionIceEventInit): RTCPeerConnectionIceEvent;
    isInstance: IsInstance<RTCPeerConnectionIceEvent>;
};

interface RTCPeerConnectionStatic {
    registerPeerConnectionLifecycleCallback(cb: PeerConnectionLifecycleCallback): void;
}

declare var RTCPeerConnectionStatic: {
    prototype: RTCPeerConnectionStatic;
    new(): RTCPeerConnectionStatic;
    isInstance: IsInstance<RTCPeerConnectionStatic>;
};

interface RTCRtpReceiver {
    jitterBufferTarget: DOMHighResTimeStamp | null;
    readonly track: MediaStreamTrack;
    transform: RTCRtpTransform | null;
    readonly transport: RTCDtlsTransport | null;
    getContributingSources(): RTCRtpContributingSource[];
    getParameters(): RTCRtpReceiveParameters;
    getStats(): Promise<RTCStatsReport>;
    getSynchronizationSources(): RTCRtpSynchronizationSource[];
    mozInsertAudioLevelForContributingSource(source: number, timestamp: DOMHighResTimeStamp, rtpTimestamp: number, hasLevel: boolean, level: number): void;
}

declare var RTCRtpReceiver: {
    prototype: RTCRtpReceiver;
    new(): RTCRtpReceiver;
    isInstance: IsInstance<RTCRtpReceiver>;
    getCapabilities(kind: string): RTCRtpCapabilities | null;
};

interface RTCRtpScriptTransform {
}

declare var RTCRtpScriptTransform: {
    prototype: RTCRtpScriptTransform;
    new(worker: Worker, options?: any, transfer?: any[]): RTCRtpScriptTransform;
    isInstance: IsInstance<RTCRtpScriptTransform>;
};

interface RTCRtpSender {
    readonly dtmf: RTCDTMFSender | null;
    readonly track: MediaStreamTrack | null;
    transform: RTCRtpTransform | null;
    readonly transport: RTCDtlsTransport | null;
    getParameters(): RTCRtpSendParameters;
    getStats(): Promise<RTCStatsReport>;
    getStreams(): MediaStream[];
    replaceTrack(withTrack: MediaStreamTrack | null): Promise<void>;
    setParameters(parameters: RTCRtpSendParameters): Promise<void>;
    setStreams(...streams: MediaStream[]): void;
    setStreamsImpl(...streams: MediaStream[]): void;
    setTrack(track: MediaStreamTrack | null): void;
}

declare var RTCRtpSender: {
    prototype: RTCRtpSender;
    new(): RTCRtpSender;
    isInstance: IsInstance<RTCRtpSender>;
    getCapabilities(kind: string): RTCRtpCapabilities | null;
};

interface RTCRtpTransceiver {
    readonly currentDirection: RTCRtpTransceiverDirection | null;
    direction: RTCRtpTransceiverDirection;
    readonly mid: string | null;
    readonly receiver: RTCRtpReceiver;
    readonly sender: RTCRtpSender;
    readonly stopped: boolean;
    getKind(): string;
    hasBeenUsedToSend(): boolean;
    setCodecPreferences(codecs: RTCRtpCodec[]): void;
    setDirectionInternal(direction: RTCRtpTransceiverDirection): void;
    stop(): void;
}

declare var RTCRtpTransceiver: {
    prototype: RTCRtpTransceiver;
    new(): RTCRtpTransceiver;
    isInstance: IsInstance<RTCRtpTransceiver>;
};

interface RTCSctpTransportEventMap {
    "statechange": Event;
}

interface RTCSctpTransport extends EventTarget {
    readonly maxChannels: number | null;
    readonly maxMessageSize: number;
    onstatechange: ((this: RTCSctpTransport, ev: Event) => any) | null;
    readonly state: RTCSctpTransportState;
    readonly transport: RTCDtlsTransport;
    addEventListener<K extends keyof RTCSctpTransportEventMap>(type: K, listener: (this: RTCSctpTransport, ev: RTCSctpTransportEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof RTCSctpTransportEventMap>(type: K, listener: (this: RTCSctpTransport, ev: RTCSctpTransportEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var RTCSctpTransport: {
    prototype: RTCSctpTransport;
    new(): RTCSctpTransport;
    isInstance: IsInstance<RTCSctpTransport>;
};

interface RTCSessionDescription {
    sdp: string;
    type: RTCSdpType;
    toJSON(): any;
}

declare var RTCSessionDescription: {
    prototype: RTCSessionDescription;
    new(descriptionInitDict: RTCSessionDescriptionInit): RTCSessionDescription;
    isInstance: IsInstance<RTCSessionDescription>;
};

interface RTCStatsReport {
    forEach(callbackfn: (value: any, key: string, parent: RTCStatsReport) => void, thisArg?: any): void;
}

declare var RTCStatsReport: {
    prototype: RTCStatsReport;
    new(): RTCStatsReport;
    isInstance: IsInstance<RTCStatsReport>;
};

interface RTCTrackEvent extends Event {
    readonly receiver: RTCRtpReceiver;
    readonly streams: MediaStream[];
    readonly track: MediaStreamTrack;
    readonly transceiver: RTCRtpTransceiver;
}

declare var RTCTrackEvent: {
    prototype: RTCTrackEvent;
    new(type: string, eventInitDict: RTCTrackEventInit): RTCTrackEvent;
    isInstance: IsInstance<RTCTrackEvent>;
};

interface RadioNodeList extends NodeList {
    value: string;
}

declare var RadioNodeList: {
    prototype: RadioNodeList;
    new(): RadioNodeList;
    isInstance: IsInstance<RadioNodeList>;
};

interface Range extends AbstractRange {
    readonly commonAncestorContainer: Node;
    cloneContents(): DocumentFragment;
    cloneRange(): Range;
    collapse(toStart?: boolean): void;
    compareBoundaryPoints(how: number, sourceRange: Range): number;
    comparePoint(node: Node, offset: number): number;
    createContextualFragment(fragment: string): DocumentFragment;
    deleteContents(): void;
    detach(): void;
    extractContents(): DocumentFragment;
    getBoundingClientRect(): DOMRect;
    getClientRects(): DOMRectList | null;
    getClientRectsAndTexts(): ClientRectsAndTexts;
    insertNode(node: Node): void;
    intersectsNode(node: Node): boolean;
    isPointInRange(node: Node, offset: number): boolean;
    selectNode(refNode: Node): void;
    selectNodeContents(refNode: Node): void;
    setEnd(refNode: Node, offset: number): void;
    setEndAfter(refNode: Node): void;
    setEndAllowCrossShadowBoundary(refNode: Node, offset: number): void;
    setEndBefore(refNode: Node): void;
    setStart(refNode: Node, offset: number): void;
    setStartAfter(refNode: Node): void;
    setStartAllowCrossShadowBoundary(refNode: Node, offset: number): void;
    setStartBefore(refNode: Node): void;
    surroundContents(newParent: Node): void;
    toString(): string;
    readonly START_TO_START: 0;
    readonly START_TO_END: 1;
    readonly END_TO_END: 2;
    readonly END_TO_START: 3;
}

declare var Range: {
    prototype: Range;
    new(): Range;
    readonly START_TO_START: 0;
    readonly START_TO_END: 1;
    readonly END_TO_END: 2;
    readonly END_TO_START: 3;
    isInstance: IsInstance<Range>;
};

interface ReadableByteStreamController {
    readonly byobRequest: ReadableStreamBYOBRequest | null;
    readonly desiredSize: number | null;
    close(): void;
    enqueue(chunk: ArrayBufferView): void;
    error(e?: any): void;
}

declare var ReadableByteStreamController: {
    prototype: ReadableByteStreamController;
    new(): ReadableByteStreamController;
    isInstance: IsInstance<ReadableByteStreamController>;
};

interface ReadableStream {
    readonly locked: boolean;
    cancel(reason?: any): Promise<void>;
    getReader(options?: ReadableStreamGetReaderOptions): ReadableStreamReader;
    pipeThrough(transform: ReadableWritablePair, options?: StreamPipeOptions): ReadableStream;
    pipeTo(destination: WritableStream, options?: StreamPipeOptions): Promise<void>;
    tee(): ReadableStream[];
}

declare var ReadableStream: {
    prototype: ReadableStream;
    new(underlyingSource?: any, strategy?: QueuingStrategy): ReadableStream;
    isInstance: IsInstance<ReadableStream>;
    from(asyncIterable: any): ReadableStream;
};

interface ReadableStreamBYOBReader extends ReadableStreamGenericReader {
    read(view: ArrayBufferView): Promise<ReadableStreamReadResult>;
    releaseLock(): void;
}

declare var ReadableStreamBYOBReader: {
    prototype: ReadableStreamBYOBReader;
    new(stream: ReadableStream): ReadableStreamBYOBReader;
    isInstance: IsInstance<ReadableStreamBYOBReader>;
};

interface ReadableStreamBYOBRequest {
    readonly view: ArrayBufferView | null;
    respond(bytesWritten: number): void;
    respondWithNewView(view: ArrayBufferView): void;
}

declare var ReadableStreamBYOBRequest: {
    prototype: ReadableStreamBYOBRequest;
    new(): ReadableStreamBYOBRequest;
    isInstance: IsInstance<ReadableStreamBYOBRequest>;
};

interface ReadableStreamDefaultController {
    readonly desiredSize: number | null;
    close(): void;
    enqueue(chunk?: any): void;
    error(e?: any): void;
}

declare var ReadableStreamDefaultController: {
    prototype: ReadableStreamDefaultController;
    new(): ReadableStreamDefaultController;
    isInstance: IsInstance<ReadableStreamDefaultController>;
};

interface ReadableStreamDefaultReader extends ReadableStreamGenericReader {
    read(): Promise<ReadableStreamReadResult>;
    releaseLock(): void;
}

declare var ReadableStreamDefaultReader: {
    prototype: ReadableStreamDefaultReader;
    new(stream: ReadableStream): ReadableStreamDefaultReader;
    isInstance: IsInstance<ReadableStreamDefaultReader>;
};

interface ReadableStreamGenericReader {
    readonly closed: Promise<undefined>;
    cancel(reason?: any): Promise<void>;
}

interface ReferrerInfo {
}

interface RemoteTab {
}

interface Report {
    readonly body: ReportBody | null;
    readonly type: string;
    readonly url: string;
    toJSON(): any;
}

declare var Report: {
    prototype: Report;
    new(): Report;
    isInstance: IsInstance<Report>;
};

interface ReportBody {
    toJSON(): any;
}

declare var ReportBody: {
    prototype: ReportBody;
    new(): ReportBody;
    isInstance: IsInstance<ReportBody>;
};

interface ReportingObserver {
    disconnect(): void;
    observe(): void;
    takeRecords(): ReportList;
}

declare var ReportingObserver: {
    prototype: ReportingObserver;
    new(callback: ReportingObserverCallback, options?: ReportingObserverOptions): ReportingObserver;
    isInstance: IsInstance<ReportingObserver>;
};

interface Request extends Body {
    readonly cache: RequestCache;
    readonly credentials: RequestCredentials;
    readonly destination: RequestDestination;
    readonly headers: Headers;
    readonly integrity: string;
    readonly keepalive: boolean;
    readonly method: string;
    readonly mode: RequestMode;
    readonly mozErrors: boolean;
    readonly redirect: RequestRedirect;
    readonly referrer: string;
    readonly referrerPolicy: ReferrerPolicy;
    readonly signal: AbortSignal;
    readonly url: string;
    clone(): Request;
    overrideContentPolicyType(context: nsContentPolicyType): void;
}

declare var Request: {
    prototype: Request;
    new(input: RequestInfo | URL, init?: RequestInit): Request;
    isInstance: IsInstance<Request>;
};

interface ResizeObserver {
    disconnect(): void;
    observe(target: Element, options?: ResizeObserverOptions): void;
    unobserve(target: Element): void;
}

declare var ResizeObserver: {
    prototype: ResizeObserver;
    new(callback: ResizeObserverCallback): ResizeObserver;
    isInstance: IsInstance<ResizeObserver>;
};

interface ResizeObserverEntry {
    readonly borderBoxSize: ResizeObserverSize[];
    readonly contentBoxSize: ResizeObserverSize[];
    readonly contentRect: DOMRectReadOnly;
    readonly devicePixelContentBoxSize: ResizeObserverSize[];
    readonly target: Element;
}

declare var ResizeObserverEntry: {
    prototype: ResizeObserverEntry;
    new(): ResizeObserverEntry;
    isInstance: IsInstance<ResizeObserverEntry>;
};

interface ResizeObserverSize {
    readonly blockSize: number;
    readonly inlineSize: number;
}

declare var ResizeObserverSize: {
    prototype: ResizeObserverSize;
    new(): ResizeObserverSize;
    isInstance: IsInstance<ResizeObserverSize>;
};

interface Response extends Body {
    readonly body: ReadableStream | null;
    readonly hasCacheInfoChannel: boolean;
    readonly headers: Headers;
    readonly ok: boolean;
    readonly redirected: boolean;
    readonly status: number;
    readonly statusText: string;
    readonly type: ResponseType;
    readonly url: string;
    clone(): Response;
    cloneUnfiltered(): Response;
}

declare var Response: {
    prototype: Response;
    new(body?: Blob | BufferSource | FormData | URLSearchParams | ReadableStream | string | null, init?: ResponseInit): Response;
    isInstance: IsInstance<Response>;
    error(): Response;
    json(data: any, init?: ResponseInit): Response;
    redirect(url: string, status?: number): Response;
};

interface SVGAElement extends SVGGraphicsElement, SVGURIReference {
    download: string;
    hreflang: string;
    ping: string;
    referrerPolicy: string;
    rel: string;
    readonly relList: DOMTokenList;
    readonly target: SVGAnimatedString;
    text: string;
    type: string;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGAElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGAElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGAElement: {
    prototype: SVGAElement;
    new(): SVGAElement;
    isInstance: IsInstance<SVGAElement>;
};

interface SVGAngle {
    readonly unitType: number;
    value: number;
    valueAsString: string;
    valueInSpecifiedUnits: number;
    convertToSpecifiedUnits(unitType: number): void;
    newValueSpecifiedUnits(unitType: number, valueInSpecifiedUnits: number): void;
    readonly SVG_ANGLETYPE_UNKNOWN: 0;
    readonly SVG_ANGLETYPE_UNSPECIFIED: 1;
    readonly SVG_ANGLETYPE_DEG: 2;
    readonly SVG_ANGLETYPE_RAD: 3;
    readonly SVG_ANGLETYPE_GRAD: 4;
}

declare var SVGAngle: {
    prototype: SVGAngle;
    new(): SVGAngle;
    readonly SVG_ANGLETYPE_UNKNOWN: 0;
    readonly SVG_ANGLETYPE_UNSPECIFIED: 1;
    readonly SVG_ANGLETYPE_DEG: 2;
    readonly SVG_ANGLETYPE_RAD: 3;
    readonly SVG_ANGLETYPE_GRAD: 4;
    isInstance: IsInstance<SVGAngle>;
};

interface SVGAnimateElement extends SVGAnimationElement {
    addEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGAnimateElement: {
    prototype: SVGAnimateElement;
    new(): SVGAnimateElement;
    isInstance: IsInstance<SVGAnimateElement>;
};

interface SVGAnimateMotionElement extends SVGAnimationElement {
    addEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateMotionElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateMotionElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGAnimateMotionElement: {
    prototype: SVGAnimateMotionElement;
    new(): SVGAnimateMotionElement;
    isInstance: IsInstance<SVGAnimateMotionElement>;
};

interface SVGAnimateTransformElement extends SVGAnimationElement {
    addEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateTransformElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimateTransformElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGAnimateTransformElement: {
    prototype: SVGAnimateTransformElement;
    new(): SVGAnimateTransformElement;
    isInstance: IsInstance<SVGAnimateTransformElement>;
};

interface SVGAnimatedAngle {
    readonly animVal: SVGAngle;
    readonly baseVal: SVGAngle;
}

declare var SVGAnimatedAngle: {
    prototype: SVGAnimatedAngle;
    new(): SVGAnimatedAngle;
    isInstance: IsInstance<SVGAnimatedAngle>;
};

interface SVGAnimatedBoolean {
    readonly animVal: boolean;
    baseVal: boolean;
}

declare var SVGAnimatedBoolean: {
    prototype: SVGAnimatedBoolean;
    new(): SVGAnimatedBoolean;
    isInstance: IsInstance<SVGAnimatedBoolean>;
};

interface SVGAnimatedEnumeration {
    readonly animVal: number;
    baseVal: number;
}

declare var SVGAnimatedEnumeration: {
    prototype: SVGAnimatedEnumeration;
    new(): SVGAnimatedEnumeration;
    isInstance: IsInstance<SVGAnimatedEnumeration>;
};

interface SVGAnimatedInteger {
    readonly animVal: number;
    baseVal: number;
}

declare var SVGAnimatedInteger: {
    prototype: SVGAnimatedInteger;
    new(): SVGAnimatedInteger;
    isInstance: IsInstance<SVGAnimatedInteger>;
};

interface SVGAnimatedLength {
    readonly animVal: SVGLength;
    readonly baseVal: SVGLength;
}

declare var SVGAnimatedLength: {
    prototype: SVGAnimatedLength;
    new(): SVGAnimatedLength;
    isInstance: IsInstance<SVGAnimatedLength>;
};

interface SVGAnimatedLengthList {
    readonly animVal: SVGLengthList;
    readonly baseVal: SVGLengthList;
}

declare var SVGAnimatedLengthList: {
    prototype: SVGAnimatedLengthList;
    new(): SVGAnimatedLengthList;
    isInstance: IsInstance<SVGAnimatedLengthList>;
};

interface SVGAnimatedNumber {
    readonly animVal: number;
    baseVal: number;
}

declare var SVGAnimatedNumber: {
    prototype: SVGAnimatedNumber;
    new(): SVGAnimatedNumber;
    isInstance: IsInstance<SVGAnimatedNumber>;
};

interface SVGAnimatedNumberList {
    readonly animVal: SVGNumberList;
    readonly baseVal: SVGNumberList;
}

declare var SVGAnimatedNumberList: {
    prototype: SVGAnimatedNumberList;
    new(): SVGAnimatedNumberList;
    isInstance: IsInstance<SVGAnimatedNumberList>;
};

interface SVGAnimatedPoints {
    readonly animatedPoints: SVGPointList;
    readonly points: SVGPointList;
}

interface SVGAnimatedPreserveAspectRatio {
    readonly animVal: SVGPreserveAspectRatio;
    readonly baseVal: SVGPreserveAspectRatio;
}

declare var SVGAnimatedPreserveAspectRatio: {
    prototype: SVGAnimatedPreserveAspectRatio;
    new(): SVGAnimatedPreserveAspectRatio;
    isInstance: IsInstance<SVGAnimatedPreserveAspectRatio>;
};

interface SVGAnimatedRect {
    readonly animVal: SVGRect | null;
    readonly baseVal: SVGRect | null;
}

declare var SVGAnimatedRect: {
    prototype: SVGAnimatedRect;
    new(): SVGAnimatedRect;
    isInstance: IsInstance<SVGAnimatedRect>;
};

interface SVGAnimatedString {
    readonly animVal: string;
    baseVal: string;
}

declare var SVGAnimatedString: {
    prototype: SVGAnimatedString;
    new(): SVGAnimatedString;
    isInstance: IsInstance<SVGAnimatedString>;
};

interface SVGAnimatedTransformList {
    readonly animVal: SVGTransformList;
    readonly baseVal: SVGTransformList;
}

declare var SVGAnimatedTransformList: {
    prototype: SVGAnimatedTransformList;
    new(): SVGAnimatedTransformList;
    isInstance: IsInstance<SVGAnimatedTransformList>;
};

interface SVGAnimationElementEventMap extends SVGElementEventMap {
    "begin": Event;
    "end": Event;
    "repeat": Event;
}

interface SVGAnimationElement extends SVGElement, SVGTests {
    onbegin: ((this: SVGAnimationElement, ev: Event) => any) | null;
    onend: ((this: SVGAnimationElement, ev: Event) => any) | null;
    onrepeat: ((this: SVGAnimationElement, ev: Event) => any) | null;
    readonly targetElement: SVGElement | null;
    beginElement(): void;
    beginElementAt(offset: number): void;
    endElement(): void;
    endElementAt(offset: number): void;
    getCurrentTime(): number;
    getSimpleDuration(): number;
    getStartTime(): number;
    addEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimationElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGAnimationElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGAnimationElement: {
    prototype: SVGAnimationElement;
    new(): SVGAnimationElement;
    isInstance: IsInstance<SVGAnimationElement>;
};

interface SVGCircleElement extends SVGGeometryElement {
    readonly cx: SVGAnimatedLength;
    readonly cy: SVGAnimatedLength;
    readonly r: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGCircleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGCircleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGCircleElement: {
    prototype: SVGCircleElement;
    new(): SVGCircleElement;
    isInstance: IsInstance<SVGCircleElement>;
};

interface SVGClipPathElement extends SVGElement {
    readonly clipPathUnits: SVGAnimatedEnumeration;
    readonly transform: SVGAnimatedTransformList;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGClipPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGClipPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGClipPathElement: {
    prototype: SVGClipPathElement;
    new(): SVGClipPathElement;
    isInstance: IsInstance<SVGClipPathElement>;
};

interface SVGComponentTransferFunctionElement extends SVGElement {
    readonly amplitude: SVGAnimatedNumber;
    readonly exponent: SVGAnimatedNumber;
    readonly intercept: SVGAnimatedNumber;
    readonly offset: SVGAnimatedNumber;
    readonly slope: SVGAnimatedNumber;
    readonly tableValues: SVGAnimatedNumberList;
    readonly type: SVGAnimatedEnumeration;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN: 0;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY: 1;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_TABLE: 2;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE: 3;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_LINEAR: 4;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_GAMMA: 5;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGComponentTransferFunctionElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGComponentTransferFunctionElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGComponentTransferFunctionElement: {
    prototype: SVGComponentTransferFunctionElement;
    new(): SVGComponentTransferFunctionElement;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN: 0;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY: 1;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_TABLE: 2;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE: 3;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_LINEAR: 4;
    readonly SVG_FECOMPONENTTRANSFER_TYPE_GAMMA: 5;
    isInstance: IsInstance<SVGComponentTransferFunctionElement>;
};

interface SVGDefsElement extends SVGGraphicsElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGDefsElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGDefsElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGDefsElement: {
    prototype: SVGDefsElement;
    new(): SVGDefsElement;
    isInstance: IsInstance<SVGDefsElement>;
};

interface SVGDescElement extends SVGElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGDescElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGDescElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGDescElement: {
    prototype: SVGDescElement;
    new(): SVGDescElement;
    isInstance: IsInstance<SVGDescElement>;
};

interface SVGElementEventMap extends ElementEventMap, GlobalEventHandlersEventMap, OnErrorEventHandlerForNodesEventMap, TouchEventHandlersEventMap {
}

// @ts-ignore
interface SVGElement extends Element, ElementCSSInlineStyle, GlobalEventHandlers, HTMLOrForeignElement, OnErrorEventHandlerForNodes, TouchEventHandlers {
    readonly className: SVGAnimatedString;
    id: string;
    nonce: string;
    readonly ownerSVGElement: SVGSVGElement | null;
    readonly viewportElement: SVGElement | null;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGElement: {
    prototype: SVGElement;
    new(): SVGElement;
    isInstance: IsInstance<SVGElement>;
};

interface SVGEllipseElement extends SVGGeometryElement {
    readonly cx: SVGAnimatedLength;
    readonly cy: SVGAnimatedLength;
    readonly rx: SVGAnimatedLength;
    readonly ry: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGEllipseElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGEllipseElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGEllipseElement: {
    prototype: SVGEllipseElement;
    new(): SVGEllipseElement;
    isInstance: IsInstance<SVGEllipseElement>;
};

interface SVGFEBlendElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly in2: SVGAnimatedString;
    readonly mode: SVGAnimatedEnumeration;
    readonly SVG_FEBLEND_MODE_UNKNOWN: 0;
    readonly SVG_FEBLEND_MODE_NORMAL: 1;
    readonly SVG_FEBLEND_MODE_MULTIPLY: 2;
    readonly SVG_FEBLEND_MODE_SCREEN: 3;
    readonly SVG_FEBLEND_MODE_DARKEN: 4;
    readonly SVG_FEBLEND_MODE_LIGHTEN: 5;
    readonly SVG_FEBLEND_MODE_OVERLAY: 6;
    readonly SVG_FEBLEND_MODE_COLOR_DODGE: 7;
    readonly SVG_FEBLEND_MODE_COLOR_BURN: 8;
    readonly SVG_FEBLEND_MODE_HARD_LIGHT: 9;
    readonly SVG_FEBLEND_MODE_SOFT_LIGHT: 10;
    readonly SVG_FEBLEND_MODE_DIFFERENCE: 11;
    readonly SVG_FEBLEND_MODE_EXCLUSION: 12;
    readonly SVG_FEBLEND_MODE_HUE: 13;
    readonly SVG_FEBLEND_MODE_SATURATION: 14;
    readonly SVG_FEBLEND_MODE_COLOR: 15;
    readonly SVG_FEBLEND_MODE_LUMINOSITY: 16;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEBlendElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEBlendElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEBlendElement: {
    prototype: SVGFEBlendElement;
    new(): SVGFEBlendElement;
    readonly SVG_FEBLEND_MODE_UNKNOWN: 0;
    readonly SVG_FEBLEND_MODE_NORMAL: 1;
    readonly SVG_FEBLEND_MODE_MULTIPLY: 2;
    readonly SVG_FEBLEND_MODE_SCREEN: 3;
    readonly SVG_FEBLEND_MODE_DARKEN: 4;
    readonly SVG_FEBLEND_MODE_LIGHTEN: 5;
    readonly SVG_FEBLEND_MODE_OVERLAY: 6;
    readonly SVG_FEBLEND_MODE_COLOR_DODGE: 7;
    readonly SVG_FEBLEND_MODE_COLOR_BURN: 8;
    readonly SVG_FEBLEND_MODE_HARD_LIGHT: 9;
    readonly SVG_FEBLEND_MODE_SOFT_LIGHT: 10;
    readonly SVG_FEBLEND_MODE_DIFFERENCE: 11;
    readonly SVG_FEBLEND_MODE_EXCLUSION: 12;
    readonly SVG_FEBLEND_MODE_HUE: 13;
    readonly SVG_FEBLEND_MODE_SATURATION: 14;
    readonly SVG_FEBLEND_MODE_COLOR: 15;
    readonly SVG_FEBLEND_MODE_LUMINOSITY: 16;
    isInstance: IsInstance<SVGFEBlendElement>;
};

interface SVGFEColorMatrixElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly type: SVGAnimatedEnumeration;
    readonly values: SVGAnimatedNumberList;
    readonly SVG_FECOLORMATRIX_TYPE_UNKNOWN: 0;
    readonly SVG_FECOLORMATRIX_TYPE_MATRIX: 1;
    readonly SVG_FECOLORMATRIX_TYPE_SATURATE: 2;
    readonly SVG_FECOLORMATRIX_TYPE_HUEROTATE: 3;
    readonly SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA: 4;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEColorMatrixElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEColorMatrixElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEColorMatrixElement: {
    prototype: SVGFEColorMatrixElement;
    new(): SVGFEColorMatrixElement;
    readonly SVG_FECOLORMATRIX_TYPE_UNKNOWN: 0;
    readonly SVG_FECOLORMATRIX_TYPE_MATRIX: 1;
    readonly SVG_FECOLORMATRIX_TYPE_SATURATE: 2;
    readonly SVG_FECOLORMATRIX_TYPE_HUEROTATE: 3;
    readonly SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA: 4;
    isInstance: IsInstance<SVGFEColorMatrixElement>;
};

interface SVGFEComponentTransferElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEComponentTransferElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEComponentTransferElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEComponentTransferElement: {
    prototype: SVGFEComponentTransferElement;
    new(): SVGFEComponentTransferElement;
    isInstance: IsInstance<SVGFEComponentTransferElement>;
};

interface SVGFECompositeElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly in2: SVGAnimatedString;
    readonly k1: SVGAnimatedNumber;
    readonly k2: SVGAnimatedNumber;
    readonly k3: SVGAnimatedNumber;
    readonly k4: SVGAnimatedNumber;
    readonly operator: SVGAnimatedEnumeration;
    readonly SVG_FECOMPOSITE_OPERATOR_UNKNOWN: 0;
    readonly SVG_FECOMPOSITE_OPERATOR_OVER: 1;
    readonly SVG_FECOMPOSITE_OPERATOR_IN: 2;
    readonly SVG_FECOMPOSITE_OPERATOR_OUT: 3;
    readonly SVG_FECOMPOSITE_OPERATOR_ATOP: 4;
    readonly SVG_FECOMPOSITE_OPERATOR_XOR: 5;
    readonly SVG_FECOMPOSITE_OPERATOR_ARITHMETIC: 6;
    readonly SVG_FECOMPOSITE_OPERATOR_LIGHTER: 7;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFECompositeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFECompositeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFECompositeElement: {
    prototype: SVGFECompositeElement;
    new(): SVGFECompositeElement;
    readonly SVG_FECOMPOSITE_OPERATOR_UNKNOWN: 0;
    readonly SVG_FECOMPOSITE_OPERATOR_OVER: 1;
    readonly SVG_FECOMPOSITE_OPERATOR_IN: 2;
    readonly SVG_FECOMPOSITE_OPERATOR_OUT: 3;
    readonly SVG_FECOMPOSITE_OPERATOR_ATOP: 4;
    readonly SVG_FECOMPOSITE_OPERATOR_XOR: 5;
    readonly SVG_FECOMPOSITE_OPERATOR_ARITHMETIC: 6;
    readonly SVG_FECOMPOSITE_OPERATOR_LIGHTER: 7;
    isInstance: IsInstance<SVGFECompositeElement>;
};

interface SVGFEConvolveMatrixElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly bias: SVGAnimatedNumber;
    readonly divisor: SVGAnimatedNumber;
    readonly edgeMode: SVGAnimatedEnumeration;
    readonly in1: SVGAnimatedString;
    readonly kernelMatrix: SVGAnimatedNumberList;
    readonly kernelUnitLengthX: SVGAnimatedNumber;
    readonly kernelUnitLengthY: SVGAnimatedNumber;
    readonly orderX: SVGAnimatedInteger;
    readonly orderY: SVGAnimatedInteger;
    readonly preserveAlpha: SVGAnimatedBoolean;
    readonly targetX: SVGAnimatedInteger;
    readonly targetY: SVGAnimatedInteger;
    readonly SVG_EDGEMODE_UNKNOWN: 0;
    readonly SVG_EDGEMODE_DUPLICATE: 1;
    readonly SVG_EDGEMODE_WRAP: 2;
    readonly SVG_EDGEMODE_NONE: 3;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEConvolveMatrixElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEConvolveMatrixElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEConvolveMatrixElement: {
    prototype: SVGFEConvolveMatrixElement;
    new(): SVGFEConvolveMatrixElement;
    readonly SVG_EDGEMODE_UNKNOWN: 0;
    readonly SVG_EDGEMODE_DUPLICATE: 1;
    readonly SVG_EDGEMODE_WRAP: 2;
    readonly SVG_EDGEMODE_NONE: 3;
    isInstance: IsInstance<SVGFEConvolveMatrixElement>;
};

interface SVGFEDiffuseLightingElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly diffuseConstant: SVGAnimatedNumber;
    readonly in1: SVGAnimatedString;
    readonly kernelUnitLengthX: SVGAnimatedNumber;
    readonly kernelUnitLengthY: SVGAnimatedNumber;
    readonly surfaceScale: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDiffuseLightingElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDiffuseLightingElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEDiffuseLightingElement: {
    prototype: SVGFEDiffuseLightingElement;
    new(): SVGFEDiffuseLightingElement;
    isInstance: IsInstance<SVGFEDiffuseLightingElement>;
};

interface SVGFEDisplacementMapElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly in2: SVGAnimatedString;
    readonly scale: SVGAnimatedNumber;
    readonly xChannelSelector: SVGAnimatedEnumeration;
    readonly yChannelSelector: SVGAnimatedEnumeration;
    readonly SVG_CHANNEL_UNKNOWN: 0;
    readonly SVG_CHANNEL_R: 1;
    readonly SVG_CHANNEL_G: 2;
    readonly SVG_CHANNEL_B: 3;
    readonly SVG_CHANNEL_A: 4;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDisplacementMapElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDisplacementMapElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEDisplacementMapElement: {
    prototype: SVGFEDisplacementMapElement;
    new(): SVGFEDisplacementMapElement;
    readonly SVG_CHANNEL_UNKNOWN: 0;
    readonly SVG_CHANNEL_R: 1;
    readonly SVG_CHANNEL_G: 2;
    readonly SVG_CHANNEL_B: 3;
    readonly SVG_CHANNEL_A: 4;
    isInstance: IsInstance<SVGFEDisplacementMapElement>;
};

interface SVGFEDistantLightElement extends SVGElement {
    readonly azimuth: SVGAnimatedNumber;
    readonly elevation: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDistantLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDistantLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEDistantLightElement: {
    prototype: SVGFEDistantLightElement;
    new(): SVGFEDistantLightElement;
    isInstance: IsInstance<SVGFEDistantLightElement>;
};

interface SVGFEDropShadowElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly dx: SVGAnimatedNumber;
    readonly dy: SVGAnimatedNumber;
    readonly in1: SVGAnimatedString;
    readonly stdDeviationX: SVGAnimatedNumber;
    readonly stdDeviationY: SVGAnimatedNumber;
    setStdDeviation(stdDeviationX: number, stdDeviationY: number): void;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDropShadowElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEDropShadowElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEDropShadowElement: {
    prototype: SVGFEDropShadowElement;
    new(): SVGFEDropShadowElement;
    isInstance: IsInstance<SVGFEDropShadowElement>;
};

interface SVGFEFloodElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFloodElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFloodElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEFloodElement: {
    prototype: SVGFEFloodElement;
    new(): SVGFEFloodElement;
    isInstance: IsInstance<SVGFEFloodElement>;
};

interface SVGFEFuncAElement extends SVGComponentTransferFunctionElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncAElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncAElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEFuncAElement: {
    prototype: SVGFEFuncAElement;
    new(): SVGFEFuncAElement;
    isInstance: IsInstance<SVGFEFuncAElement>;
};

interface SVGFEFuncBElement extends SVGComponentTransferFunctionElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncBElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncBElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEFuncBElement: {
    prototype: SVGFEFuncBElement;
    new(): SVGFEFuncBElement;
    isInstance: IsInstance<SVGFEFuncBElement>;
};

interface SVGFEFuncGElement extends SVGComponentTransferFunctionElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEFuncGElement: {
    prototype: SVGFEFuncGElement;
    new(): SVGFEFuncGElement;
    isInstance: IsInstance<SVGFEFuncGElement>;
};

interface SVGFEFuncRElement extends SVGComponentTransferFunctionElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncRElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEFuncRElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEFuncRElement: {
    prototype: SVGFEFuncRElement;
    new(): SVGFEFuncRElement;
    isInstance: IsInstance<SVGFEFuncRElement>;
};

interface SVGFEGaussianBlurElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly stdDeviationX: SVGAnimatedNumber;
    readonly stdDeviationY: SVGAnimatedNumber;
    setStdDeviation(stdDeviationX: number, stdDeviationY: number): void;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEGaussianBlurElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEGaussianBlurElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEGaussianBlurElement: {
    prototype: SVGFEGaussianBlurElement;
    new(): SVGFEGaussianBlurElement;
    isInstance: IsInstance<SVGFEGaussianBlurElement>;
};

interface SVGFEImageElement extends SVGElement, SVGFilterPrimitiveStandardAttributes, SVGURIReference {
    crossOrigin: string | null;
    readonly preserveAspectRatio: SVGAnimatedPreserveAspectRatio;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEImageElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEImageElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEImageElement: {
    prototype: SVGFEImageElement;
    new(): SVGFEImageElement;
    isInstance: IsInstance<SVGFEImageElement>;
};

interface SVGFEMergeElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMergeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMergeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEMergeElement: {
    prototype: SVGFEMergeElement;
    new(): SVGFEMergeElement;
    isInstance: IsInstance<SVGFEMergeElement>;
};

interface SVGFEMergeNodeElement extends SVGElement {
    readonly in1: SVGAnimatedString;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMergeNodeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMergeNodeElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEMergeNodeElement: {
    prototype: SVGFEMergeNodeElement;
    new(): SVGFEMergeNodeElement;
    isInstance: IsInstance<SVGFEMergeNodeElement>;
};

interface SVGFEMorphologyElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly operator: SVGAnimatedEnumeration;
    readonly radiusX: SVGAnimatedNumber;
    readonly radiusY: SVGAnimatedNumber;
    readonly SVG_MORPHOLOGY_OPERATOR_UNKNOWN: 0;
    readonly SVG_MORPHOLOGY_OPERATOR_ERODE: 1;
    readonly SVG_MORPHOLOGY_OPERATOR_DILATE: 2;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMorphologyElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEMorphologyElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEMorphologyElement: {
    prototype: SVGFEMorphologyElement;
    new(): SVGFEMorphologyElement;
    readonly SVG_MORPHOLOGY_OPERATOR_UNKNOWN: 0;
    readonly SVG_MORPHOLOGY_OPERATOR_ERODE: 1;
    readonly SVG_MORPHOLOGY_OPERATOR_DILATE: 2;
    isInstance: IsInstance<SVGFEMorphologyElement>;
};

interface SVGFEOffsetElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly dx: SVGAnimatedNumber;
    readonly dy: SVGAnimatedNumber;
    readonly in1: SVGAnimatedString;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEOffsetElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEOffsetElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEOffsetElement: {
    prototype: SVGFEOffsetElement;
    new(): SVGFEOffsetElement;
    isInstance: IsInstance<SVGFEOffsetElement>;
};

interface SVGFEPointLightElement extends SVGElement {
    readonly x: SVGAnimatedNumber;
    readonly y: SVGAnimatedNumber;
    readonly z: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEPointLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFEPointLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFEPointLightElement: {
    prototype: SVGFEPointLightElement;
    new(): SVGFEPointLightElement;
    isInstance: IsInstance<SVGFEPointLightElement>;
};

interface SVGFESpecularLightingElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    readonly kernelUnitLengthX: SVGAnimatedNumber;
    readonly kernelUnitLengthY: SVGAnimatedNumber;
    readonly specularConstant: SVGAnimatedNumber;
    readonly specularExponent: SVGAnimatedNumber;
    readonly surfaceScale: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFESpecularLightingElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFESpecularLightingElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFESpecularLightingElement: {
    prototype: SVGFESpecularLightingElement;
    new(): SVGFESpecularLightingElement;
    isInstance: IsInstance<SVGFESpecularLightingElement>;
};

interface SVGFESpotLightElement extends SVGElement {
    readonly limitingConeAngle: SVGAnimatedNumber;
    readonly pointsAtX: SVGAnimatedNumber;
    readonly pointsAtY: SVGAnimatedNumber;
    readonly pointsAtZ: SVGAnimatedNumber;
    readonly specularExponent: SVGAnimatedNumber;
    readonly x: SVGAnimatedNumber;
    readonly y: SVGAnimatedNumber;
    readonly z: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFESpotLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFESpotLightElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFESpotLightElement: {
    prototype: SVGFESpotLightElement;
    new(): SVGFESpotLightElement;
    isInstance: IsInstance<SVGFESpotLightElement>;
};

interface SVGFETileElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly in1: SVGAnimatedString;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFETileElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFETileElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFETileElement: {
    prototype: SVGFETileElement;
    new(): SVGFETileElement;
    isInstance: IsInstance<SVGFETileElement>;
};

interface SVGFETurbulenceElement extends SVGElement, SVGFilterPrimitiveStandardAttributes {
    readonly baseFrequencyX: SVGAnimatedNumber;
    readonly baseFrequencyY: SVGAnimatedNumber;
    readonly numOctaves: SVGAnimatedInteger;
    readonly seed: SVGAnimatedNumber;
    readonly stitchTiles: SVGAnimatedEnumeration;
    readonly type: SVGAnimatedEnumeration;
    readonly SVG_TURBULENCE_TYPE_UNKNOWN: 0;
    readonly SVG_TURBULENCE_TYPE_FRACTALNOISE: 1;
    readonly SVG_TURBULENCE_TYPE_TURBULENCE: 2;
    readonly SVG_STITCHTYPE_UNKNOWN: 0;
    readonly SVG_STITCHTYPE_STITCH: 1;
    readonly SVG_STITCHTYPE_NOSTITCH: 2;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFETurbulenceElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFETurbulenceElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFETurbulenceElement: {
    prototype: SVGFETurbulenceElement;
    new(): SVGFETurbulenceElement;
    readonly SVG_TURBULENCE_TYPE_UNKNOWN: 0;
    readonly SVG_TURBULENCE_TYPE_FRACTALNOISE: 1;
    readonly SVG_TURBULENCE_TYPE_TURBULENCE: 2;
    readonly SVG_STITCHTYPE_UNKNOWN: 0;
    readonly SVG_STITCHTYPE_STITCH: 1;
    readonly SVG_STITCHTYPE_NOSTITCH: 2;
    isInstance: IsInstance<SVGFETurbulenceElement>;
};

interface SVGFilterElement extends SVGElement, SVGURIReference {
    readonly filterUnits: SVGAnimatedEnumeration;
    readonly height: SVGAnimatedLength;
    readonly primitiveUnits: SVGAnimatedEnumeration;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFilterElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGFilterElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGFilterElement: {
    prototype: SVGFilterElement;
    new(): SVGFilterElement;
    isInstance: IsInstance<SVGFilterElement>;
};

interface SVGFilterPrimitiveStandardAttributes {
    readonly height: SVGAnimatedLength;
    readonly result: SVGAnimatedString;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
}

interface SVGFitToViewBox {
    readonly preserveAspectRatio: SVGAnimatedPreserveAspectRatio;
    readonly viewBox: SVGAnimatedRect;
}

interface SVGForeignObjectElement extends SVGGraphicsElement {
    readonly height: SVGAnimatedLength;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGForeignObjectElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGForeignObjectElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGForeignObjectElement: {
    prototype: SVGForeignObjectElement;
    new(): SVGForeignObjectElement;
    isInstance: IsInstance<SVGForeignObjectElement>;
};

interface SVGGElement extends SVGGraphicsElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGGElement: {
    prototype: SVGGElement;
    new(): SVGGElement;
    isInstance: IsInstance<SVGGElement>;
};

interface SVGGeometryElement extends SVGGraphicsElement {
    readonly pathLength: SVGAnimatedNumber;
    getPointAtLength(distance: number): SVGPoint;
    getTotalLength(): number;
    isPointInFill(point?: DOMPointInit): boolean;
    isPointInStroke(point?: DOMPointInit): boolean;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGeometryElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGeometryElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGGeometryElement: {
    prototype: SVGGeometryElement;
    new(): SVGGeometryElement;
    isInstance: IsInstance<SVGGeometryElement>;
};

interface SVGGradientElement extends SVGElement, SVGURIReference {
    readonly gradientTransform: SVGAnimatedTransformList;
    readonly gradientUnits: SVGAnimatedEnumeration;
    readonly spreadMethod: SVGAnimatedEnumeration;
    readonly SVG_SPREADMETHOD_UNKNOWN: 0;
    readonly SVG_SPREADMETHOD_PAD: 1;
    readonly SVG_SPREADMETHOD_REFLECT: 2;
    readonly SVG_SPREADMETHOD_REPEAT: 3;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGGradientElement: {
    prototype: SVGGradientElement;
    new(): SVGGradientElement;
    readonly SVG_SPREADMETHOD_UNKNOWN: 0;
    readonly SVG_SPREADMETHOD_PAD: 1;
    readonly SVG_SPREADMETHOD_REFLECT: 2;
    readonly SVG_SPREADMETHOD_REPEAT: 3;
    isInstance: IsInstance<SVGGradientElement>;
};

interface SVGGraphicsElement extends SVGElement, SVGTests {
    readonly farthestViewportElement: SVGElement | null;
    readonly nearestViewportElement: SVGElement | null;
    readonly transform: SVGAnimatedTransformList;
    getBBox(aOptions?: SVGBoundingBoxOptions): SVGRect;
    getCTM(): SVGMatrix | null;
    getScreenCTM(): SVGMatrix | null;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGraphicsElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGGraphicsElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGGraphicsElement: {
    prototype: SVGGraphicsElement;
    new(): SVGGraphicsElement;
    isInstance: IsInstance<SVGGraphicsElement>;
};

interface SVGImageElement extends SVGGraphicsElement, MozImageLoadingContent, SVGURIReference {
    crossOrigin: string | null;
    decoding: string;
    readonly height: SVGAnimatedLength;
    readonly preserveAspectRatio: SVGAnimatedPreserveAspectRatio;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    decode(): Promise<void>;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGImageElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGImageElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGImageElement: {
    prototype: SVGImageElement;
    new(): SVGImageElement;
    readonly UNKNOWN_REQUEST: -1;
    readonly CURRENT_REQUEST: 0;
    readonly PENDING_REQUEST: 1;
    isInstance: IsInstance<SVGImageElement>;
};

interface SVGLength {
    readonly unitType: number;
    value: number;
    valueAsString: string;
    valueInSpecifiedUnits: number;
    convertToSpecifiedUnits(unitType: number): void;
    newValueSpecifiedUnits(unitType: number, valueInSpecifiedUnits: number): void;
    readonly SVG_LENGTHTYPE_UNKNOWN: 0;
    readonly SVG_LENGTHTYPE_NUMBER: 1;
    readonly SVG_LENGTHTYPE_PERCENTAGE: 2;
    readonly SVG_LENGTHTYPE_EMS: 3;
    readonly SVG_LENGTHTYPE_EXS: 4;
    readonly SVG_LENGTHTYPE_PX: 5;
    readonly SVG_LENGTHTYPE_CM: 6;
    readonly SVG_LENGTHTYPE_MM: 7;
    readonly SVG_LENGTHTYPE_IN: 8;
    readonly SVG_LENGTHTYPE_PT: 9;
    readonly SVG_LENGTHTYPE_PC: 10;
}

declare var SVGLength: {
    prototype: SVGLength;
    new(): SVGLength;
    readonly SVG_LENGTHTYPE_UNKNOWN: 0;
    readonly SVG_LENGTHTYPE_NUMBER: 1;
    readonly SVG_LENGTHTYPE_PERCENTAGE: 2;
    readonly SVG_LENGTHTYPE_EMS: 3;
    readonly SVG_LENGTHTYPE_EXS: 4;
    readonly SVG_LENGTHTYPE_PX: 5;
    readonly SVG_LENGTHTYPE_CM: 6;
    readonly SVG_LENGTHTYPE_MM: 7;
    readonly SVG_LENGTHTYPE_IN: 8;
    readonly SVG_LENGTHTYPE_PT: 9;
    readonly SVG_LENGTHTYPE_PC: 10;
    isInstance: IsInstance<SVGLength>;
};

interface SVGLengthList {
    readonly length: number;
    readonly numberOfItems: number;
    appendItem(newItem: SVGLength): SVGLength;
    clear(): void;
    getItem(index: number): SVGLength;
    initialize(newItem: SVGLength): SVGLength;
    insertItemBefore(newItem: SVGLength, index: number): SVGLength;
    removeItem(index: number): SVGLength;
    replaceItem(newItem: SVGLength, index: number): SVGLength;
    [index: number]: SVGLength;
}

declare var SVGLengthList: {
    prototype: SVGLengthList;
    new(): SVGLengthList;
    isInstance: IsInstance<SVGLengthList>;
};

interface SVGLineElement extends SVGGeometryElement {
    readonly x1: SVGAnimatedLength;
    readonly x2: SVGAnimatedLength;
    readonly y1: SVGAnimatedLength;
    readonly y2: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGLineElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGLineElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGLineElement: {
    prototype: SVGLineElement;
    new(): SVGLineElement;
    isInstance: IsInstance<SVGLineElement>;
};

interface SVGLinearGradientElement extends SVGGradientElement {
    readonly x1: SVGAnimatedLength;
    readonly x2: SVGAnimatedLength;
    readonly y1: SVGAnimatedLength;
    readonly y2: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGLinearGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGLinearGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGLinearGradientElement: {
    prototype: SVGLinearGradientElement;
    new(): SVGLinearGradientElement;
    isInstance: IsInstance<SVGLinearGradientElement>;
};

interface SVGMPathElement extends SVGElement, SVGURIReference {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGMPathElement: {
    prototype: SVGMPathElement;
    new(): SVGMPathElement;
    isInstance: IsInstance<SVGMPathElement>;
};

interface SVGMarkerElement extends SVGElement, SVGFitToViewBox {
    readonly markerHeight: SVGAnimatedLength;
    readonly markerUnits: SVGAnimatedEnumeration;
    readonly markerWidth: SVGAnimatedLength;
    readonly orientAngle: SVGAnimatedAngle;
    readonly orientType: SVGAnimatedEnumeration;
    readonly refX: SVGAnimatedLength;
    readonly refY: SVGAnimatedLength;
    setOrientToAngle(angle: SVGAngle): void;
    setOrientToAuto(): void;
    readonly SVG_MARKERUNITS_UNKNOWN: 0;
    readonly SVG_MARKERUNITS_USERSPACEONUSE: 1;
    readonly SVG_MARKERUNITS_STROKEWIDTH: 2;
    readonly SVG_MARKER_ORIENT_UNKNOWN: 0;
    readonly SVG_MARKER_ORIENT_AUTO: 1;
    readonly SVG_MARKER_ORIENT_ANGLE: 2;
    readonly SVG_MARKER_ORIENT_AUTO_START_REVERSE: 3;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMarkerElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMarkerElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGMarkerElement: {
    prototype: SVGMarkerElement;
    new(): SVGMarkerElement;
    readonly SVG_MARKERUNITS_UNKNOWN: 0;
    readonly SVG_MARKERUNITS_USERSPACEONUSE: 1;
    readonly SVG_MARKERUNITS_STROKEWIDTH: 2;
    readonly SVG_MARKER_ORIENT_UNKNOWN: 0;
    readonly SVG_MARKER_ORIENT_AUTO: 1;
    readonly SVG_MARKER_ORIENT_ANGLE: 2;
    readonly SVG_MARKER_ORIENT_AUTO_START_REVERSE: 3;
    isInstance: IsInstance<SVGMarkerElement>;
};

interface SVGMaskElement extends SVGElement {
    readonly height: SVGAnimatedLength;
    readonly maskContentUnits: SVGAnimatedEnumeration;
    readonly maskUnits: SVGAnimatedEnumeration;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    readonly SVG_MASKTYPE_LUMINANCE: 0;
    readonly SVG_MASKTYPE_ALPHA: 1;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMaskElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMaskElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGMaskElement: {
    prototype: SVGMaskElement;
    new(): SVGMaskElement;
    readonly SVG_MASKTYPE_LUMINANCE: 0;
    readonly SVG_MASKTYPE_ALPHA: 1;
    isInstance: IsInstance<SVGMaskElement>;
};

interface SVGMatrix {
    a: number;
    b: number;
    c: number;
    d: number;
    e: number;
    f: number;
    flipX(): SVGMatrix;
    flipY(): SVGMatrix;
    inverse(): SVGMatrix;
    multiply(secondMatrix: SVGMatrix): SVGMatrix;
    rotate(angle: number): SVGMatrix;
    rotateFromVector(x: number, y: number): SVGMatrix;
    scale(scaleFactor: number): SVGMatrix;
    scaleNonUniform(scaleFactorX: number, scaleFactorY: number): SVGMatrix;
    skewX(angle: number): SVGMatrix;
    skewY(angle: number): SVGMatrix;
    translate(x: number, y: number): SVGMatrix;
}

declare var SVGMatrix: {
    prototype: SVGMatrix;
    new(): SVGMatrix;
    isInstance: IsInstance<SVGMatrix>;
};

interface SVGMetadataElement extends SVGElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMetadataElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGMetadataElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGMetadataElement: {
    prototype: SVGMetadataElement;
    new(): SVGMetadataElement;
    isInstance: IsInstance<SVGMetadataElement>;
};

interface SVGNumber {
    value: number;
}

declare var SVGNumber: {
    prototype: SVGNumber;
    new(): SVGNumber;
    isInstance: IsInstance<SVGNumber>;
};

interface SVGNumberList {
    readonly length: number;
    readonly numberOfItems: number;
    appendItem(newItem: SVGNumber): SVGNumber;
    clear(): void;
    getItem(index: number): SVGNumber;
    initialize(newItem: SVGNumber): SVGNumber;
    insertItemBefore(newItem: SVGNumber, index: number): SVGNumber;
    removeItem(index: number): SVGNumber;
    replaceItem(newItem: SVGNumber, index: number): SVGNumber;
    [index: number]: SVGNumber;
}

declare var SVGNumberList: {
    prototype: SVGNumberList;
    new(): SVGNumberList;
    isInstance: IsInstance<SVGNumberList>;
};

interface SVGPathElement extends SVGGeometryElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGPathElement: {
    prototype: SVGPathElement;
    new(): SVGPathElement;
    isInstance: IsInstance<SVGPathElement>;
};

interface SVGPatternElement extends SVGElement, SVGFitToViewBox, SVGURIReference {
    readonly height: SVGAnimatedLength;
    readonly patternContentUnits: SVGAnimatedEnumeration;
    readonly patternTransform: SVGAnimatedTransformList;
    readonly patternUnits: SVGAnimatedEnumeration;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPatternElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPatternElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGPatternElement: {
    prototype: SVGPatternElement;
    new(): SVGPatternElement;
    isInstance: IsInstance<SVGPatternElement>;
};

interface SVGPoint {
    x: number;
    y: number;
    matrixTransform(matrix?: DOMMatrix2DInit): SVGPoint;
}

declare var SVGPoint: {
    prototype: SVGPoint;
    new(): SVGPoint;
    isInstance: IsInstance<SVGPoint>;
};

interface SVGPointList {
    readonly length: number;
    readonly numberOfItems: number;
    appendItem(newItem: SVGPoint): SVGPoint;
    clear(): void;
    getItem(index: number): SVGPoint;
    initialize(newItem: SVGPoint): SVGPoint;
    insertItemBefore(newItem: SVGPoint, index: number): SVGPoint;
    removeItem(index: number): SVGPoint;
    replaceItem(newItem: SVGPoint, index: number): SVGPoint;
    [index: number]: SVGPoint;
}

declare var SVGPointList: {
    prototype: SVGPointList;
    new(): SVGPointList;
    isInstance: IsInstance<SVGPointList>;
};

interface SVGPolygonElement extends SVGGeometryElement, SVGAnimatedPoints {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPolygonElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPolygonElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGPolygonElement: {
    prototype: SVGPolygonElement;
    new(): SVGPolygonElement;
    isInstance: IsInstance<SVGPolygonElement>;
};

interface SVGPolylineElement extends SVGGeometryElement, SVGAnimatedPoints {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPolylineElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGPolylineElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGPolylineElement: {
    prototype: SVGPolylineElement;
    new(): SVGPolylineElement;
    isInstance: IsInstance<SVGPolylineElement>;
};

interface SVGPreserveAspectRatio {
    align: number;
    meetOrSlice: number;
    readonly SVG_PRESERVEASPECTRATIO_UNKNOWN: 0;
    readonly SVG_PRESERVEASPECTRATIO_NONE: 1;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMIN: 2;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMIN: 3;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMIN: 4;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMID: 5;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMID: 6;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMID: 7;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMAX: 8;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMAX: 9;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMAX: 10;
    readonly SVG_MEETORSLICE_UNKNOWN: 0;
    readonly SVG_MEETORSLICE_MEET: 1;
    readonly SVG_MEETORSLICE_SLICE: 2;
}

declare var SVGPreserveAspectRatio: {
    prototype: SVGPreserveAspectRatio;
    new(): SVGPreserveAspectRatio;
    readonly SVG_PRESERVEASPECTRATIO_UNKNOWN: 0;
    readonly SVG_PRESERVEASPECTRATIO_NONE: 1;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMIN: 2;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMIN: 3;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMIN: 4;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMID: 5;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMID: 6;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMID: 7;
    readonly SVG_PRESERVEASPECTRATIO_XMINYMAX: 8;
    readonly SVG_PRESERVEASPECTRATIO_XMIDYMAX: 9;
    readonly SVG_PRESERVEASPECTRATIO_XMAXYMAX: 10;
    readonly SVG_MEETORSLICE_UNKNOWN: 0;
    readonly SVG_MEETORSLICE_MEET: 1;
    readonly SVG_MEETORSLICE_SLICE: 2;
    isInstance: IsInstance<SVGPreserveAspectRatio>;
};

interface SVGRadialGradientElement extends SVGGradientElement {
    readonly cx: SVGAnimatedLength;
    readonly cy: SVGAnimatedLength;
    readonly fr: SVGAnimatedLength;
    readonly fx: SVGAnimatedLength;
    readonly fy: SVGAnimatedLength;
    readonly r: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGRadialGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGRadialGradientElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGRadialGradientElement: {
    prototype: SVGRadialGradientElement;
    new(): SVGRadialGradientElement;
    isInstance: IsInstance<SVGRadialGradientElement>;
};

interface SVGRect {
    height: number;
    width: number;
    x: number;
    y: number;
}

declare var SVGRect: {
    prototype: SVGRect;
    new(): SVGRect;
    isInstance: IsInstance<SVGRect>;
};

interface SVGRectElement extends SVGGeometryElement {
    readonly height: SVGAnimatedLength;
    readonly rx: SVGAnimatedLength;
    readonly ry: SVGAnimatedLength;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGRectElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGRectElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGRectElement: {
    prototype: SVGRectElement;
    new(): SVGRectElement;
    isInstance: IsInstance<SVGRectElement>;
};

interface SVGSVGElement extends SVGGraphicsElement, SVGFitToViewBox, SVGZoomAndPan {
    currentScale: number;
    readonly currentTranslate: SVGPoint;
    readonly height: SVGAnimatedLength;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    animationsPaused(): boolean;
    createSVGAngle(): SVGAngle;
    createSVGLength(): SVGLength;
    createSVGMatrix(): SVGMatrix;
    createSVGNumber(): SVGNumber;
    createSVGPoint(): SVGPoint;
    createSVGRect(): SVGRect;
    createSVGTransform(): SVGTransform;
    createSVGTransformFromMatrix(matrix?: DOMMatrix2DInit): SVGTransform;
    deselectAll(): void;
    forceRedraw(): void;
    getCurrentTime(): number;
    getElementById(elementId: string): Element | null;
    pauseAnimations(): void;
    setCurrentTime(seconds: number): void;
    suspendRedraw(maxWaitMilliseconds: number): number;
    unpauseAnimations(): void;
    unsuspendRedraw(suspendHandleID: number): void;
    unsuspendRedrawAll(): void;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSVGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSVGElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGSVGElement: {
    prototype: SVGSVGElement;
    new(): SVGSVGElement;
    readonly SVG_ZOOMANDPAN_UNKNOWN: 0;
    readonly SVG_ZOOMANDPAN_DISABLE: 1;
    readonly SVG_ZOOMANDPAN_MAGNIFY: 2;
    isInstance: IsInstance<SVGSVGElement>;
};

interface SVGScriptElement extends SVGElement, SVGURIReference {
    async: boolean;
    crossOrigin: string | null;
    defer: boolean;
    type: string;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGScriptElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGScriptElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGScriptElement: {
    prototype: SVGScriptElement;
    new(): SVGScriptElement;
    isInstance: IsInstance<SVGScriptElement>;
};

interface SVGSetElement extends SVGAnimationElement {
    addEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGSetElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGAnimationElementEventMap>(type: K, listener: (this: SVGSetElement, ev: SVGAnimationElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGSetElement: {
    prototype: SVGSetElement;
    new(): SVGSetElement;
    isInstance: IsInstance<SVGSetElement>;
};

interface SVGStopElement extends SVGElement {
    readonly offset: SVGAnimatedNumber;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGStopElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGStopElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGStopElement: {
    prototype: SVGStopElement;
    new(): SVGStopElement;
    isInstance: IsInstance<SVGStopElement>;
};

interface SVGStringList {
    readonly length: number;
    readonly numberOfItems: number;
    appendItem(newItem: string): string;
    clear(): void;
    getItem(index: number): string;
    initialize(newItem: string): string;
    insertItemBefore(newItem: string, index: number): string;
    removeItem(index: number): string;
    replaceItem(newItem: string, index: number): string;
    [index: number]: string;
}

declare var SVGStringList: {
    prototype: SVGStringList;
    new(): SVGStringList;
    isInstance: IsInstance<SVGStringList>;
};

interface SVGStyleElement extends SVGElement, LinkStyle {
    disabled: boolean;
    media: string;
    title: string;
    type: string;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGStyleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGStyleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGStyleElement: {
    prototype: SVGStyleElement;
    new(): SVGStyleElement;
    isInstance: IsInstance<SVGStyleElement>;
};

interface SVGSwitchElement extends SVGGraphicsElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSwitchElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSwitchElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGSwitchElement: {
    prototype: SVGSwitchElement;
    new(): SVGSwitchElement;
    isInstance: IsInstance<SVGSwitchElement>;
};

interface SVGSymbolElement extends SVGElement, SVGFitToViewBox, SVGTests {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSymbolElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGSymbolElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGSymbolElement: {
    prototype: SVGSymbolElement;
    new(): SVGSymbolElement;
    isInstance: IsInstance<SVGSymbolElement>;
};

interface SVGTSpanElement extends SVGTextPositioningElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTSpanElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTSpanElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTSpanElement: {
    prototype: SVGTSpanElement;
    new(): SVGTSpanElement;
    isInstance: IsInstance<SVGTSpanElement>;
};

interface SVGTests {
    readonly requiredExtensions: SVGStringList;
    readonly systemLanguage: SVGStringList;
}

interface SVGTextContentElement extends SVGGraphicsElement {
    readonly lengthAdjust: SVGAnimatedEnumeration;
    readonly textLength: SVGAnimatedLength;
    getCharNumAtPosition(point?: DOMPointInit): number;
    getComputedTextLength(): number;
    getEndPositionOfChar(charnum: number): SVGPoint;
    getExtentOfChar(charnum: number): SVGRect;
    getNumberOfChars(): number;
    getRotationOfChar(charnum: number): number;
    getStartPositionOfChar(charnum: number): SVGPoint;
    getSubStringLength(charnum: number, nchars: number): number;
    selectSubString(charnum: number, nchars: number): void;
    readonly LENGTHADJUST_UNKNOWN: 0;
    readonly LENGTHADJUST_SPACING: 1;
    readonly LENGTHADJUST_SPACINGANDGLYPHS: 2;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextContentElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextContentElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTextContentElement: {
    prototype: SVGTextContentElement;
    new(): SVGTextContentElement;
    readonly LENGTHADJUST_UNKNOWN: 0;
    readonly LENGTHADJUST_SPACING: 1;
    readonly LENGTHADJUST_SPACINGANDGLYPHS: 2;
    isInstance: IsInstance<SVGTextContentElement>;
};

interface SVGTextElement extends SVGTextPositioningElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTextElement: {
    prototype: SVGTextElement;
    new(): SVGTextElement;
    isInstance: IsInstance<SVGTextElement>;
};

interface SVGTextPathElement extends SVGTextContentElement, SVGURIReference {
    readonly method: SVGAnimatedEnumeration;
    readonly spacing: SVGAnimatedEnumeration;
    readonly startOffset: SVGAnimatedLength;
    readonly TEXTPATH_METHODTYPE_UNKNOWN: 0;
    readonly TEXTPATH_METHODTYPE_ALIGN: 1;
    readonly TEXTPATH_METHODTYPE_STRETCH: 2;
    readonly TEXTPATH_SPACINGTYPE_UNKNOWN: 0;
    readonly TEXTPATH_SPACINGTYPE_AUTO: 1;
    readonly TEXTPATH_SPACINGTYPE_EXACT: 2;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextPathElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTextPathElement: {
    prototype: SVGTextPathElement;
    new(): SVGTextPathElement;
    readonly TEXTPATH_METHODTYPE_UNKNOWN: 0;
    readonly TEXTPATH_METHODTYPE_ALIGN: 1;
    readonly TEXTPATH_METHODTYPE_STRETCH: 2;
    readonly TEXTPATH_SPACINGTYPE_UNKNOWN: 0;
    readonly TEXTPATH_SPACINGTYPE_AUTO: 1;
    readonly TEXTPATH_SPACINGTYPE_EXACT: 2;
    isInstance: IsInstance<SVGTextPathElement>;
};

interface SVGTextPositioningElement extends SVGTextContentElement {
    readonly dx: SVGAnimatedLengthList;
    readonly dy: SVGAnimatedLengthList;
    readonly rotate: SVGAnimatedNumberList;
    readonly x: SVGAnimatedLengthList;
    readonly y: SVGAnimatedLengthList;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextPositioningElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTextPositioningElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTextPositioningElement: {
    prototype: SVGTextPositioningElement;
    new(): SVGTextPositioningElement;
    isInstance: IsInstance<SVGTextPositioningElement>;
};

interface SVGTitleElement extends SVGElement {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTitleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGTitleElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGTitleElement: {
    prototype: SVGTitleElement;
    new(): SVGTitleElement;
    isInstance: IsInstance<SVGTitleElement>;
};

interface SVGTransform {
    readonly angle: number;
    readonly matrix: SVGMatrix;
    readonly type: number;
    setMatrix(matrix?: DOMMatrix2DInit): void;
    setRotate(angle: number, cx: number, cy: number): void;
    setScale(sx: number, sy: number): void;
    setSkewX(angle: number): void;
    setSkewY(angle: number): void;
    setTranslate(tx: number, ty: number): void;
    readonly SVG_TRANSFORM_UNKNOWN: 0;
    readonly SVG_TRANSFORM_MATRIX: 1;
    readonly SVG_TRANSFORM_TRANSLATE: 2;
    readonly SVG_TRANSFORM_SCALE: 3;
    readonly SVG_TRANSFORM_ROTATE: 4;
    readonly SVG_TRANSFORM_SKEWX: 5;
    readonly SVG_TRANSFORM_SKEWY: 6;
}

declare var SVGTransform: {
    prototype: SVGTransform;
    new(): SVGTransform;
    readonly SVG_TRANSFORM_UNKNOWN: 0;
    readonly SVG_TRANSFORM_MATRIX: 1;
    readonly SVG_TRANSFORM_TRANSLATE: 2;
    readonly SVG_TRANSFORM_SCALE: 3;
    readonly SVG_TRANSFORM_ROTATE: 4;
    readonly SVG_TRANSFORM_SKEWX: 5;
    readonly SVG_TRANSFORM_SKEWY: 6;
    isInstance: IsInstance<SVGTransform>;
};

interface SVGTransformList {
    readonly length: number;
    readonly numberOfItems: number;
    appendItem(newItem: SVGTransform): SVGTransform;
    clear(): void;
    consolidate(): SVGTransform | null;
    createSVGTransformFromMatrix(matrix?: DOMMatrix2DInit): SVGTransform;
    getItem(index: number): SVGTransform;
    initialize(newItem: SVGTransform): SVGTransform;
    insertItemBefore(newItem: SVGTransform, index: number): SVGTransform;
    removeItem(index: number): SVGTransform;
    replaceItem(newItem: SVGTransform, index: number): SVGTransform;
    [index: number]: SVGTransform;
}

declare var SVGTransformList: {
    prototype: SVGTransformList;
    new(): SVGTransformList;
    isInstance: IsInstance<SVGTransformList>;
};

interface SVGURIReference {
    readonly href: SVGAnimatedString;
}

interface SVGUnitTypes {
    readonly SVG_UNIT_TYPE_UNKNOWN: 0;
    readonly SVG_UNIT_TYPE_USERSPACEONUSE: 1;
    readonly SVG_UNIT_TYPE_OBJECTBOUNDINGBOX: 2;
}

declare var SVGUnitTypes: {
    prototype: SVGUnitTypes;
    new(): SVGUnitTypes;
    readonly SVG_UNIT_TYPE_UNKNOWN: 0;
    readonly SVG_UNIT_TYPE_USERSPACEONUSE: 1;
    readonly SVG_UNIT_TYPE_OBJECTBOUNDINGBOX: 2;
    isInstance: IsInstance<SVGUnitTypes>;
};

interface SVGUseElement extends SVGGraphicsElement, SVGURIReference {
    readonly height: SVGAnimatedLength;
    readonly width: SVGAnimatedLength;
    readonly x: SVGAnimatedLength;
    readonly y: SVGAnimatedLength;
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGUseElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGUseElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGUseElement: {
    prototype: SVGUseElement;
    new(): SVGUseElement;
    isInstance: IsInstance<SVGUseElement>;
};

interface SVGViewElement extends SVGElement, SVGFitToViewBox, SVGZoomAndPan {
    addEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGViewElement, ev: SVGElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SVGElementEventMap>(type: K, listener: (this: SVGViewElement, ev: SVGElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SVGViewElement: {
    prototype: SVGViewElement;
    new(): SVGViewElement;
    readonly SVG_ZOOMANDPAN_UNKNOWN: 0;
    readonly SVG_ZOOMANDPAN_DISABLE: 1;
    readonly SVG_ZOOMANDPAN_MAGNIFY: 2;
    isInstance: IsInstance<SVGViewElement>;
};

interface SVGZoomAndPan {
    zoomAndPan: number;
    readonly SVG_ZOOMANDPAN_UNKNOWN: 0;
    readonly SVG_ZOOMANDPAN_DISABLE: 1;
    readonly SVG_ZOOMANDPAN_MAGNIFY: 2;
}

/** Available only in secure contexts. */
interface Sanitizer {
    sanitize(input: SanitizerInput): DocumentFragment;
}

declare var Sanitizer: {
    prototype: Sanitizer;
    new(sanitizerConfig?: SanitizerConfig): Sanitizer;
    isInstance: IsInstance<Sanitizer>;
};

interface Scheduler {
    postTask(callback: SchedulerPostTaskCallback, options?: SchedulerPostTaskOptions): Promise<any>;
}

declare var Scheduler: {
    prototype: Scheduler;
    new(): Scheduler;
    isInstance: IsInstance<Scheduler>;
};

interface ScreenEventMap {
    "change": Event;
    "mozorientationchange": Event;
}

interface Screen extends EventTarget {
    readonly availHeight: number;
    readonly availLeft: number;
    readonly availTop: number;
    readonly availWidth: number;
    readonly colorDepth: number;
    readonly colorGamut: ScreenColorGamut;
    readonly height: number;
    readonly left: number;
    readonly luminance: ScreenLuminance | null;
    readonly mozOrientation: string;
    onchange: ((this: Screen, ev: Event) => any) | null;
    onmozorientationchange: ((this: Screen, ev: Event) => any) | null;
    readonly orientation: ScreenOrientation;
    readonly pixelDepth: number;
    readonly top: number;
    readonly width: number;
    mozLockOrientation(orientation: string): boolean;
    mozLockOrientation(orientation: string[]): boolean;
    mozUnlockOrientation(): void;
    addEventListener<K extends keyof ScreenEventMap>(type: K, listener: (this: Screen, ev: ScreenEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ScreenEventMap>(type: K, listener: (this: Screen, ev: ScreenEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Screen: {
    prototype: Screen;
    new(): Screen;
    isInstance: IsInstance<Screen>;
};

interface ScreenLuminance {
    readonly max: number;
    readonly maxAverage: number;
    readonly min: number;
}

declare var ScreenLuminance: {
    prototype: ScreenLuminance;
    new(): ScreenLuminance;
    isInstance: IsInstance<ScreenLuminance>;
};

interface ScreenOrientationEventMap {
    "change": Event;
}

interface ScreenOrientation extends EventTarget {
    readonly angle: number;
    onchange: ((this: ScreenOrientation, ev: Event) => any) | null;
    readonly type: OrientationType;
    lock(orientation: OrientationLockType): Promise<void>;
    unlock(): void;
    addEventListener<K extends keyof ScreenOrientationEventMap>(type: K, listener: (this: ScreenOrientation, ev: ScreenOrientationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ScreenOrientationEventMap>(type: K, listener: (this: ScreenOrientation, ev: ScreenOrientationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ScreenOrientation: {
    prototype: ScreenOrientation;
    new(): ScreenOrientation;
    isInstance: IsInstance<ScreenOrientation>;
};

interface ScriptProcessorNodeEventMap {
    "audioprocess": Event;
}

interface ScriptProcessorNode extends AudioNode, AudioNodePassThrough {
    readonly bufferSize: number;
    onaudioprocess: ((this: ScriptProcessorNode, ev: Event) => any) | null;
    addEventListener<K extends keyof ScriptProcessorNodeEventMap>(type: K, listener: (this: ScriptProcessorNode, ev: ScriptProcessorNodeEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ScriptProcessorNodeEventMap>(type: K, listener: (this: ScriptProcessorNode, ev: ScriptProcessorNodeEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ScriptProcessorNode: {
    prototype: ScriptProcessorNode;
    new(): ScriptProcessorNode;
    isInstance: IsInstance<ScriptProcessorNode>;
};

interface ScrollAreaEvent extends UIEvent {
    readonly height: number;
    readonly width: number;
    readonly x: number;
    readonly y: number;
    initScrollAreaEvent(type: string, canBubble?: boolean, cancelable?: boolean, view?: Window | null, detail?: number, x?: number, y?: number, width?: number, height?: number): void;
}

declare var ScrollAreaEvent: {
    prototype: ScrollAreaEvent;
    new(): ScrollAreaEvent;
    isInstance: IsInstance<ScrollAreaEvent>;
};

interface ScrollViewChangeEvent extends Event {
    readonly state: ScrollState;
}

declare var ScrollViewChangeEvent: {
    prototype: ScrollViewChangeEvent;
    new(type: string, eventInit?: ScrollViewChangeEventInit): ScrollViewChangeEvent;
    isInstance: IsInstance<ScrollViewChangeEvent>;
};

interface SecurityPolicyViolationEvent extends Event {
    readonly blockedURI: string;
    readonly columnNumber: number;
    readonly disposition: SecurityPolicyViolationEventDisposition;
    readonly documentURI: string;
    readonly effectiveDirective: string;
    readonly lineNumber: number;
    readonly originalPolicy: string;
    readonly referrer: string;
    readonly sample: string;
    readonly sourceFile: string;
    readonly statusCode: number;
    readonly violatedDirective: string;
}

declare var SecurityPolicyViolationEvent: {
    prototype: SecurityPolicyViolationEvent;
    new(type: string, eventInitDict?: SecurityPolicyViolationEventInit): SecurityPolicyViolationEvent;
    isInstance: IsInstance<SecurityPolicyViolationEvent>;
};

interface Selection {
    readonly anchorNode: Node | null;
    readonly anchorOffset: number;
    readonly areNormalAndCrossShadowBoundaryRangesCollapsed: boolean;
    caretBidiLevel: number | null;
    readonly direction: string;
    readonly focusNode: Node | null;
    readonly focusOffset: number;
    interlinePosition: boolean;
    readonly isCollapsed: boolean;
    readonly rangeCount: number;
    readonly selectionType: number;
    readonly type: string;
    GetRangesForInterval(beginNode: Node, beginOffset: number, endNode: Node, endOffset: number, allowAdjacent: boolean): Range[];
    addRange(range: Range): void;
    addSelectionListener(newListener: nsISelectionListener): void;
    collapse(node: Node | null, offset?: number): void;
    collapseToEnd(): void;
    collapseToStart(): void;
    containsNode(node: Node, allowPartialContainment?: boolean): boolean;
    deleteFromDocument(): void;
    empty(): void;
    extend(node: Node, offset?: number): void;
    getComposedRanges(...shadowRoots: ShadowRoot[]): StaticRange[];
    getRangeAt(index: number): Range;
    modify(alter: string, direction: string, granularity: string): void;
    removeAllRanges(): void;
    removeRange(range: Range): void;
    removeSelectionListener(listenerToRemove: nsISelectionListener): void;
    resetColors(): void;
    selectAllChildren(node: Node): void;
    setBaseAndExtent(anchorNode: Node, anchorOffset: number, focusNode: Node, focusOffset: number): void;
    setColors(aForegroundColor: string, aBackgroundColor: string, aAltForegroundColor: string, aAltBackgroundColor: string): void;
    setPosition(node: Node | null, offset?: number): void;
    toStringWithFormat(formatType: string, flags: number, wrapColumn: number): string;
    toString(): string;
}

declare var Selection: {
    prototype: Selection;
    new(): Selection;
    isInstance: IsInstance<Selection>;
};

interface ServiceWorkerEventMap extends AbstractWorkerEventMap {
    "statechange": Event;
}

interface ServiceWorker extends EventTarget, AbstractWorker {
    onstatechange: ((this: ServiceWorker, ev: Event) => any) | null;
    readonly scriptURL: string;
    readonly state: ServiceWorkerState;
    postMessage(message: any, transferable: any[]): void;
    postMessage(message: any, options?: StructuredSerializeOptions): void;
    addEventListener<K extends keyof ServiceWorkerEventMap>(type: K, listener: (this: ServiceWorker, ev: ServiceWorkerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ServiceWorkerEventMap>(type: K, listener: (this: ServiceWorker, ev: ServiceWorkerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ServiceWorker: {
    prototype: ServiceWorker;
    new(): ServiceWorker;
    isInstance: IsInstance<ServiceWorker>;
};

interface ServiceWorkerContainerEventMap {
    "controllerchange": Event;
    "message": Event;
    "messageerror": Event;
}

interface ServiceWorkerContainer extends EventTarget {
    readonly controller: ServiceWorker | null;
    oncontrollerchange: ((this: ServiceWorkerContainer, ev: Event) => any) | null;
    onmessage: ((this: ServiceWorkerContainer, ev: Event) => any) | null;
    onmessageerror: ((this: ServiceWorkerContainer, ev: Event) => any) | null;
    readonly ready: Promise<ServiceWorkerRegistration>;
    getRegistration(documentURL?: string | URL): Promise<ServiceWorkerRegistration | undefined>;
    getRegistrations(): Promise<ServiceWorkerRegistration[]>;
    getScopeForUrl(url: string): string;
    register(scriptURL: string | URL, options?: RegistrationOptions): Promise<ServiceWorkerRegistration>;
    startMessages(): void;
    addEventListener<K extends keyof ServiceWorkerContainerEventMap>(type: K, listener: (this: ServiceWorkerContainer, ev: ServiceWorkerContainerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ServiceWorkerContainerEventMap>(type: K, listener: (this: ServiceWorkerContainer, ev: ServiceWorkerContainerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ServiceWorkerContainer: {
    prototype: ServiceWorkerContainer;
    new(): ServiceWorkerContainer;
    isInstance: IsInstance<ServiceWorkerContainer>;
};

interface ServiceWorkerRegistrationEventMap {
    "updatefound": Event;
}

interface ServiceWorkerRegistration extends EventTarget {
    readonly active: ServiceWorker | null;
    readonly installing: ServiceWorker | null;
    readonly navigationPreload: NavigationPreloadManager;
    onupdatefound: ((this: ServiceWorkerRegistration, ev: Event) => any) | null;
    readonly pushManager: PushManager;
    readonly scope: string;
    readonly updateViaCache: ServiceWorkerUpdateViaCache;
    readonly waiting: ServiceWorker | null;
    getNotifications(filter?: GetNotificationOptions): Promise<Notification[]>;
    showNotification(title: string, options?: NotificationOptions): Promise<void>;
    unregister(): Promise<boolean>;
    update(): Promise<void>;
    addEventListener<K extends keyof ServiceWorkerRegistrationEventMap>(type: K, listener: (this: ServiceWorkerRegistration, ev: ServiceWorkerRegistrationEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ServiceWorkerRegistrationEventMap>(type: K, listener: (this: ServiceWorkerRegistration, ev: ServiceWorkerRegistrationEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ServiceWorkerRegistration: {
    prototype: ServiceWorkerRegistration;
    new(): ServiceWorkerRegistration;
    isInstance: IsInstance<ServiceWorkerRegistration>;
};

interface SessionStoreFormData {
    readonly children: (SessionStoreFormData | null)[] | null;
    readonly id: Record<string, FormDataValue> | null;
    readonly innerHTML: string | null;
    readonly url: string | null;
    readonly xpath: Record<string, FormDataValue> | null;
    toJSON(): any;
}

declare var SessionStoreFormData: {
    prototype: SessionStoreFormData;
    new(): SessionStoreFormData;
    isInstance: IsInstance<SessionStoreFormData>;
};

interface SessionStoreScrollData {
    readonly children: (SessionStoreScrollData | null)[] | null;
    readonly scroll: string | null;
    toJSON(): any;
}

declare var SessionStoreScrollData: {
    prototype: SessionStoreScrollData;
    new(): SessionStoreScrollData;
    isInstance: IsInstance<SessionStoreScrollData>;
};

interface ShadowRootEventMap {
    "slotchange": Event;
}

interface ShadowRoot extends DocumentFragment, DocumentOrShadowRoot {
    readonly clonable: boolean;
    readonly delegatesFocus: boolean;
    readonly host: Element;
    innerHTML: string;
    readonly mode: ShadowRootMode;
    onslotchange: ((this: ShadowRoot, ev: Event) => any) | null;
    readonly serializable: boolean;
    readonly slotAssignment: SlotAssignmentMode;
    createElementAndAppendChildAt(parentNode: Node, localName: string): Node;
    getElementById(elementId: string): Element | null;
    getHTML(options?: GetHTMLOptions): string;
    importNodeAndAppendChildAt(parentNode: Node, node: Node, deep?: boolean): Node;
    isUAWidget(): boolean;
    setHTMLUnsafe(html: string): void;
    setIsUAWidget(): void;
    addEventListener<K extends keyof ShadowRootEventMap>(type: K, listener: (this: ShadowRoot, ev: ShadowRootEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof ShadowRootEventMap>(type: K, listener: (this: ShadowRoot, ev: ShadowRootEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var ShadowRoot: {
    prototype: ShadowRoot;
    new(): ShadowRoot;
    isInstance: IsInstance<ShadowRoot>;
};

interface SharedWorker extends EventTarget, AbstractWorker {
    readonly port: MessagePort;
    addEventListener<K extends keyof AbstractWorkerEventMap>(type: K, listener: (this: SharedWorker, ev: AbstractWorkerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof AbstractWorkerEventMap>(type: K, listener: (this: SharedWorker, ev: AbstractWorkerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SharedWorker: {
    prototype: SharedWorker;
    new(scriptURL: string | URL, options?: string | WorkerOptions): SharedWorker;
    isInstance: IsInstance<SharedWorker>;
};

interface SimpleGestureEvent extends MouseEvent {
    allowedDirections: number;
    readonly clickCount: number;
    readonly delta: number;
    readonly direction: number;
    initSimpleGestureEvent(typeArg: string, canBubbleArg?: boolean, cancelableArg?: boolean, viewArg?: Window | null, detailArg?: number, screenXArg?: number, screenYArg?: number, clientXArg?: number, clientYArg?: number, ctrlKeyArg?: boolean, altKeyArg?: boolean, shiftKeyArg?: boolean, metaKeyArg?: boolean, buttonArg?: number, relatedTargetArg?: EventTarget | null, allowedDirectionsArg?: number, directionArg?: number, deltaArg?: number, clickCount?: number): void;
    readonly DIRECTION_UP: 1;
    readonly DIRECTION_DOWN: 2;
    readonly DIRECTION_LEFT: 4;
    readonly DIRECTION_RIGHT: 8;
    readonly ROTATION_COUNTERCLOCKWISE: 1;
    readonly ROTATION_CLOCKWISE: 2;
}

declare var SimpleGestureEvent: {
    prototype: SimpleGestureEvent;
    new(): SimpleGestureEvent;
    readonly DIRECTION_UP: 1;
    readonly DIRECTION_DOWN: 2;
    readonly DIRECTION_LEFT: 4;
    readonly DIRECTION_RIGHT: 8;
    readonly ROTATION_COUNTERCLOCKWISE: 1;
    readonly ROTATION_CLOCKWISE: 2;
    isInstance: IsInstance<SimpleGestureEvent>;
};

interface SourceBufferEventMap {
    "abort": Event;
    "error": Event;
    "update": Event;
    "updateend": Event;
    "updatestart": Event;
}

interface SourceBuffer extends EventTarget {
    appendWindowEnd: number;
    appendWindowStart: number;
    readonly buffered: TimeRanges;
    mode: SourceBufferAppendMode;
    onabort: ((this: SourceBuffer, ev: Event) => any) | null;
    onerror: ((this: SourceBuffer, ev: Event) => any) | null;
    onupdate: ((this: SourceBuffer, ev: Event) => any) | null;
    onupdateend: ((this: SourceBuffer, ev: Event) => any) | null;
    onupdatestart: ((this: SourceBuffer, ev: Event) => any) | null;
    timestampOffset: number;
    readonly updating: boolean;
    abort(): void;
    appendBuffer(data: ArrayBuffer): void;
    appendBuffer(data: ArrayBufferView): void;
    appendBufferAsync(data: ArrayBuffer): Promise<void>;
    appendBufferAsync(data: ArrayBufferView): Promise<void>;
    changeType(type: string): void;
    remove(start: number, end: number): void;
    removeAsync(start: number, end: number): Promise<void>;
    addEventListener<K extends keyof SourceBufferEventMap>(type: K, listener: (this: SourceBuffer, ev: SourceBufferEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SourceBufferEventMap>(type: K, listener: (this: SourceBuffer, ev: SourceBufferEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SourceBuffer: {
    prototype: SourceBuffer;
    new(): SourceBuffer;
    isInstance: IsInstance<SourceBuffer>;
};

interface SourceBufferListEventMap {
    "addsourcebuffer": Event;
    "removesourcebuffer": Event;
}

interface SourceBufferList extends EventTarget {
    readonly length: number;
    onaddsourcebuffer: ((this: SourceBufferList, ev: Event) => any) | null;
    onremovesourcebuffer: ((this: SourceBufferList, ev: Event) => any) | null;
    addEventListener<K extends keyof SourceBufferListEventMap>(type: K, listener: (this: SourceBufferList, ev: SourceBufferListEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SourceBufferListEventMap>(type: K, listener: (this: SourceBufferList, ev: SourceBufferListEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: SourceBuffer;
}

declare var SourceBufferList: {
    prototype: SourceBufferList;
    new(): SourceBufferList;
    isInstance: IsInstance<SourceBufferList>;
};

interface SpeechGrammar {
    src: string;
    weight: number;
}

declare var SpeechGrammar: {
    prototype: SpeechGrammar;
    new(): SpeechGrammar;
    isInstance: IsInstance<SpeechGrammar>;
};

interface SpeechGrammarList {
    readonly length: number;
    addFromString(string: string, weight?: number): void;
    addFromURI(src: string, weight?: number): void;
    item(index: number): SpeechGrammar;
    [index: number]: SpeechGrammar;
}

declare var SpeechGrammarList: {
    prototype: SpeechGrammarList;
    new(): SpeechGrammarList;
    isInstance: IsInstance<SpeechGrammarList>;
};

interface SpeechRecognitionEventMap {
    "audioend": Event;
    "audiostart": Event;
    "end": Event;
    "error": Event;
    "nomatch": Event;
    "result": Event;
    "soundend": Event;
    "soundstart": Event;
    "speechend": Event;
    "speechstart": Event;
    "start": Event;
}

interface SpeechRecognition extends EventTarget {
    continuous: boolean;
    grammars: SpeechGrammarList;
    interimResults: boolean;
    lang: string;
    maxAlternatives: number;
    onaudioend: ((this: SpeechRecognition, ev: Event) => any) | null;
    onaudiostart: ((this: SpeechRecognition, ev: Event) => any) | null;
    onend: ((this: SpeechRecognition, ev: Event) => any) | null;
    onerror: ((this: SpeechRecognition, ev: Event) => any) | null;
    onnomatch: ((this: SpeechRecognition, ev: Event) => any) | null;
    onresult: ((this: SpeechRecognition, ev: Event) => any) | null;
    onsoundend: ((this: SpeechRecognition, ev: Event) => any) | null;
    onsoundstart: ((this: SpeechRecognition, ev: Event) => any) | null;
    onspeechend: ((this: SpeechRecognition, ev: Event) => any) | null;
    onspeechstart: ((this: SpeechRecognition, ev: Event) => any) | null;
    onstart: ((this: SpeechRecognition, ev: Event) => any) | null;
    serviceURI: string;
    abort(): void;
    start(stream?: MediaStream): void;
    stop(): void;
    addEventListener<K extends keyof SpeechRecognitionEventMap>(type: K, listener: (this: SpeechRecognition, ev: SpeechRecognitionEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SpeechRecognitionEventMap>(type: K, listener: (this: SpeechRecognition, ev: SpeechRecognitionEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SpeechRecognition: {
    prototype: SpeechRecognition;
    new(): SpeechRecognition;
    isInstance: IsInstance<SpeechRecognition>;
};

interface SpeechRecognitionAlternative {
    readonly confidence: number;
    readonly transcript: string;
}

declare var SpeechRecognitionAlternative: {
    prototype: SpeechRecognitionAlternative;
    new(): SpeechRecognitionAlternative;
    isInstance: IsInstance<SpeechRecognitionAlternative>;
};

interface SpeechRecognitionError extends Event {
    readonly error: SpeechRecognitionErrorCode;
    readonly message: string | null;
}

declare var SpeechRecognitionError: {
    prototype: SpeechRecognitionError;
    new(type: string, eventInitDict?: SpeechRecognitionErrorInit): SpeechRecognitionError;
    isInstance: IsInstance<SpeechRecognitionError>;
};

interface SpeechRecognitionEvent extends Event {
    readonly emma: Document | null;
    readonly interpretation: any;
    readonly resultIndex: number;
    readonly results: SpeechRecognitionResultList | null;
}

declare var SpeechRecognitionEvent: {
    prototype: SpeechRecognitionEvent;
    new(type: string, eventInitDict?: SpeechRecognitionEventInit): SpeechRecognitionEvent;
    isInstance: IsInstance<SpeechRecognitionEvent>;
};

interface SpeechRecognitionResult {
    readonly isFinal: boolean;
    readonly length: number;
    item(index: number): SpeechRecognitionAlternative;
    [index: number]: SpeechRecognitionAlternative;
}

declare var SpeechRecognitionResult: {
    prototype: SpeechRecognitionResult;
    new(): SpeechRecognitionResult;
    isInstance: IsInstance<SpeechRecognitionResult>;
};

interface SpeechRecognitionResultList {
    readonly length: number;
    item(index: number): SpeechRecognitionResult;
    [index: number]: SpeechRecognitionResult;
}

declare var SpeechRecognitionResultList: {
    prototype: SpeechRecognitionResultList;
    new(): SpeechRecognitionResultList;
    isInstance: IsInstance<SpeechRecognitionResultList>;
};

interface SpeechSynthesisEventMap {
    "voiceschanged": Event;
}

interface SpeechSynthesis extends EventTarget {
    onvoiceschanged: ((this: SpeechSynthesis, ev: Event) => any) | null;
    readonly paused: boolean;
    readonly pending: boolean;
    readonly speaking: boolean;
    cancel(): void;
    forceEnd(): void;
    getVoices(): SpeechSynthesisVoice[];
    pause(): void;
    resume(): void;
    speak(utterance: SpeechSynthesisUtterance): void;
    addEventListener<K extends keyof SpeechSynthesisEventMap>(type: K, listener: (this: SpeechSynthesis, ev: SpeechSynthesisEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SpeechSynthesisEventMap>(type: K, listener: (this: SpeechSynthesis, ev: SpeechSynthesisEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SpeechSynthesis: {
    prototype: SpeechSynthesis;
    new(): SpeechSynthesis;
    isInstance: IsInstance<SpeechSynthesis>;
};

interface SpeechSynthesisErrorEvent extends SpeechSynthesisEvent {
    readonly error: SpeechSynthesisErrorCode;
}

declare var SpeechSynthesisErrorEvent: {
    prototype: SpeechSynthesisErrorEvent;
    new(type: string, eventInitDict: SpeechSynthesisErrorEventInit): SpeechSynthesisErrorEvent;
    isInstance: IsInstance<SpeechSynthesisErrorEvent>;
};

interface SpeechSynthesisEvent extends Event {
    readonly charIndex: number;
    readonly charLength: number | null;
    readonly elapsedTime: number;
    readonly name: string | null;
    readonly utterance: SpeechSynthesisUtterance;
}

declare var SpeechSynthesisEvent: {
    prototype: SpeechSynthesisEvent;
    new(type: string, eventInitDict: SpeechSynthesisEventInit): SpeechSynthesisEvent;
    isInstance: IsInstance<SpeechSynthesisEvent>;
};

interface SpeechSynthesisGetter {
    readonly speechSynthesis: SpeechSynthesis;
}

interface SpeechSynthesisUtteranceEventMap {
    "boundary": Event;
    "end": Event;
    "error": Event;
    "mark": Event;
    "pause": Event;
    "resume": Event;
    "start": Event;
}

interface SpeechSynthesisUtterance extends EventTarget {
    readonly chosenVoiceURI: string;
    lang: string;
    onboundary: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onend: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onerror: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onmark: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onpause: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onresume: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    onstart: ((this: SpeechSynthesisUtterance, ev: Event) => any) | null;
    pitch: number;
    rate: number;
    text: string;
    voice: SpeechSynthesisVoice | null;
    volume: number;
    addEventListener<K extends keyof SpeechSynthesisUtteranceEventMap>(type: K, listener: (this: SpeechSynthesisUtterance, ev: SpeechSynthesisUtteranceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof SpeechSynthesisUtteranceEventMap>(type: K, listener: (this: SpeechSynthesisUtterance, ev: SpeechSynthesisUtteranceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var SpeechSynthesisUtterance: {
    prototype: SpeechSynthesisUtterance;
    new(): SpeechSynthesisUtterance;
    new(text: string): SpeechSynthesisUtterance;
    isInstance: IsInstance<SpeechSynthesisUtterance>;
};

interface SpeechSynthesisVoice {
    readonly default: boolean;
    readonly lang: string;
    readonly localService: boolean;
    readonly name: string;
    readonly voiceURI: string;
}

declare var SpeechSynthesisVoice: {
    prototype: SpeechSynthesisVoice;
    new(): SpeechSynthesisVoice;
    isInstance: IsInstance<SpeechSynthesisVoice>;
};

interface StackFrame {
}

interface StaticRange extends AbstractRange {
}

declare var StaticRange: {
    prototype: StaticRange;
    new(init: StaticRangeInit): StaticRange;
    isInstance: IsInstance<StaticRange>;
};

interface StereoPannerNode extends AudioNode, AudioNodePassThrough {
    readonly pan: AudioParam;
}

declare var StereoPannerNode: {
    prototype: StereoPannerNode;
    new(context: BaseAudioContext, options?: StereoPannerOptions): StereoPannerNode;
    isInstance: IsInstance<StereoPannerNode>;
};

interface Storage {
    readonly hasSnapshot: boolean;
    readonly length: number;
    readonly snapshotUsage: number;
    beginExplicitSnapshot(): void;
    checkpointExplicitSnapshot(): void;
    clear(): void;
    close(): void;
    endExplicitSnapshot(): void;
    getItem(key: string): string | null;
    key(index: number): string | null;
    open(): void;
    removeItem(key: string): void;
    setItem(key: string, value: string): void;
}

declare var Storage: {
    prototype: Storage;
    new(): Storage;
    isInstance: IsInstance<Storage>;
};

interface StorageEvent extends Event {
    readonly key: string | null;
    readonly newValue: string | null;
    readonly oldValue: string | null;
    readonly storageArea: Storage | null;
    readonly url: string;
    initStorageEvent(type: string, canBubble?: boolean, cancelable?: boolean, key?: string | null, oldValue?: string | null, newValue?: string | null, url?: string | URL, storageArea?: Storage | null): void;
}

declare var StorageEvent: {
    prototype: StorageEvent;
    new(type: string, eventInitDict?: StorageEventInit): StorageEvent;
    isInstance: IsInstance<StorageEvent>;
};

/** Available only in secure contexts. */
interface StorageManager {
    estimate(): Promise<StorageEstimate>;
    getDirectory(): Promise<FileSystemDirectoryHandle>;
    persist(): Promise<boolean>;
    persisted(): Promise<boolean>;
    shutdown(): void;
}

declare var StorageManager: {
    prototype: StorageManager;
    new(): StorageManager;
    isInstance: IsInstance<StorageManager>;
};

interface StreamFilterEventMap {
    "data": Event;
    "error": Event;
    "start": Event;
    "stop": Event;
}

interface StreamFilter extends EventTarget {
    readonly error: string;
    ondata: ((this: StreamFilter, ev: Event) => any) | null;
    onerror: ((this: StreamFilter, ev: Event) => any) | null;
    onstart: ((this: StreamFilter, ev: Event) => any) | null;
    onstop: ((this: StreamFilter, ev: Event) => any) | null;
    readonly status: StreamFilterStatus;
    close(): void;
    disconnect(): void;
    resume(): void;
    suspend(): void;
    write(data: ArrayBuffer | Uint8Array): void;
    addEventListener<K extends keyof StreamFilterEventMap>(type: K, listener: (this: StreamFilter, ev: StreamFilterEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof StreamFilterEventMap>(type: K, listener: (this: StreamFilter, ev: StreamFilterEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var StreamFilter: {
    prototype: StreamFilter;
    new(): StreamFilter;
    isInstance: IsInstance<StreamFilter>;
    create(requestId: number, addonId: string): StreamFilter;
};

interface StreamFilterDataEvent extends Event {
    readonly data: ArrayBuffer;
}

declare var StreamFilterDataEvent: {
    prototype: StreamFilterDataEvent;
    new(type: string, eventInitDict?: StreamFilterDataEventInit): StreamFilterDataEvent;
    isInstance: IsInstance<StreamFilterDataEvent>;
};

interface StructuredCloneHolder {
    readonly dataSize: number;
    deserialize(global: any, keepData?: boolean): any;
}

declare var StructuredCloneHolder: {
    prototype: StructuredCloneHolder;
    new(name: string, anonymizedName: string | null, data: any, global?: any): StructuredCloneHolder;
    isInstance: IsInstance<StructuredCloneHolder>;
};

interface StructuredCloneTester {
    readonly deserializable: boolean;
    readonly serializable: boolean;
}

declare var StructuredCloneTester: {
    prototype: StructuredCloneTester;
    new(serializable: boolean, deserializable: boolean): StructuredCloneTester;
    isInstance: IsInstance<StructuredCloneTester>;
};

interface StyleSheet {
    readonly associatedDocument: Document | null;
    readonly constructed: boolean;
    disabled: boolean;
    readonly href: string | null;
    readonly media: MediaList;
    readonly ownerNode: Node | null;
    readonly parentStyleSheet: StyleSheet | null;
    readonly sourceMapURL: string;
    readonly sourceURL: string;
    readonly title: string | null;
    readonly type: string;
}

declare var StyleSheet: {
    prototype: StyleSheet;
    new(): StyleSheet;
    isInstance: IsInstance<StyleSheet>;
};

interface StyleSheetApplicableStateChangeEvent extends Event {
    readonly applicable: boolean;
    readonly stylesheet: CSSStyleSheet | null;
}

declare var StyleSheetApplicableStateChangeEvent: {
    prototype: StyleSheetApplicableStateChangeEvent;
    new(type: string, eventInitDict?: StyleSheetApplicableStateChangeEventInit): StyleSheetApplicableStateChangeEvent;
    isInstance: IsInstance<StyleSheetApplicableStateChangeEvent>;
};

interface StyleSheetList {
    readonly length: number;
    item(index: number): CSSStyleSheet | null;
    [index: number]: CSSStyleSheet;
}

declare var StyleSheetList: {
    prototype: StyleSheetList;
    new(): StyleSheetList;
    isInstance: IsInstance<StyleSheetList>;
};

interface StyleSheetRemovedEvent extends Event {
    readonly stylesheet: CSSStyleSheet | null;
}

declare var StyleSheetRemovedEvent: {
    prototype: StyleSheetRemovedEvent;
    new(type: string, eventInitDict?: StyleSheetRemovedEventInit): StyleSheetRemovedEvent;
    isInstance: IsInstance<StyleSheetRemovedEvent>;
};

interface SubmitEvent extends Event {
    readonly submitter: HTMLElement | null;
}

declare var SubmitEvent: {
    prototype: SubmitEvent;
    new(type: string, eventInitDict?: SubmitEventInit): SubmitEvent;
    isInstance: IsInstance<SubmitEvent>;
};

/** Available only in secure contexts. */
interface SubtleCrypto {
    decrypt(algorithm: AlgorithmIdentifier, key: CryptoKey, data: BufferSource): Promise<any>;
    deriveBits(algorithm: AlgorithmIdentifier, baseKey: CryptoKey, length: number): Promise<any>;
    deriveKey(algorithm: AlgorithmIdentifier, baseKey: CryptoKey, derivedKeyType: AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<any>;
    digest(algorithm: AlgorithmIdentifier, data: BufferSource): Promise<any>;
    encrypt(algorithm: AlgorithmIdentifier, key: CryptoKey, data: BufferSource): Promise<any>;
    exportKey(format: KeyFormat, key: CryptoKey): Promise<any>;
    generateKey(algorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<any>;
    importKey(format: KeyFormat, keyData: any, algorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<any>;
    sign(algorithm: AlgorithmIdentifier, key: CryptoKey, data: BufferSource): Promise<any>;
    unwrapKey(format: KeyFormat, wrappedKey: BufferSource, unwrappingKey: CryptoKey, unwrapAlgorithm: AlgorithmIdentifier, unwrappedKeyAlgorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<any>;
    verify(algorithm: AlgorithmIdentifier, key: CryptoKey, signature: BufferSource, data: BufferSource): Promise<any>;
    wrapKey(format: KeyFormat, key: CryptoKey, wrappingKey: CryptoKey, wrapAlgorithm: AlgorithmIdentifier): Promise<any>;
}

declare var SubtleCrypto: {
    prototype: SubtleCrypto;
    new(): SubtleCrypto;
    isInstance: IsInstance<SubtleCrypto>;
};

interface SyncMessageSender extends MessageSender, SyncMessageSenderMixin {
}

declare var SyncMessageSender: {
    prototype: SyncMessageSender;
    new(): SyncMessageSender;
    isInstance: IsInstance<SyncMessageSender>;
};

interface SyncMessageSenderMixin {
    sendSyncMessage(messageName?: string | null, obj?: any): any[];
}

interface TCPServerSocketEventMap {
    "connect": Event;
    "error": Event;
}

interface TCPServerSocket extends EventTarget {
    readonly localPort: number;
    onconnect: ((this: TCPServerSocket, ev: Event) => any) | null;
    onerror: ((this: TCPServerSocket, ev: Event) => any) | null;
    close(): void;
    addEventListener<K extends keyof TCPServerSocketEventMap>(type: K, listener: (this: TCPServerSocket, ev: TCPServerSocketEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TCPServerSocketEventMap>(type: K, listener: (this: TCPServerSocket, ev: TCPServerSocketEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TCPServerSocket: {
    prototype: TCPServerSocket;
    new(port: number, options?: ServerSocketOptions, backlog?: number): TCPServerSocket;
    isInstance: IsInstance<TCPServerSocket>;
};

interface TCPServerSocketEvent extends Event {
    readonly socket: TCPSocket;
}

declare var TCPServerSocketEvent: {
    prototype: TCPServerSocketEvent;
    new(type: string, eventInitDict?: TCPServerSocketEventInit): TCPServerSocketEvent;
    isInstance: IsInstance<TCPServerSocketEvent>;
};

interface TCPSocketEventMap {
    "close": Event;
    "data": Event;
    "drain": Event;
    "error": Event;
    "open": Event;
}

interface TCPSocket extends EventTarget {
    readonly binaryType: TCPSocketBinaryType;
    readonly bufferedAmount: number;
    readonly host: string;
    onclose: ((this: TCPSocket, ev: Event) => any) | null;
    ondata: ((this: TCPSocket, ev: Event) => any) | null;
    ondrain: ((this: TCPSocket, ev: Event) => any) | null;
    onerror: ((this: TCPSocket, ev: Event) => any) | null;
    onopen: ((this: TCPSocket, ev: Event) => any) | null;
    readonly port: number;
    readonly readyState: TCPReadyState;
    readonly ssl: boolean;
    readonly transport: nsISocketTransport | null;
    close(): void;
    closeImmediately(): void;
    resume(): void;
    send(data: string): boolean;
    send(data: ArrayBuffer, byteOffset?: number, byteLength?: number): boolean;
    suspend(): void;
    upgradeToSecure(): void;
    addEventListener<K extends keyof TCPSocketEventMap>(type: K, listener: (this: TCPSocket, ev: TCPSocketEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TCPSocketEventMap>(type: K, listener: (this: TCPSocket, ev: TCPSocketEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TCPSocket: {
    prototype: TCPSocket;
    new(host: string, port: number, options?: SocketOptions): TCPSocket;
    isInstance: IsInstance<TCPSocket>;
};

interface TCPSocketErrorEvent extends Event {
    readonly errorCode: number;
    readonly message: string;
    readonly name: string;
}

declare var TCPSocketErrorEvent: {
    prototype: TCPSocketErrorEvent;
    new(type: string, eventInitDict?: TCPSocketErrorEventInit): TCPSocketErrorEvent;
    isInstance: IsInstance<TCPSocketErrorEvent>;
};

interface TCPSocketEvent extends Event {
    readonly data: any;
}

declare var TCPSocketEvent: {
    prototype: TCPSocketEvent;
    new(type: string, eventInitDict?: TCPSocketEventInit): TCPSocketEvent;
    isInstance: IsInstance<TCPSocketEvent>;
};

interface TaskController extends AbortController {
    setPriority(priority: TaskPriority): void;
}

declare var TaskController: {
    prototype: TaskController;
    new(init?: TaskControllerInit): TaskController;
    isInstance: IsInstance<TaskController>;
};

interface TaskPriorityChangeEvent extends Event {
    readonly previousPriority: TaskPriority;
}

declare var TaskPriorityChangeEvent: {
    prototype: TaskPriorityChangeEvent;
    new(type: string, priorityChangeEventInitDict: TaskPriorityChangeEventInit): TaskPriorityChangeEvent;
    isInstance: IsInstance<TaskPriorityChangeEvent>;
};

interface TaskSignalEventMap extends AbortSignalEventMap {
    "prioritychange": Event;
}

interface TaskSignal extends AbortSignal {
    onprioritychange: ((this: TaskSignal, ev: Event) => any) | null;
    readonly priority: TaskPriority;
    addEventListener<K extends keyof TaskSignalEventMap>(type: K, listener: (this: TaskSignal, ev: TaskSignalEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TaskSignalEventMap>(type: K, listener: (this: TaskSignal, ev: TaskSignalEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TaskSignal: {
    prototype: TaskSignal;
    new(): TaskSignal;
    isInstance: IsInstance<TaskSignal>;
};

interface TestFunctions {
    allowSharedArrayBuffer: ArrayBuffer;
    allowSharedArrayBufferView: ArrayBufferView;
    arrayBuffer: ArrayBuffer;
    arrayBufferView: ArrayBufferView;
    clampedNullableOctet: number | null;
    enforcedNullableOctet: number | null;
    readonly one: number;
    sequenceOfAllowSharedArrayBuffer: ArrayBuffer[];
    sequenceOfAllowSharedArrayBufferView: ArrayBufferView[];
    sequenceOfArrayBuffer: ArrayBuffer[];
    sequenceOfArrayBufferView: ArrayBufferView[];
    readonly two: number;
    readonly wrapperCachedNonISupportsObject: WrapperCachedNonISupportsTestInterface;
    getLongLiteralString(): string;
    getMediumLiteralString(): string;
    getShortLiteralString(): string;
    getStringDataAsAString(): string;
    getStringDataAsAString(length: number): string;
    getStringDataAsDOMString(length?: number): string;
    getStringType(str: string): StringType;
    getStringbufferString(input: string): string;
    setStringData(arg: string): void;
    staticAndNonStaticOverload(): boolean;
    staticAndNonStaticOverload(foo?: number): boolean;
    stringbufferMatchesStored(str: string): boolean;
    testAllowShared(buffer: ArrayBufferView): void;
    testAllowShared(buffer: ArrayBuffer): void;
    testDictWithAllowShared(buffer?: DictWithAllowSharedBufferSource): void;
    testNotAllowShared(buffer: ArrayBufferView): void;
    testNotAllowShared(buffer: ArrayBuffer): void;
    testNotAllowShared(buffer: string): void;
    testThrowNsresult(): void;
    testThrowNsresultFromNative(): void;
    testUnionOfAllowSharedBuffferSource(foo: ArrayBuffer | ArrayBufferView): void;
    testUnionOfBuffferSource(foo: ArrayBuffer | ArrayBufferView | string): void;
    toJSON(): any;
}

declare var TestFunctions: {
    prototype: TestFunctions;
    new(): TestFunctions;
    isInstance: IsInstance<TestFunctions>;
    passThroughCallbackPromise(callback: PromiseReturner): Promise<any>;
    passThroughPromise(arg: any): Promise<any>;
    throwToRejectPromise(): Promise<any>;
    throwUncatchableException(): void;
};

interface TestInterfaceAsyncIterableDouble {
}

declare var TestInterfaceAsyncIterableDouble: {
    prototype: TestInterfaceAsyncIterableDouble;
    new(): TestInterfaceAsyncIterableDouble;
    isInstance: IsInstance<TestInterfaceAsyncIterableDouble>;
};

interface TestInterfaceAsyncIterableDoubleUnion {
}

declare var TestInterfaceAsyncIterableDoubleUnion: {
    prototype: TestInterfaceAsyncIterableDoubleUnion;
    new(): TestInterfaceAsyncIterableDoubleUnion;
    isInstance: IsInstance<TestInterfaceAsyncIterableDoubleUnion>;
};

interface TestInterfaceAsyncIterableSingle {
}

declare var TestInterfaceAsyncIterableSingle: {
    prototype: TestInterfaceAsyncIterableSingle;
    new(options?: TestInterfaceAsyncIterableSingleOptions): TestInterfaceAsyncIterableSingle;
    isInstance: IsInstance<TestInterfaceAsyncIterableSingle>;
};

interface TestInterfaceAsyncIterableSingleWithArgs {
    readonly returnCallCount: number;
    readonly returnLastCalledWith: any;
}

declare var TestInterfaceAsyncIterableSingleWithArgs: {
    prototype: TestInterfaceAsyncIterableSingleWithArgs;
    new(): TestInterfaceAsyncIterableSingleWithArgs;
    isInstance: IsInstance<TestInterfaceAsyncIterableSingleWithArgs>;
};

interface TestInterfaceIterableDouble {
    forEach(callbackfn: (value: string, key: string, parent: TestInterfaceIterableDouble) => void, thisArg?: any): void;
}

declare var TestInterfaceIterableDouble: {
    prototype: TestInterfaceIterableDouble;
    new(): TestInterfaceIterableDouble;
    isInstance: IsInstance<TestInterfaceIterableDouble>;
};

interface TestInterfaceIterableDoubleUnion {
    forEach(callbackfn: (value: string | number, key: string, parent: TestInterfaceIterableDoubleUnion) => void, thisArg?: any): void;
}

declare var TestInterfaceIterableDoubleUnion: {
    prototype: TestInterfaceIterableDoubleUnion;
    new(): TestInterfaceIterableDoubleUnion;
    isInstance: IsInstance<TestInterfaceIterableDoubleUnion>;
};

interface TestInterfaceIterableSingle {
    readonly length: number;
    forEach(callbackfn: (value: number, key: number, parent: TestInterfaceIterableSingle) => void, thisArg?: any): void;
    [index: number]: number;
}

declare var TestInterfaceIterableSingle: {
    prototype: TestInterfaceIterableSingle;
    new(): TestInterfaceIterableSingle;
    isInstance: IsInstance<TestInterfaceIterableSingle>;
};

interface TestInterfaceJSEventMap {
    "something": Event;
}

interface TestInterfaceJS extends EventTarget {
    readonly anyArg: any;
    anyAttr: any;
    readonly objectArg: any;
    objectAttr: any;
    onsomething: ((this: TestInterfaceJS, ev: Event) => any) | null;
    anySequenceLength(seq: any[]): number;
    convertSVS(svs: string): string;
    getCallerPrincipal(): string;
    getDictionaryArg(): TestInterfaceJSDictionary;
    getDictionaryAttr(): TestInterfaceJSDictionary;
    objectSequenceLength(seq: any[]): number;
    pingPongAny(arg: any): any;
    pingPongDictionary(dict?: TestInterfaceJSDictionary): TestInterfaceJSDictionary;
    pingPongDictionaryOrLong(dictOrLong?: TestInterfaceJSUnionableDictionary | number): number;
    pingPongNullableUnion(something: TestInterfaceJS | number | null): TestInterfaceJS | number | null;
    pingPongObject(obj: any): any;
    pingPongObjectOrString(objOrString: any): any;
    pingPongRecord(rec: Record<string, any>): string;
    pingPongUnion(something: TestInterfaceJS | number): TestInterfaceJS | number;
    pingPongUnionContainingNull(something: TestInterfaceJS | string): string | TestInterfaceJS;
    returnBadUnion(): Location | TestInterfaceJS;
    setDictionaryAttr(dict?: TestInterfaceJSDictionary): void;
    testPromiseWithDOMExceptionThrowingPromiseInit(): Promise<void>;
    testPromiseWithDOMExceptionThrowingThenFunction(): Promise<void>;
    testPromiseWithDOMExceptionThrowingThenable(): Promise<void>;
    testPromiseWithThrowingChromePromiseInit(): Promise<void>;
    testPromiseWithThrowingChromeThenFunction(): Promise<void>;
    testPromiseWithThrowingChromeThenable(): Promise<void>;
    testPromiseWithThrowingContentPromiseInit(func: Function): Promise<void>;
    testPromiseWithThrowingContentThenFunction(func: AnyCallback): Promise<void>;
    testPromiseWithThrowingContentThenable(thenable: any): Promise<void>;
    testSequenceOverload(arg: string[]): void;
    testSequenceOverload(arg: string): void;
    testSequenceUnion(arg: string[] | string): void;
    testThrowCallbackError(callback: Function): void;
    testThrowDOMException(): void;
    testThrowError(): void;
    testThrowSelfHosted(): void;
    testThrowTypeError(): void;
    testThrowXraySelfHosted(): void;
    addEventListener<K extends keyof TestInterfaceJSEventMap>(type: K, listener: (this: TestInterfaceJS, ev: TestInterfaceJSEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TestInterfaceJSEventMap>(type: K, listener: (this: TestInterfaceJS, ev: TestInterfaceJSEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TestInterfaceJS: {
    prototype: TestInterfaceJS;
    new(anyArg?: any, objectArg?: any, dictionaryArg?: TestInterfaceJSDictionary): TestInterfaceJS;
    isInstance: IsInstance<TestInterfaceJS>;
};

interface TestInterfaceLength {
}

declare var TestInterfaceLength: {
    prototype: TestInterfaceLength;
    new(arg: boolean): TestInterfaceLength;
    isInstance: IsInstance<TestInterfaceLength>;
};

interface TestInterfaceMaplike {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): number;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string, aValue: number): void;
    forEach(callbackfn: (value: number, key: string, parent: TestInterfaceMaplike) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplike: {
    prototype: TestInterfaceMaplike;
    new(): TestInterfaceMaplike;
    isInstance: IsInstance<TestInterfaceMaplike>;
};

interface TestInterfaceMaplikeJSObject {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): any;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string, aObject: any): void;
    forEach(callbackfn: (value: any, key: string, parent: TestInterfaceMaplikeJSObject) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplikeJSObject: {
    prototype: TestInterfaceMaplikeJSObject;
    new(): TestInterfaceMaplikeJSObject;
    isInstance: IsInstance<TestInterfaceMaplikeJSObject>;
};

interface TestInterfaceMaplikeObject {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): TestInterfaceMaplike | null;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string): void;
    forEach(callbackfn: (value: TestInterfaceMaplike, key: string, parent: TestInterfaceMaplikeObject) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplikeObject: {
    prototype: TestInterfaceMaplikeObject;
    new(): TestInterfaceMaplikeObject;
    isInstance: IsInstance<TestInterfaceMaplikeObject>;
};

interface TestInterfaceObservableArray {
    observableArrayBoolean: boolean[];
    observableArrayInterface: TestInterfaceObservableArray[];
    observableArrayObject: any[];
    booleanAppendElementInternal(value: boolean): void;
    booleanElementAtInternal(index: number): boolean;
    booleanLengthInternal(): number;
    booleanRemoveLastElementInternal(): void;
    booleanReplaceElementAtInternal(index: number, value: boolean): void;
    interfaceAppendElementInternal(value: TestInterfaceObservableArray): void;
    interfaceElementAtInternal(index: number): TestInterfaceObservableArray;
    interfaceLengthInternal(): number;
    interfaceRemoveLastElementInternal(): void;
    interfaceReplaceElementAtInternal(index: number, value: TestInterfaceObservableArray): void;
    objectAppendElementInternal(value: any): void;
    objectElementAtInternal(index: number): any;
    objectLengthInternal(): number;
    objectRemoveLastElementInternal(): void;
    objectReplaceElementAtInternal(index: number, value: any): void;
}

declare var TestInterfaceObservableArray: {
    prototype: TestInterfaceObservableArray;
    new(callbacks?: ObservableArrayCallbacks): TestInterfaceObservableArray;
    isInstance: IsInstance<TestInterfaceObservableArray>;
};

interface TestInterfaceSetlike {
    forEach(callbackfn: (value: string, key: string, parent: TestInterfaceSetlike) => void, thisArg?: any): void;
}

declare var TestInterfaceSetlike: {
    prototype: TestInterfaceSetlike;
    new(): TestInterfaceSetlike;
    isInstance: IsInstance<TestInterfaceSetlike>;
};

interface TestInterfaceSetlikeNode {
    forEach(callbackfn: (value: Node, key: Node, parent: TestInterfaceSetlikeNode) => void, thisArg?: any): void;
}

declare var TestInterfaceSetlikeNode: {
    prototype: TestInterfaceSetlikeNode;
    new(): TestInterfaceSetlikeNode;
    isInstance: IsInstance<TestInterfaceSetlikeNode>;
};

interface TestReflectedHTMLAttribute {
    reflectedHTMLAttribute: Element[] | null;
    setReflectedHTMLAttributeValue(seq: Element[]): void;
}

declare var TestReflectedHTMLAttribute: {
    prototype: TestReflectedHTMLAttribute;
    new(): TestReflectedHTMLAttribute;
    isInstance: IsInstance<TestReflectedHTMLAttribute>;
};

interface TestTrialInterface {
}

declare var TestTrialInterface: {
    prototype: TestTrialInterface;
    new(): TestTrialInterface;
    isInstance: IsInstance<TestTrialInterface>;
};

interface TestingDeprecatedInterface {
    readonly deprecatedAttribute: boolean;
    deprecatedMethod(): void;
}

declare var TestingDeprecatedInterface: {
    prototype: TestingDeprecatedInterface;
    new(): TestingDeprecatedInterface;
    isInstance: IsInstance<TestingDeprecatedInterface>;
};

interface Text extends CharacterData, GeometryUtils {
    readonly assignedSlot: HTMLSlotElement | null;
    readonly openOrClosedAssignedSlot: HTMLSlotElement | null;
    readonly wholeText: string;
    splitText(offset: number): Text;
}

declare var Text: {
    prototype: Text;
    new(data?: string): Text;
    isInstance: IsInstance<Text>;
};

interface TextClause {
    readonly endOffset: number;
    readonly isCaret: boolean;
    readonly isTargetClause: boolean;
    readonly startOffset: number;
}

declare var TextClause: {
    prototype: TextClause;
    new(): TextClause;
    isInstance: IsInstance<TextClause>;
};

interface TextDecoder extends TextDecoderCommon {
    decode(input?: BufferSource, options?: TextDecodeOptions): string;
}

declare var TextDecoder: {
    prototype: TextDecoder;
    new(label?: string, options?: TextDecoderOptions): TextDecoder;
    isInstance: IsInstance<TextDecoder>;
};

interface TextDecoderCommon {
    readonly encoding: string;
    readonly fatal: boolean;
    readonly ignoreBOM: boolean;
}

interface TextDecoderStream extends GenericTransformStream, TextDecoderCommon {
}

declare var TextDecoderStream: {
    prototype: TextDecoderStream;
    new(label?: string, options?: TextDecoderOptions): TextDecoderStream;
    isInstance: IsInstance<TextDecoderStream>;
};

interface TextEncoder extends TextEncoderCommon {
    encode(input?: string): Uint8Array;
    encodeInto(source: JSString, destination: Uint8Array): TextEncoderEncodeIntoResult;
}

declare var TextEncoder: {
    prototype: TextEncoder;
    new(): TextEncoder;
    isInstance: IsInstance<TextEncoder>;
};

interface TextEncoderCommon {
    readonly encoding: string;
}

interface TextEncoderStream extends GenericTransformStream, TextEncoderCommon {
}

declare var TextEncoderStream: {
    prototype: TextEncoderStream;
    new(): TextEncoderStream;
    isInstance: IsInstance<TextEncoderStream>;
};

interface TextEvent extends UIEvent {
    readonly data: string;
    initTextEvent(type: string, bubbles?: boolean, cancelable?: boolean, view?: Window | null, data?: string): void;
}

declare var TextEvent: {
    prototype: TextEvent;
    new(): TextEvent;
    isInstance: IsInstance<TextEvent>;
};

interface TextMetrics {
    readonly actualBoundingBoxAscent: number;
    readonly actualBoundingBoxDescent: number;
    readonly actualBoundingBoxLeft: number;
    readonly actualBoundingBoxRight: number;
    readonly alphabeticBaseline: number;
    readonly emHeightAscent: number;
    readonly emHeightDescent: number;
    readonly fontBoundingBoxAscent: number;
    readonly fontBoundingBoxDescent: number;
    readonly hangingBaseline: number;
    readonly ideographicBaseline: number;
    readonly width: number;
}

declare var TextMetrics: {
    prototype: TextMetrics;
    new(): TextMetrics;
    isInstance: IsInstance<TextMetrics>;
};

interface TextTrackEventMap {
    "cuechange": Event;
}

interface TextTrack extends EventTarget {
    readonly activeCues: TextTrackCueList | null;
    readonly cues: TextTrackCueList | null;
    readonly id: string;
    readonly inBandMetadataTrackDispatchType: string;
    readonly kind: TextTrackKind;
    readonly label: string;
    readonly language: string;
    mode: TextTrackMode;
    oncuechange: ((this: TextTrack, ev: Event) => any) | null;
    readonly textTrackList: TextTrackList | null;
    addCue(cue: VTTCue): void;
    removeCue(cue: VTTCue): void;
    addEventListener<K extends keyof TextTrackEventMap>(type: K, listener: (this: TextTrack, ev: TextTrackEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TextTrackEventMap>(type: K, listener: (this: TextTrack, ev: TextTrackEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TextTrack: {
    prototype: TextTrack;
    new(): TextTrack;
    isInstance: IsInstance<TextTrack>;
};

interface TextTrackCueEventMap {
    "enter": Event;
    "exit": Event;
}

interface TextTrackCue extends EventTarget {
    endTime: number;
    id: string;
    onenter: ((this: TextTrackCue, ev: Event) => any) | null;
    onexit: ((this: TextTrackCue, ev: Event) => any) | null;
    pauseOnExit: boolean;
    startTime: number;
    readonly track: TextTrack | null;
    addEventListener<K extends keyof TextTrackCueEventMap>(type: K, listener: (this: TextTrackCue, ev: TextTrackCueEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TextTrackCueEventMap>(type: K, listener: (this: TextTrackCue, ev: TextTrackCueEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TextTrackCue: {
    prototype: TextTrackCue;
    new(): TextTrackCue;
    isInstance: IsInstance<TextTrackCue>;
};

interface TextTrackCueList {
    readonly length: number;
    getCueById(id: string): VTTCue | null;
    [index: number]: VTTCue;
}

declare var TextTrackCueList: {
    prototype: TextTrackCueList;
    new(): TextTrackCueList;
    isInstance: IsInstance<TextTrackCueList>;
};

interface TextTrackListEventMap {
    "addtrack": Event;
    "change": Event;
    "removetrack": Event;
}

interface TextTrackList extends EventTarget {
    readonly length: number;
    readonly mediaElement: HTMLMediaElement | null;
    onaddtrack: ((this: TextTrackList, ev: Event) => any) | null;
    onchange: ((this: TextTrackList, ev: Event) => any) | null;
    onremovetrack: ((this: TextTrackList, ev: Event) => any) | null;
    getTrackById(id: string): TextTrack | null;
    addEventListener<K extends keyof TextTrackListEventMap>(type: K, listener: (this: TextTrackList, ev: TextTrackListEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TextTrackListEventMap>(type: K, listener: (this: TextTrackList, ev: TextTrackListEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: TextTrack;
}

declare var TextTrackList: {
    prototype: TextTrackList;
    new(): TextTrackList;
    isInstance: IsInstance<TextTrackList>;
};

interface TimeEvent extends Event {
    readonly detail: number;
    readonly view: WindowProxy | null;
    initTimeEvent(aType: string, aView?: Window | null, aDetail?: number): void;
}

declare var TimeEvent: {
    prototype: TimeEvent;
    new(): TimeEvent;
    isInstance: IsInstance<TimeEvent>;
};

interface TimeRanges {
    readonly length: number;
    end(index: number): number;
    start(index: number): number;
}

declare var TimeRanges: {
    prototype: TimeRanges;
    new(): TimeRanges;
    isInstance: IsInstance<TimeRanges>;
};

interface ToggleEvent extends Event {
    readonly newState: string;
    readonly oldState: string;
}

declare var ToggleEvent: {
    prototype: ToggleEvent;
    new(type: string, eventInitDict?: ToggleEventInit): ToggleEvent;
    isInstance: IsInstance<ToggleEvent>;
};

interface Touch {
    readonly clientX: number;
    readonly clientY: number;
    readonly force: number;
    readonly identifier: number;
    readonly pageX: number;
    readonly pageY: number;
    readonly radiusX: number;
    readonly radiusY: number;
    readonly rotationAngle: number;
    readonly screenX: number;
    readonly screenY: number;
    readonly target: EventTarget | null;
}

declare var Touch: {
    prototype: Touch;
    new(touchInitDict: TouchInit): Touch;
    isInstance: IsInstance<Touch>;
};

interface TouchEvent extends UIEvent {
    readonly altKey: boolean;
    readonly changedTouches: TouchList;
    readonly ctrlKey: boolean;
    readonly metaKey: boolean;
    readonly shiftKey: boolean;
    readonly targetTouches: TouchList;
    readonly touches: TouchList;
    initTouchEvent(type: string, canBubble?: boolean, cancelable?: boolean, view?: Window | null, detail?: number, ctrlKey?: boolean, altKey?: boolean, shiftKey?: boolean, metaKey?: boolean, touches?: TouchList | null, targetTouches?: TouchList | null, changedTouches?: TouchList | null): void;
}

declare var TouchEvent: {
    prototype: TouchEvent;
    new(type: string, eventInitDict?: TouchEventInit): TouchEvent;
    isInstance: IsInstance<TouchEvent>;
};

interface TouchEventHandlersEventMap {
    "touchcancel": Event;
    "touchend": Event;
    "touchmove": Event;
    "touchstart": Event;
}

interface TouchEventHandlers {
    ontouchcancel: ((this: TouchEventHandlers, ev: Event) => any) | null;
    ontouchend: ((this: TouchEventHandlers, ev: Event) => any) | null;
    ontouchmove: ((this: TouchEventHandlers, ev: Event) => any) | null;
    ontouchstart: ((this: TouchEventHandlers, ev: Event) => any) | null;
    addEventListener<K extends keyof TouchEventHandlersEventMap>(type: K, listener: (this: TouchEventHandlers, ev: TouchEventHandlersEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TouchEventHandlersEventMap>(type: K, listener: (this: TouchEventHandlers, ev: TouchEventHandlersEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface TouchList {
    readonly length: number;
    item(index: number): Touch | null;
    [index: number]: Touch;
}

declare var TouchList: {
    prototype: TouchList;
    new(): TouchList;
    isInstance: IsInstance<TouchList>;
};

interface TrackEvent extends Event {
    readonly track: VideoTrack | AudioTrack | TextTrack | null;
}

declare var TrackEvent: {
    prototype: TrackEvent;
    new(type: string, eventInitDict?: TrackEventInit): TrackEvent;
    isInstance: IsInstance<TrackEvent>;
};

interface TransformStream {
    readonly readable: ReadableStream;
    readonly writable: WritableStream;
}

declare var TransformStream: {
    prototype: TransformStream;
    new(transformer?: any, writableStrategy?: QueuingStrategy, readableStrategy?: QueuingStrategy): TransformStream;
    isInstance: IsInstance<TransformStream>;
};

interface TransformStreamDefaultController {
    readonly desiredSize: number | null;
    enqueue(chunk?: any): void;
    error(reason?: any): void;
    terminate(): void;
}

declare var TransformStreamDefaultController: {
    prototype: TransformStreamDefaultController;
    new(): TransformStreamDefaultController;
    isInstance: IsInstance<TransformStreamDefaultController>;
};

interface TransitionEvent extends Event {
    readonly elapsedTime: number;
    readonly propertyName: string;
    readonly pseudoElement: string;
}

declare var TransitionEvent: {
    prototype: TransitionEvent;
    new(type: string, eventInitDict?: TransitionEventInit): TransitionEvent;
    isInstance: IsInstance<TransitionEvent>;
};

interface TreeColumn {
    readonly columns: TreeColumns | null;
    readonly cycler: boolean;
    readonly editable: boolean;
    readonly element: Element;
    readonly id: string;
    readonly index: number;
    readonly previousColumn: TreeColumn | null;
    readonly primary: boolean;
    readonly type: number;
    readonly width: number;
    readonly x: number;
    getNext(): TreeColumn | null;
    getPrevious(): TreeColumn | null;
    invalidate(): void;
    readonly TYPE_TEXT: 1;
    readonly TYPE_CHECKBOX: 2;
}

declare var TreeColumn: {
    prototype: TreeColumn;
    new(): TreeColumn;
    readonly TYPE_TEXT: 1;
    readonly TYPE_CHECKBOX: 2;
    isInstance: IsInstance<TreeColumn>;
};

interface TreeColumns {
    readonly count: number;
    readonly length: number;
    readonly tree: XULTreeElement | null;
    getColumnAt(index: number): TreeColumn | null;
    getColumnFor(element: Element | null): TreeColumn | null;
    getFirstColumn(): TreeColumn | null;
    getKeyColumn(): TreeColumn | null;
    getLastColumn(): TreeColumn | null;
    getNamedColumn(name: string): TreeColumn | null;
    getPrimaryColumn(): TreeColumn | null;
    getSortedColumn(): TreeColumn | null;
    invalidateColumns(): void;
    [index: number]: TreeColumn;
}

declare var TreeColumns: {
    prototype: TreeColumns;
    new(): TreeColumns;
    isInstance: IsInstance<TreeColumns>;
};

interface TreeContentView extends TreeView {
    getIndexOfItem(item: Element | null): number;
    getItemAtIndex(row: number): Element | null;
}

declare var TreeContentView: {
    prototype: TreeContentView;
    new(): TreeContentView;
    readonly DROP_BEFORE: -1;
    readonly DROP_ON: 0;
    readonly DROP_AFTER: 1;
    isInstance: IsInstance<TreeContentView>;
};

interface TreeView {
    readonly rowCount: number;
    selection: nsITreeSelection | null;
    canDrop(row: number, orientation: number, dataTransfer: DataTransfer | null): boolean;
    cycleCell(row: number, column: TreeColumn): void;
    cycleHeader(column: TreeColumn): void;
    drop(row: number, orientation: number, dataTransfer: DataTransfer | null): void;
    getCellProperties(row: number, column: TreeColumn): string;
    getCellText(row: number, column: TreeColumn): string;
    getCellValue(row: number, column: TreeColumn): string;
    getColumnProperties(column: TreeColumn): string;
    getImageSrc(row: number, column: TreeColumn): string;
    getLevel(row: number): number;
    getParentIndex(row: number): number;
    getRowProperties(row: number): string;
    hasNextSibling(row: number, afterIndex: number): boolean;
    isContainer(row: number): boolean;
    isContainerEmpty(row: number): boolean;
    isContainerOpen(row: number): boolean;
    isEditable(row: number, column: TreeColumn): boolean;
    isSeparator(row: number): boolean;
    isSorted(): boolean;
    selectionChanged(): void;
    setCellText(row: number, column: TreeColumn, value: string): void;
    setCellValue(row: number, column: TreeColumn, value: string): void;
    setTree(tree: XULTreeElement | null): void;
    toggleOpenState(row: number): void;
    readonly DROP_BEFORE: -1;
    readonly DROP_ON: 0;
    readonly DROP_AFTER: 1;
}

interface TreeWalker {
    currentNode: Node;
    readonly filter: NodeFilter | null;
    readonly root: Node;
    readonly whatToShow: number;
    firstChild(): Node | null;
    lastChild(): Node | null;
    nextNode(): Node | null;
    nextSibling(): Node | null;
    parentNode(): Node | null;
    previousNode(): Node | null;
    previousSibling(): Node | null;
}

declare var TreeWalker: {
    prototype: TreeWalker;
    new(): TreeWalker;
    isInstance: IsInstance<TreeWalker>;
};

interface TrustedHTML {
    toJSON(): string;
    toString(): string;
}

declare var TrustedHTML: {
    prototype: TrustedHTML;
    new(): TrustedHTML;
    isInstance: IsInstance<TrustedHTML>;
};

interface TrustedScript {
    toJSON(): string;
    toString(): string;
}

declare var TrustedScript: {
    prototype: TrustedScript;
    new(): TrustedScript;
    isInstance: IsInstance<TrustedScript>;
};

interface TrustedScriptURL {
    toJSON(): string;
    toString(): string;
}

declare var TrustedScriptURL: {
    prototype: TrustedScriptURL;
    new(): TrustedScriptURL;
    isInstance: IsInstance<TrustedScriptURL>;
};

interface TrustedTypePolicy {
    readonly name: string;
    createHTML(input: string, ...arguments: any[]): TrustedHTML;
    createScript(input: string, ...arguments: any[]): TrustedScript;
    createScriptURL(input: string, ...arguments: any[]): TrustedScriptURL;
}

declare var TrustedTypePolicy: {
    prototype: TrustedTypePolicy;
    new(): TrustedTypePolicy;
    isInstance: IsInstance<TrustedTypePolicy>;
};

interface TrustedTypePolicyFactory {
    readonly defaultPolicy: TrustedTypePolicy | null;
    readonly emptyHTML: TrustedHTML;
    readonly emptyScript: TrustedScript;
    createPolicy(policyName: string, policyOptions?: TrustedTypePolicyOptions): TrustedTypePolicy;
    getAttributeType(tagName: string, attribute: string, elementNs?: string, attrNs?: string): string | null;
    getPropertyType(tagName: string, property: string, elementNs?: string): string | null;
    isHTML(value: any): boolean;
    isScript(value: any): boolean;
    isScriptURL(value: any): boolean;
}

declare var TrustedTypePolicyFactory: {
    prototype: TrustedTypePolicyFactory;
    new(): TrustedTypePolicyFactory;
    isInstance: IsInstance<TrustedTypePolicyFactory>;
};

interface UDPMessageEvent extends Event {
    readonly data: any;
    readonly remoteAddress: string;
    readonly remotePort: number;
}

declare var UDPMessageEvent: {
    prototype: UDPMessageEvent;
    new(type: string, eventInitDict?: UDPMessageEventInit): UDPMessageEvent;
    isInstance: IsInstance<UDPMessageEvent>;
};

interface UDPSocketEventMap {
    "message": Event;
}

interface UDPSocket extends EventTarget {
    readonly addressReuse: boolean;
    readonly closed: Promise<undefined>;
    readonly localAddress: string | null;
    readonly localPort: number | null;
    readonly loopback: boolean;
    onmessage: ((this: UDPSocket, ev: Event) => any) | null;
    readonly opened: Promise<undefined>;
    readonly readyState: SocketReadyState;
    readonly remoteAddress: string | null;
    readonly remotePort: number | null;
    close(): Promise<void>;
    joinMulticastGroup(multicastGroupAddress: string): void;
    leaveMulticastGroup(multicastGroupAddress: string): void;
    send(data: string | Blob | ArrayBuffer | ArrayBufferView, remoteAddress?: string | null, remotePort?: number | null): boolean;
    addEventListener<K extends keyof UDPSocketEventMap>(type: K, listener: (this: UDPSocket, ev: UDPSocketEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof UDPSocketEventMap>(type: K, listener: (this: UDPSocket, ev: UDPSocketEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var UDPSocket: {
    prototype: UDPSocket;
    new(options?: UDPOptions): UDPSocket;
    isInstance: IsInstance<UDPSocket>;
};

interface UIEvent extends Event {
    readonly detail: number;
    readonly layerX: number;
    readonly layerY: number;
    readonly rangeOffset: number;
    readonly rangeParent: Node | null;
    readonly view: WindowProxy | null;
    readonly which: number;
    initUIEvent(aType: string, aCanBubble?: boolean, aCancelable?: boolean, aView?: Window | null, aDetail?: number): void;
    readonly SCROLL_PAGE_UP: -32768;
    readonly SCROLL_PAGE_DOWN: 32768;
}

declare var UIEvent: {
    prototype: UIEvent;
    new(type: string, eventInitDict?: UIEventInit): UIEvent;
    readonly SCROLL_PAGE_UP: -32768;
    readonly SCROLL_PAGE_DOWN: 32768;
    isInstance: IsInstance<UIEvent>;
};

interface URI {
}

interface URL {
    readonly URI: URI;
    hash: string;
    host: string;
    hostname: string;
    href: string;
    toString(): string;
    readonly origin: string;
    password: string;
    pathname: string;
    port: string;
    protocol: string;
    search: string;
    readonly searchParams: URLSearchParams;
    username: string;
    toJSON(): string;
}

declare var URL: {
    prototype: URL;
    new(url: string, base?: string): URL;
    isInstance: IsInstance<URL>;
    canParse(url: string, base?: string): boolean;
    createObjectURL(blob: Blob): string;
    createObjectURL(source: MediaSource): string;
    fromURI(uri: URI): URL;
    isValidObjectURL(url: string): boolean;
    parse(url: string, base?: string): URL | null;
    revokeObjectURL(url: string): void;
};

type webkitURL = URL;
declare var webkitURL: typeof URL;

interface URLSearchParams {
    readonly size: number;
    append(name: string, value: string): void;
    delete(name: string, value?: string): void;
    get(name: string): string | null;
    getAll(name: string): string[];
    has(name: string, value?: string): boolean;
    set(name: string, value: string): void;
    sort(): void;
    toString(): string;
    forEach(callbackfn: (value: string, key: string, parent: URLSearchParams) => void, thisArg?: any): void;
}

declare var URLSearchParams: {
    prototype: URLSearchParams;
    new(init?: string[][] | Record<string, string> | string): URLSearchParams;
    isInstance: IsInstance<URLSearchParams>;
};

interface UniFFIPointer {
}

declare var UniFFIPointer: {
    prototype: UniFFIPointer;
    new(): UniFFIPointer;
    isInstance: IsInstance<UniFFIPointer>;
};

interface UserActivation {
    readonly hasBeenActive: boolean;
    readonly isActive: boolean;
}

declare var UserActivation: {
    prototype: UserActivation;
    new(): UserActivation;
    isInstance: IsInstance<UserActivation>;
};

interface UserProximityEvent extends Event {
    readonly near: boolean;
}

declare var UserProximityEvent: {
    prototype: UserProximityEvent;
    new(type: string, eventInitDict?: UserProximityEventInit): UserProximityEvent;
    isInstance: IsInstance<UserProximityEvent>;
};

/** Available only in secure contexts. */
interface VRDisplay extends EventTarget {
    readonly capabilities: VRDisplayCapabilities;
    depthFar: number;
    depthNear: number;
    readonly displayId: number;
    readonly displayName: string;
    groupMask: number;
    readonly isConnected: boolean;
    readonly isPresenting: boolean;
    readonly presentingGroups: number;
    readonly stageParameters: VRStageParameters | null;
    cancelAnimationFrame(handle: number): void;
    exitPresent(): Promise<void>;
    getEyeParameters(whichEye: VREye): VREyeParameters;
    getFrameData(frameData: VRFrameData): boolean;
    getLayers(): VRLayer[];
    getPose(): VRPose;
    requestAnimationFrame(callback: FrameRequestCallback): number;
    requestPresent(layers: VRLayer[]): Promise<void>;
    resetPose(): void;
    submitFrame(): void;
}

declare var VRDisplay: {
    prototype: VRDisplay;
    new(): VRDisplay;
    isInstance: IsInstance<VRDisplay>;
};

/** Available only in secure contexts. */
interface VRDisplayCapabilities {
    readonly canPresent: boolean;
    readonly hasExternalDisplay: boolean;
    readonly hasOrientation: boolean;
    readonly hasPosition: boolean;
    readonly maxLayers: number;
}

declare var VRDisplayCapabilities: {
    prototype: VRDisplayCapabilities;
    new(): VRDisplayCapabilities;
    isInstance: IsInstance<VRDisplayCapabilities>;
};

/** Available only in secure contexts. */
interface VRDisplayEvent extends Event {
    readonly display: VRDisplay;
    readonly reason: VRDisplayEventReason | null;
}

declare var VRDisplayEvent: {
    prototype: VRDisplayEvent;
    new(type: string, eventInitDict: VRDisplayEventInit): VRDisplayEvent;
    isInstance: IsInstance<VRDisplayEvent>;
};

/** Available only in secure contexts. */
interface VREyeParameters {
    readonly fieldOfView: VRFieldOfView;
    readonly offset: Float32Array;
    readonly renderHeight: number;
    readonly renderWidth: number;
}

declare var VREyeParameters: {
    prototype: VREyeParameters;
    new(): VREyeParameters;
    isInstance: IsInstance<VREyeParameters>;
};

/** Available only in secure contexts. */
interface VRFieldOfView {
    readonly downDegrees: number;
    readonly leftDegrees: number;
    readonly rightDegrees: number;
    readonly upDegrees: number;
}

declare var VRFieldOfView: {
    prototype: VRFieldOfView;
    new(): VRFieldOfView;
    isInstance: IsInstance<VRFieldOfView>;
};

/** Available only in secure contexts. */
interface VRFrameData {
    readonly leftProjectionMatrix: Float32Array;
    readonly leftViewMatrix: Float32Array;
    readonly pose: VRPose;
    readonly rightProjectionMatrix: Float32Array;
    readonly rightViewMatrix: Float32Array;
    readonly timestamp: DOMHighResTimeStamp;
}

declare var VRFrameData: {
    prototype: VRFrameData;
    new(): VRFrameData;
    isInstance: IsInstance<VRFrameData>;
};

interface VRMockController {
    axisCount: number;
    buttonCount: number;
    capAngularAcceleration: boolean;
    capLinearAcceleration: boolean;
    capOrientation: boolean;
    capPosition: boolean;
    hand: GamepadHand;
    hapticCount: number;
    clear(): void;
    create(): void;
    setAxisValue(axisIdx: number, value: number): void;
    setButtonPressed(buttonIdx: number, pressed: boolean): void;
    setButtonTouched(buttonIdx: number, touched: boolean): void;
    setButtonTrigger(buttonIdx: number, trigger: number): void;
    setPose(position: Float32Array | null, linearVelocity: Float32Array | null, linearAcceleration: Float32Array | null, orientation: Float32Array | null, angularVelocity: Float32Array | null, angularAcceleration: Float32Array | null): void;
}

declare var VRMockController: {
    prototype: VRMockController;
    new(): VRMockController;
    isInstance: IsInstance<VRMockController>;
};

interface VRMockDisplay {
    capAngularAcceleration: boolean;
    capExternal: boolean;
    capLinearAcceleration: boolean;
    capMountDetection: boolean;
    capOrientation: boolean;
    capPosition: boolean;
    capPositionEmulated: boolean;
    capPresent: boolean;
    capStageParameters: boolean;
    create(): void;
    setConnected(connected: boolean): void;
    setEyeFOV(eye: VREye, upDegree: number, rightDegree: number, downDegree: number, leftDegree: number): void;
    setEyeOffset(eye: VREye, offsetX: number, offsetY: number, offsetZ: number): void;
    setEyeResolution(renderWidth: number, renderHeight: number): void;
    setMounted(mounted: boolean): void;
    setPose(position: Float32Array | null, linearVelocity: Float32Array | null, linearAcceleration: Float32Array | null, orientation: Float32Array | null, angularVelocity: Float32Array | null, angularAcceleration: Float32Array | null): void;
    setSittingToStandingTransform(sittingToStandingTransform: Float32Array): void;
    setStageSize(width: number, height: number): void;
}

declare var VRMockDisplay: {
    prototype: VRMockDisplay;
    new(): VRMockDisplay;
    isInstance: IsInstance<VRMockDisplay>;
};

/** Available only in secure contexts. */
interface VRPose {
    readonly angularAcceleration: Float32Array | null;
    readonly angularVelocity: Float32Array | null;
    readonly linearAcceleration: Float32Array | null;
    readonly linearVelocity: Float32Array | null;
    readonly orientation: Float32Array | null;
    readonly position: Float32Array | null;
}

declare var VRPose: {
    prototype: VRPose;
    new(): VRPose;
    isInstance: IsInstance<VRPose>;
};

interface VRServiceTest {
    acknowledgeFrame(): void;
    captureFrame(): void;
    clearAll(): void;
    commit(): void;
    end(): void;
    getVRController(controllerIdx: number): VRMockController;
    getVRDisplay(): VRMockDisplay;
    rejectFrame(): void;
    reset(): Promise<void>;
    run(): Promise<void>;
    startTimer(): void;
    stopTimer(): void;
    timeout(duration: number): void;
    wait(duration: number): void;
    waitHapticIntensity(controllerIdx: number, hapticIdx: number, intensity: number): void;
    waitPresentationEnd(): void;
    waitPresentationStart(): void;
    waitSubmit(): void;
}

declare var VRServiceTest: {
    prototype: VRServiceTest;
    new(): VRServiceTest;
    isInstance: IsInstance<VRServiceTest>;
};

/** Available only in secure contexts. */
interface VRStageParameters {
    readonly sittingToStandingTransform: Float32Array;
    readonly sizeX: number;
    readonly sizeZ: number;
}

declare var VRStageParameters: {
    prototype: VRStageParameters;
    new(): VRStageParameters;
    isInstance: IsInstance<VRStageParameters>;
};

interface VTTCue extends TextTrackCue {
    align: AlignSetting;
    readonly computedLine: number;
    readonly computedPosition: number;
    readonly computedPositionAlign: PositionAlignSetting;
    displayState: HTMLDivElement | null;
    readonly getActive: boolean;
    readonly hasBeenReset: boolean;
    line: number | AutoKeyword;
    lineAlign: LineAlignSetting;
    position: number | AutoKeyword;
    positionAlign: PositionAlignSetting;
    region: VTTRegion | null;
    size: number;
    snapToLines: boolean;
    text: string;
    vertical: DirectionSetting;
    getCueAsHTML(): DocumentFragment;
    addEventListener<K extends keyof TextTrackCueEventMap>(type: K, listener: (this: VTTCue, ev: TextTrackCueEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TextTrackCueEventMap>(type: K, listener: (this: VTTCue, ev: TextTrackCueEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var VTTCue: {
    prototype: VTTCue;
    new(startTime: number, endTime: number, text: string): VTTCue;
    isInstance: IsInstance<VTTCue>;
};

interface VTTRegion {
    id: string;
    lines: number;
    regionAnchorX: number;
    regionAnchorY: number;
    scroll: ScrollSetting;
    viewportAnchorX: number;
    viewportAnchorY: number;
    width: number;
}

declare var VTTRegion: {
    prototype: VTTRegion;
    new(): VTTRegion;
    isInstance: IsInstance<VTTRegion>;
};

interface ValidityState {
    readonly badInput: boolean;
    readonly customError: boolean;
    readonly patternMismatch: boolean;
    readonly rangeOverflow: boolean;
    readonly rangeUnderflow: boolean;
    readonly stepMismatch: boolean;
    readonly tooLong: boolean;
    readonly tooShort: boolean;
    readonly typeMismatch: boolean;
    readonly valid: boolean;
    readonly valueMissing: boolean;
}

declare var ValidityState: {
    prototype: ValidityState;
    new(): ValidityState;
    isInstance: IsInstance<ValidityState>;
};

interface VideoColorSpace {
    readonly fullRange: boolean | null;
    readonly matrix: VideoMatrixCoefficients | null;
    readonly primaries: VideoColorPrimaries | null;
    readonly transfer: VideoTransferCharacteristics | null;
    toJSON(): any;
}

declare var VideoColorSpace: {
    prototype: VideoColorSpace;
    new(init?: VideoColorSpaceInit): VideoColorSpace;
    isInstance: IsInstance<VideoColorSpace>;
};

interface VideoDecoderEventMap {
    "dequeue": Event;
}

/** Available only in secure contexts. */
interface VideoDecoder extends EventTarget {
    readonly decodeQueueSize: number;
    ondequeue: ((this: VideoDecoder, ev: Event) => any) | null;
    readonly state: CodecState;
    close(): void;
    configure(config: VideoDecoderConfig): void;
    decode(chunk: EncodedVideoChunk): void;
    flush(): Promise<void>;
    reset(): void;
    addEventListener<K extends keyof VideoDecoderEventMap>(type: K, listener: (this: VideoDecoder, ev: VideoDecoderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof VideoDecoderEventMap>(type: K, listener: (this: VideoDecoder, ev: VideoDecoderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var VideoDecoder: {
    prototype: VideoDecoder;
    new(init: VideoDecoderInit): VideoDecoder;
    isInstance: IsInstance<VideoDecoder>;
    isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport>;
};

interface VideoEncoderEventMap {
    "dequeue": Event;
}

/** Available only in secure contexts. */
interface VideoEncoder extends EventTarget {
    readonly encodeQueueSize: number;
    ondequeue: ((this: VideoEncoder, ev: Event) => any) | null;
    readonly state: CodecState;
    close(): void;
    configure(config: VideoEncoderConfig): void;
    encode(frame: VideoFrame, options?: VideoEncoderEncodeOptions): void;
    flush(): Promise<void>;
    reset(): void;
    addEventListener<K extends keyof VideoEncoderEventMap>(type: K, listener: (this: VideoEncoder, ev: VideoEncoderEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof VideoEncoderEventMap>(type: K, listener: (this: VideoEncoder, ev: VideoEncoderEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var VideoEncoder: {
    prototype: VideoEncoder;
    new(init: VideoEncoderInit): VideoEncoder;
    isInstance: IsInstance<VideoEncoder>;
    isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport>;
};

interface VideoFrame {
    readonly codedHeight: number;
    readonly codedRect: DOMRectReadOnly | null;
    readonly codedWidth: number;
    readonly colorSpace: VideoColorSpace;
    readonly displayHeight: number;
    readonly displayWidth: number;
    readonly duration: number | null;
    readonly format: VideoPixelFormat | null;
    readonly timestamp: number;
    readonly visibleRect: DOMRectReadOnly | null;
    allocationSize(options?: VideoFrameCopyToOptions): number;
    clone(): VideoFrame;
    close(): void;
    copyTo(destination: ArrayBufferView | ArrayBuffer, options?: VideoFrameCopyToOptions): Promise<PlaneLayout[]>;
}

declare var VideoFrame: {
    prototype: VideoFrame;
    new(imageElement: HTMLImageElement, init?: VideoFrameInit): VideoFrame;
    new(svgImageElement: SVGImageElement, init?: VideoFrameInit): VideoFrame;
    new(canvasElement: HTMLCanvasElement, init?: VideoFrameInit): VideoFrame;
    new(videoElement: HTMLVideoElement, init?: VideoFrameInit): VideoFrame;
    new(offscreenCanvas: OffscreenCanvas, init?: VideoFrameInit): VideoFrame;
    new(imageBitmap: ImageBitmap, init?: VideoFrameInit): VideoFrame;
    new(videoFrame: VideoFrame, init?: VideoFrameInit): VideoFrame;
    new(bufferView: ArrayBufferView, init: VideoFrameBufferInit): VideoFrame;
    new(buffer: ArrayBuffer, init: VideoFrameBufferInit): VideoFrame;
    isInstance: IsInstance<VideoFrame>;
};

interface VideoPlaybackQuality {
    readonly creationTime: DOMHighResTimeStamp;
    readonly droppedVideoFrames: number;
    readonly totalVideoFrames: number;
}

declare var VideoPlaybackQuality: {
    prototype: VideoPlaybackQuality;
    new(): VideoPlaybackQuality;
    isInstance: IsInstance<VideoPlaybackQuality>;
};

interface VideoTrack {
    readonly id: string;
    readonly kind: string;
    readonly label: string;
    readonly language: string;
    selected: boolean;
}

declare var VideoTrack: {
    prototype: VideoTrack;
    new(): VideoTrack;
    isInstance: IsInstance<VideoTrack>;
};

interface VideoTrackListEventMap {
    "addtrack": Event;
    "change": Event;
    "removetrack": Event;
}

interface VideoTrackList extends EventTarget {
    readonly length: number;
    onaddtrack: ((this: VideoTrackList, ev: Event) => any) | null;
    onchange: ((this: VideoTrackList, ev: Event) => any) | null;
    onremovetrack: ((this: VideoTrackList, ev: Event) => any) | null;
    readonly selectedIndex: number;
    getTrackById(id: string): VideoTrack | null;
    addEventListener<K extends keyof VideoTrackListEventMap>(type: K, listener: (this: VideoTrackList, ev: VideoTrackListEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof VideoTrackListEventMap>(type: K, listener: (this: VideoTrackList, ev: VideoTrackListEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [index: number]: VideoTrack;
}

declare var VideoTrackList: {
    prototype: VideoTrackList;
    new(): VideoTrackList;
    isInstance: IsInstance<VideoTrackList>;
};

interface VisualViewportEventMap {
    "resize": Event;
    "scroll": Event;
}

interface VisualViewport extends EventTarget {
    readonly height: number;
    readonly offsetLeft: number;
    readonly offsetTop: number;
    onresize: ((this: VisualViewport, ev: Event) => any) | null;
    onscroll: ((this: VisualViewport, ev: Event) => any) | null;
    readonly pageLeft: number;
    readonly pageTop: number;
    readonly scale: number;
    readonly width: number;
    addEventListener<K extends keyof VisualViewportEventMap>(type: K, listener: (this: VisualViewport, ev: VisualViewportEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof VisualViewportEventMap>(type: K, listener: (this: VisualViewport, ev: VisualViewportEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var VisualViewport: {
    prototype: VisualViewport;
    new(): VisualViewport;
    isInstance: IsInstance<VisualViewport>;
};

interface WEBGL_color_buffer_float {
    readonly RGBA32F_EXT: 0x8814;
    readonly RGB32F_EXT: 0x8815;
    readonly FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT: 0x8211;
    readonly UNSIGNED_NORMALIZED_EXT: 0x8C17;
}

interface WEBGL_compressed_texture_astc {
    getSupportedProfiles(): string[] | null;
    readonly COMPRESSED_RGBA_ASTC_4x4_KHR: 0x93B0;
    readonly COMPRESSED_RGBA_ASTC_5x4_KHR: 0x93B1;
    readonly COMPRESSED_RGBA_ASTC_5x5_KHR: 0x93B2;
    readonly COMPRESSED_RGBA_ASTC_6x5_KHR: 0x93B3;
    readonly COMPRESSED_RGBA_ASTC_6x6_KHR: 0x93B4;
    readonly COMPRESSED_RGBA_ASTC_8x5_KHR: 0x93B5;
    readonly COMPRESSED_RGBA_ASTC_8x6_KHR: 0x93B6;
    readonly COMPRESSED_RGBA_ASTC_8x8_KHR: 0x93B7;
    readonly COMPRESSED_RGBA_ASTC_10x5_KHR: 0x93B8;
    readonly COMPRESSED_RGBA_ASTC_10x6_KHR: 0x93B9;
    readonly COMPRESSED_RGBA_ASTC_10x8_KHR: 0x93BA;
    readonly COMPRESSED_RGBA_ASTC_10x10_KHR: 0x93BB;
    readonly COMPRESSED_RGBA_ASTC_12x10_KHR: 0x93BC;
    readonly COMPRESSED_RGBA_ASTC_12x12_KHR: 0x93BD;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR: 0x93D0;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR: 0x93D1;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR: 0x93D2;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR: 0x93D3;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR: 0x93D4;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR: 0x93D5;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR: 0x93D6;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR: 0x93D7;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR: 0x93D8;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR: 0x93D9;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR: 0x93DA;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR: 0x93DB;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR: 0x93DC;
    readonly COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: 0x93DD;
}

interface WEBGL_compressed_texture_etc {
    readonly COMPRESSED_R11_EAC: 0x9270;
    readonly COMPRESSED_SIGNED_R11_EAC: 0x9271;
    readonly COMPRESSED_RG11_EAC: 0x9272;
    readonly COMPRESSED_SIGNED_RG11_EAC: 0x9273;
    readonly COMPRESSED_RGB8_ETC2: 0x9274;
    readonly COMPRESSED_SRGB8_ETC2: 0x9275;
    readonly COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: 0x9276;
    readonly COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: 0x9277;
    readonly COMPRESSED_RGBA8_ETC2_EAC: 0x9278;
    readonly COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: 0x9279;
}

interface WEBGL_compressed_texture_etc1 {
    readonly COMPRESSED_RGB_ETC1_WEBGL: 0x8D64;
}

interface WEBGL_compressed_texture_pvrtc {
    readonly COMPRESSED_RGB_PVRTC_4BPPV1_IMG: 0x8C00;
    readonly COMPRESSED_RGB_PVRTC_2BPPV1_IMG: 0x8C01;
    readonly COMPRESSED_RGBA_PVRTC_4BPPV1_IMG: 0x8C02;
    readonly COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: 0x8C03;
}

interface WEBGL_compressed_texture_s3tc {
    readonly COMPRESSED_RGB_S3TC_DXT1_EXT: 0x83F0;
    readonly COMPRESSED_RGBA_S3TC_DXT1_EXT: 0x83F1;
    readonly COMPRESSED_RGBA_S3TC_DXT3_EXT: 0x83F2;
    readonly COMPRESSED_RGBA_S3TC_DXT5_EXT: 0x83F3;
}

interface WEBGL_compressed_texture_s3tc_srgb {
    readonly COMPRESSED_SRGB_S3TC_DXT1_EXT: 0x8C4C;
    readonly COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: 0x8C4D;
    readonly COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: 0x8C4E;
    readonly COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: 0x8C4F;
}

interface WEBGL_debug_renderer_info {
    readonly UNMASKED_VENDOR_WEBGL: 0x9245;
    readonly UNMASKED_RENDERER_WEBGL: 0x9246;
}

interface WEBGL_debug_shaders {
    getTranslatedShaderSource(shader: WebGLShader): string;
}

interface WEBGL_depth_texture {
    readonly UNSIGNED_INT_24_8_WEBGL: 0x84FA;
}

interface WEBGL_draw_buffers {
    drawBuffersWEBGL(buffers: GLenum[]): void;
    readonly COLOR_ATTACHMENT0_WEBGL: 0x8CE0;
    readonly COLOR_ATTACHMENT1_WEBGL: 0x8CE1;
    readonly COLOR_ATTACHMENT2_WEBGL: 0x8CE2;
    readonly COLOR_ATTACHMENT3_WEBGL: 0x8CE3;
    readonly COLOR_ATTACHMENT4_WEBGL: 0x8CE4;
    readonly COLOR_ATTACHMENT5_WEBGL: 0x8CE5;
    readonly COLOR_ATTACHMENT6_WEBGL: 0x8CE6;
    readonly COLOR_ATTACHMENT7_WEBGL: 0x8CE7;
    readonly COLOR_ATTACHMENT8_WEBGL: 0x8CE8;
    readonly COLOR_ATTACHMENT9_WEBGL: 0x8CE9;
    readonly COLOR_ATTACHMENT10_WEBGL: 0x8CEA;
    readonly COLOR_ATTACHMENT11_WEBGL: 0x8CEB;
    readonly COLOR_ATTACHMENT12_WEBGL: 0x8CEC;
    readonly COLOR_ATTACHMENT13_WEBGL: 0x8CED;
    readonly COLOR_ATTACHMENT14_WEBGL: 0x8CEE;
    readonly COLOR_ATTACHMENT15_WEBGL: 0x8CEF;
    readonly DRAW_BUFFER0_WEBGL: 0x8825;
    readonly DRAW_BUFFER1_WEBGL: 0x8826;
    readonly DRAW_BUFFER2_WEBGL: 0x8827;
    readonly DRAW_BUFFER3_WEBGL: 0x8828;
    readonly DRAW_BUFFER4_WEBGL: 0x8829;
    readonly DRAW_BUFFER5_WEBGL: 0x882A;
    readonly DRAW_BUFFER6_WEBGL: 0x882B;
    readonly DRAW_BUFFER7_WEBGL: 0x882C;
    readonly DRAW_BUFFER8_WEBGL: 0x882D;
    readonly DRAW_BUFFER9_WEBGL: 0x882E;
    readonly DRAW_BUFFER10_WEBGL: 0x882F;
    readonly DRAW_BUFFER11_WEBGL: 0x8830;
    readonly DRAW_BUFFER12_WEBGL: 0x8831;
    readonly DRAW_BUFFER13_WEBGL: 0x8832;
    readonly DRAW_BUFFER14_WEBGL: 0x8833;
    readonly DRAW_BUFFER15_WEBGL: 0x8834;
    readonly MAX_COLOR_ATTACHMENTS_WEBGL: 0x8CDF;
    readonly MAX_DRAW_BUFFERS_WEBGL: 0x8824;
}

interface WEBGL_explicit_present {
    present(): void;
}

interface WEBGL_lose_context {
    loseContext(): void;
    restoreContext(): void;
}

interface WEBGL_provoking_vertex {
    provokingVertexWEBGL(provokeMode: GLenum): void;
    readonly FIRST_VERTEX_CONVENTION_WEBGL: 0x8E4D;
    readonly LAST_VERTEX_CONVENTION_WEBGL: 0x8E4E;
    readonly PROVOKING_VERTEX_WEBGL: 0x8E4F;
}

/** Available only in secure contexts. */
interface WakeLock {
    request(type?: WakeLockType): Promise<WakeLockSentinel>;
}

declare var WakeLock: {
    prototype: WakeLock;
    new(): WakeLock;
    isInstance: IsInstance<WakeLock>;
};

interface WakeLockSentinelEventMap {
    "release": Event;
}

/** Available only in secure contexts. */
interface WakeLockSentinel extends EventTarget {
    onrelease: ((this: WakeLockSentinel, ev: Event) => any) | null;
    readonly released: boolean;
    readonly type: WakeLockType;
    release(): Promise<void>;
    addEventListener<K extends keyof WakeLockSentinelEventMap>(type: K, listener: (this: WakeLockSentinel, ev: WakeLockSentinelEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WakeLockSentinelEventMap>(type: K, listener: (this: WakeLockSentinel, ev: WakeLockSentinelEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var WakeLockSentinel: {
    prototype: WakeLockSentinel;
    new(): WakeLockSentinel;
    isInstance: IsInstance<WakeLockSentinel>;
};

interface WaveShaperNode extends AudioNode, AudioNodePassThrough {
    curve: Float32Array | null;
    oversample: OverSampleType;
}

declare var WaveShaperNode: {
    prototype: WaveShaperNode;
    new(context: BaseAudioContext, options?: WaveShaperOptions): WaveShaperNode;
    isInstance: IsInstance<WaveShaperNode>;
};

interface WebBrowserPersistable {
    startPersistence(aContext: BrowsingContext | null, aRecv: nsIWebBrowserPersistDocumentReceiver): void;
}

interface WebExtensionContentScript extends MozDocumentMatcher {
    readonly cssPaths: string[];
    readonly jsPaths: string[];
    readonly runAt: ContentScriptRunAt;
    readonly world: ContentScriptExecutionWorld;
}

declare var WebExtensionContentScript: {
    prototype: WebExtensionContentScript;
    new(extension: WebExtensionPolicy, options: WebExtensionContentScriptInit): WebExtensionContentScript;
    isInstance: IsInstance<WebExtensionContentScript>;
};

interface WebExtensionPolicy {
    active: boolean;
    allowedOrigins: MatchPatternSet;
    readonly baseCSP: string;
    readonly baseURL: string;
    readonly browsingContextGroupId: number;
    readonly contentScripts: WebExtensionContentScript[];
    readonly extensionPageCSP: string;
    readonly id: string;
    ignoreQuarantine: boolean;
    readonly isPrivileged: boolean;
    readonly manifestVersion: number;
    readonly mozExtensionHostname: string;
    readonly name: string;
    permissions: string[];
    readonly privateBrowsingAllowed: boolean;
    readonly readyPromise: any;
    readonly temporarilyInstalled: boolean;
    readonly type: string;
    canAccessURI(uri: URI, explicit?: boolean, checkRestricted?: boolean, allowFilePermission?: boolean): boolean;
    canAccessWindow(window: WindowProxy): boolean;
    getURL(path?: string): string;
    hasPermission(permission: string): boolean;
    injectContentScripts(): void;
    isManifestBackgroundWorker(workerURL: string): boolean;
    isWebAccessiblePath(pathname: string): boolean;
    localize(unlocalizedText: string): string;
    quarantinedFromURI(uri: URI): boolean;
    registerContentScript(script: WebExtensionContentScript): void;
    sourceMayAccessPath(sourceURI: URI, pathname: string): boolean;
    unregisterContentScript(script: WebExtensionContentScript): void;
}

declare var WebExtensionPolicy: {
    prototype: WebExtensionPolicy;
    new(options: WebExtensionInit): WebExtensionPolicy;
    readonly backgroundServiceWorkerEnabled: boolean;
    readonly isExtensionProcess: boolean;
    isInstance: IsInstance<WebExtensionPolicy>;
    readonly quarantinedDomainsEnabled: boolean;
    readonly useRemoteWebExtensions: boolean;
    getActiveExtensions(): WebExtensionPolicy[];
    getByHostname(hostname: string): WebExtensionPolicy | null;
    getByID(id: string): WebExtensionPolicy | null;
    getByURI(uri: URI): WebExtensionPolicy | null;
    isQuarantinedURI(uri: URI): boolean;
    isRestrictedURI(uri: URI): boolean;
};

interface WebGL2RenderingContext extends WebGL2RenderingContextBase, WebGLRenderingContextBase {
}

declare var WebGL2RenderingContext: {
    prototype: WebGL2RenderingContext;
    new(): WebGL2RenderingContext;
    readonly READ_BUFFER: 0x0C02;
    readonly UNPACK_ROW_LENGTH: 0x0CF2;
    readonly UNPACK_SKIP_ROWS: 0x0CF3;
    readonly UNPACK_SKIP_PIXELS: 0x0CF4;
    readonly PACK_ROW_LENGTH: 0x0D02;
    readonly PACK_SKIP_ROWS: 0x0D03;
    readonly PACK_SKIP_PIXELS: 0x0D04;
    readonly COLOR: 0x1800;
    readonly DEPTH: 0x1801;
    readonly STENCIL: 0x1802;
    readonly RED: 0x1903;
    readonly RGB8: 0x8051;
    readonly RGBA8: 0x8058;
    readonly RGB10_A2: 0x8059;
    readonly TEXTURE_BINDING_3D: 0x806A;
    readonly UNPACK_SKIP_IMAGES: 0x806D;
    readonly UNPACK_IMAGE_HEIGHT: 0x806E;
    readonly TEXTURE_3D: 0x806F;
    readonly TEXTURE_WRAP_R: 0x8072;
    readonly MAX_3D_TEXTURE_SIZE: 0x8073;
    readonly UNSIGNED_INT_2_10_10_10_REV: 0x8368;
    readonly MAX_ELEMENTS_VERTICES: 0x80E8;
    readonly MAX_ELEMENTS_INDICES: 0x80E9;
    readonly TEXTURE_MIN_LOD: 0x813A;
    readonly TEXTURE_MAX_LOD: 0x813B;
    readonly TEXTURE_BASE_LEVEL: 0x813C;
    readonly TEXTURE_MAX_LEVEL: 0x813D;
    readonly MIN: 0x8007;
    readonly MAX: 0x8008;
    readonly DEPTH_COMPONENT24: 0x81A6;
    readonly MAX_TEXTURE_LOD_BIAS: 0x84FD;
    readonly TEXTURE_COMPARE_MODE: 0x884C;
    readonly TEXTURE_COMPARE_FUNC: 0x884D;
    readonly CURRENT_QUERY: 0x8865;
    readonly QUERY_RESULT: 0x8866;
    readonly QUERY_RESULT_AVAILABLE: 0x8867;
    readonly STREAM_READ: 0x88E1;
    readonly STREAM_COPY: 0x88E2;
    readonly STATIC_READ: 0x88E5;
    readonly STATIC_COPY: 0x88E6;
    readonly DYNAMIC_READ: 0x88E9;
    readonly DYNAMIC_COPY: 0x88EA;
    readonly MAX_DRAW_BUFFERS: 0x8824;
    readonly DRAW_BUFFER0: 0x8825;
    readonly DRAW_BUFFER1: 0x8826;
    readonly DRAW_BUFFER2: 0x8827;
    readonly DRAW_BUFFER3: 0x8828;
    readonly DRAW_BUFFER4: 0x8829;
    readonly DRAW_BUFFER5: 0x882A;
    readonly DRAW_BUFFER6: 0x882B;
    readonly DRAW_BUFFER7: 0x882C;
    readonly DRAW_BUFFER8: 0x882D;
    readonly DRAW_BUFFER9: 0x882E;
    readonly DRAW_BUFFER10: 0x882F;
    readonly DRAW_BUFFER11: 0x8830;
    readonly DRAW_BUFFER12: 0x8831;
    readonly DRAW_BUFFER13: 0x8832;
    readonly DRAW_BUFFER14: 0x8833;
    readonly DRAW_BUFFER15: 0x8834;
    readonly MAX_FRAGMENT_UNIFORM_COMPONENTS: 0x8B49;
    readonly MAX_VERTEX_UNIFORM_COMPONENTS: 0x8B4A;
    readonly SAMPLER_3D: 0x8B5F;
    readonly SAMPLER_2D_SHADOW: 0x8B62;
    readonly FRAGMENT_SHADER_DERIVATIVE_HINT: 0x8B8B;
    readonly PIXEL_PACK_BUFFER: 0x88EB;
    readonly PIXEL_UNPACK_BUFFER: 0x88EC;
    readonly PIXEL_PACK_BUFFER_BINDING: 0x88ED;
    readonly PIXEL_UNPACK_BUFFER_BINDING: 0x88EF;
    readonly FLOAT_MAT2x3: 0x8B65;
    readonly FLOAT_MAT2x4: 0x8B66;
    readonly FLOAT_MAT3x2: 0x8B67;
    readonly FLOAT_MAT3x4: 0x8B68;
    readonly FLOAT_MAT4x2: 0x8B69;
    readonly FLOAT_MAT4x3: 0x8B6A;
    readonly SRGB: 0x8C40;
    readonly SRGB8: 0x8C41;
    readonly SRGB8_ALPHA8: 0x8C43;
    readonly COMPARE_REF_TO_TEXTURE: 0x884E;
    readonly RGBA32F: 0x8814;
    readonly RGB32F: 0x8815;
    readonly RGBA16F: 0x881A;
    readonly RGB16F: 0x881B;
    readonly VERTEX_ATTRIB_ARRAY_INTEGER: 0x88FD;
    readonly MAX_ARRAY_TEXTURE_LAYERS: 0x88FF;
    readonly MIN_PROGRAM_TEXEL_OFFSET: 0x8904;
    readonly MAX_PROGRAM_TEXEL_OFFSET: 0x8905;
    readonly MAX_VARYING_COMPONENTS: 0x8B4B;
    readonly TEXTURE_2D_ARRAY: 0x8C1A;
    readonly TEXTURE_BINDING_2D_ARRAY: 0x8C1D;
    readonly R11F_G11F_B10F: 0x8C3A;
    readonly UNSIGNED_INT_10F_11F_11F_REV: 0x8C3B;
    readonly RGB9_E5: 0x8C3D;
    readonly UNSIGNED_INT_5_9_9_9_REV: 0x8C3E;
    readonly TRANSFORM_FEEDBACK_BUFFER_MODE: 0x8C7F;
    readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: 0x8C80;
    readonly TRANSFORM_FEEDBACK_VARYINGS: 0x8C83;
    readonly TRANSFORM_FEEDBACK_BUFFER_START: 0x8C84;
    readonly TRANSFORM_FEEDBACK_BUFFER_SIZE: 0x8C85;
    readonly TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: 0x8C88;
    readonly RASTERIZER_DISCARD: 0x8C89;
    readonly MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: 0x8C8A;
    readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: 0x8C8B;
    readonly INTERLEAVED_ATTRIBS: 0x8C8C;
    readonly SEPARATE_ATTRIBS: 0x8C8D;
    readonly TRANSFORM_FEEDBACK_BUFFER: 0x8C8E;
    readonly TRANSFORM_FEEDBACK_BUFFER_BINDING: 0x8C8F;
    readonly RGBA32UI: 0x8D70;
    readonly RGB32UI: 0x8D71;
    readonly RGBA16UI: 0x8D76;
    readonly RGB16UI: 0x8D77;
    readonly RGBA8UI: 0x8D7C;
    readonly RGB8UI: 0x8D7D;
    readonly RGBA32I: 0x8D82;
    readonly RGB32I: 0x8D83;
    readonly RGBA16I: 0x8D88;
    readonly RGB16I: 0x8D89;
    readonly RGBA8I: 0x8D8E;
    readonly RGB8I: 0x8D8F;
    readonly RED_INTEGER: 0x8D94;
    readonly RGB_INTEGER: 0x8D98;
    readonly RGBA_INTEGER: 0x8D99;
    readonly SAMPLER_2D_ARRAY: 0x8DC1;
    readonly SAMPLER_2D_ARRAY_SHADOW: 0x8DC4;
    readonly SAMPLER_CUBE_SHADOW: 0x8DC5;
    readonly UNSIGNED_INT_VEC2: 0x8DC6;
    readonly UNSIGNED_INT_VEC3: 0x8DC7;
    readonly UNSIGNED_INT_VEC4: 0x8DC8;
    readonly INT_SAMPLER_2D: 0x8DCA;
    readonly INT_SAMPLER_3D: 0x8DCB;
    readonly INT_SAMPLER_CUBE: 0x8DCC;
    readonly INT_SAMPLER_2D_ARRAY: 0x8DCF;
    readonly UNSIGNED_INT_SAMPLER_2D: 0x8DD2;
    readonly UNSIGNED_INT_SAMPLER_3D: 0x8DD3;
    readonly UNSIGNED_INT_SAMPLER_CUBE: 0x8DD4;
    readonly UNSIGNED_INT_SAMPLER_2D_ARRAY: 0x8DD7;
    readonly DEPTH_COMPONENT32F: 0x8CAC;
    readonly DEPTH32F_STENCIL8: 0x8CAD;
    readonly FLOAT_32_UNSIGNED_INT_24_8_REV: 0x8DAD;
    readonly FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: 0x8210;
    readonly FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: 0x8211;
    readonly FRAMEBUFFER_ATTACHMENT_RED_SIZE: 0x8212;
    readonly FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: 0x8213;
    readonly FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: 0x8214;
    readonly FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: 0x8215;
    readonly FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: 0x8216;
    readonly FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: 0x8217;
    readonly FRAMEBUFFER_DEFAULT: 0x8218;
    readonly UNSIGNED_INT_24_8: 0x84FA;
    readonly DEPTH24_STENCIL8: 0x88F0;
    readonly UNSIGNED_NORMALIZED: 0x8C17;
    readonly DRAW_FRAMEBUFFER_BINDING: 0x8CA6;
    readonly READ_FRAMEBUFFER: 0x8CA8;
    readonly DRAW_FRAMEBUFFER: 0x8CA9;
    readonly READ_FRAMEBUFFER_BINDING: 0x8CAA;
    readonly RENDERBUFFER_SAMPLES: 0x8CAB;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: 0x8CD4;
    readonly MAX_COLOR_ATTACHMENTS: 0x8CDF;
    readonly COLOR_ATTACHMENT1: 0x8CE1;
    readonly COLOR_ATTACHMENT2: 0x8CE2;
    readonly COLOR_ATTACHMENT3: 0x8CE3;
    readonly COLOR_ATTACHMENT4: 0x8CE4;
    readonly COLOR_ATTACHMENT5: 0x8CE5;
    readonly COLOR_ATTACHMENT6: 0x8CE6;
    readonly COLOR_ATTACHMENT7: 0x8CE7;
    readonly COLOR_ATTACHMENT8: 0x8CE8;
    readonly COLOR_ATTACHMENT9: 0x8CE9;
    readonly COLOR_ATTACHMENT10: 0x8CEA;
    readonly COLOR_ATTACHMENT11: 0x8CEB;
    readonly COLOR_ATTACHMENT12: 0x8CEC;
    readonly COLOR_ATTACHMENT13: 0x8CED;
    readonly COLOR_ATTACHMENT14: 0x8CEE;
    readonly COLOR_ATTACHMENT15: 0x8CEF;
    readonly FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: 0x8D56;
    readonly MAX_SAMPLES: 0x8D57;
    readonly HALF_FLOAT: 0x140B;
    readonly RG: 0x8227;
    readonly RG_INTEGER: 0x8228;
    readonly R8: 0x8229;
    readonly RG8: 0x822B;
    readonly R16F: 0x822D;
    readonly R32F: 0x822E;
    readonly RG16F: 0x822F;
    readonly RG32F: 0x8230;
    readonly R8I: 0x8231;
    readonly R8UI: 0x8232;
    readonly R16I: 0x8233;
    readonly R16UI: 0x8234;
    readonly R32I: 0x8235;
    readonly R32UI: 0x8236;
    readonly RG8I: 0x8237;
    readonly RG8UI: 0x8238;
    readonly RG16I: 0x8239;
    readonly RG16UI: 0x823A;
    readonly RG32I: 0x823B;
    readonly RG32UI: 0x823C;
    readonly VERTEX_ARRAY_BINDING: 0x85B5;
    readonly R8_SNORM: 0x8F94;
    readonly RG8_SNORM: 0x8F95;
    readonly RGB8_SNORM: 0x8F96;
    readonly RGBA8_SNORM: 0x8F97;
    readonly SIGNED_NORMALIZED: 0x8F9C;
    readonly COPY_READ_BUFFER: 0x8F36;
    readonly COPY_WRITE_BUFFER: 0x8F37;
    readonly COPY_READ_BUFFER_BINDING: 0x8F36;
    readonly COPY_WRITE_BUFFER_BINDING: 0x8F37;
    readonly UNIFORM_BUFFER: 0x8A11;
    readonly UNIFORM_BUFFER_BINDING: 0x8A28;
    readonly UNIFORM_BUFFER_START: 0x8A29;
    readonly UNIFORM_BUFFER_SIZE: 0x8A2A;
    readonly MAX_VERTEX_UNIFORM_BLOCKS: 0x8A2B;
    readonly MAX_FRAGMENT_UNIFORM_BLOCKS: 0x8A2D;
    readonly MAX_COMBINED_UNIFORM_BLOCKS: 0x8A2E;
    readonly MAX_UNIFORM_BUFFER_BINDINGS: 0x8A2F;
    readonly MAX_UNIFORM_BLOCK_SIZE: 0x8A30;
    readonly MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: 0x8A31;
    readonly MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: 0x8A33;
    readonly UNIFORM_BUFFER_OFFSET_ALIGNMENT: 0x8A34;
    readonly ACTIVE_UNIFORM_BLOCKS: 0x8A36;
    readonly UNIFORM_TYPE: 0x8A37;
    readonly UNIFORM_SIZE: 0x8A38;
    readonly UNIFORM_BLOCK_INDEX: 0x8A3A;
    readonly UNIFORM_OFFSET: 0x8A3B;
    readonly UNIFORM_ARRAY_STRIDE: 0x8A3C;
    readonly UNIFORM_MATRIX_STRIDE: 0x8A3D;
    readonly UNIFORM_IS_ROW_MAJOR: 0x8A3E;
    readonly UNIFORM_BLOCK_BINDING: 0x8A3F;
    readonly UNIFORM_BLOCK_DATA_SIZE: 0x8A40;
    readonly UNIFORM_BLOCK_ACTIVE_UNIFORMS: 0x8A42;
    readonly UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: 0x8A43;
    readonly UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: 0x8A44;
    readonly UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: 0x8A46;
    readonly INVALID_INDEX: 0xFFFFFFFF;
    readonly MAX_VERTEX_OUTPUT_COMPONENTS: 0x9122;
    readonly MAX_FRAGMENT_INPUT_COMPONENTS: 0x9125;
    readonly MAX_SERVER_WAIT_TIMEOUT: 0x9111;
    readonly OBJECT_TYPE: 0x9112;
    readonly SYNC_CONDITION: 0x9113;
    readonly SYNC_STATUS: 0x9114;
    readonly SYNC_FLAGS: 0x9115;
    readonly SYNC_FENCE: 0x9116;
    readonly SYNC_GPU_COMMANDS_COMPLETE: 0x9117;
    readonly UNSIGNALED: 0x9118;
    readonly SIGNALED: 0x9119;
    readonly ALREADY_SIGNALED: 0x911A;
    readonly TIMEOUT_EXPIRED: 0x911B;
    readonly CONDITION_SATISFIED: 0x911C;
    readonly WAIT_FAILED: 0x911D;
    readonly SYNC_FLUSH_COMMANDS_BIT: 0x00000001;
    readonly VERTEX_ATTRIB_ARRAY_DIVISOR: 0x88FE;
    readonly ANY_SAMPLES_PASSED: 0x8C2F;
    readonly ANY_SAMPLES_PASSED_CONSERVATIVE: 0x8D6A;
    readonly SAMPLER_BINDING: 0x8919;
    readonly RGB10_A2UI: 0x906F;
    readonly INT_2_10_10_10_REV: 0x8D9F;
    readonly TRANSFORM_FEEDBACK: 0x8E22;
    readonly TRANSFORM_FEEDBACK_PAUSED: 0x8E23;
    readonly TRANSFORM_FEEDBACK_ACTIVE: 0x8E24;
    readonly TRANSFORM_FEEDBACK_BINDING: 0x8E25;
    readonly TEXTURE_IMMUTABLE_FORMAT: 0x912F;
    readonly MAX_ELEMENT_INDEX: 0x8D6B;
    readonly TEXTURE_IMMUTABLE_LEVELS: 0x82DF;
    readonly TIMEOUT_IGNORED: -1;
    readonly MAX_CLIENT_WAIT_TIMEOUT_WEBGL: 0x9247;
    readonly DEPTH_BUFFER_BIT: 0x00000100;
    readonly STENCIL_BUFFER_BIT: 0x00000400;
    readonly COLOR_BUFFER_BIT: 0x00004000;
    readonly POINTS: 0x0000;
    readonly LINES: 0x0001;
    readonly LINE_LOOP: 0x0002;
    readonly LINE_STRIP: 0x0003;
    readonly TRIANGLES: 0x0004;
    readonly TRIANGLE_STRIP: 0x0005;
    readonly TRIANGLE_FAN: 0x0006;
    readonly ZERO: 0;
    readonly ONE: 1;
    readonly SRC_COLOR: 0x0300;
    readonly ONE_MINUS_SRC_COLOR: 0x0301;
    readonly SRC_ALPHA: 0x0302;
    readonly ONE_MINUS_SRC_ALPHA: 0x0303;
    readonly DST_ALPHA: 0x0304;
    readonly ONE_MINUS_DST_ALPHA: 0x0305;
    readonly DST_COLOR: 0x0306;
    readonly ONE_MINUS_DST_COLOR: 0x0307;
    readonly SRC_ALPHA_SATURATE: 0x0308;
    readonly FUNC_ADD: 0x8006;
    readonly BLEND_EQUATION: 0x8009;
    readonly BLEND_EQUATION_RGB: 0x8009;
    readonly BLEND_EQUATION_ALPHA: 0x883D;
    readonly FUNC_SUBTRACT: 0x800A;
    readonly FUNC_REVERSE_SUBTRACT: 0x800B;
    readonly BLEND_DST_RGB: 0x80C8;
    readonly BLEND_SRC_RGB: 0x80C9;
    readonly BLEND_DST_ALPHA: 0x80CA;
    readonly BLEND_SRC_ALPHA: 0x80CB;
    readonly CONSTANT_COLOR: 0x8001;
    readonly ONE_MINUS_CONSTANT_COLOR: 0x8002;
    readonly CONSTANT_ALPHA: 0x8003;
    readonly ONE_MINUS_CONSTANT_ALPHA: 0x8004;
    readonly BLEND_COLOR: 0x8005;
    readonly ARRAY_BUFFER: 0x8892;
    readonly ELEMENT_ARRAY_BUFFER: 0x8893;
    readonly ARRAY_BUFFER_BINDING: 0x8894;
    readonly ELEMENT_ARRAY_BUFFER_BINDING: 0x8895;
    readonly STREAM_DRAW: 0x88E0;
    readonly STATIC_DRAW: 0x88E4;
    readonly DYNAMIC_DRAW: 0x88E8;
    readonly BUFFER_SIZE: 0x8764;
    readonly BUFFER_USAGE: 0x8765;
    readonly CURRENT_VERTEX_ATTRIB: 0x8626;
    readonly FRONT: 0x0404;
    readonly BACK: 0x0405;
    readonly FRONT_AND_BACK: 0x0408;
    readonly CULL_FACE: 0x0B44;
    readonly BLEND: 0x0BE2;
    readonly DITHER: 0x0BD0;
    readonly STENCIL_TEST: 0x0B90;
    readonly DEPTH_TEST: 0x0B71;
    readonly SCISSOR_TEST: 0x0C11;
    readonly POLYGON_OFFSET_FILL: 0x8037;
    readonly SAMPLE_ALPHA_TO_COVERAGE: 0x809E;
    readonly SAMPLE_COVERAGE: 0x80A0;
    readonly NO_ERROR: 0;
    readonly INVALID_ENUM: 0x0500;
    readonly INVALID_VALUE: 0x0501;
    readonly INVALID_OPERATION: 0x0502;
    readonly OUT_OF_MEMORY: 0x0505;
    readonly CW: 0x0900;
    readonly CCW: 0x0901;
    readonly LINE_WIDTH: 0x0B21;
    readonly ALIASED_POINT_SIZE_RANGE: 0x846D;
    readonly ALIASED_LINE_WIDTH_RANGE: 0x846E;
    readonly CULL_FACE_MODE: 0x0B45;
    readonly FRONT_FACE: 0x0B46;
    readonly DEPTH_RANGE: 0x0B70;
    readonly DEPTH_WRITEMASK: 0x0B72;
    readonly DEPTH_CLEAR_VALUE: 0x0B73;
    readonly DEPTH_FUNC: 0x0B74;
    readonly STENCIL_CLEAR_VALUE: 0x0B91;
    readonly STENCIL_FUNC: 0x0B92;
    readonly STENCIL_FAIL: 0x0B94;
    readonly STENCIL_PASS_DEPTH_FAIL: 0x0B95;
    readonly STENCIL_PASS_DEPTH_PASS: 0x0B96;
    readonly STENCIL_REF: 0x0B97;
    readonly STENCIL_VALUE_MASK: 0x0B93;
    readonly STENCIL_WRITEMASK: 0x0B98;
    readonly STENCIL_BACK_FUNC: 0x8800;
    readonly STENCIL_BACK_FAIL: 0x8801;
    readonly STENCIL_BACK_PASS_DEPTH_FAIL: 0x8802;
    readonly STENCIL_BACK_PASS_DEPTH_PASS: 0x8803;
    readonly STENCIL_BACK_REF: 0x8CA3;
    readonly STENCIL_BACK_VALUE_MASK: 0x8CA4;
    readonly STENCIL_BACK_WRITEMASK: 0x8CA5;
    readonly VIEWPORT: 0x0BA2;
    readonly SCISSOR_BOX: 0x0C10;
    readonly COLOR_CLEAR_VALUE: 0x0C22;
    readonly COLOR_WRITEMASK: 0x0C23;
    readonly UNPACK_ALIGNMENT: 0x0CF5;
    readonly PACK_ALIGNMENT: 0x0D05;
    readonly MAX_TEXTURE_SIZE: 0x0D33;
    readonly MAX_VIEWPORT_DIMS: 0x0D3A;
    readonly SUBPIXEL_BITS: 0x0D50;
    readonly RED_BITS: 0x0D52;
    readonly GREEN_BITS: 0x0D53;
    readonly BLUE_BITS: 0x0D54;
    readonly ALPHA_BITS: 0x0D55;
    readonly DEPTH_BITS: 0x0D56;
    readonly STENCIL_BITS: 0x0D57;
    readonly POLYGON_OFFSET_UNITS: 0x2A00;
    readonly POLYGON_OFFSET_FACTOR: 0x8038;
    readonly TEXTURE_BINDING_2D: 0x8069;
    readonly SAMPLE_BUFFERS: 0x80A8;
    readonly SAMPLES: 0x80A9;
    readonly SAMPLE_COVERAGE_VALUE: 0x80AA;
    readonly SAMPLE_COVERAGE_INVERT: 0x80AB;
    readonly COMPRESSED_TEXTURE_FORMATS: 0x86A3;
    readonly DONT_CARE: 0x1100;
    readonly FASTEST: 0x1101;
    readonly NICEST: 0x1102;
    readonly GENERATE_MIPMAP_HINT: 0x8192;
    readonly BYTE: 0x1400;
    readonly UNSIGNED_BYTE: 0x1401;
    readonly SHORT: 0x1402;
    readonly UNSIGNED_SHORT: 0x1403;
    readonly INT: 0x1404;
    readonly UNSIGNED_INT: 0x1405;
    readonly FLOAT: 0x1406;
    readonly DEPTH_COMPONENT: 0x1902;
    readonly ALPHA: 0x1906;
    readonly RGB: 0x1907;
    readonly RGBA: 0x1908;
    readonly LUMINANCE: 0x1909;
    readonly LUMINANCE_ALPHA: 0x190A;
    readonly UNSIGNED_SHORT_4_4_4_4: 0x8033;
    readonly UNSIGNED_SHORT_5_5_5_1: 0x8034;
    readonly UNSIGNED_SHORT_5_6_5: 0x8363;
    readonly FRAGMENT_SHADER: 0x8B30;
    readonly VERTEX_SHADER: 0x8B31;
    readonly MAX_VERTEX_ATTRIBS: 0x8869;
    readonly MAX_VERTEX_UNIFORM_VECTORS: 0x8DFB;
    readonly MAX_VARYING_VECTORS: 0x8DFC;
    readonly MAX_COMBINED_TEXTURE_IMAGE_UNITS: 0x8B4D;
    readonly MAX_VERTEX_TEXTURE_IMAGE_UNITS: 0x8B4C;
    readonly MAX_TEXTURE_IMAGE_UNITS: 0x8872;
    readonly MAX_FRAGMENT_UNIFORM_VECTORS: 0x8DFD;
    readonly SHADER_TYPE: 0x8B4F;
    readonly DELETE_STATUS: 0x8B80;
    readonly LINK_STATUS: 0x8B82;
    readonly VALIDATE_STATUS: 0x8B83;
    readonly ATTACHED_SHADERS: 0x8B85;
    readonly ACTIVE_UNIFORMS: 0x8B86;
    readonly ACTIVE_ATTRIBUTES: 0x8B89;
    readonly SHADING_LANGUAGE_VERSION: 0x8B8C;
    readonly CURRENT_PROGRAM: 0x8B8D;
    readonly NEVER: 0x0200;
    readonly LESS: 0x0201;
    readonly EQUAL: 0x0202;
    readonly LEQUAL: 0x0203;
    readonly GREATER: 0x0204;
    readonly NOTEQUAL: 0x0205;
    readonly GEQUAL: 0x0206;
    readonly ALWAYS: 0x0207;
    readonly KEEP: 0x1E00;
    readonly REPLACE: 0x1E01;
    readonly INCR: 0x1E02;
    readonly DECR: 0x1E03;
    readonly INVERT: 0x150A;
    readonly INCR_WRAP: 0x8507;
    readonly DECR_WRAP: 0x8508;
    readonly VENDOR: 0x1F00;
    readonly RENDERER: 0x1F01;
    readonly VERSION: 0x1F02;
    readonly NEAREST: 0x2600;
    readonly LINEAR: 0x2601;
    readonly NEAREST_MIPMAP_NEAREST: 0x2700;
    readonly LINEAR_MIPMAP_NEAREST: 0x2701;
    readonly NEAREST_MIPMAP_LINEAR: 0x2702;
    readonly LINEAR_MIPMAP_LINEAR: 0x2703;
    readonly TEXTURE_MAG_FILTER: 0x2800;
    readonly TEXTURE_MIN_FILTER: 0x2801;
    readonly TEXTURE_WRAP_S: 0x2802;
    readonly TEXTURE_WRAP_T: 0x2803;
    readonly TEXTURE_2D: 0x0DE1;
    readonly TEXTURE: 0x1702;
    readonly TEXTURE_CUBE_MAP: 0x8513;
    readonly TEXTURE_BINDING_CUBE_MAP: 0x8514;
    readonly TEXTURE_CUBE_MAP_POSITIVE_X: 0x8515;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_X: 0x8516;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Y: 0x8517;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Y: 0x8518;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Z: 0x8519;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Z: 0x851A;
    readonly MAX_CUBE_MAP_TEXTURE_SIZE: 0x851C;
    readonly TEXTURE0: 0x84C0;
    readonly TEXTURE1: 0x84C1;
    readonly TEXTURE2: 0x84C2;
    readonly TEXTURE3: 0x84C3;
    readonly TEXTURE4: 0x84C4;
    readonly TEXTURE5: 0x84C5;
    readonly TEXTURE6: 0x84C6;
    readonly TEXTURE7: 0x84C7;
    readonly TEXTURE8: 0x84C8;
    readonly TEXTURE9: 0x84C9;
    readonly TEXTURE10: 0x84CA;
    readonly TEXTURE11: 0x84CB;
    readonly TEXTURE12: 0x84CC;
    readonly TEXTURE13: 0x84CD;
    readonly TEXTURE14: 0x84CE;
    readonly TEXTURE15: 0x84CF;
    readonly TEXTURE16: 0x84D0;
    readonly TEXTURE17: 0x84D1;
    readonly TEXTURE18: 0x84D2;
    readonly TEXTURE19: 0x84D3;
    readonly TEXTURE20: 0x84D4;
    readonly TEXTURE21: 0x84D5;
    readonly TEXTURE22: 0x84D6;
    readonly TEXTURE23: 0x84D7;
    readonly TEXTURE24: 0x84D8;
    readonly TEXTURE25: 0x84D9;
    readonly TEXTURE26: 0x84DA;
    readonly TEXTURE27: 0x84DB;
    readonly TEXTURE28: 0x84DC;
    readonly TEXTURE29: 0x84DD;
    readonly TEXTURE30: 0x84DE;
    readonly TEXTURE31: 0x84DF;
    readonly ACTIVE_TEXTURE: 0x84E0;
    readonly REPEAT: 0x2901;
    readonly CLAMP_TO_EDGE: 0x812F;
    readonly MIRRORED_REPEAT: 0x8370;
    readonly FLOAT_VEC2: 0x8B50;
    readonly FLOAT_VEC3: 0x8B51;
    readonly FLOAT_VEC4: 0x8B52;
    readonly INT_VEC2: 0x8B53;
    readonly INT_VEC3: 0x8B54;
    readonly INT_VEC4: 0x8B55;
    readonly BOOL: 0x8B56;
    readonly BOOL_VEC2: 0x8B57;
    readonly BOOL_VEC3: 0x8B58;
    readonly BOOL_VEC4: 0x8B59;
    readonly FLOAT_MAT2: 0x8B5A;
    readonly FLOAT_MAT3: 0x8B5B;
    readonly FLOAT_MAT4: 0x8B5C;
    readonly SAMPLER_2D: 0x8B5E;
    readonly SAMPLER_CUBE: 0x8B60;
    readonly VERTEX_ATTRIB_ARRAY_ENABLED: 0x8622;
    readonly VERTEX_ATTRIB_ARRAY_SIZE: 0x8623;
    readonly VERTEX_ATTRIB_ARRAY_STRIDE: 0x8624;
    readonly VERTEX_ATTRIB_ARRAY_TYPE: 0x8625;
    readonly VERTEX_ATTRIB_ARRAY_NORMALIZED: 0x886A;
    readonly VERTEX_ATTRIB_ARRAY_POINTER: 0x8645;
    readonly VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: 0x889F;
    readonly IMPLEMENTATION_COLOR_READ_TYPE: 0x8B9A;
    readonly IMPLEMENTATION_COLOR_READ_FORMAT: 0x8B9B;
    readonly COMPILE_STATUS: 0x8B81;
    readonly LOW_FLOAT: 0x8DF0;
    readonly MEDIUM_FLOAT: 0x8DF1;
    readonly HIGH_FLOAT: 0x8DF2;
    readonly LOW_INT: 0x8DF3;
    readonly MEDIUM_INT: 0x8DF4;
    readonly HIGH_INT: 0x8DF5;
    readonly FRAMEBUFFER: 0x8D40;
    readonly RENDERBUFFER: 0x8D41;
    readonly RGBA4: 0x8056;
    readonly RGB5_A1: 0x8057;
    readonly RGB565: 0x8D62;
    readonly DEPTH_COMPONENT16: 0x81A5;
    readonly STENCIL_INDEX8: 0x8D48;
    readonly DEPTH_STENCIL: 0x84F9;
    readonly RENDERBUFFER_WIDTH: 0x8D42;
    readonly RENDERBUFFER_HEIGHT: 0x8D43;
    readonly RENDERBUFFER_INTERNAL_FORMAT: 0x8D44;
    readonly RENDERBUFFER_RED_SIZE: 0x8D50;
    readonly RENDERBUFFER_GREEN_SIZE: 0x8D51;
    readonly RENDERBUFFER_BLUE_SIZE: 0x8D52;
    readonly RENDERBUFFER_ALPHA_SIZE: 0x8D53;
    readonly RENDERBUFFER_DEPTH_SIZE: 0x8D54;
    readonly RENDERBUFFER_STENCIL_SIZE: 0x8D55;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: 0x8CD0;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: 0x8CD1;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: 0x8CD2;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: 0x8CD3;
    readonly COLOR_ATTACHMENT0: 0x8CE0;
    readonly DEPTH_ATTACHMENT: 0x8D00;
    readonly STENCIL_ATTACHMENT: 0x8D20;
    readonly DEPTH_STENCIL_ATTACHMENT: 0x821A;
    readonly NONE: 0;
    readonly FRAMEBUFFER_COMPLETE: 0x8CD5;
    readonly FRAMEBUFFER_INCOMPLETE_ATTACHMENT: 0x8CD6;
    readonly FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: 0x8CD7;
    readonly FRAMEBUFFER_INCOMPLETE_DIMENSIONS: 0x8CD9;
    readonly FRAMEBUFFER_UNSUPPORTED: 0x8CDD;
    readonly FRAMEBUFFER_BINDING: 0x8CA6;
    readonly RENDERBUFFER_BINDING: 0x8CA7;
    readonly MAX_RENDERBUFFER_SIZE: 0x84E8;
    readonly INVALID_FRAMEBUFFER_OPERATION: 0x0506;
    readonly UNPACK_FLIP_Y_WEBGL: 0x9240;
    readonly UNPACK_PREMULTIPLY_ALPHA_WEBGL: 0x9241;
    readonly CONTEXT_LOST_WEBGL: 0x9242;
    readonly UNPACK_COLORSPACE_CONVERSION_WEBGL: 0x9243;
    readonly BROWSER_DEFAULT_WEBGL: 0x9244;
    isInstance: IsInstance<WebGL2RenderingContext>;
};

interface WebGL2RenderingContextBase {
    beginQuery(target: GLenum, query: WebGLQuery): void;
    beginTransformFeedback(primitiveMode: GLenum): void;
    bindBufferBase(target: GLenum, index: GLuint, buffer: WebGLBuffer | null): void;
    bindBufferRange(target: GLenum, index: GLuint, buffer: WebGLBuffer | null, offset: GLintptr, size: GLsizeiptr): void;
    bindSampler(unit: GLuint, sampler: WebGLSampler | null): void;
    bindTransformFeedback(target: GLenum, tf: WebGLTransformFeedback | null): void;
    bindVertexArray(array: WebGLVertexArrayObject | null): void;
    blitFramebuffer(srcX0: GLint, srcY0: GLint, srcX1: GLint, srcY1: GLint, dstX0: GLint, dstY0: GLint, dstX1: GLint, dstY1: GLint, mask: GLbitfield, filter: GLenum): void;
    bufferData(target: GLenum, size: GLsizeiptr, usage: GLenum): void;
    bufferData(target: GLenum, srcData: ArrayBuffer | null, usage: GLenum): void;
    bufferData(target: GLenum, srcData: ArrayBufferView, usage: GLenum): void;
    bufferData(target: GLenum, srcData: ArrayBufferView, usage: GLenum, srcOffset: GLuint, length?: GLuint): void;
    bufferSubData(target: GLenum, offset: GLintptr, srcData: ArrayBuffer): void;
    bufferSubData(target: GLenum, offset: GLintptr, srcData: ArrayBufferView): void;
    bufferSubData(target: GLenum, dstByteOffset: GLintptr, srcData: ArrayBufferView, srcOffset: GLuint, length?: GLuint): void;
    clearBufferfi(buffer: GLenum, drawbuffer: GLint, depth: GLfloat, stencil: GLint): void;
    clearBufferfv(buffer: GLenum, drawbuffer: GLint, values: Float32List, srcOffset?: GLuint): void;
    clearBufferiv(buffer: GLenum, drawbuffer: GLint, values: Int32List, srcOffset?: GLuint): void;
    clearBufferuiv(buffer: GLenum, drawbuffer: GLint, values: Uint32List, srcOffset?: GLuint): void;
    clientWaitSync(sync: WebGLSync, flags: GLbitfield, timeout: GLuint64): GLenum;
    compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, imageSize: GLsizei, offset: GLintptr): void;
    compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, srcData: ArrayBufferView, srcOffset?: GLuint, srcLengthOverride?: GLuint): void;
    compressedTexImage3D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, imageSize: GLsizei, offset: GLintptr): void;
    compressedTexImage3D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, srcData: ArrayBufferView, srcOffset?: GLuint, srcLengthOverride?: GLuint): void;
    compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, imageSize: GLsizei, offset: GLintptr): void;
    compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, srcData: ArrayBufferView, srcOffset?: GLuint, srcLengthOverride?: GLuint): void;
    compressedTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, imageSize: GLsizei, offset: GLintptr): void;
    compressedTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, srcData: ArrayBufferView, srcOffset?: GLuint, srcLengthOverride?: GLuint): void;
    copyBufferSubData(readTarget: GLenum, writeTarget: GLenum, readOffset: GLintptr, writeOffset: GLintptr, size: GLsizeiptr): void;
    copyTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    createQuery(): WebGLQuery;
    createSampler(): WebGLSampler;
    createTransformFeedback(): WebGLTransformFeedback;
    createVertexArray(): WebGLVertexArrayObject;
    deleteQuery(query: WebGLQuery | null): void;
    deleteSampler(sampler: WebGLSampler | null): void;
    deleteSync(sync: WebGLSync | null): void;
    deleteTransformFeedback(tf: WebGLTransformFeedback | null): void;
    deleteVertexArray(vertexArray: WebGLVertexArrayObject | null): void;
    drawArraysInstanced(mode: GLenum, first: GLint, count: GLsizei, instanceCount: GLsizei): void;
    drawBuffers(buffers: GLenum[]): void;
    drawElementsInstanced(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr, instanceCount: GLsizei): void;
    drawRangeElements(mode: GLenum, start: GLuint, end: GLuint, count: GLsizei, type: GLenum, offset: GLintptr): void;
    endQuery(target: GLenum): void;
    endTransformFeedback(): void;
    fenceSync(condition: GLenum, flags: GLbitfield): WebGLSync | null;
    framebufferTextureLayer(target: GLenum, attachment: GLenum, texture: WebGLTexture | null, level: GLint, layer: GLint): void;
    getActiveUniformBlockName(program: WebGLProgram, uniformBlockIndex: GLuint): string | null;
    getActiveUniformBlockParameter(program: WebGLProgram, uniformBlockIndex: GLuint, pname: GLenum): any;
    getActiveUniforms(program: WebGLProgram, uniformIndices: GLuint[], pname: GLenum): any;
    getBufferSubData(target: GLenum, srcByteOffset: GLintptr, dstData: ArrayBufferView, dstOffset?: GLuint, length?: GLuint): void;
    getFragDataLocation(program: WebGLProgram, name: string): GLint;
    getIndexedParameter(target: GLenum, index: GLuint): any;
    getInternalformatParameter(target: GLenum, internalformat: GLenum, pname: GLenum): any;
    getQuery(target: GLenum, pname: GLenum): any;
    getQueryParameter(query: WebGLQuery, pname: GLenum): any;
    getSamplerParameter(sampler: WebGLSampler, pname: GLenum): any;
    getSyncParameter(sync: WebGLSync, pname: GLenum): any;
    getTransformFeedbackVarying(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null;
    getUniformBlockIndex(program: WebGLProgram, uniformBlockName: string): GLuint;
    getUniformIndices(program: WebGLProgram, uniformNames: string[]): GLuint[] | null;
    invalidateFramebuffer(target: GLenum, attachments: GLenum[]): void;
    invalidateSubFramebuffer(target: GLenum, attachments: GLenum[], x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    isQuery(query: WebGLQuery | null): GLboolean;
    isSampler(sampler: WebGLSampler | null): GLboolean;
    isSync(sync: WebGLSync | null): GLboolean;
    isTransformFeedback(tf: WebGLTransformFeedback | null): GLboolean;
    isVertexArray(vertexArray: WebGLVertexArrayObject | null): GLboolean;
    pauseTransformFeedback(): void;
    readBuffer(src: GLenum): void;
    readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, dstData: ArrayBufferView | null): void;
    readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, offset: GLintptr): void;
    readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, dstData: ArrayBufferView, dstOffset: GLuint): void;
    renderbufferStorageMultisample(target: GLenum, samples: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei): void;
    resumeTransformFeedback(): void;
    samplerParameterf(sampler: WebGLSampler, pname: GLenum, param: GLfloat): void;
    samplerParameteri(sampler: WebGLSampler, pname: GLenum, param: GLint): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: ImageData): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: VideoFrame): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: ImageData): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: VideoFrame): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: GLuint): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: ImageData): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: VideoFrame): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView | null): void;
    texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: GLuint): void;
    texStorage2D(target: GLenum, levels: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei): void;
    texStorage3D(target: GLenum, levels: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: ImageData): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: VideoFrame): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: ImageData): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: VideoFrame): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: GLuint): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: HTMLCanvasElement): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: HTMLImageElement): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: HTMLVideoElement): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: ImageBitmap): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: ImageData): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: OffscreenCanvas): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: VideoFrame): void;
    texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, srcData: ArrayBufferView | null, srcOffset?: GLuint): void;
    transformFeedbackVaryings(program: WebGLProgram, varyings: string[], bufferMode: GLenum): void;
    uniform1fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform1iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform1ui(location: WebGLUniformLocation | null, v0: GLuint): void;
    uniform1uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint): void;
    uniform2uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint, v2: GLuint): void;
    uniform3uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint, v2: GLuint, v3: GLuint): void;
    uniform4uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformBlockBinding(program: WebGLProgram, uniformBlockIndex: GLuint, uniformBlockBinding: GLuint): void;
    uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix2x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix2x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: GLuint, srcLength?: GLuint): void;
    vertexAttribDivisor(index: GLuint, divisor: GLuint): void;
    vertexAttribI4i(index: GLuint, x: GLint, y: GLint, z: GLint, w: GLint): void;
    vertexAttribI4iv(index: GLuint, values: Int32List): void;
    vertexAttribI4ui(index: GLuint, x: GLuint, y: GLuint, z: GLuint, w: GLuint): void;
    vertexAttribI4uiv(index: GLuint, values: Uint32List): void;
    vertexAttribIPointer(index: GLuint, size: GLint, type: GLenum, stride: GLsizei, offset: GLintptr): void;
    waitSync(sync: WebGLSync, flags: GLbitfield, timeout: GLint64): void;
    readonly READ_BUFFER: 0x0C02;
    readonly UNPACK_ROW_LENGTH: 0x0CF2;
    readonly UNPACK_SKIP_ROWS: 0x0CF3;
    readonly UNPACK_SKIP_PIXELS: 0x0CF4;
    readonly PACK_ROW_LENGTH: 0x0D02;
    readonly PACK_SKIP_ROWS: 0x0D03;
    readonly PACK_SKIP_PIXELS: 0x0D04;
    readonly COLOR: 0x1800;
    readonly DEPTH: 0x1801;
    readonly STENCIL: 0x1802;
    readonly RED: 0x1903;
    readonly RGB8: 0x8051;
    readonly RGBA8: 0x8058;
    readonly RGB10_A2: 0x8059;
    readonly TEXTURE_BINDING_3D: 0x806A;
    readonly UNPACK_SKIP_IMAGES: 0x806D;
    readonly UNPACK_IMAGE_HEIGHT: 0x806E;
    readonly TEXTURE_3D: 0x806F;
    readonly TEXTURE_WRAP_R: 0x8072;
    readonly MAX_3D_TEXTURE_SIZE: 0x8073;
    readonly UNSIGNED_INT_2_10_10_10_REV: 0x8368;
    readonly MAX_ELEMENTS_VERTICES: 0x80E8;
    readonly MAX_ELEMENTS_INDICES: 0x80E9;
    readonly TEXTURE_MIN_LOD: 0x813A;
    readonly TEXTURE_MAX_LOD: 0x813B;
    readonly TEXTURE_BASE_LEVEL: 0x813C;
    readonly TEXTURE_MAX_LEVEL: 0x813D;
    readonly MIN: 0x8007;
    readonly MAX: 0x8008;
    readonly DEPTH_COMPONENT24: 0x81A6;
    readonly MAX_TEXTURE_LOD_BIAS: 0x84FD;
    readonly TEXTURE_COMPARE_MODE: 0x884C;
    readonly TEXTURE_COMPARE_FUNC: 0x884D;
    readonly CURRENT_QUERY: 0x8865;
    readonly QUERY_RESULT: 0x8866;
    readonly QUERY_RESULT_AVAILABLE: 0x8867;
    readonly STREAM_READ: 0x88E1;
    readonly STREAM_COPY: 0x88E2;
    readonly STATIC_READ: 0x88E5;
    readonly STATIC_COPY: 0x88E6;
    readonly DYNAMIC_READ: 0x88E9;
    readonly DYNAMIC_COPY: 0x88EA;
    readonly MAX_DRAW_BUFFERS: 0x8824;
    readonly DRAW_BUFFER0: 0x8825;
    readonly DRAW_BUFFER1: 0x8826;
    readonly DRAW_BUFFER2: 0x8827;
    readonly DRAW_BUFFER3: 0x8828;
    readonly DRAW_BUFFER4: 0x8829;
    readonly DRAW_BUFFER5: 0x882A;
    readonly DRAW_BUFFER6: 0x882B;
    readonly DRAW_BUFFER7: 0x882C;
    readonly DRAW_BUFFER8: 0x882D;
    readonly DRAW_BUFFER9: 0x882E;
    readonly DRAW_BUFFER10: 0x882F;
    readonly DRAW_BUFFER11: 0x8830;
    readonly DRAW_BUFFER12: 0x8831;
    readonly DRAW_BUFFER13: 0x8832;
    readonly DRAW_BUFFER14: 0x8833;
    readonly DRAW_BUFFER15: 0x8834;
    readonly MAX_FRAGMENT_UNIFORM_COMPONENTS: 0x8B49;
    readonly MAX_VERTEX_UNIFORM_COMPONENTS: 0x8B4A;
    readonly SAMPLER_3D: 0x8B5F;
    readonly SAMPLER_2D_SHADOW: 0x8B62;
    readonly FRAGMENT_SHADER_DERIVATIVE_HINT: 0x8B8B;
    readonly PIXEL_PACK_BUFFER: 0x88EB;
    readonly PIXEL_UNPACK_BUFFER: 0x88EC;
    readonly PIXEL_PACK_BUFFER_BINDING: 0x88ED;
    readonly PIXEL_UNPACK_BUFFER_BINDING: 0x88EF;
    readonly FLOAT_MAT2x3: 0x8B65;
    readonly FLOAT_MAT2x4: 0x8B66;
    readonly FLOAT_MAT3x2: 0x8B67;
    readonly FLOAT_MAT3x4: 0x8B68;
    readonly FLOAT_MAT4x2: 0x8B69;
    readonly FLOAT_MAT4x3: 0x8B6A;
    readonly SRGB: 0x8C40;
    readonly SRGB8: 0x8C41;
    readonly SRGB8_ALPHA8: 0x8C43;
    readonly COMPARE_REF_TO_TEXTURE: 0x884E;
    readonly RGBA32F: 0x8814;
    readonly RGB32F: 0x8815;
    readonly RGBA16F: 0x881A;
    readonly RGB16F: 0x881B;
    readonly VERTEX_ATTRIB_ARRAY_INTEGER: 0x88FD;
    readonly MAX_ARRAY_TEXTURE_LAYERS: 0x88FF;
    readonly MIN_PROGRAM_TEXEL_OFFSET: 0x8904;
    readonly MAX_PROGRAM_TEXEL_OFFSET: 0x8905;
    readonly MAX_VARYING_COMPONENTS: 0x8B4B;
    readonly TEXTURE_2D_ARRAY: 0x8C1A;
    readonly TEXTURE_BINDING_2D_ARRAY: 0x8C1D;
    readonly R11F_G11F_B10F: 0x8C3A;
    readonly UNSIGNED_INT_10F_11F_11F_REV: 0x8C3B;
    readonly RGB9_E5: 0x8C3D;
    readonly UNSIGNED_INT_5_9_9_9_REV: 0x8C3E;
    readonly TRANSFORM_FEEDBACK_BUFFER_MODE: 0x8C7F;
    readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: 0x8C80;
    readonly TRANSFORM_FEEDBACK_VARYINGS: 0x8C83;
    readonly TRANSFORM_FEEDBACK_BUFFER_START: 0x8C84;
    readonly TRANSFORM_FEEDBACK_BUFFER_SIZE: 0x8C85;
    readonly TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: 0x8C88;
    readonly RASTERIZER_DISCARD: 0x8C89;
    readonly MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: 0x8C8A;
    readonly MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: 0x8C8B;
    readonly INTERLEAVED_ATTRIBS: 0x8C8C;
    readonly SEPARATE_ATTRIBS: 0x8C8D;
    readonly TRANSFORM_FEEDBACK_BUFFER: 0x8C8E;
    readonly TRANSFORM_FEEDBACK_BUFFER_BINDING: 0x8C8F;
    readonly RGBA32UI: 0x8D70;
    readonly RGB32UI: 0x8D71;
    readonly RGBA16UI: 0x8D76;
    readonly RGB16UI: 0x8D77;
    readonly RGBA8UI: 0x8D7C;
    readonly RGB8UI: 0x8D7D;
    readonly RGBA32I: 0x8D82;
    readonly RGB32I: 0x8D83;
    readonly RGBA16I: 0x8D88;
    readonly RGB16I: 0x8D89;
    readonly RGBA8I: 0x8D8E;
    readonly RGB8I: 0x8D8F;
    readonly RED_INTEGER: 0x8D94;
    readonly RGB_INTEGER: 0x8D98;
    readonly RGBA_INTEGER: 0x8D99;
    readonly SAMPLER_2D_ARRAY: 0x8DC1;
    readonly SAMPLER_2D_ARRAY_SHADOW: 0x8DC4;
    readonly SAMPLER_CUBE_SHADOW: 0x8DC5;
    readonly UNSIGNED_INT_VEC2: 0x8DC6;
    readonly UNSIGNED_INT_VEC3: 0x8DC7;
    readonly UNSIGNED_INT_VEC4: 0x8DC8;
    readonly INT_SAMPLER_2D: 0x8DCA;
    readonly INT_SAMPLER_3D: 0x8DCB;
    readonly INT_SAMPLER_CUBE: 0x8DCC;
    readonly INT_SAMPLER_2D_ARRAY: 0x8DCF;
    readonly UNSIGNED_INT_SAMPLER_2D: 0x8DD2;
    readonly UNSIGNED_INT_SAMPLER_3D: 0x8DD3;
    readonly UNSIGNED_INT_SAMPLER_CUBE: 0x8DD4;
    readonly UNSIGNED_INT_SAMPLER_2D_ARRAY: 0x8DD7;
    readonly DEPTH_COMPONENT32F: 0x8CAC;
    readonly DEPTH32F_STENCIL8: 0x8CAD;
    readonly FLOAT_32_UNSIGNED_INT_24_8_REV: 0x8DAD;
    readonly FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: 0x8210;
    readonly FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: 0x8211;
    readonly FRAMEBUFFER_ATTACHMENT_RED_SIZE: 0x8212;
    readonly FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: 0x8213;
    readonly FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: 0x8214;
    readonly FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: 0x8215;
    readonly FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: 0x8216;
    readonly FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: 0x8217;
    readonly FRAMEBUFFER_DEFAULT: 0x8218;
    readonly UNSIGNED_INT_24_8: 0x84FA;
    readonly DEPTH24_STENCIL8: 0x88F0;
    readonly UNSIGNED_NORMALIZED: 0x8C17;
    readonly DRAW_FRAMEBUFFER_BINDING: 0x8CA6;
    readonly READ_FRAMEBUFFER: 0x8CA8;
    readonly DRAW_FRAMEBUFFER: 0x8CA9;
    readonly READ_FRAMEBUFFER_BINDING: 0x8CAA;
    readonly RENDERBUFFER_SAMPLES: 0x8CAB;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: 0x8CD4;
    readonly MAX_COLOR_ATTACHMENTS: 0x8CDF;
    readonly COLOR_ATTACHMENT1: 0x8CE1;
    readonly COLOR_ATTACHMENT2: 0x8CE2;
    readonly COLOR_ATTACHMENT3: 0x8CE3;
    readonly COLOR_ATTACHMENT4: 0x8CE4;
    readonly COLOR_ATTACHMENT5: 0x8CE5;
    readonly COLOR_ATTACHMENT6: 0x8CE6;
    readonly COLOR_ATTACHMENT7: 0x8CE7;
    readonly COLOR_ATTACHMENT8: 0x8CE8;
    readonly COLOR_ATTACHMENT9: 0x8CE9;
    readonly COLOR_ATTACHMENT10: 0x8CEA;
    readonly COLOR_ATTACHMENT11: 0x8CEB;
    readonly COLOR_ATTACHMENT12: 0x8CEC;
    readonly COLOR_ATTACHMENT13: 0x8CED;
    readonly COLOR_ATTACHMENT14: 0x8CEE;
    readonly COLOR_ATTACHMENT15: 0x8CEF;
    readonly FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: 0x8D56;
    readonly MAX_SAMPLES: 0x8D57;
    readonly HALF_FLOAT: 0x140B;
    readonly RG: 0x8227;
    readonly RG_INTEGER: 0x8228;
    readonly R8: 0x8229;
    readonly RG8: 0x822B;
    readonly R16F: 0x822D;
    readonly R32F: 0x822E;
    readonly RG16F: 0x822F;
    readonly RG32F: 0x8230;
    readonly R8I: 0x8231;
    readonly R8UI: 0x8232;
    readonly R16I: 0x8233;
    readonly R16UI: 0x8234;
    readonly R32I: 0x8235;
    readonly R32UI: 0x8236;
    readonly RG8I: 0x8237;
    readonly RG8UI: 0x8238;
    readonly RG16I: 0x8239;
    readonly RG16UI: 0x823A;
    readonly RG32I: 0x823B;
    readonly RG32UI: 0x823C;
    readonly VERTEX_ARRAY_BINDING: 0x85B5;
    readonly R8_SNORM: 0x8F94;
    readonly RG8_SNORM: 0x8F95;
    readonly RGB8_SNORM: 0x8F96;
    readonly RGBA8_SNORM: 0x8F97;
    readonly SIGNED_NORMALIZED: 0x8F9C;
    readonly COPY_READ_BUFFER: 0x8F36;
    readonly COPY_WRITE_BUFFER: 0x8F37;
    readonly COPY_READ_BUFFER_BINDING: 0x8F36;
    readonly COPY_WRITE_BUFFER_BINDING: 0x8F37;
    readonly UNIFORM_BUFFER: 0x8A11;
    readonly UNIFORM_BUFFER_BINDING: 0x8A28;
    readonly UNIFORM_BUFFER_START: 0x8A29;
    readonly UNIFORM_BUFFER_SIZE: 0x8A2A;
    readonly MAX_VERTEX_UNIFORM_BLOCKS: 0x8A2B;
    readonly MAX_FRAGMENT_UNIFORM_BLOCKS: 0x8A2D;
    readonly MAX_COMBINED_UNIFORM_BLOCKS: 0x8A2E;
    readonly MAX_UNIFORM_BUFFER_BINDINGS: 0x8A2F;
    readonly MAX_UNIFORM_BLOCK_SIZE: 0x8A30;
    readonly MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: 0x8A31;
    readonly MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: 0x8A33;
    readonly UNIFORM_BUFFER_OFFSET_ALIGNMENT: 0x8A34;
    readonly ACTIVE_UNIFORM_BLOCKS: 0x8A36;
    readonly UNIFORM_TYPE: 0x8A37;
    readonly UNIFORM_SIZE: 0x8A38;
    readonly UNIFORM_BLOCK_INDEX: 0x8A3A;
    readonly UNIFORM_OFFSET: 0x8A3B;
    readonly UNIFORM_ARRAY_STRIDE: 0x8A3C;
    readonly UNIFORM_MATRIX_STRIDE: 0x8A3D;
    readonly UNIFORM_IS_ROW_MAJOR: 0x8A3E;
    readonly UNIFORM_BLOCK_BINDING: 0x8A3F;
    readonly UNIFORM_BLOCK_DATA_SIZE: 0x8A40;
    readonly UNIFORM_BLOCK_ACTIVE_UNIFORMS: 0x8A42;
    readonly UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: 0x8A43;
    readonly UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: 0x8A44;
    readonly UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: 0x8A46;
    readonly INVALID_INDEX: 0xFFFFFFFF;
    readonly MAX_VERTEX_OUTPUT_COMPONENTS: 0x9122;
    readonly MAX_FRAGMENT_INPUT_COMPONENTS: 0x9125;
    readonly MAX_SERVER_WAIT_TIMEOUT: 0x9111;
    readonly OBJECT_TYPE: 0x9112;
    readonly SYNC_CONDITION: 0x9113;
    readonly SYNC_STATUS: 0x9114;
    readonly SYNC_FLAGS: 0x9115;
    readonly SYNC_FENCE: 0x9116;
    readonly SYNC_GPU_COMMANDS_COMPLETE: 0x9117;
    readonly UNSIGNALED: 0x9118;
    readonly SIGNALED: 0x9119;
    readonly ALREADY_SIGNALED: 0x911A;
    readonly TIMEOUT_EXPIRED: 0x911B;
    readonly CONDITION_SATISFIED: 0x911C;
    readonly WAIT_FAILED: 0x911D;
    readonly SYNC_FLUSH_COMMANDS_BIT: 0x00000001;
    readonly VERTEX_ATTRIB_ARRAY_DIVISOR: 0x88FE;
    readonly ANY_SAMPLES_PASSED: 0x8C2F;
    readonly ANY_SAMPLES_PASSED_CONSERVATIVE: 0x8D6A;
    readonly SAMPLER_BINDING: 0x8919;
    readonly RGB10_A2UI: 0x906F;
    readonly INT_2_10_10_10_REV: 0x8D9F;
    readonly TRANSFORM_FEEDBACK: 0x8E22;
    readonly TRANSFORM_FEEDBACK_PAUSED: 0x8E23;
    readonly TRANSFORM_FEEDBACK_ACTIVE: 0x8E24;
    readonly TRANSFORM_FEEDBACK_BINDING: 0x8E25;
    readonly TEXTURE_IMMUTABLE_FORMAT: 0x912F;
    readonly MAX_ELEMENT_INDEX: 0x8D6B;
    readonly TEXTURE_IMMUTABLE_LEVELS: 0x82DF;
    readonly TIMEOUT_IGNORED: -1;
    readonly MAX_CLIENT_WAIT_TIMEOUT_WEBGL: 0x9247;
}

interface WebGLActiveInfo {
    readonly name: string;
    readonly size: GLint;
    readonly type: GLenum;
}

declare var WebGLActiveInfo: {
    prototype: WebGLActiveInfo;
    new(): WebGLActiveInfo;
    isInstance: IsInstance<WebGLActiveInfo>;
};

interface WebGLBuffer {
}

declare var WebGLBuffer: {
    prototype: WebGLBuffer;
    new(): WebGLBuffer;
    isInstance: IsInstance<WebGLBuffer>;
};

interface WebGLContextEvent extends Event {
    readonly statusMessage: string;
}

declare var WebGLContextEvent: {
    prototype: WebGLContextEvent;
    new(type: string, eventInit?: WebGLContextEventInit): WebGLContextEvent;
    isInstance: IsInstance<WebGLContextEvent>;
};

interface WebGLFramebuffer {
}

declare var WebGLFramebuffer: {
    prototype: WebGLFramebuffer;
    new(): WebGLFramebuffer;
    isInstance: IsInstance<WebGLFramebuffer>;
};

interface WebGLProgram {
}

declare var WebGLProgram: {
    prototype: WebGLProgram;
    new(): WebGLProgram;
    isInstance: IsInstance<WebGLProgram>;
};

interface WebGLQuery {
}

declare var WebGLQuery: {
    prototype: WebGLQuery;
    new(): WebGLQuery;
    isInstance: IsInstance<WebGLQuery>;
};

interface WebGLRenderbuffer {
}

declare var WebGLRenderbuffer: {
    prototype: WebGLRenderbuffer;
    new(): WebGLRenderbuffer;
    isInstance: IsInstance<WebGLRenderbuffer>;
};

interface WebGLRenderingContext extends WebGLRenderingContextBase {
    bufferData(target: GLenum, size: GLsizeiptr, usage: GLenum): void;
    bufferData(target: GLenum, data: ArrayBuffer | null, usage: GLenum): void;
    bufferData(target: GLenum, data: ArrayBufferView, usage: GLenum): void;
    bufferSubData(target: GLenum, offset: GLintptr, data: ArrayBuffer): void;
    bufferSubData(target: GLenum, offset: GLintptr, data: ArrayBufferView): void;
    compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, data: ArrayBufferView): void;
    compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, data: ArrayBufferView): void;
    readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, pixels: ImageBitmap): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, pixels: ImageData): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, image: HTMLImageElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, canvas: HTMLCanvasElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, video: HTMLVideoElement): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, canvas: OffscreenCanvas): void;
    texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, videoFrame: VideoFrame): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, pixels: ImageBitmap): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, pixels: ImageData): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, image: HTMLImageElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, canvas: HTMLCanvasElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, video: HTMLVideoElement): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, canvas: OffscreenCanvas): void;
    texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, videoFrame: VideoFrame): void;
    uniform1fv(location: WebGLUniformLocation | null, data: Float32List): void;
    uniform1iv(location: WebGLUniformLocation | null, data: Int32List): void;
    uniform2fv(location: WebGLUniformLocation | null, data: Float32List): void;
    uniform2iv(location: WebGLUniformLocation | null, data: Int32List): void;
    uniform3fv(location: WebGLUniformLocation | null, data: Float32List): void;
    uniform3iv(location: WebGLUniformLocation | null, data: Int32List): void;
    uniform4fv(location: WebGLUniformLocation | null, data: Float32List): void;
    uniform4iv(location: WebGLUniformLocation | null, data: Int32List): void;
    uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List): void;
    uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List): void;
    uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List): void;
}

declare var WebGLRenderingContext: {
    prototype: WebGLRenderingContext;
    new(): WebGLRenderingContext;
    readonly DEPTH_BUFFER_BIT: 0x00000100;
    readonly STENCIL_BUFFER_BIT: 0x00000400;
    readonly COLOR_BUFFER_BIT: 0x00004000;
    readonly POINTS: 0x0000;
    readonly LINES: 0x0001;
    readonly LINE_LOOP: 0x0002;
    readonly LINE_STRIP: 0x0003;
    readonly TRIANGLES: 0x0004;
    readonly TRIANGLE_STRIP: 0x0005;
    readonly TRIANGLE_FAN: 0x0006;
    readonly ZERO: 0;
    readonly ONE: 1;
    readonly SRC_COLOR: 0x0300;
    readonly ONE_MINUS_SRC_COLOR: 0x0301;
    readonly SRC_ALPHA: 0x0302;
    readonly ONE_MINUS_SRC_ALPHA: 0x0303;
    readonly DST_ALPHA: 0x0304;
    readonly ONE_MINUS_DST_ALPHA: 0x0305;
    readonly DST_COLOR: 0x0306;
    readonly ONE_MINUS_DST_COLOR: 0x0307;
    readonly SRC_ALPHA_SATURATE: 0x0308;
    readonly FUNC_ADD: 0x8006;
    readonly BLEND_EQUATION: 0x8009;
    readonly BLEND_EQUATION_RGB: 0x8009;
    readonly BLEND_EQUATION_ALPHA: 0x883D;
    readonly FUNC_SUBTRACT: 0x800A;
    readonly FUNC_REVERSE_SUBTRACT: 0x800B;
    readonly BLEND_DST_RGB: 0x80C8;
    readonly BLEND_SRC_RGB: 0x80C9;
    readonly BLEND_DST_ALPHA: 0x80CA;
    readonly BLEND_SRC_ALPHA: 0x80CB;
    readonly CONSTANT_COLOR: 0x8001;
    readonly ONE_MINUS_CONSTANT_COLOR: 0x8002;
    readonly CONSTANT_ALPHA: 0x8003;
    readonly ONE_MINUS_CONSTANT_ALPHA: 0x8004;
    readonly BLEND_COLOR: 0x8005;
    readonly ARRAY_BUFFER: 0x8892;
    readonly ELEMENT_ARRAY_BUFFER: 0x8893;
    readonly ARRAY_BUFFER_BINDING: 0x8894;
    readonly ELEMENT_ARRAY_BUFFER_BINDING: 0x8895;
    readonly STREAM_DRAW: 0x88E0;
    readonly STATIC_DRAW: 0x88E4;
    readonly DYNAMIC_DRAW: 0x88E8;
    readonly BUFFER_SIZE: 0x8764;
    readonly BUFFER_USAGE: 0x8765;
    readonly CURRENT_VERTEX_ATTRIB: 0x8626;
    readonly FRONT: 0x0404;
    readonly BACK: 0x0405;
    readonly FRONT_AND_BACK: 0x0408;
    readonly CULL_FACE: 0x0B44;
    readonly BLEND: 0x0BE2;
    readonly DITHER: 0x0BD0;
    readonly STENCIL_TEST: 0x0B90;
    readonly DEPTH_TEST: 0x0B71;
    readonly SCISSOR_TEST: 0x0C11;
    readonly POLYGON_OFFSET_FILL: 0x8037;
    readonly SAMPLE_ALPHA_TO_COVERAGE: 0x809E;
    readonly SAMPLE_COVERAGE: 0x80A0;
    readonly NO_ERROR: 0;
    readonly INVALID_ENUM: 0x0500;
    readonly INVALID_VALUE: 0x0501;
    readonly INVALID_OPERATION: 0x0502;
    readonly OUT_OF_MEMORY: 0x0505;
    readonly CW: 0x0900;
    readonly CCW: 0x0901;
    readonly LINE_WIDTH: 0x0B21;
    readonly ALIASED_POINT_SIZE_RANGE: 0x846D;
    readonly ALIASED_LINE_WIDTH_RANGE: 0x846E;
    readonly CULL_FACE_MODE: 0x0B45;
    readonly FRONT_FACE: 0x0B46;
    readonly DEPTH_RANGE: 0x0B70;
    readonly DEPTH_WRITEMASK: 0x0B72;
    readonly DEPTH_CLEAR_VALUE: 0x0B73;
    readonly DEPTH_FUNC: 0x0B74;
    readonly STENCIL_CLEAR_VALUE: 0x0B91;
    readonly STENCIL_FUNC: 0x0B92;
    readonly STENCIL_FAIL: 0x0B94;
    readonly STENCIL_PASS_DEPTH_FAIL: 0x0B95;
    readonly STENCIL_PASS_DEPTH_PASS: 0x0B96;
    readonly STENCIL_REF: 0x0B97;
    readonly STENCIL_VALUE_MASK: 0x0B93;
    readonly STENCIL_WRITEMASK: 0x0B98;
    readonly STENCIL_BACK_FUNC: 0x8800;
    readonly STENCIL_BACK_FAIL: 0x8801;
    readonly STENCIL_BACK_PASS_DEPTH_FAIL: 0x8802;
    readonly STENCIL_BACK_PASS_DEPTH_PASS: 0x8803;
    readonly STENCIL_BACK_REF: 0x8CA3;
    readonly STENCIL_BACK_VALUE_MASK: 0x8CA4;
    readonly STENCIL_BACK_WRITEMASK: 0x8CA5;
    readonly VIEWPORT: 0x0BA2;
    readonly SCISSOR_BOX: 0x0C10;
    readonly COLOR_CLEAR_VALUE: 0x0C22;
    readonly COLOR_WRITEMASK: 0x0C23;
    readonly UNPACK_ALIGNMENT: 0x0CF5;
    readonly PACK_ALIGNMENT: 0x0D05;
    readonly MAX_TEXTURE_SIZE: 0x0D33;
    readonly MAX_VIEWPORT_DIMS: 0x0D3A;
    readonly SUBPIXEL_BITS: 0x0D50;
    readonly RED_BITS: 0x0D52;
    readonly GREEN_BITS: 0x0D53;
    readonly BLUE_BITS: 0x0D54;
    readonly ALPHA_BITS: 0x0D55;
    readonly DEPTH_BITS: 0x0D56;
    readonly STENCIL_BITS: 0x0D57;
    readonly POLYGON_OFFSET_UNITS: 0x2A00;
    readonly POLYGON_OFFSET_FACTOR: 0x8038;
    readonly TEXTURE_BINDING_2D: 0x8069;
    readonly SAMPLE_BUFFERS: 0x80A8;
    readonly SAMPLES: 0x80A9;
    readonly SAMPLE_COVERAGE_VALUE: 0x80AA;
    readonly SAMPLE_COVERAGE_INVERT: 0x80AB;
    readonly COMPRESSED_TEXTURE_FORMATS: 0x86A3;
    readonly DONT_CARE: 0x1100;
    readonly FASTEST: 0x1101;
    readonly NICEST: 0x1102;
    readonly GENERATE_MIPMAP_HINT: 0x8192;
    readonly BYTE: 0x1400;
    readonly UNSIGNED_BYTE: 0x1401;
    readonly SHORT: 0x1402;
    readonly UNSIGNED_SHORT: 0x1403;
    readonly INT: 0x1404;
    readonly UNSIGNED_INT: 0x1405;
    readonly FLOAT: 0x1406;
    readonly DEPTH_COMPONENT: 0x1902;
    readonly ALPHA: 0x1906;
    readonly RGB: 0x1907;
    readonly RGBA: 0x1908;
    readonly LUMINANCE: 0x1909;
    readonly LUMINANCE_ALPHA: 0x190A;
    readonly UNSIGNED_SHORT_4_4_4_4: 0x8033;
    readonly UNSIGNED_SHORT_5_5_5_1: 0x8034;
    readonly UNSIGNED_SHORT_5_6_5: 0x8363;
    readonly FRAGMENT_SHADER: 0x8B30;
    readonly VERTEX_SHADER: 0x8B31;
    readonly MAX_VERTEX_ATTRIBS: 0x8869;
    readonly MAX_VERTEX_UNIFORM_VECTORS: 0x8DFB;
    readonly MAX_VARYING_VECTORS: 0x8DFC;
    readonly MAX_COMBINED_TEXTURE_IMAGE_UNITS: 0x8B4D;
    readonly MAX_VERTEX_TEXTURE_IMAGE_UNITS: 0x8B4C;
    readonly MAX_TEXTURE_IMAGE_UNITS: 0x8872;
    readonly MAX_FRAGMENT_UNIFORM_VECTORS: 0x8DFD;
    readonly SHADER_TYPE: 0x8B4F;
    readonly DELETE_STATUS: 0x8B80;
    readonly LINK_STATUS: 0x8B82;
    readonly VALIDATE_STATUS: 0x8B83;
    readonly ATTACHED_SHADERS: 0x8B85;
    readonly ACTIVE_UNIFORMS: 0x8B86;
    readonly ACTIVE_ATTRIBUTES: 0x8B89;
    readonly SHADING_LANGUAGE_VERSION: 0x8B8C;
    readonly CURRENT_PROGRAM: 0x8B8D;
    readonly NEVER: 0x0200;
    readonly LESS: 0x0201;
    readonly EQUAL: 0x0202;
    readonly LEQUAL: 0x0203;
    readonly GREATER: 0x0204;
    readonly NOTEQUAL: 0x0205;
    readonly GEQUAL: 0x0206;
    readonly ALWAYS: 0x0207;
    readonly KEEP: 0x1E00;
    readonly REPLACE: 0x1E01;
    readonly INCR: 0x1E02;
    readonly DECR: 0x1E03;
    readonly INVERT: 0x150A;
    readonly INCR_WRAP: 0x8507;
    readonly DECR_WRAP: 0x8508;
    readonly VENDOR: 0x1F00;
    readonly RENDERER: 0x1F01;
    readonly VERSION: 0x1F02;
    readonly NEAREST: 0x2600;
    readonly LINEAR: 0x2601;
    readonly NEAREST_MIPMAP_NEAREST: 0x2700;
    readonly LINEAR_MIPMAP_NEAREST: 0x2701;
    readonly NEAREST_MIPMAP_LINEAR: 0x2702;
    readonly LINEAR_MIPMAP_LINEAR: 0x2703;
    readonly TEXTURE_MAG_FILTER: 0x2800;
    readonly TEXTURE_MIN_FILTER: 0x2801;
    readonly TEXTURE_WRAP_S: 0x2802;
    readonly TEXTURE_WRAP_T: 0x2803;
    readonly TEXTURE_2D: 0x0DE1;
    readonly TEXTURE: 0x1702;
    readonly TEXTURE_CUBE_MAP: 0x8513;
    readonly TEXTURE_BINDING_CUBE_MAP: 0x8514;
    readonly TEXTURE_CUBE_MAP_POSITIVE_X: 0x8515;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_X: 0x8516;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Y: 0x8517;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Y: 0x8518;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Z: 0x8519;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Z: 0x851A;
    readonly MAX_CUBE_MAP_TEXTURE_SIZE: 0x851C;
    readonly TEXTURE0: 0x84C0;
    readonly TEXTURE1: 0x84C1;
    readonly TEXTURE2: 0x84C2;
    readonly TEXTURE3: 0x84C3;
    readonly TEXTURE4: 0x84C4;
    readonly TEXTURE5: 0x84C5;
    readonly TEXTURE6: 0x84C6;
    readonly TEXTURE7: 0x84C7;
    readonly TEXTURE8: 0x84C8;
    readonly TEXTURE9: 0x84C9;
    readonly TEXTURE10: 0x84CA;
    readonly TEXTURE11: 0x84CB;
    readonly TEXTURE12: 0x84CC;
    readonly TEXTURE13: 0x84CD;
    readonly TEXTURE14: 0x84CE;
    readonly TEXTURE15: 0x84CF;
    readonly TEXTURE16: 0x84D0;
    readonly TEXTURE17: 0x84D1;
    readonly TEXTURE18: 0x84D2;
    readonly TEXTURE19: 0x84D3;
    readonly TEXTURE20: 0x84D4;
    readonly TEXTURE21: 0x84D5;
    readonly TEXTURE22: 0x84D6;
    readonly TEXTURE23: 0x84D7;
    readonly TEXTURE24: 0x84D8;
    readonly TEXTURE25: 0x84D9;
    readonly TEXTURE26: 0x84DA;
    readonly TEXTURE27: 0x84DB;
    readonly TEXTURE28: 0x84DC;
    readonly TEXTURE29: 0x84DD;
    readonly TEXTURE30: 0x84DE;
    readonly TEXTURE31: 0x84DF;
    readonly ACTIVE_TEXTURE: 0x84E0;
    readonly REPEAT: 0x2901;
    readonly CLAMP_TO_EDGE: 0x812F;
    readonly MIRRORED_REPEAT: 0x8370;
    readonly FLOAT_VEC2: 0x8B50;
    readonly FLOAT_VEC3: 0x8B51;
    readonly FLOAT_VEC4: 0x8B52;
    readonly INT_VEC2: 0x8B53;
    readonly INT_VEC3: 0x8B54;
    readonly INT_VEC4: 0x8B55;
    readonly BOOL: 0x8B56;
    readonly BOOL_VEC2: 0x8B57;
    readonly BOOL_VEC3: 0x8B58;
    readonly BOOL_VEC4: 0x8B59;
    readonly FLOAT_MAT2: 0x8B5A;
    readonly FLOAT_MAT3: 0x8B5B;
    readonly FLOAT_MAT4: 0x8B5C;
    readonly SAMPLER_2D: 0x8B5E;
    readonly SAMPLER_CUBE: 0x8B60;
    readonly VERTEX_ATTRIB_ARRAY_ENABLED: 0x8622;
    readonly VERTEX_ATTRIB_ARRAY_SIZE: 0x8623;
    readonly VERTEX_ATTRIB_ARRAY_STRIDE: 0x8624;
    readonly VERTEX_ATTRIB_ARRAY_TYPE: 0x8625;
    readonly VERTEX_ATTRIB_ARRAY_NORMALIZED: 0x886A;
    readonly VERTEX_ATTRIB_ARRAY_POINTER: 0x8645;
    readonly VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: 0x889F;
    readonly IMPLEMENTATION_COLOR_READ_TYPE: 0x8B9A;
    readonly IMPLEMENTATION_COLOR_READ_FORMAT: 0x8B9B;
    readonly COMPILE_STATUS: 0x8B81;
    readonly LOW_FLOAT: 0x8DF0;
    readonly MEDIUM_FLOAT: 0x8DF1;
    readonly HIGH_FLOAT: 0x8DF2;
    readonly LOW_INT: 0x8DF3;
    readonly MEDIUM_INT: 0x8DF4;
    readonly HIGH_INT: 0x8DF5;
    readonly FRAMEBUFFER: 0x8D40;
    readonly RENDERBUFFER: 0x8D41;
    readonly RGBA4: 0x8056;
    readonly RGB5_A1: 0x8057;
    readonly RGB565: 0x8D62;
    readonly DEPTH_COMPONENT16: 0x81A5;
    readonly STENCIL_INDEX8: 0x8D48;
    readonly DEPTH_STENCIL: 0x84F9;
    readonly RENDERBUFFER_WIDTH: 0x8D42;
    readonly RENDERBUFFER_HEIGHT: 0x8D43;
    readonly RENDERBUFFER_INTERNAL_FORMAT: 0x8D44;
    readonly RENDERBUFFER_RED_SIZE: 0x8D50;
    readonly RENDERBUFFER_GREEN_SIZE: 0x8D51;
    readonly RENDERBUFFER_BLUE_SIZE: 0x8D52;
    readonly RENDERBUFFER_ALPHA_SIZE: 0x8D53;
    readonly RENDERBUFFER_DEPTH_SIZE: 0x8D54;
    readonly RENDERBUFFER_STENCIL_SIZE: 0x8D55;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: 0x8CD0;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: 0x8CD1;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: 0x8CD2;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: 0x8CD3;
    readonly COLOR_ATTACHMENT0: 0x8CE0;
    readonly DEPTH_ATTACHMENT: 0x8D00;
    readonly STENCIL_ATTACHMENT: 0x8D20;
    readonly DEPTH_STENCIL_ATTACHMENT: 0x821A;
    readonly NONE: 0;
    readonly FRAMEBUFFER_COMPLETE: 0x8CD5;
    readonly FRAMEBUFFER_INCOMPLETE_ATTACHMENT: 0x8CD6;
    readonly FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: 0x8CD7;
    readonly FRAMEBUFFER_INCOMPLETE_DIMENSIONS: 0x8CD9;
    readonly FRAMEBUFFER_UNSUPPORTED: 0x8CDD;
    readonly FRAMEBUFFER_BINDING: 0x8CA6;
    readonly RENDERBUFFER_BINDING: 0x8CA7;
    readonly MAX_RENDERBUFFER_SIZE: 0x84E8;
    readonly INVALID_FRAMEBUFFER_OPERATION: 0x0506;
    readonly UNPACK_FLIP_Y_WEBGL: 0x9240;
    readonly UNPACK_PREMULTIPLY_ALPHA_WEBGL: 0x9241;
    readonly CONTEXT_LOST_WEBGL: 0x9242;
    readonly UNPACK_COLORSPACE_CONVERSION_WEBGL: 0x9243;
    readonly BROWSER_DEFAULT_WEBGL: 0x9244;
    isInstance: IsInstance<WebGLRenderingContext>;
};

interface WebGLRenderingContextBase {
    readonly canvas: CanvasSource | null;
    drawingBufferColorSpace: PredefinedColorSpace;
    readonly drawingBufferHeight: GLsizei;
    readonly drawingBufferWidth: GLsizei;
    activeTexture(texture: GLenum): void;
    attachShader(program: WebGLProgram, shader: WebGLShader): void;
    bindAttribLocation(program: WebGLProgram, index: GLuint, name: string): void;
    bindBuffer(target: GLenum, buffer: WebGLBuffer | null): void;
    bindFramebuffer(target: GLenum, framebuffer: WebGLFramebuffer | null): void;
    bindRenderbuffer(target: GLenum, renderbuffer: WebGLRenderbuffer | null): void;
    bindTexture(target: GLenum, texture: WebGLTexture | null): void;
    blendColor(red: GLfloat, green: GLfloat, blue: GLfloat, alpha: GLfloat): void;
    blendEquation(mode: GLenum): void;
    blendEquationSeparate(modeRGB: GLenum, modeAlpha: GLenum): void;
    blendFunc(sfactor: GLenum, dfactor: GLenum): void;
    blendFuncSeparate(srcRGB: GLenum, dstRGB: GLenum, srcAlpha: GLenum, dstAlpha: GLenum): void;
    checkFramebufferStatus(target: GLenum): GLenum;
    clear(mask: GLbitfield): void;
    clearColor(red: GLfloat, green: GLfloat, blue: GLfloat, alpha: GLfloat): void;
    clearDepth(depth: GLclampf): void;
    clearStencil(s: GLint): void;
    colorMask(red: GLboolean, green: GLboolean, blue: GLboolean, alpha: GLboolean): void;
    compileShader(shader: WebGLShader): void;
    copyTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, x: GLint, y: GLint, width: GLsizei, height: GLsizei, border: GLint): void;
    copyTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    createBuffer(): WebGLBuffer;
    createFramebuffer(): WebGLFramebuffer;
    createProgram(): WebGLProgram;
    createRenderbuffer(): WebGLRenderbuffer;
    createShader(type: GLenum): WebGLShader | null;
    createTexture(): WebGLTexture;
    cullFace(mode: GLenum): void;
    deleteBuffer(buffer: WebGLBuffer | null): void;
    deleteFramebuffer(framebuffer: WebGLFramebuffer | null): void;
    deleteProgram(program: WebGLProgram | null): void;
    deleteRenderbuffer(renderbuffer: WebGLRenderbuffer | null): void;
    deleteShader(shader: WebGLShader | null): void;
    deleteTexture(texture: WebGLTexture | null): void;
    depthFunc(func: GLenum): void;
    depthMask(flag: GLboolean): void;
    depthRange(zNear: GLclampf, zFar: GLclampf): void;
    detachShader(program: WebGLProgram, shader: WebGLShader): void;
    disable(cap: GLenum): void;
    disableVertexAttribArray(index: GLuint): void;
    drawArrays(mode: GLenum, first: GLint, count: GLsizei): void;
    drawElements(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr): void;
    enable(cap: GLenum): void;
    enableVertexAttribArray(index: GLuint): void;
    finish(): void;
    flush(): void;
    framebufferRenderbuffer(target: GLenum, attachment: GLenum, renderbuffertarget: GLenum, renderbuffer: WebGLRenderbuffer | null): void;
    framebufferTexture2D(target: GLenum, attachment: GLenum, textarget: GLenum, texture: WebGLTexture | null, level: GLint): void;
    frontFace(mode: GLenum): void;
    generateMipmap(target: GLenum): void;
    getActiveAttrib(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null;
    getActiveUniform(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null;
    getAttachedShaders(program: WebGLProgram): WebGLShader[] | null;
    getAttribLocation(program: WebGLProgram, name: string): GLint;
    getBufferParameter(target: GLenum, pname: GLenum): any;
    getContextAttributes(): WebGLContextAttributes | null;
    getError(): GLenum;
    getExtension(name: string): any;
    getFramebufferAttachmentParameter(target: GLenum, attachment: GLenum, pname: GLenum): any;
    getParameter(pname: GLenum): any;
    getProgramInfoLog(program: WebGLProgram): string | null;
    getProgramParameter(program: WebGLProgram, pname: GLenum): any;
    getRenderbufferParameter(target: GLenum, pname: GLenum): any;
    getShaderInfoLog(shader: WebGLShader): string | null;
    getShaderParameter(shader: WebGLShader, pname: GLenum): any;
    getShaderPrecisionFormat(shadertype: GLenum, precisiontype: GLenum): WebGLShaderPrecisionFormat | null;
    getShaderSource(shader: WebGLShader): string | null;
    getSupportedExtensions(): string[] | null;
    getTexParameter(target: GLenum, pname: GLenum): any;
    getUniform(program: WebGLProgram, location: WebGLUniformLocation): any;
    getUniformLocation(program: WebGLProgram, name: string): WebGLUniformLocation | null;
    getVertexAttrib(index: GLuint, pname: GLenum): any;
    getVertexAttribOffset(index: GLuint, pname: GLenum): GLintptr;
    hint(target: GLenum, mode: GLenum): void;
    isBuffer(buffer: WebGLBuffer | null): GLboolean;
    isContextLost(): boolean;
    isEnabled(cap: GLenum): GLboolean;
    isFramebuffer(framebuffer: WebGLFramebuffer | null): GLboolean;
    isProgram(program: WebGLProgram | null): GLboolean;
    isRenderbuffer(renderbuffer: WebGLRenderbuffer | null): GLboolean;
    isShader(shader: WebGLShader | null): GLboolean;
    isTexture(texture: WebGLTexture | null): GLboolean;
    lineWidth(width: GLfloat): void;
    linkProgram(program: WebGLProgram): void;
    makeXRCompatible(): Promise<void>;
    pixelStorei(pname: GLenum, param: GLint): void;
    polygonOffset(factor: GLfloat, units: GLfloat): void;
    renderbufferStorage(target: GLenum, internalformat: GLenum, width: GLsizei, height: GLsizei): void;
    sampleCoverage(value: GLclampf, invert: GLboolean): void;
    scissor(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    shaderSource(shader: WebGLShader, source: string): void;
    stencilFunc(func: GLenum, ref: GLint, mask: GLuint): void;
    stencilFuncSeparate(face: GLenum, func: GLenum, ref: GLint, mask: GLuint): void;
    stencilMask(mask: GLuint): void;
    stencilMaskSeparate(face: GLenum, mask: GLuint): void;
    stencilOp(fail: GLenum, zfail: GLenum, zpass: GLenum): void;
    stencilOpSeparate(face: GLenum, fail: GLenum, zfail: GLenum, zpass: GLenum): void;
    texParameterf(target: GLenum, pname: GLenum, param: GLfloat): void;
    texParameteri(target: GLenum, pname: GLenum, param: GLint): void;
    uniform1f(location: WebGLUniformLocation | null, x: GLfloat): void;
    uniform1i(location: WebGLUniformLocation | null, x: GLint): void;
    uniform2f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat): void;
    uniform2i(location: WebGLUniformLocation | null, x: GLint, y: GLint): void;
    uniform3f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat): void;
    uniform3i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint): void;
    uniform4f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void;
    uniform4i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint, w: GLint): void;
    useProgram(program: WebGLProgram | null): void;
    validateProgram(program: WebGLProgram): void;
    vertexAttrib1f(indx: GLuint, x: GLfloat): void;
    vertexAttrib1fv(indx: GLuint, values: Float32List): void;
    vertexAttrib2f(indx: GLuint, x: GLfloat, y: GLfloat): void;
    vertexAttrib2fv(indx: GLuint, values: Float32List): void;
    vertexAttrib3f(indx: GLuint, x: GLfloat, y: GLfloat, z: GLfloat): void;
    vertexAttrib3fv(indx: GLuint, values: Float32List): void;
    vertexAttrib4f(indx: GLuint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void;
    vertexAttrib4fv(indx: GLuint, values: Float32List): void;
    vertexAttribPointer(indx: GLuint, size: GLint, type: GLenum, normalized: GLboolean, stride: GLsizei, offset: GLintptr): void;
    viewport(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    readonly DEPTH_BUFFER_BIT: 0x00000100;
    readonly STENCIL_BUFFER_BIT: 0x00000400;
    readonly COLOR_BUFFER_BIT: 0x00004000;
    readonly POINTS: 0x0000;
    readonly LINES: 0x0001;
    readonly LINE_LOOP: 0x0002;
    readonly LINE_STRIP: 0x0003;
    readonly TRIANGLES: 0x0004;
    readonly TRIANGLE_STRIP: 0x0005;
    readonly TRIANGLE_FAN: 0x0006;
    readonly ZERO: 0;
    readonly ONE: 1;
    readonly SRC_COLOR: 0x0300;
    readonly ONE_MINUS_SRC_COLOR: 0x0301;
    readonly SRC_ALPHA: 0x0302;
    readonly ONE_MINUS_SRC_ALPHA: 0x0303;
    readonly DST_ALPHA: 0x0304;
    readonly ONE_MINUS_DST_ALPHA: 0x0305;
    readonly DST_COLOR: 0x0306;
    readonly ONE_MINUS_DST_COLOR: 0x0307;
    readonly SRC_ALPHA_SATURATE: 0x0308;
    readonly FUNC_ADD: 0x8006;
    readonly BLEND_EQUATION: 0x8009;
    readonly BLEND_EQUATION_RGB: 0x8009;
    readonly BLEND_EQUATION_ALPHA: 0x883D;
    readonly FUNC_SUBTRACT: 0x800A;
    readonly FUNC_REVERSE_SUBTRACT: 0x800B;
    readonly BLEND_DST_RGB: 0x80C8;
    readonly BLEND_SRC_RGB: 0x80C9;
    readonly BLEND_DST_ALPHA: 0x80CA;
    readonly BLEND_SRC_ALPHA: 0x80CB;
    readonly CONSTANT_COLOR: 0x8001;
    readonly ONE_MINUS_CONSTANT_COLOR: 0x8002;
    readonly CONSTANT_ALPHA: 0x8003;
    readonly ONE_MINUS_CONSTANT_ALPHA: 0x8004;
    readonly BLEND_COLOR: 0x8005;
    readonly ARRAY_BUFFER: 0x8892;
    readonly ELEMENT_ARRAY_BUFFER: 0x8893;
    readonly ARRAY_BUFFER_BINDING: 0x8894;
    readonly ELEMENT_ARRAY_BUFFER_BINDING: 0x8895;
    readonly STREAM_DRAW: 0x88E0;
    readonly STATIC_DRAW: 0x88E4;
    readonly DYNAMIC_DRAW: 0x88E8;
    readonly BUFFER_SIZE: 0x8764;
    readonly BUFFER_USAGE: 0x8765;
    readonly CURRENT_VERTEX_ATTRIB: 0x8626;
    readonly FRONT: 0x0404;
    readonly BACK: 0x0405;
    readonly FRONT_AND_BACK: 0x0408;
    readonly CULL_FACE: 0x0B44;
    readonly BLEND: 0x0BE2;
    readonly DITHER: 0x0BD0;
    readonly STENCIL_TEST: 0x0B90;
    readonly DEPTH_TEST: 0x0B71;
    readonly SCISSOR_TEST: 0x0C11;
    readonly POLYGON_OFFSET_FILL: 0x8037;
    readonly SAMPLE_ALPHA_TO_COVERAGE: 0x809E;
    readonly SAMPLE_COVERAGE: 0x80A0;
    readonly NO_ERROR: 0;
    readonly INVALID_ENUM: 0x0500;
    readonly INVALID_VALUE: 0x0501;
    readonly INVALID_OPERATION: 0x0502;
    readonly OUT_OF_MEMORY: 0x0505;
    readonly CW: 0x0900;
    readonly CCW: 0x0901;
    readonly LINE_WIDTH: 0x0B21;
    readonly ALIASED_POINT_SIZE_RANGE: 0x846D;
    readonly ALIASED_LINE_WIDTH_RANGE: 0x846E;
    readonly CULL_FACE_MODE: 0x0B45;
    readonly FRONT_FACE: 0x0B46;
    readonly DEPTH_RANGE: 0x0B70;
    readonly DEPTH_WRITEMASK: 0x0B72;
    readonly DEPTH_CLEAR_VALUE: 0x0B73;
    readonly DEPTH_FUNC: 0x0B74;
    readonly STENCIL_CLEAR_VALUE: 0x0B91;
    readonly STENCIL_FUNC: 0x0B92;
    readonly STENCIL_FAIL: 0x0B94;
    readonly STENCIL_PASS_DEPTH_FAIL: 0x0B95;
    readonly STENCIL_PASS_DEPTH_PASS: 0x0B96;
    readonly STENCIL_REF: 0x0B97;
    readonly STENCIL_VALUE_MASK: 0x0B93;
    readonly STENCIL_WRITEMASK: 0x0B98;
    readonly STENCIL_BACK_FUNC: 0x8800;
    readonly STENCIL_BACK_FAIL: 0x8801;
    readonly STENCIL_BACK_PASS_DEPTH_FAIL: 0x8802;
    readonly STENCIL_BACK_PASS_DEPTH_PASS: 0x8803;
    readonly STENCIL_BACK_REF: 0x8CA3;
    readonly STENCIL_BACK_VALUE_MASK: 0x8CA4;
    readonly STENCIL_BACK_WRITEMASK: 0x8CA5;
    readonly VIEWPORT: 0x0BA2;
    readonly SCISSOR_BOX: 0x0C10;
    readonly COLOR_CLEAR_VALUE: 0x0C22;
    readonly COLOR_WRITEMASK: 0x0C23;
    readonly UNPACK_ALIGNMENT: 0x0CF5;
    readonly PACK_ALIGNMENT: 0x0D05;
    readonly MAX_TEXTURE_SIZE: 0x0D33;
    readonly MAX_VIEWPORT_DIMS: 0x0D3A;
    readonly SUBPIXEL_BITS: 0x0D50;
    readonly RED_BITS: 0x0D52;
    readonly GREEN_BITS: 0x0D53;
    readonly BLUE_BITS: 0x0D54;
    readonly ALPHA_BITS: 0x0D55;
    readonly DEPTH_BITS: 0x0D56;
    readonly STENCIL_BITS: 0x0D57;
    readonly POLYGON_OFFSET_UNITS: 0x2A00;
    readonly POLYGON_OFFSET_FACTOR: 0x8038;
    readonly TEXTURE_BINDING_2D: 0x8069;
    readonly SAMPLE_BUFFERS: 0x80A8;
    readonly SAMPLES: 0x80A9;
    readonly SAMPLE_COVERAGE_VALUE: 0x80AA;
    readonly SAMPLE_COVERAGE_INVERT: 0x80AB;
    readonly COMPRESSED_TEXTURE_FORMATS: 0x86A3;
    readonly DONT_CARE: 0x1100;
    readonly FASTEST: 0x1101;
    readonly NICEST: 0x1102;
    readonly GENERATE_MIPMAP_HINT: 0x8192;
    readonly BYTE: 0x1400;
    readonly UNSIGNED_BYTE: 0x1401;
    readonly SHORT: 0x1402;
    readonly UNSIGNED_SHORT: 0x1403;
    readonly INT: 0x1404;
    readonly UNSIGNED_INT: 0x1405;
    readonly FLOAT: 0x1406;
    readonly DEPTH_COMPONENT: 0x1902;
    readonly ALPHA: 0x1906;
    readonly RGB: 0x1907;
    readonly RGBA: 0x1908;
    readonly LUMINANCE: 0x1909;
    readonly LUMINANCE_ALPHA: 0x190A;
    readonly UNSIGNED_SHORT_4_4_4_4: 0x8033;
    readonly UNSIGNED_SHORT_5_5_5_1: 0x8034;
    readonly UNSIGNED_SHORT_5_6_5: 0x8363;
    readonly FRAGMENT_SHADER: 0x8B30;
    readonly VERTEX_SHADER: 0x8B31;
    readonly MAX_VERTEX_ATTRIBS: 0x8869;
    readonly MAX_VERTEX_UNIFORM_VECTORS: 0x8DFB;
    readonly MAX_VARYING_VECTORS: 0x8DFC;
    readonly MAX_COMBINED_TEXTURE_IMAGE_UNITS: 0x8B4D;
    readonly MAX_VERTEX_TEXTURE_IMAGE_UNITS: 0x8B4C;
    readonly MAX_TEXTURE_IMAGE_UNITS: 0x8872;
    readonly MAX_FRAGMENT_UNIFORM_VECTORS: 0x8DFD;
    readonly SHADER_TYPE: 0x8B4F;
    readonly DELETE_STATUS: 0x8B80;
    readonly LINK_STATUS: 0x8B82;
    readonly VALIDATE_STATUS: 0x8B83;
    readonly ATTACHED_SHADERS: 0x8B85;
    readonly ACTIVE_UNIFORMS: 0x8B86;
    readonly ACTIVE_ATTRIBUTES: 0x8B89;
    readonly SHADING_LANGUAGE_VERSION: 0x8B8C;
    readonly CURRENT_PROGRAM: 0x8B8D;
    readonly NEVER: 0x0200;
    readonly LESS: 0x0201;
    readonly EQUAL: 0x0202;
    readonly LEQUAL: 0x0203;
    readonly GREATER: 0x0204;
    readonly NOTEQUAL: 0x0205;
    readonly GEQUAL: 0x0206;
    readonly ALWAYS: 0x0207;
    readonly KEEP: 0x1E00;
    readonly REPLACE: 0x1E01;
    readonly INCR: 0x1E02;
    readonly DECR: 0x1E03;
    readonly INVERT: 0x150A;
    readonly INCR_WRAP: 0x8507;
    readonly DECR_WRAP: 0x8508;
    readonly VENDOR: 0x1F00;
    readonly RENDERER: 0x1F01;
    readonly VERSION: 0x1F02;
    readonly NEAREST: 0x2600;
    readonly LINEAR: 0x2601;
    readonly NEAREST_MIPMAP_NEAREST: 0x2700;
    readonly LINEAR_MIPMAP_NEAREST: 0x2701;
    readonly NEAREST_MIPMAP_LINEAR: 0x2702;
    readonly LINEAR_MIPMAP_LINEAR: 0x2703;
    readonly TEXTURE_MAG_FILTER: 0x2800;
    readonly TEXTURE_MIN_FILTER: 0x2801;
    readonly TEXTURE_WRAP_S: 0x2802;
    readonly TEXTURE_WRAP_T: 0x2803;
    readonly TEXTURE_2D: 0x0DE1;
    readonly TEXTURE: 0x1702;
    readonly TEXTURE_CUBE_MAP: 0x8513;
    readonly TEXTURE_BINDING_CUBE_MAP: 0x8514;
    readonly TEXTURE_CUBE_MAP_POSITIVE_X: 0x8515;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_X: 0x8516;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Y: 0x8517;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Y: 0x8518;
    readonly TEXTURE_CUBE_MAP_POSITIVE_Z: 0x8519;
    readonly TEXTURE_CUBE_MAP_NEGATIVE_Z: 0x851A;
    readonly MAX_CUBE_MAP_TEXTURE_SIZE: 0x851C;
    readonly TEXTURE0: 0x84C0;
    readonly TEXTURE1: 0x84C1;
    readonly TEXTURE2: 0x84C2;
    readonly TEXTURE3: 0x84C3;
    readonly TEXTURE4: 0x84C4;
    readonly TEXTURE5: 0x84C5;
    readonly TEXTURE6: 0x84C6;
    readonly TEXTURE7: 0x84C7;
    readonly TEXTURE8: 0x84C8;
    readonly TEXTURE9: 0x84C9;
    readonly TEXTURE10: 0x84CA;
    readonly TEXTURE11: 0x84CB;
    readonly TEXTURE12: 0x84CC;
    readonly TEXTURE13: 0x84CD;
    readonly TEXTURE14: 0x84CE;
    readonly TEXTURE15: 0x84CF;
    readonly TEXTURE16: 0x84D0;
    readonly TEXTURE17: 0x84D1;
    readonly TEXTURE18: 0x84D2;
    readonly TEXTURE19: 0x84D3;
    readonly TEXTURE20: 0x84D4;
    readonly TEXTURE21: 0x84D5;
    readonly TEXTURE22: 0x84D6;
    readonly TEXTURE23: 0x84D7;
    readonly TEXTURE24: 0x84D8;
    readonly TEXTURE25: 0x84D9;
    readonly TEXTURE26: 0x84DA;
    readonly TEXTURE27: 0x84DB;
    readonly TEXTURE28: 0x84DC;
    readonly TEXTURE29: 0x84DD;
    readonly TEXTURE30: 0x84DE;
    readonly TEXTURE31: 0x84DF;
    readonly ACTIVE_TEXTURE: 0x84E0;
    readonly REPEAT: 0x2901;
    readonly CLAMP_TO_EDGE: 0x812F;
    readonly MIRRORED_REPEAT: 0x8370;
    readonly FLOAT_VEC2: 0x8B50;
    readonly FLOAT_VEC3: 0x8B51;
    readonly FLOAT_VEC4: 0x8B52;
    readonly INT_VEC2: 0x8B53;
    readonly INT_VEC3: 0x8B54;
    readonly INT_VEC4: 0x8B55;
    readonly BOOL: 0x8B56;
    readonly BOOL_VEC2: 0x8B57;
    readonly BOOL_VEC3: 0x8B58;
    readonly BOOL_VEC4: 0x8B59;
    readonly FLOAT_MAT2: 0x8B5A;
    readonly FLOAT_MAT3: 0x8B5B;
    readonly FLOAT_MAT4: 0x8B5C;
    readonly SAMPLER_2D: 0x8B5E;
    readonly SAMPLER_CUBE: 0x8B60;
    readonly VERTEX_ATTRIB_ARRAY_ENABLED: 0x8622;
    readonly VERTEX_ATTRIB_ARRAY_SIZE: 0x8623;
    readonly VERTEX_ATTRIB_ARRAY_STRIDE: 0x8624;
    readonly VERTEX_ATTRIB_ARRAY_TYPE: 0x8625;
    readonly VERTEX_ATTRIB_ARRAY_NORMALIZED: 0x886A;
    readonly VERTEX_ATTRIB_ARRAY_POINTER: 0x8645;
    readonly VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: 0x889F;
    readonly IMPLEMENTATION_COLOR_READ_TYPE: 0x8B9A;
    readonly IMPLEMENTATION_COLOR_READ_FORMAT: 0x8B9B;
    readonly COMPILE_STATUS: 0x8B81;
    readonly LOW_FLOAT: 0x8DF0;
    readonly MEDIUM_FLOAT: 0x8DF1;
    readonly HIGH_FLOAT: 0x8DF2;
    readonly LOW_INT: 0x8DF3;
    readonly MEDIUM_INT: 0x8DF4;
    readonly HIGH_INT: 0x8DF5;
    readonly FRAMEBUFFER: 0x8D40;
    readonly RENDERBUFFER: 0x8D41;
    readonly RGBA4: 0x8056;
    readonly RGB5_A1: 0x8057;
    readonly RGB565: 0x8D62;
    readonly DEPTH_COMPONENT16: 0x81A5;
    readonly STENCIL_INDEX8: 0x8D48;
    readonly DEPTH_STENCIL: 0x84F9;
    readonly RENDERBUFFER_WIDTH: 0x8D42;
    readonly RENDERBUFFER_HEIGHT: 0x8D43;
    readonly RENDERBUFFER_INTERNAL_FORMAT: 0x8D44;
    readonly RENDERBUFFER_RED_SIZE: 0x8D50;
    readonly RENDERBUFFER_GREEN_SIZE: 0x8D51;
    readonly RENDERBUFFER_BLUE_SIZE: 0x8D52;
    readonly RENDERBUFFER_ALPHA_SIZE: 0x8D53;
    readonly RENDERBUFFER_DEPTH_SIZE: 0x8D54;
    readonly RENDERBUFFER_STENCIL_SIZE: 0x8D55;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: 0x8CD0;
    readonly FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: 0x8CD1;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: 0x8CD2;
    readonly FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: 0x8CD3;
    readonly COLOR_ATTACHMENT0: 0x8CE0;
    readonly DEPTH_ATTACHMENT: 0x8D00;
    readonly STENCIL_ATTACHMENT: 0x8D20;
    readonly DEPTH_STENCIL_ATTACHMENT: 0x821A;
    readonly NONE: 0;
    readonly FRAMEBUFFER_COMPLETE: 0x8CD5;
    readonly FRAMEBUFFER_INCOMPLETE_ATTACHMENT: 0x8CD6;
    readonly FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: 0x8CD7;
    readonly FRAMEBUFFER_INCOMPLETE_DIMENSIONS: 0x8CD9;
    readonly FRAMEBUFFER_UNSUPPORTED: 0x8CDD;
    readonly FRAMEBUFFER_BINDING: 0x8CA6;
    readonly RENDERBUFFER_BINDING: 0x8CA7;
    readonly MAX_RENDERBUFFER_SIZE: 0x84E8;
    readonly INVALID_FRAMEBUFFER_OPERATION: 0x0506;
    readonly UNPACK_FLIP_Y_WEBGL: 0x9240;
    readonly UNPACK_PREMULTIPLY_ALPHA_WEBGL: 0x9241;
    readonly CONTEXT_LOST_WEBGL: 0x9242;
    readonly UNPACK_COLORSPACE_CONVERSION_WEBGL: 0x9243;
    readonly BROWSER_DEFAULT_WEBGL: 0x9244;
}

interface WebGLSampler {
}

declare var WebGLSampler: {
    prototype: WebGLSampler;
    new(): WebGLSampler;
    isInstance: IsInstance<WebGLSampler>;
};

interface WebGLShader {
}

declare var WebGLShader: {
    prototype: WebGLShader;
    new(): WebGLShader;
    isInstance: IsInstance<WebGLShader>;
};

interface WebGLShaderPrecisionFormat {
    readonly precision: GLint;
    readonly rangeMax: GLint;
    readonly rangeMin: GLint;
}

declare var WebGLShaderPrecisionFormat: {
    prototype: WebGLShaderPrecisionFormat;
    new(): WebGLShaderPrecisionFormat;
    isInstance: IsInstance<WebGLShaderPrecisionFormat>;
};

interface WebGLSync {
}

declare var WebGLSync: {
    prototype: WebGLSync;
    new(): WebGLSync;
    isInstance: IsInstance<WebGLSync>;
};

interface WebGLTexture {
}

declare var WebGLTexture: {
    prototype: WebGLTexture;
    new(): WebGLTexture;
    isInstance: IsInstance<WebGLTexture>;
};

interface WebGLTransformFeedback {
}

declare var WebGLTransformFeedback: {
    prototype: WebGLTransformFeedback;
    new(): WebGLTransformFeedback;
    isInstance: IsInstance<WebGLTransformFeedback>;
};

interface WebGLUniformLocation {
}

declare var WebGLUniformLocation: {
    prototype: WebGLUniformLocation;
    new(): WebGLUniformLocation;
    isInstance: IsInstance<WebGLUniformLocation>;
};

interface WebGLVertexArrayObject {
}

declare var WebGLVertexArrayObject: {
    prototype: WebGLVertexArrayObject;
    new(): WebGLVertexArrayObject;
    isInstance: IsInstance<WebGLVertexArrayObject>;
};

interface WebSocketEventMap {
    "close": Event;
    "error": Event;
    "message": Event;
    "open": Event;
}

interface WebSocket extends EventTarget {
    binaryType: BinaryType;
    readonly bufferedAmount: number;
    readonly extensions: string;
    onclose: ((this: WebSocket, ev: Event) => any) | null;
    onerror: ((this: WebSocket, ev: Event) => any) | null;
    onmessage: ((this: WebSocket, ev: Event) => any) | null;
    onopen: ((this: WebSocket, ev: Event) => any) | null;
    readonly protocol: string;
    readonly readyState: number;
    readonly url: string;
    close(code?: number, reason?: string): void;
    send(data: string): void;
    send(data: Blob): void;
    send(data: ArrayBuffer): void;
    send(data: ArrayBufferView): void;
    readonly CONNECTING: 0;
    readonly OPEN: 1;
    readonly CLOSING: 2;
    readonly CLOSED: 3;
    addEventListener<K extends keyof WebSocketEventMap>(type: K, listener: (this: WebSocket, ev: WebSocketEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WebSocketEventMap>(type: K, listener: (this: WebSocket, ev: WebSocketEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var WebSocket: {
    prototype: WebSocket;
    new(url: string, protocols?: string | string[]): WebSocket;
    readonly CONNECTING: 0;
    readonly OPEN: 1;
    readonly CLOSING: 2;
    readonly CLOSED: 3;
    isInstance: IsInstance<WebSocket>;
    createServerWebSocket(url: string, protocols: string[], transportProvider: nsITransportProvider, negotiatedExtensions: string): WebSocket;
};

/** Available only in secure contexts. */
interface WebTransport {
    readonly closed: Promise<WebTransportCloseInfo>;
    readonly congestionControl: WebTransportCongestionControl;
    readonly datagrams: WebTransportDatagramDuplexStream;
    readonly incomingBidirectionalStreams: ReadableStream;
    readonly incomingUnidirectionalStreams: ReadableStream;
    readonly ready: Promise<undefined>;
    readonly reliability: WebTransportReliabilityMode;
    close(closeInfo?: WebTransportCloseInfo): void;
    createBidirectionalStream(options?: WebTransportSendStreamOptions): Promise<WebTransportBidirectionalStream>;
    createUnidirectionalStream(options?: WebTransportSendStreamOptions): Promise<WritableStream>;
    getStats(): Promise<WebTransportStats>;
}

declare var WebTransport: {
    prototype: WebTransport;
    new(url: string | URL, options?: WebTransportOptions): WebTransport;
    isInstance: IsInstance<WebTransport>;
};

/** Available only in secure contexts. */
interface WebTransportBidirectionalStream {
    readonly readable: WebTransportReceiveStream;
    readonly writable: WebTransportSendStream;
}

declare var WebTransportBidirectionalStream: {
    prototype: WebTransportBidirectionalStream;
    new(): WebTransportBidirectionalStream;
    isInstance: IsInstance<WebTransportBidirectionalStream>;
};

/** Available only in secure contexts. */
interface WebTransportDatagramDuplexStream {
    incomingHighWaterMark: number;
    incomingMaxAge: number;
    readonly maxDatagramSize: number;
    outgoingHighWaterMark: number;
    outgoingMaxAge: number;
    readonly readable: ReadableStream;
    readonly writable: WritableStream;
}

declare var WebTransportDatagramDuplexStream: {
    prototype: WebTransportDatagramDuplexStream;
    new(): WebTransportDatagramDuplexStream;
    isInstance: IsInstance<WebTransportDatagramDuplexStream>;
};

/** Available only in secure contexts. */
interface WebTransportError extends DOMException {
    readonly source: WebTransportErrorSource;
    readonly streamErrorCode: number | null;
}

declare var WebTransportError: {
    prototype: WebTransportError;
    new(init?: WebTransportErrorInit): WebTransportError;
    isInstance: IsInstance<WebTransportError>;
};

/** Available only in secure contexts. */
interface WebTransportReceiveStream extends ReadableStream {
    getStats(): Promise<WebTransportReceiveStreamStats>;
}

declare var WebTransportReceiveStream: {
    prototype: WebTransportReceiveStream;
    new(): WebTransportReceiveStream;
    isInstance: IsInstance<WebTransportReceiveStream>;
};

/** Available only in secure contexts. */
interface WebTransportSendStream extends WritableStream {
    sendOrder: number | null;
    getStats(): Promise<WebTransportSendStreamStats>;
}

declare var WebTransportSendStream: {
    prototype: WebTransportSendStream;
    new(): WebTransportSendStream;
    isInstance: IsInstance<WebTransportSendStream>;
};

interface WheelEvent extends MouseEvent {
    readonly deltaMode: number;
    readonly deltaX: number;
    readonly deltaY: number;
    readonly deltaZ: number;
    readonly wheelDelta: number;
    readonly wheelDeltaX: number;
    readonly wheelDeltaY: number;
    readonly DOM_DELTA_PIXEL: 0x00;
    readonly DOM_DELTA_LINE: 0x01;
    readonly DOM_DELTA_PAGE: 0x02;
}

declare var WheelEvent: {
    prototype: WheelEvent;
    new(type: string, eventInitDict?: WheelEventInit): WheelEvent;
    readonly DOM_DELTA_PIXEL: 0x00;
    readonly DOM_DELTA_LINE: 0x01;
    readonly DOM_DELTA_PAGE: 0x02;
    isInstance: IsInstance<WheelEvent>;
};

interface WindowEventMap extends GlobalEventHandlersEventMap, OnErrorEventHandlerForWindowEventMap, TouchEventHandlersEventMap, WindowEventHandlersEventMap {
    "devicelight": Event;
    "devicemotion": Event;
    "deviceorientation": Event;
    "deviceorientationabsolute": Event;
    "orientationchange": Event;
    "userproximity": Event;
    "vrdisplayactivate": Event;
    "vrdisplayconnect": Event;
    "vrdisplaydeactivate": Event;
    "vrdisplaydisconnect": Event;
    "vrdisplaypresentchange": Event;
}

interface Window extends EventTarget, AnimationFrameProvider, GlobalCrypto, GlobalEventHandlers, OnErrorEventHandlerForWindow, SpeechSynthesisGetter, TouchEventHandlers, WindowEventHandlers, WindowLocalStorage, WindowOrWorkerGlobalScope, WindowSessionStorage {
    readonly Glean: GleanImpl;
    readonly GleanPings: GleanPingsImpl;
    readonly InstallTrigger: InstallTriggerImpl | null;
    browserDOMWindow: nsIBrowserDOMWindow | null;
    readonly browsingContext: BrowsingContext;
    readonly clientInformation: Navigator;
    readonly clientPrincipal: Principal | null;
    readonly closed: boolean;
    readonly content: any;
    readonly controllers: XULControllers;
    readonly customElements: CustomElementRegistry;
    readonly desktopToDeviceScale: number;
    readonly devicePixelRatio: number;
    readonly docShell: nsIDocShell | null;
    readonly document: Document | null;
    readonly event: Event | undefined;
    readonly external: External;
    readonly frameElement: Element | null;
    readonly frames: WindowProxy;
    fullScreen: boolean;
    readonly history: History;
    readonly innerHeight: number;
    readonly innerWidth: number;
    readonly intlUtils: IntlUtils;
    readonly isChromeWindow: boolean;
    readonly isFullyOccluded: boolean;
    readonly isInFullScreenTransition: boolean;
    readonly length: number;
    readonly location: Location;
    readonly locationbar: BarProp;
    readonly menubar: BarProp;
    readonly messageManager: ChromeMessageBroadcaster;
    readonly mozInnerScreenX: number;
    readonly mozInnerScreenY: number;
    name: string;
    readonly navigation: Navigation;
    readonly navigator: Navigator;
    ondevicelight: ((this: Window, ev: Event) => any) | null;
    ondevicemotion: ((this: Window, ev: Event) => any) | null;
    ondeviceorientation: ((this: Window, ev: Event) => any) | null;
    ondeviceorientationabsolute: ((this: Window, ev: Event) => any) | null;
    onorientationchange: ((this: Window, ev: Event) => any) | null;
    onuserproximity: ((this: Window, ev: Event) => any) | null;
    onvrdisplayactivate: ((this: Window, ev: Event) => any) | null;
    onvrdisplayconnect: ((this: Window, ev: Event) => any) | null;
    onvrdisplaydeactivate: ((this: Window, ev: Event) => any) | null;
    onvrdisplaydisconnect: ((this: Window, ev: Event) => any) | null;
    onvrdisplaypresentchange: ((this: Window, ev: Event) => any) | null;
    opener: any;
    readonly orientation: number;
    readonly outerHeight: number;
    readonly outerWidth: number;
    readonly pageXOffset: number;
    readonly pageYOffset: number;
    readonly paintWorklet: Worklet;
    readonly parent: WindowProxy | null;
    readonly performance: Performance | null;
    readonly personalbar: BarProp;
    readonly realFrameElement: Element | null;
    readonly screen: Screen;
    readonly screenEdgeSlopX: number;
    readonly screenEdgeSlopY: number;
    readonly screenLeft: number;
    readonly screenTop: number;
    readonly screenX: number;
    readonly screenY: number;
    readonly scrollMaxX: number;
    readonly scrollMaxY: number;
    readonly scrollMinX: number;
    readonly scrollMinY: number;
    readonly scrollX: number;
    readonly scrollY: number;
    readonly scrollbars: BarProp;
    readonly self: WindowProxy;
    status: string;
    readonly statusbar: BarProp;
    readonly toolbar: BarProp;
    readonly top: WindowProxy | null;
    readonly visualViewport: VisualViewport;
    readonly window: WindowProxy;
    readonly windowGlobalChild: WindowGlobalChild | null;
    readonly windowRoot: WindowRoot | null;
    readonly windowState: number;
    readonly windowUtils: nsIDOMWindowUtils;
    alert(): void;
    alert(message: string): void;
    blur(): void;
    cancelIdleCallback(handle: number): void;
    captureEvents(): void;
    close(): void;
    confirm(message?: string): boolean;
    dump(str: string): void;
    find(str?: string, caseSensitive?: boolean, backwards?: boolean, wrapAround?: boolean, wholeWord?: boolean, searchInFrames?: boolean, showDialog?: boolean): boolean;
    focus(): void;
    getAttention(): void;
    getAttentionWithCycleCount(aCycleCount: number): void;
    getComputedStyle(elt: Element, pseudoElt?: string | null): CSSStyleDeclaration | null;
    getDefaultComputedStyle(elt: Element, pseudoElt?: string): CSSStyleDeclaration | null;
    getGroupMessageManager(aGroup: string): ChromeMessageBroadcaster;
    getInterface(iid: any): any;
    getRegionalPrefsLocales(): string[];
    getSelection(): Selection | null;
    getWebExposedLocales(): string[];
    getWorkspaceID(): string;
    matchMedia(query: string): MediaQueryList | null;
    maximize(): void;
    minimize(): void;
    moveBy(x: number, y: number): void;
    moveTo(x: number, y: number): void;
    moveToWorkspace(workspaceID: string): void;
    mozScrollSnap(): void;
    notifyDefaultButtonLoaded(defaultButton: Element): void;
    open(url?: string | URL, target?: string, features?: string): WindowProxy | null;
    openDialog(url?: string, name?: string, options?: string, ...extraArguments: any[]): WindowProxy | null;
    postMessage(message: any, targetOrigin: string, transfer?: any[]): void;
    postMessage(message: any, options?: WindowPostMessageOptions): void;
    print(): void;
    printPreview(settings?: nsIPrintSettings | null, listener?: nsIWebProgressListener | null, docShellToPreviewInto?: nsIDocShell | null): WindowProxy | null;
    promiseDocumentFlushed(callback: PromiseDocumentFlushedCallback): Promise<any>;
    prompt(message?: string, _default?: string): string | null;
    releaseEvents(): void;
    requestIdleCallback(callback: IdleRequestCallback, options?: IdleRequestOptions): number;
    resizeBy(x: number, y: number): void;
    resizeTo(x: number, y: number): void;
    restore(): void;
    scroll(x: number, y: number): void;
    scroll(options?: ScrollToOptions): void;
    scrollBy(x: number, y: number): void;
    scrollBy(options?: ScrollToOptions): void;
    scrollByLines(numLines: number, options?: ScrollOptions): void;
    scrollByPages(numPages: number, options?: ScrollOptions): void;
    scrollTo(x: number, y: number): void;
    scrollTo(options?: ScrollToOptions): void;
    setCursor(cursor: string): void;
    setResizable(resizable: boolean): void;
    setScrollMarks(marks: number[], onHorizontalScrollbar?: boolean): void;
    shouldReportForServiceWorkerScope(aScope: string): boolean;
    sizeToContent(): void;
    sizeToContentConstrained(constraints?: SizeToContentConstraints): void;
    stop(): void;
    updateCommands(action: string): void;
    readonly STATE_MAXIMIZED: 1;
    readonly STATE_MINIMIZED: 2;
    readonly STATE_NORMAL: 3;
    readonly STATE_FULLSCREEN: 4;
    addEventListener<K extends keyof WindowEventMap>(type: K, listener: (this: Window, ev: WindowEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WindowEventMap>(type: K, listener: (this: Window, ev: WindowEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
    [name: string]: any;
}

declare var Window: {
    prototype: Window;
    new(): Window;
    readonly STATE_MAXIMIZED: 1;
    readonly STATE_MINIMIZED: 2;
    readonly STATE_NORMAL: 3;
    readonly STATE_FULLSCREEN: 4;
    isInstance: IsInstance<Window>;
};

interface WindowContext {
    allowJavascript: boolean;
    readonly browsingContext: BrowsingContext | null;
    readonly hasBeforeUnload: boolean;
    readonly innerWindowId: number;
    readonly isInBFCache: boolean;
    readonly isInProcess: boolean;
    readonly isLocalIP: boolean;
    readonly overriddenFingerprintingSettings: number | null;
    readonly parentWindowContext: WindowContext | null;
    readonly shouldResistFingerprinting: boolean;
    readonly topWindowContext: WindowContext;
    readonly windowGlobalChild: WindowGlobalChild | null;
}

declare var WindowContext: {
    prototype: WindowContext;
    new(): WindowContext;
    isInstance: IsInstance<WindowContext>;
};

interface WindowEventHandlersEventMap {
    "afterprint": Event;
    "beforeprint": Event;
    "beforeunload": Event;
    "gamepadconnected": Event;
    "gamepaddisconnected": Event;
    "hashchange": Event;
    "languagechange": Event;
    "message": Event;
    "messageerror": Event;
    "offline": Event;
    "online": Event;
    "pagehide": Event;
    "pageshow": Event;
    "popstate": Event;
    "rejectionhandled": Event;
    "storage": Event;
    "unhandledrejection": Event;
    "unload": Event;
}

interface WindowEventHandlers {
    onafterprint: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onbeforeprint: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onbeforeunload: ((this: WindowEventHandlers, ev: Event) => any) | null;
    ongamepadconnected: ((this: WindowEventHandlers, ev: Event) => any) | null;
    ongamepaddisconnected: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onhashchange: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onlanguagechange: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onmessage: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onmessageerror: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onoffline: ((this: WindowEventHandlers, ev: Event) => any) | null;
    ononline: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onpagehide: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onpageshow: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onpopstate: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onrejectionhandled: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onstorage: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onunhandledrejection: ((this: WindowEventHandlers, ev: Event) => any) | null;
    onunload: ((this: WindowEventHandlers, ev: Event) => any) | null;
    addEventListener<K extends keyof WindowEventHandlersEventMap>(type: K, listener: (this: WindowEventHandlers, ev: WindowEventHandlersEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WindowEventHandlersEventMap>(type: K, listener: (this: WindowEventHandlers, ev: WindowEventHandlersEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

interface WindowGlobalChild {
    readonly browsingContext: BrowsingContext;
    readonly contentParentId: number;
    readonly innerWindowId: number;
    readonly isClosed: boolean;
    readonly isCurrentGlobal: boolean;
    readonly isInProcess: boolean;
    readonly isProcessRoot: boolean;
    readonly outerWindowId: number;
    readonly parentActor: WindowGlobalParent | null;
    readonly sameOriginWithTop: boolean;
    readonly windowContext: WindowContext;
    findBrowsingContextWithName(name: string): BrowsingContext | null;
    getActor(name: string): JSWindowActorChild;
    getExistingActor(name: string): JSWindowActorChild | null;
}

declare var WindowGlobalChild: {
    prototype: WindowGlobalChild;
    new(): WindowGlobalChild;
    isInstance: IsInstance<WindowGlobalChild>;
    getByInnerWindowId(innerWIndowId: number): WindowGlobalChild | null;
};

interface WindowGlobalParent extends WindowContext {
    readonly childActor: WindowGlobalChild | null;
    readonly contentBlockingAllowListPrincipal: Principal | null;
    readonly contentBlockingEvents: number;
    readonly contentBlockingLog: string;
    readonly contentParentId: number;
    readonly cookieJarSettings: nsICookieJarSettings | null;
    readonly documentPrincipal: Principal;
    readonly documentStoragePrincipal: Principal;
    readonly documentTitle: string;
    readonly documentURI: URI | null;
    readonly domProcess: nsIDOMProcessParent | null;
    fullscreen: boolean;
    readonly isActiveInTab: boolean;
    readonly isClosed: boolean;
    readonly isCurrentGlobal: boolean;
    readonly isInitialDocument: boolean;
    readonly isProcessRoot: boolean;
    readonly osPid: number;
    readonly outerWindowId: number;
    readonly rootFrameLoader: FrameLoader | null;
    drawSnapshot(rect: DOMRect | null, scale: number, backgroundColor: string, resetScrollPosition?: boolean): Promise<ImageBitmap>;
    getActor(name: string): JSWindowActorParent;
    getExistingActor(name: string): JSWindowActorParent | null;
    hasActivePeerConnections(): boolean;
    permitUnload(action?: PermitUnloadAction, timeout?: number): Promise<boolean>;
}

declare var WindowGlobalParent: {
    prototype: WindowGlobalParent;
    new(): WindowGlobalParent;
    isInstance: IsInstance<WindowGlobalParent>;
    getByInnerWindowId(innerWindowId: number): WindowGlobalParent | null;
};

interface WindowLocalStorage {
    readonly localStorage: Storage | null;
}

interface WindowOrWorkerGlobalScope {
    readonly caches: CacheStorage;
    readonly crossOriginIsolated: boolean;
    readonly indexedDB: IDBFactory | null;
    readonly isSecureContext: boolean;
    readonly origin: string;
    readonly scheduler: Scheduler;
    readonly trustedTypes: TrustedTypePolicyFactory;
    atob(atob: string): string;
    btoa(btoa: string): string;
    clearInterval(handle?: number): void;
    clearTimeout(handle?: number): void;
    createImageBitmap(aImage: ImageBitmapSource, aOptions?: ImageBitmapOptions): Promise<ImageBitmap>;
    createImageBitmap(aImage: ImageBitmapSource, aSx: number, aSy: number, aSw: number, aSh: number, aOptions?: ImageBitmapOptions): Promise<ImageBitmap>;
    fetch(input: RequestInfo | URL, init?: RequestInit): Promise<Response>;
    queueMicrotask(callback: VoidFunction): void;
    reportError(e: any): void;
    setInterval(handler: Function, timeout?: number, ...arguments: any[]): number;
    setInterval(handler: string, timeout?: number, ...unused: any[]): number;
    setTimeout(handler: Function, timeout?: number, ...arguments: any[]): number;
    setTimeout(handler: string, timeout?: number, ...unused: any[]): number;
    structuredClone(value: any, options?: StructuredSerializeOptions): any;
}

interface WindowProxy {
}

interface WindowRoot extends EventTarget {
}

declare var WindowRoot: {
    prototype: WindowRoot;
    new(): WindowRoot;
    isInstance: IsInstance<WindowRoot>;
};

interface WindowSessionStorage {
    readonly sessionStorage: Storage | null;
}

interface WorkerEventMap extends AbstractWorkerEventMap {
    "message": Event;
    "messageerror": Event;
}

interface Worker extends EventTarget, AbstractWorker {
    onmessage: ((this: Worker, ev: Event) => any) | null;
    onmessageerror: ((this: Worker, ev: Event) => any) | null;
    postMessage(message: any, transfer: any[]): void;
    postMessage(message: any, aOptions?: StructuredSerializeOptions): void;
    terminate(): void;
    addEventListener<K extends keyof WorkerEventMap>(type: K, listener: (this: Worker, ev: WorkerEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof WorkerEventMap>(type: K, listener: (this: Worker, ev: WorkerEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var Worker: {
    prototype: Worker;
    new(scriptURL: string | URL, options?: WorkerOptions): Worker;
    isInstance: IsInstance<Worker>;
};

/** Available only in secure contexts. */
interface Worklet {
    addModule(moduleURL: string | URL, options?: WorkletOptions): Promise<void>;
}

declare var Worklet: {
    prototype: Worklet;
    new(): Worklet;
    isInstance: IsInstance<Worklet>;
};

interface WrapperCachedNonISupportsTestInterface {
}

declare var WrapperCachedNonISupportsTestInterface: {
    prototype: WrapperCachedNonISupportsTestInterface;
    new(): WrapperCachedNonISupportsTestInterface;
    isInstance: IsInstance<WrapperCachedNonISupportsTestInterface>;
};

interface WritableStream {
    readonly locked: boolean;
    abort(reason?: any): Promise<void>;
    close(): Promise<void>;
    getWriter(): WritableStreamDefaultWriter;
}

declare var WritableStream: {
    prototype: WritableStream;
    new(underlyingSink?: any, strategy?: QueuingStrategy): WritableStream;
    isInstance: IsInstance<WritableStream>;
};

interface WritableStreamDefaultController {
    readonly signal: AbortSignal;
    error(e?: any): void;
}

declare var WritableStreamDefaultController: {
    prototype: WritableStreamDefaultController;
    new(): WritableStreamDefaultController;
    isInstance: IsInstance<WritableStreamDefaultController>;
};

interface WritableStreamDefaultWriter {
    readonly closed: Promise<undefined>;
    readonly desiredSize: number | null;
    readonly ready: Promise<undefined>;
    abort(reason?: any): Promise<void>;
    close(): Promise<void>;
    releaseLock(): void;
    write(chunk?: any): Promise<void>;
}

declare var WritableStreamDefaultWriter: {
    prototype: WritableStreamDefaultWriter;
    new(stream: WritableStream): WritableStreamDefaultWriter;
    isInstance: IsInstance<WritableStreamDefaultWriter>;
};

interface XMLDocument extends Document {
    addEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: XMLDocument, ev: DocumentEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof DocumentEventMap>(type: K, listener: (this: XMLDocument, ev: DocumentEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XMLDocument: {
    prototype: XMLDocument;
    new(): XMLDocument;
    isInstance: IsInstance<XMLDocument>;
};

interface XMLHttpRequestEventMap extends XMLHttpRequestEventTargetEventMap {
    "readystatechange": Event;
}

interface XMLHttpRequest extends XMLHttpRequestEventTarget {
    readonly channel: MozChannel | null;
    readonly errorCode: number;
    readonly mozAnon: boolean;
    mozBackgroundRequest: boolean;
    readonly mozSystem: boolean;
    onreadystatechange: ((this: XMLHttpRequest, ev: Event) => any) | null;
    readonly readyState: number;
    readonly response: any;
    readonly responseText: string | null;
    responseType: XMLHttpRequestResponseType;
    readonly responseURL: string;
    readonly responseXML: Document | null;
    readonly status: number;
    readonly statusText: string;
    timeout: number;
    readonly upload: XMLHttpRequestUpload;
    withCredentials: boolean;
    abort(): void;
    getAllResponseHeaders(): string;
    getInterface(iid: any): any;
    getResponseHeader(header: string): string | null;
    open(method: string, url: string | URL): void;
    open(method: string, url: string | URL, async: boolean, user?: string | null, password?: string | null): void;
    overrideMimeType(mime: string): void;
    send(body?: Document | XMLHttpRequestBodyInit | null): void;
    sendInputStream(body: InputStream): void;
    setOriginAttributes(originAttributes?: OriginAttributesDictionary): void;
    setRequestHeader(header: string, value: string): void;
    readonly UNSENT: 0;
    readonly OPENED: 1;
    readonly HEADERS_RECEIVED: 2;
    readonly LOADING: 3;
    readonly DONE: 4;
    addEventListener<K extends keyof XMLHttpRequestEventMap>(type: K, listener: (this: XMLHttpRequest, ev: XMLHttpRequestEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XMLHttpRequestEventMap>(type: K, listener: (this: XMLHttpRequest, ev: XMLHttpRequestEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XMLHttpRequest: {
    prototype: XMLHttpRequest;
    new(params?: MozXMLHttpRequestParameters): XMLHttpRequest;
    new(ignored: string): XMLHttpRequest;
    readonly UNSENT: 0;
    readonly OPENED: 1;
    readonly HEADERS_RECEIVED: 2;
    readonly LOADING: 3;
    readonly DONE: 4;
    isInstance: IsInstance<XMLHttpRequest>;
};

interface XMLHttpRequestEventTargetEventMap {
    "abort": Event;
    "error": Event;
    "load": Event;
    "loadend": Event;
    "loadstart": Event;
    "progress": Event;
    "timeout": Event;
}

interface XMLHttpRequestEventTarget extends EventTarget {
    onabort: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    onerror: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    onload: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    onloadend: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    onloadstart: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    onprogress: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    ontimeout: ((this: XMLHttpRequestEventTarget, ev: Event) => any) | null;
    addEventListener<K extends keyof XMLHttpRequestEventTargetEventMap>(type: K, listener: (this: XMLHttpRequestEventTarget, ev: XMLHttpRequestEventTargetEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XMLHttpRequestEventTargetEventMap>(type: K, listener: (this: XMLHttpRequestEventTarget, ev: XMLHttpRequestEventTargetEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XMLHttpRequestEventTarget: {
    prototype: XMLHttpRequestEventTarget;
    new(): XMLHttpRequestEventTarget;
    isInstance: IsInstance<XMLHttpRequestEventTarget>;
};

interface XMLHttpRequestUpload extends XMLHttpRequestEventTarget {
    addEventListener<K extends keyof XMLHttpRequestEventTargetEventMap>(type: K, listener: (this: XMLHttpRequestUpload, ev: XMLHttpRequestEventTargetEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XMLHttpRequestEventTargetEventMap>(type: K, listener: (this: XMLHttpRequestUpload, ev: XMLHttpRequestEventTargetEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XMLHttpRequestUpload: {
    prototype: XMLHttpRequestUpload;
    new(): XMLHttpRequestUpload;
    isInstance: IsInstance<XMLHttpRequestUpload>;
};

interface XMLSerializer {
    serializeToStream(root: Node, stream: OutputStream, charset: string | null): void;
    serializeToString(root: Node): string;
}

declare var XMLSerializer: {
    prototype: XMLSerializer;
    new(): XMLSerializer;
    isInstance: IsInstance<XMLSerializer>;
};

interface XPathEvaluator extends XPathEvaluatorMixin {
}

declare var XPathEvaluator: {
    prototype: XPathEvaluator;
    new(): XPathEvaluator;
    isInstance: IsInstance<XPathEvaluator>;
};

interface XPathEvaluatorMixin {
    createExpression(expression: string, resolver?: XPathNSResolver | null): XPathExpression;
    createNSResolver(nodeResolver: Node): Node;
    evaluate(expression: string, contextNode: Node, resolver?: XPathNSResolver | null, type?: number, result?: any): XPathResult;
}

interface XPathExpression {
    evaluate(contextNode: Node, type?: number, result?: any): XPathResult;
    evaluateWithContext(contextNode: Node, contextPosition: number, contextSize: number, type?: number, result?: any): XPathResult;
}

declare var XPathExpression: {
    prototype: XPathExpression;
    new(): XPathExpression;
    isInstance: IsInstance<XPathExpression>;
};

interface XPathResult {
    readonly booleanValue: boolean;
    readonly invalidIteratorState: boolean;
    readonly numberValue: number;
    readonly resultType: number;
    readonly singleNodeValue: Node | null;
    readonly snapshotLength: number;
    readonly stringValue: string;
    iterateNext(): Node | null;
    snapshotItem(index: number): Node | null;
    readonly ANY_TYPE: 0;
    readonly NUMBER_TYPE: 1;
    readonly STRING_TYPE: 2;
    readonly BOOLEAN_TYPE: 3;
    readonly UNORDERED_NODE_ITERATOR_TYPE: 4;
    readonly ORDERED_NODE_ITERATOR_TYPE: 5;
    readonly UNORDERED_NODE_SNAPSHOT_TYPE: 6;
    readonly ORDERED_NODE_SNAPSHOT_TYPE: 7;
    readonly ANY_UNORDERED_NODE_TYPE: 8;
    readonly FIRST_ORDERED_NODE_TYPE: 9;
}

declare var XPathResult: {
    prototype: XPathResult;
    new(): XPathResult;
    readonly ANY_TYPE: 0;
    readonly NUMBER_TYPE: 1;
    readonly STRING_TYPE: 2;
    readonly BOOLEAN_TYPE: 3;
    readonly UNORDERED_NODE_ITERATOR_TYPE: 4;
    readonly ORDERED_NODE_ITERATOR_TYPE: 5;
    readonly UNORDERED_NODE_SNAPSHOT_TYPE: 6;
    readonly ORDERED_NODE_SNAPSHOT_TYPE: 7;
    readonly ANY_UNORDERED_NODE_TYPE: 8;
    readonly FIRST_ORDERED_NODE_TYPE: 9;
    isInstance: IsInstance<XPathResult>;
};

/** Available only in secure contexts. */
interface XRBoundedReferenceSpace extends XRReferenceSpace {
    readonly boundsGeometry: DOMPointReadOnly[];
    addEventListener<K extends keyof XRReferenceSpaceEventMap>(type: K, listener: (this: XRBoundedReferenceSpace, ev: XRReferenceSpaceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XRReferenceSpaceEventMap>(type: K, listener: (this: XRBoundedReferenceSpace, ev: XRReferenceSpaceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XRBoundedReferenceSpace: {
    prototype: XRBoundedReferenceSpace;
    new(): XRBoundedReferenceSpace;
    isInstance: IsInstance<XRBoundedReferenceSpace>;
};

/** Available only in secure contexts. */
interface XRFrame {
    readonly session: XRSession;
    getPose(space: XRSpace, baseSpace: XRSpace): XRPose | null;
    getViewerPose(referenceSpace: XRReferenceSpace): XRViewerPose | null;
}

declare var XRFrame: {
    prototype: XRFrame;
    new(): XRFrame;
    isInstance: IsInstance<XRFrame>;
};

/** Available only in secure contexts. */
interface XRInputSource {
    readonly gamepad: Gamepad | null;
    readonly gripSpace: XRSpace | null;
    readonly handedness: XRHandedness;
    readonly profiles: string[];
    readonly targetRayMode: XRTargetRayMode;
    readonly targetRaySpace: XRSpace;
}

declare var XRInputSource: {
    prototype: XRInputSource;
    new(): XRInputSource;
    isInstance: IsInstance<XRInputSource>;
};

/** Available only in secure contexts. */
interface XRInputSourceArray {
    readonly length: number;
    forEach(callbackfn: (value: XRInputSource, key: number, parent: XRInputSourceArray) => void, thisArg?: any): void;
    [index: number]: XRInputSource;
}

declare var XRInputSourceArray: {
    prototype: XRInputSourceArray;
    new(): XRInputSourceArray;
    isInstance: IsInstance<XRInputSourceArray>;
};

/** Available only in secure contexts. */
interface XRInputSourceEvent extends Event {
    readonly frame: XRFrame;
    readonly inputSource: XRInputSource;
}

declare var XRInputSourceEvent: {
    prototype: XRInputSourceEvent;
    new(type: string, eventInitDict: XRInputSourceEventInit): XRInputSourceEvent;
    isInstance: IsInstance<XRInputSourceEvent>;
};

/** Available only in secure contexts. */
interface XRInputSourcesChangeEvent extends Event {
    readonly added: XRInputSource[];
    readonly removed: XRInputSource[];
    readonly session: XRSession;
}

declare var XRInputSourcesChangeEvent: {
    prototype: XRInputSourcesChangeEvent;
    new(type: string, eventInitDict: XRInputSourcesChangeEventInit): XRInputSourcesChangeEvent;
    isInstance: IsInstance<XRInputSourcesChangeEvent>;
};

/** Available only in secure contexts. */
interface XRPose {
    readonly emulatedPosition: boolean;
    readonly transform: XRRigidTransform;
}

declare var XRPose: {
    prototype: XRPose;
    new(): XRPose;
    isInstance: IsInstance<XRPose>;
};

interface XRReferenceSpaceEventMap {
    "reset": Event;
}

/** Available only in secure contexts. */
interface XRReferenceSpace extends XRSpace {
    onreset: ((this: XRReferenceSpace, ev: Event) => any) | null;
    getOffsetReferenceSpace(originOffset: XRRigidTransform): XRReferenceSpace;
    addEventListener<K extends keyof XRReferenceSpaceEventMap>(type: K, listener: (this: XRReferenceSpace, ev: XRReferenceSpaceEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XRReferenceSpaceEventMap>(type: K, listener: (this: XRReferenceSpace, ev: XRReferenceSpaceEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XRReferenceSpace: {
    prototype: XRReferenceSpace;
    new(): XRReferenceSpace;
    isInstance: IsInstance<XRReferenceSpace>;
};

/** Available only in secure contexts. */
interface XRReferenceSpaceEvent extends Event {
    readonly referenceSpace: XRReferenceSpace;
    readonly transform: XRRigidTransform | null;
}

declare var XRReferenceSpaceEvent: {
    prototype: XRReferenceSpaceEvent;
    new(type: string, eventInitDict: XRReferenceSpaceEventInit): XRReferenceSpaceEvent;
    isInstance: IsInstance<XRReferenceSpaceEvent>;
};

/** Available only in secure contexts. */
interface XRRenderState {
    readonly baseLayer: XRWebGLLayer | null;
    readonly depthFar: number;
    readonly depthNear: number;
    readonly inlineVerticalFieldOfView: number | null;
}

declare var XRRenderState: {
    prototype: XRRenderState;
    new(): XRRenderState;
    isInstance: IsInstance<XRRenderState>;
};

/** Available only in secure contexts. */
interface XRRigidTransform {
    readonly inverse: XRRigidTransform;
    readonly matrix: Float32Array;
    readonly orientation: DOMPointReadOnly;
    readonly position: DOMPointReadOnly;
}

declare var XRRigidTransform: {
    prototype: XRRigidTransform;
    new(position?: DOMPointInit, orientation?: DOMPointInit): XRRigidTransform;
    isInstance: IsInstance<XRRigidTransform>;
};

interface XRSessionEventMap {
    "end": Event;
    "inputsourceschange": Event;
    "select": Event;
    "selectend": Event;
    "selectstart": Event;
    "squeeze": Event;
    "squeezeend": Event;
    "squeezestart": Event;
    "visibilitychange": Event;
}

/** Available only in secure contexts. */
interface XRSession extends EventTarget {
    readonly frameRate: number | null;
    readonly inputSources: XRInputSourceArray;
    onend: ((this: XRSession, ev: Event) => any) | null;
    oninputsourceschange: ((this: XRSession, ev: Event) => any) | null;
    onselect: ((this: XRSession, ev: Event) => any) | null;
    onselectend: ((this: XRSession, ev: Event) => any) | null;
    onselectstart: ((this: XRSession, ev: Event) => any) | null;
    onsqueeze: ((this: XRSession, ev: Event) => any) | null;
    onsqueezeend: ((this: XRSession, ev: Event) => any) | null;
    onsqueezestart: ((this: XRSession, ev: Event) => any) | null;
    onvisibilitychange: ((this: XRSession, ev: Event) => any) | null;
    readonly renderState: XRRenderState;
    readonly supportedFrameRates: Float32Array | null;
    readonly visibilityState: XRVisibilityState;
    cancelAnimationFrame(handle: number): void;
    end(): Promise<void>;
    requestAnimationFrame(callback: XRFrameRequestCallback): number;
    requestReferenceSpace(type: XRReferenceSpaceType): Promise<XRReferenceSpace>;
    updateRenderState(state?: XRRenderStateInit): void;
    updateTargetFrameRate(rate: number): Promise<void>;
    addEventListener<K extends keyof XRSessionEventMap>(type: K, listener: (this: XRSession, ev: XRSessionEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XRSessionEventMap>(type: K, listener: (this: XRSession, ev: XRSessionEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XRSession: {
    prototype: XRSession;
    new(): XRSession;
    isInstance: IsInstance<XRSession>;
};

/** Available only in secure contexts. */
interface XRSessionEvent extends Event {
    readonly session: XRSession;
}

declare var XRSessionEvent: {
    prototype: XRSessionEvent;
    new(type: string, eventInitDict: XRSessionEventInit): XRSessionEvent;
    isInstance: IsInstance<XRSessionEvent>;
};

/** Available only in secure contexts. */
interface XRSpace extends EventTarget {
}

declare var XRSpace: {
    prototype: XRSpace;
    new(): XRSpace;
    isInstance: IsInstance<XRSpace>;
};

interface XRSystemEventMap {
    "devicechange": Event;
}

/** Available only in secure contexts. */
interface XRSystem extends EventTarget {
    ondevicechange: ((this: XRSystem, ev: Event) => any) | null;
    isSessionSupported(mode: XRSessionMode): Promise<boolean>;
    requestSession(mode: XRSessionMode, options?: XRSessionInit): Promise<XRSession>;
    addEventListener<K extends keyof XRSystemEventMap>(type: K, listener: (this: XRSystem, ev: XRSystemEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XRSystemEventMap>(type: K, listener: (this: XRSystem, ev: XRSystemEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XRSystem: {
    prototype: XRSystem;
    new(): XRSystem;
    isInstance: IsInstance<XRSystem>;
};

/** Available only in secure contexts. */
interface XRView {
    readonly eye: XREye;
    readonly projectionMatrix: Float32Array;
    readonly transform: XRRigidTransform;
}

declare var XRView: {
    prototype: XRView;
    new(): XRView;
    isInstance: IsInstance<XRView>;
};

/** Available only in secure contexts. */
interface XRViewerPose extends XRPose {
    readonly views: XRView[];
}

declare var XRViewerPose: {
    prototype: XRViewerPose;
    new(): XRViewerPose;
    isInstance: IsInstance<XRViewerPose>;
};

/** Available only in secure contexts. */
interface XRViewport {
    readonly height: number;
    readonly width: number;
    readonly x: number;
    readonly y: number;
}

declare var XRViewport: {
    prototype: XRViewport;
    new(): XRViewport;
    isInstance: IsInstance<XRViewport>;
};

/** Available only in secure contexts. */
interface XRWebGLLayer {
    readonly antialias: boolean;
    readonly framebuffer: WebGLFramebuffer | null;
    readonly framebufferHeight: number;
    readonly framebufferWidth: number;
    readonly ignoreDepthValues: boolean;
    getViewport(view: XRView): XRViewport | null;
}

declare var XRWebGLLayer: {
    prototype: XRWebGLLayer;
    new(session: XRSession, context: XRWebGLRenderingContext, layerInit?: XRWebGLLayerInit): XRWebGLLayer;
    isInstance: IsInstance<XRWebGLLayer>;
    getNativeFramebufferScaleFactor(session: XRSession): number;
};

interface XSLTProcessor {
    flags: number;
    clearParameters(): void;
    getParameter(namespaceURI: string | null, localName: string): XSLTParameterValue | null;
    importStylesheet(style: Node): void;
    removeParameter(namespaceURI: string | null, localName: string): void;
    reset(): void;
    setParameter(namespaceURI: string | null, localName: string, value: XSLTParameterValue): void;
    transformToDocument(source: Node): Document;
    transformToFragment(source: Node, output: Document): DocumentFragment;
    readonly DISABLE_ALL_LOADS: 1;
}

declare var XSLTProcessor: {
    prototype: XSLTProcessor;
    new(): XSLTProcessor;
    readonly DISABLE_ALL_LOADS: 1;
    isInstance: IsInstance<XSLTProcessor>;
};

interface XULCommandDispatcher {
}

interface XULCommandEvent extends UIEvent {
    readonly altKey: boolean;
    readonly button: number;
    readonly ctrlKey: boolean;
    readonly inputSource: number;
    readonly metaKey: boolean;
    readonly shiftKey: boolean;
    readonly sourceEvent: Event | null;
    initCommandEvent(type: string, canBubble?: boolean, cancelable?: boolean, view?: Window | null, detail?: number, ctrlKey?: boolean, altKey?: boolean, shiftKey?: boolean, metaKey?: boolean, buttonArg?: number, sourceEvent?: Event | null, inputSource?: number): void;
}

declare var XULCommandEvent: {
    prototype: XULCommandEvent;
    new(): XULCommandEvent;
    isInstance: IsInstance<XULCommandEvent>;
};

interface XULControllers {
}

interface XULElementEventMap extends ElementEventMap, GlobalEventHandlersEventMap, OnErrorEventHandlerForNodesEventMap, TouchEventHandlersEventMap {
}

interface XULElement extends Element, ElementCSSInlineStyle, GlobalEventHandlers, HTMLOrForeignElement, OnErrorEventHandlerForNodes, TouchEventHandlers {
    collapsed: boolean;
    contextMenu: string;
    readonly controllers: XULControllers;
    hidden: boolean;
    menu: string;
    observes: string;
    src: string;
    tooltip: string;
    tooltipText: string;
    click(): void;
    doCommand(): void;
    hasMenu(): boolean;
    openMenu(open: boolean): void;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULElement: {
    prototype: XULElement;
    new(): XULElement;
    isInstance: IsInstance<XULElement>;
};

interface XULFrameElement extends XULElement, MozFrameLoaderOwner {
    readonly browserId: number;
    readonly contentDocument: Document | null;
    readonly contentWindow: WindowProxy | null;
    readonly docShell: nsIDocShell | null;
    openWindowInfo: nsIOpenWindowInfo | null;
    readonly webNavigation: nsIWebNavigation | null;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULFrameElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULFrameElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULFrameElement: {
    prototype: XULFrameElement;
    new(): XULFrameElement;
    isInstance: IsInstance<XULFrameElement>;
};

interface XULMenuElement extends XULElement {
    activeChild: Element | null;
    readonly openedWithKey: boolean;
    handleKeyPress(keyEvent: KeyboardEvent): boolean;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULMenuElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULMenuElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULMenuElement: {
    prototype: XULMenuElement;
    new(): XULMenuElement;
    isInstance: IsInstance<XULMenuElement>;
};

interface XULPopupElement extends XULElement {
    readonly anchorNode: Element | null;
    readonly isWaylandDragSource: boolean;
    readonly isWaylandPopup: boolean;
    label: string;
    position: string;
    readonly state: string;
    readonly triggerNode: Node | null;
    activateItem(itemElement: Element, options?: ActivateMenuItemOptions): void;
    getOuterScreenRect(): DOMRect;
    hidePopup(cancel?: boolean): void;
    moveTo(left: number, top: number): void;
    moveToAnchor(anchorElement?: Element | null, position?: string, x?: number, y?: number, attributesOverride?: boolean): void;
    openPopup(anchorElement?: Element | null, options?: StringOrOpenPopupOptions, x?: number, y?: number, isContextMenu?: boolean, attributesOverride?: boolean, triggerEvent?: Event | null): void;
    openPopupAtScreen(x?: number, y?: number, isContextMenu?: boolean, triggerEvent?: Event | null): void;
    openPopupAtScreenRect(position?: string, x?: number, y?: number, width?: number, height?: number, isContextMenu?: boolean, attributesOverride?: boolean, triggerEvent?: Event | null): void;
    setConstraintRect(rect: DOMRectReadOnly): void;
    sizeTo(width: number, height: number): void;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULPopupElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULPopupElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULPopupElement: {
    prototype: XULPopupElement;
    new(): XULPopupElement;
    isInstance: IsInstance<XULPopupElement>;
};

interface XULResizerElement extends XULElement {
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULResizerElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULResizerElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULResizerElement: {
    prototype: XULResizerElement;
    new(): XULResizerElement;
    isInstance: IsInstance<XULResizerElement>;
};

interface XULTextElement extends XULElement {
    accessKey: string;
    disabled: boolean;
    value: string;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULTextElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULTextElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULTextElement: {
    prototype: XULTextElement;
    new(): XULTextElement;
    isInstance: IsInstance<XULTextElement>;
};

interface XULTreeElement extends XULElement {
    readonly columns: TreeColumns | null;
    focused: boolean;
    readonly horizontalPosition: number;
    readonly rowHeight: number;
    readonly rowWidth: number;
    readonly treeBody: Element | null;
    view: MozTreeView | null;
    beginUpdateBatch(): void;
    clearStyleAndImageCaches(): void;
    endUpdateBatch(): void;
    ensureCellIsVisible(row: number, col: TreeColumn | null): void;
    ensureRowIsVisible(index: number): void;
    getCellAt(x: number, y: number): TreeCellInfo;
    getCoordsForCellItem(row: number, col: TreeColumn, element: string): DOMRect | null;
    getFirstVisibleRow(): number;
    getLastVisibleRow(): number;
    getPageLength(): number;
    getRowAt(x: number, y: number): number;
    invalidate(): void;
    invalidateCell(row: number, col: TreeColumn | null): void;
    invalidateColumn(col: TreeColumn | null): void;
    invalidateRange(startIndex: number, endIndex: number): void;
    invalidateRow(index: number): void;
    isCellCropped(row: number, col: TreeColumn | null): boolean;
    removeImageCacheEntry(row: number, col: TreeColumn): void;
    rowCountChanged(index: number, count: number): void;
    scrollByLines(numLines: number): void;
    scrollByPages(numPages: number): void;
    scrollToRow(index: number): void;
    addEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULTreeElement, ev: XULElementEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof XULElementEventMap>(type: K, listener: (this: XULTreeElement, ev: XULElementEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var XULTreeElement: {
    prototype: XULTreeElement;
    new(): XULTreeElement;
    isInstance: IsInstance<XULTreeElement>;
};

interface imgINotificationObserver {
}

interface imgIRequest {
}

interface nsIBrowserDOMWindow {
}

interface nsICookieJarSettings {
}

interface nsIDOMProcessChild {
}

interface nsIDOMProcessParent {
}

interface nsIDOMWindowUtils {
}

interface nsIDocShell {
}

interface nsIEditor {
}

interface nsIEventTarget {
}

interface nsIFile {
}

interface nsIGleanPing {
}

interface nsILoadGroup {
}

interface nsIMediaDevice {
}

interface nsIOpenWindowInfo {
}

interface nsIPermissionDelegateHandler {
}

interface nsIPrintSettings {
}

interface nsIReferrerInfo {
}

interface nsISHEntry {
}

interface nsISHistory {
}

interface nsIScreen {
}

interface nsISecureBrowserUI {
}

interface nsISelectionListener {
}

interface nsISessionStoreRestoreData {
}

interface nsISocketTransport {
}

interface nsIStreamListener {
}

interface nsISupports {
}

interface nsITransportProvider {
}

interface nsITreeSelection {
}

interface nsIWebBrowserPersistDocumentReceiver {
}

interface nsIWebNavigation {
}

interface nsIWebProgress {
}

interface nsIWebProgressListener {
}

declare namespace AddonManagerPermissions {
    function isHostPermitted(host: string): boolean;
}

declare namespace APZHitResultFlags {
}

interface Console {
    assert(condition?: boolean, ...data: any[]): void;
    clear(): void;
    count(label?: string): void;
    countReset(label?: string): void;
    createInstance(options?: ConsoleInstanceOptions): ConsoleInstance;
    debug(...data: any[]): void;
    dir(...data: any[]): void;
    dirxml(...data: any[]): void;
    error(...data: any[]): void;
    exception(...data: any[]): void;
    group(...data: any[]): void;
    groupCollapsed(...data: any[]): void;
    groupEnd(): void;
    info(...data: any[]): void;
    log(...data: any[]): void;
    profile(...data: any[]): void;
    profileEnd(...data: any[]): void;
    table(...data: any[]): void;
    time(label?: string): void;
    timeEnd(label?: string): void;
    timeLog(label?: string, ...data: any[]): void;
    timeStamp(data?: any): void;
    trace(...data: any[]): void;
    warn(...data: any[]): void;
}

declare var console: Console;

declare namespace CSS {
    var highlights: HighlightRegistry;
    function escape(ident: string): string;
    function registerProperty(definition: PropertyDefinition): void;
    function supports(property: string, value: string): boolean;
    function supports(conditionText: string): boolean;
}

declare namespace FuzzingFunctions {
    function crash(reason?: string): void;
    function cycleCollect(): void;
    function enableAccessibility(): void;
    function garbageCollect(): void;
    function garbageCollectCompacting(): void;
    function memoryPressure(): void;
    function signalIPCReady(): void;
    function synthesizeKeyboardEvents(aKeyValue: string, aDictionary?: KeyboardEventInit): void;
}

declare namespace Nyx {
    function isEnabled(aFuzzerName: string): boolean;
    function isReplay(): boolean;
    function isStarted(): boolean;
    function start(): void;
    function release(): void;
    function getRawData(aDst: Uint8Array): void;
}

declare namespace TestUtils {
    function gc(): Promise<void>;
}

declare namespace GPUBufferUsage {
}

declare namespace GPUMapMode {
}

declare namespace GPUTextureUsage {
}

declare namespace GPUShaderStage {
}

declare namespace GPUColorWrite {
}

declare namespace WebrtcGlobalInformation {
    var aecDebug: boolean;
    var aecDebugLogDir: string;
    function clearAllStats(): void;
    function clearLogging(): void;
    function getAllStats(callback: WebrtcGlobalStatisticsCallback, pcIdFilter?: string): void;
    function getLogging(pattern: string, callback: WebrtcGlobalLoggingCallback): void;
    function getMediaContext(): WebrtcGlobalMediaContext;
    function getStatsHistoryPcIds(callback: WebrtcGlobalStatisticsHistoryPcIdsCallback): void;
    function getStatsHistorySince(callback: WebrtcGlobalStatisticsHistoryCallback, pcIdFilter: string, after?: DOMHighResTimeStamp, sdpAfter?: DOMHighResTimeStamp): void;
}

declare namespace ChromeUtils {
    var aliveUtilityProcesses: number;
    var domProcessChild: nsIDOMProcessChild | null;
    var recentJSDevError: any;
    function CreateOriginAttributesFromOriginSuffix(suffix: string): OriginAttributesDictionary;
    function addProfilerMarker(name: string, options?: ProfilerMarkerOptions | DOMHighResTimeStamp, text?: string): void;
    function base64URLDecode(string: string, options: Base64URLDecodeOptions): ArrayBuffer;
    function base64URLEncode(source: BufferSource, options: Base64URLEncodeOptions): string;
    function clearRecentJSDevError(): void;
    function clearScriptCache(): void;
    function clearScriptCacheByBaseDomain(baseDomain: string): void;
    function clearScriptCacheByPrincipal(principal: Principal): void;
    function clearStyleSheetCache(): void;
    function clearStyleSheetCacheByBaseDomain(baseDomain: string): void;
    function clearStyleSheetCacheByPrincipal(principal: Principal): void;
    function collectPerfStats(): Promise<string>;
    function collectScrollingData(): Promise<InteractionData>;
    function compileScript(url: string, options?: CompileScriptOptionsDictionary): Promise<PrecompiledScript>;
    function consumeInteractionData(): Record<string, InteractionData>;
    function createError(message: string, stack?: any): any;
    function createOriginAttributesFromOrigin(origin: string): OriginAttributesDictionary;
    function dateNow(): number;
    function defineESModuleGetters(aTarget: any, aModules: any, aOptions?: ImportESModuleOptionsDictionary): void;
    function defineLazyGetter(aTarget: any, aName: any, aLambda: any): void;
    function defineModuleGetter(target: any, id: string, resourceURI: string): void;
    function endWheelTransaction(): void;
    function ensureJSOracleStarted(): void;
    function fillNonDefaultOriginAttributes(originAttrs?: OriginAttributesDictionary): OriginAttributesDictionary;
    function generateQI(interfaces: any[]): MozQueryInterface;
    function getAllDOMProcesses(): nsIDOMProcessParent[];
    function getAllPossibleUtilityActorNames(): string[];
    function getBaseDomainFromPartitionKey(partitionKey: string): string;
    function getCallerLocation(principal: Principal): any;
    function getClassName(obj: any, unwrap?: boolean): string;
    function getFormAutofillConfidences(elements: Element[]): FormAutofillConfidences[];
    function getGMPContentDecryptionModuleInformation(): Promise<CDMInformation[]>;
    function getLibcConstants(): LibcConstants;
    function getObjectNodeId(obj: any): NodeId;
    function getPartitionKeyFromURL(url: string): string;
    function getPopupControlState(): PopupBlockerState;
    function getWMFContentDecryptionModuleInformation(): Promise<CDMInformation[]>;
    function getXPCOMErrorName(aErrorCode: number): string;
    function hasReportingHeaderForOrigin(aOrigin: string): boolean;
    function idleDispatch(callback: IdleRequestCallback, options?: IdleRequestOptions): void;
    function import_(aResourceURI: string, aTargetObj?: any): any;
    function importESModule(aResourceURI: string, aOptions?: ImportESModuleOptionsDictionary): any;
    function isClassifierBlockingErrorCode(aError: number): boolean;
    function isDOMObject(obj: any, unwrap?: boolean): boolean;
    function isDarkBackground(element: Element): boolean;
    function isDevToolsOpened(): boolean;
    function isISOStyleDate(str: string): boolean;
    function isOriginAttributesEqual(aA?: OriginAttributesDictionary, aB?: OriginAttributesDictionary): boolean;
    function lastExternalProtocolIframeAllowed(): number;
    function nondeterministicGetWeakMapKeys(map: any): any;
    function nondeterministicGetWeakSetKeys(aSet: any): any;
    function notifyDevToolsClosed(): void;
    function notifyDevToolsOpened(): void;
    function originAttributesMatchPattern(originAttrs?: OriginAttributesDictionary, pattern?: OriginAttributesPatternDictionary): boolean;
    function originAttributesToSuffix(originAttrs?: OriginAttributesDictionary): string;
    function privateNoteIntentionalCrash(): void;
    function readHeapSnapshot(filePath: string): HeapSnapshot;
    function registerProcessActor(aName: string, aOptions?: ProcessActorOptions): void;
    function registerWindowActor(aName: string, aOptions?: WindowActorOptions): void;
    function releaseAssert(condition: boolean, message?: string): void;
    function requestProcInfo(): Promise<ParentProcInfoDictionary>;
    function resetLastExternalProtocolIframeAllowed(): void;
    function saveHeapSnapshot(boundaries?: HeapSnapshotBoundaries): string;
    function saveHeapSnapshotGetId(boundaries?: HeapSnapshotBoundaries): string;
    function setPerfStatsCollectionMask(aCollectionMask: number): void;
    function shallowClone(obj: any, target?: any): any;
    function shouldResistFingerprinting(target: JSRFPTarget, overriddenFingerprintingSettings: number | null): boolean;
    function unregisterProcessActor(aName: string): void;
    function unregisterWindowActor(aName: string): void;
    function unwaiveXrays(val: any): any;
    function vsyncEnabled(): boolean;
    function waiveXrays(val: any): any;
}

declare namespace InspectorUtils {
    function addPseudoClassLock(element: Element, pseudoClass: string, enabled?: boolean): void;
    function clearPseudoClassLocks(element: Element): void;
    function colorTo(fromColor: string, toColorSpace: string): InspectorColorToResult | null;
    function colorToRGBA(colorString: string, doc?: Document | null): InspectorRGBATuple | null;
    function containingBlockOf(element: Element): Element | null;
    function cssPropertyIsShorthand(property: string): boolean;
    function cssPropertySupportsType(property: string, type: InspectorPropertyType): boolean;
    function getAllStyleSheets(document: Document, documentOnly?: boolean): StyleSheet[];
    function getBlockLineCounts(element: Element): number[] | null;
    function getCSSPropertyNames(options?: PropertyNamesOptions): string[];
    function getCSSPropertyPrefs(): PropertyPref[];
    function getCSSPseudoElementNames(): string[];
    function getCSSRegisteredProperties(document: Document): InspectorCSSPropertyDefinition[];
    function getCSSRegisteredProperty(document: Document, name: string): InspectorCSSPropertyDefinition | null;
    function getCSSStyleRules(element: Element, pseudo?: string, relevantLinkVisited?: boolean, withStartingStyle?: boolean): CSSStyleRule[];
    function getCSSValuesForProperty(property: string): string[];
    function getChildrenForNode(node: Node, showingAnonymousContent: boolean, includeAssignedNodes: boolean): Node[];
    function getContentState(element: Element): number;
    function getOverflowingChildrenOfElement(element: Element): NodeList;
    function getParentForNode(node: Node, showingAnonymousContent: boolean): Node | null;
    function getRegisteredCssHighlights(document: Document, activeOnly?: boolean): string[];
    function getRelativeRuleLine(rule: CSSRule): number;
    function getRuleBodyText(initialText: string): string | null;
    function getRuleColumn(rule: CSSRule): number;
    function getRuleIndex(rule: CSSRule): number[];
    function getRuleLine(rule: CSSRule): number;
    function getStyleSheetRuleCountAndAtRules(sheet: CSSStyleSheet): InspectorStyleSheetRuleCountAndAtRulesResult;
    function getSubpropertiesForCSSProperty(property: string): string[];
    function getUsedFontFaces(range: Range, maxRanges?: number, skipCollapsedWhitespace?: boolean): InspectorFontFace[];
    function hasPseudoClassLock(element: Element, pseudoClass: string): boolean;
    function hasRulesModifiedByCSSOM(sheet: CSSStyleSheet): boolean;
    function isCustomElementName(name: string | null, namespaceURI: string | null): boolean;
    function isElementThemed(element: Element): boolean;
    function isIgnorableWhitespace(dataNode: CharacterData): boolean;
    function isInheritedProperty(document: Document, property: string): boolean;
    function isUsedColorSchemeDark(element: Element): boolean;
    function isValidCSSColor(colorString: string): boolean;
    function parseStyleSheet(sheet: CSSStyleSheet, input: string): void;
    function removeContentState(element: Element, state: number, clearActiveDocument?: boolean): boolean;
    function removePseudoClassLock(element: Element, pseudoClass: string): void;
    function replaceBlockRuleBodyTextInStylesheet(styleSheetText: string, line: number, column: number, newBodyText: string): string | null;
    function rgbToColorName(r: number, g: number, b: number): string;
    function setContentState(element: Element, state: number): boolean;
    function supports(conditionText: string, options?: SupportsOptions): boolean;
    function valueMatchesSyntax(document: Document, value: string, syntax: string): boolean;
}

declare namespace IOUtils {
    var profileBeforeChange: any;
    var sendTelemetry: any;
    function computeHexDigest(path: string, method: HashAlgorithm): Promise<string>;
    function copy(sourcePath: string, destPath: string, options?: CopyOptions): Promise<void>;
    function createUniqueDirectory(parent: string, prefix: string, permissions?: number): Promise<string>;
    function createUniqueFile(parent: string, prefix: string, permissions?: number): Promise<string>;
    function delMacXAttr(path: string, attr: string): Promise<void>;
    function exists(path: string): Promise<boolean>;
    function getChildren(path: string, options?: GetChildrenOptions): Promise<string[]>;
    function getDirectory(...components: string[]): Promise<nsIFile>;
    function getFile(...components: string[]): Promise<nsIFile>;
    function getMacXAttr(path: string, attr: string): Promise<Uint8Array>;
    function getWindowsAttributes(path: string): Promise<WindowsFileAttributes>;
    function hasMacXAttr(path: string, attr: string): Promise<boolean>;
    function makeDirectory(path: string, options?: MakeDirectoryOptions): Promise<void>;
    function move(sourcePath: string, destPath: string, options?: MoveOptions): Promise<void>;
    function read(path: string, opts?: ReadOptions): Promise<Uint8Array>;
    function readJSON(path: string, opts?: ReadUTF8Options): Promise<any>;
    function readUTF8(path: string, opts?: ReadUTF8Options): Promise<string>;
    function remove(path: string, options?: RemoveOptions): Promise<void>;
    function setAccessTime(path: string, access?: number): Promise<number>;
    function setMacXAttr(path: string, attr: string, value: Uint8Array): Promise<void>;
    function setModificationTime(path: string, modification?: number): Promise<number>;
    function setPermissions(path: string, permissions: number, honorUmask?: boolean): Promise<void>;
    function setWindowsAttributes(path: string, attrs?: WindowsFileAttributes): Promise<void>;
    function stat(path: string): Promise<FileInfo>;
    function write(path: string, data: Uint8Array, options?: WriteOptions): Promise<number>;
    function writeJSON(path: string, value: any, options?: WriteOptions): Promise<number>;
    function writeUTF8(path: string, string: string, options?: WriteOptions): Promise<number>;
}

declare namespace L10nOverlays {
    function translateElement(element: Element, translation?: L10nMessage): L10nOverlaysError[] | null;
}

declare namespace MediaControlService {
    function generateMediaControlKey(aKey: MediaControlKey, aSeekValue?: double): void;
    function getCurrentActiveMediaMetadata(): MediaMetadataInit;
    function getCurrentMediaSessionPlaybackState(): MediaSessionPlaybackState;
}

declare namespace PathUtils {
    var localProfileDir: string;
    var profileDir: string;
    var tempDir: string;
    var xulLibraryPath: string;
    function filename(path: string): string;
    function isAbsolute(path: string): boolean;
    function join(...components: string[]): string;
    function joinRelative(base: string, relativePath: string): string;
    function normalize(path: string): string;
    function parent(path: string, depth?: number): string | null;
    function split(path: string): string[];
    function splitRelative(path: string, options?: SplitRelativeOptions): string[];
    function toExtendedWindowsPath(path: string): string;
    function toFileURI(path: string): string;
}

declare namespace PlacesObservers {
    var counts: PlacesEventCounts;
    function addListener(eventTypes: PlacesEventType[], listener: PlacesEventCallback): void;
    function addListener(eventTypes: PlacesEventType[], listener: PlacesWeakCallbackWrapper): void;
    function notifyListeners(events: PlacesEvent[]): void;
    function removeListener(eventTypes: PlacesEventType[], listener: PlacesEventCallback): void;
    function removeListener(eventTypes: PlacesEventType[], listener: PlacesWeakCallbackWrapper): void;
}

declare namespace PromiseDebugging {
    function addUncaughtRejectionObserver(o: UncaughtRejectionObserver): void;
    function getAllocationStack(p: any): any;
    function getFullfillmentStack(p: any): any;
    function getPromiseID(p: any): string;
    function getRejectionStack(p: any): any;
    function getState(p: any): PromiseDebuggingStateHolder;
    function removeUncaughtRejectionObserver(o: UncaughtRejectionObserver): boolean;
}

declare namespace SessionStoreUtils {
    function addDynamicFrameFilteredListener(target: EventTarget, type: string, listener: any, useCapture: boolean, mozSystemGroup?: boolean): nsISupports | null;
    function collectDocShellCapabilities(docShell: nsIDocShell): string;
    function collectFormData(window: WindowProxy): CollectedData | null;
    function collectScrollPosition(window: WindowProxy): CollectedData | null;
    function constructSessionStoreRestoreData(): nsISessionStoreRestoreData;
    function forEachNonDynamicChildFrame(window: WindowProxy, callback: SessionStoreUtilsFrameCallback): void;
    function initializeRestore(browsingContext: CanonicalBrowsingContext, data: nsISessionStoreRestoreData | null): Promise<void>;
    function removeDynamicFrameFilteredListener(target: EventTarget, type: string, listener: nsISupports, useCapture: boolean, mozSystemGroup?: boolean): void;
    function restoreDocShellCapabilities(docShell: nsIDocShell, disallowCapabilities: string): void;
    function restoreDocShellState(browsingContext: CanonicalBrowsingContext, url: string | null, docShellCaps: string | null): Promise<void>;
    function restoreFormData(document: Document, data?: CollectedData): boolean;
    function restoreScrollPosition(frame: Window, data?: CollectedData): void;
    function restoreSessionStorageFromParent(browsingContext: CanonicalBrowsingContext, sessionStorage: Record<string, Record<string, string>>): void;
}

declare namespace TelemetryStopwatch {
    function cancel(histogram: HistogramID, obj?: any): boolean;
    function cancelKeyed(histogram: HistogramID, key: HistogramKey, obj?: any): boolean;
    function finish(histogram: HistogramID, obj?: any, canceledOkay?: boolean): boolean;
    function finishKeyed(histogram: HistogramID, key: HistogramKey, obj?: any, canceledOkay?: boolean): boolean;
    function running(histogram: HistogramID, obj?: any): boolean;
    function runningKeyed(histogram: HistogramID, key: HistogramKey, obj?: any): boolean;
    function setTestModeEnabled(testing?: boolean): void;
    function start(histogram: HistogramID, obj?: any, options?: TelemetryStopwatchOptions): boolean;
    function startKeyed(histogram: HistogramID, key: HistogramKey, obj?: any, options?: TelemetryStopwatchOptions): boolean;
    function timeElapsed(histogram: HistogramID, obj?: any, canceledOkay?: boolean): number;
    function timeElapsedKeyed(histogram: HistogramID, key: HistogramKey, obj?: any, canceledOkay?: boolean): number;
}

declare namespace UniFFIScaffolding {
    function callAsync(id: UniFFIFunctionId, ...args: UniFFIScaffoldingValue[]): Promise<UniFFIScaffoldingCallResult>;
    function callSync(id: UniFFIFunctionId, ...args: UniFFIScaffoldingValue[]): UniFFIScaffoldingCallResult;
    function deregisterCallbackHandler(interfaceId: UniFFICallbackInterfaceId): void;
    function readPointer(id: UniFFIPointerId, buff: ArrayBuffer, position: number): UniFFIPointer;
    function registerCallbackHandler(interfaceId: UniFFICallbackInterfaceId, handler: UniFFICallbackHandler): void;
    function writePointer(id: UniFFIPointerId, ptr: UniFFIPointer, buff: ArrayBuffer, position: number): void;
}

declare namespace UserInteraction {
    function cancel(id: string, obj?: any): boolean;
    function finish(id: string, obj?: any, additionalText?: string): boolean;
    function running(id: string, obj?: any): boolean;
    function start(id: string, value: string, obj?: any): boolean;
    function update(id: string, value: string, obj?: any): boolean;
}

interface AnyCallback {
    (value: any): any;
}

interface AudioDataOutputCallback {
    (output: AudioData): void;
}

interface BlobCallback {
    (blob: Blob | null): void;
}

interface ChainedOperation {
    (): any;
}

interface ConsoleInstanceDumpCallback {
    (message: string): void;
}

interface CreateHTMLCallback {
    (input: string, ...arguments: any[]): string | null;
}

interface CreateScriptCallback {
    (input: string, ...arguments: any[]): string | null;
}

interface CreateScriptURLCallback {
    (input: string, ...arguments: any[]): string | null;
}

interface CustomElementConstructor {
    (): any;
}

interface CustomElementCreationCallback {
    (name: string): void;
}

interface DebuggerNotificationCallback {
    (n: DebuggerNotification): void;
}

interface DecodeErrorCallback {
    (error: DOMException): void;
}

interface DecodeSuccessCallback {
    (decodedData: AudioBuffer): void;
}

interface EncodedAudioChunkOutputCallback {
    (output: EncodedAudioChunk, metadata?: EncodedAudioChunkMetadata): void;
}

interface EncodedVideoChunkOutputCallback {
    (chunk: EncodedVideoChunk, metadata?: EncodedVideoChunkMetadata): void;
}

interface ErrorCallback {
    (err: DOMException): void;
}

interface FileCallback {
    (file: File): void;
}

interface FileSystemEntriesCallback {
    (entries: FileSystemEntry[]): void;
}

interface FileSystemEntryCallback {
    (entry: FileSystemEntry): void;
}

interface FontFaceSetForEachCallback {
    (value: FontFace, key: FontFace, set: FontFaceSet): void;
}

interface FrameRequestCallback {
    (time: DOMHighResTimeStamp): void;
}

interface FunctionStringCallback {
    (data: string): void;
}

interface GenerateAssertionCallback {
    (contents: string, origin: string, options: RTCIdentityProviderOptions): RTCIdentityAssertionResult | PromiseLike<RTCIdentityAssertionResult>;
}

interface IdleRequestCallback {
    (deadline: IdleDeadline): void;
}

interface InstallTriggerCallback {
    (url: string, status: number): void;
}

interface IntersectionCallback {
    (entries: IntersectionObserverEntry[], observer: IntersectionObserver): void;
}

interface LockGrantedCallback {
    (lock: Lock | null): any;
}

interface MediaSessionActionHandler {
    (details: MediaSessionActionDetails): void;
}

interface MutationCallback {
    (mutations: MutationRecord[], observer: MutationObserver): void;
}

interface NavigationInterceptHandler {
    (): void | PromiseLike<void>;
}

interface NavigatorUserMediaErrorCallback {
    (error: MediaStreamError): void;
}

interface NavigatorUserMediaSuccessCallback {
    (stream: MediaStream): void;
}

interface NotificationPermissionCallback {
    (permission: NotificationPermission): void;
}

interface OnBeforeUnloadEventHandlerNonNull {
    (event: Event): string | null;
}

interface OnErrorEventHandlerNonNull {
    (event: Event | string, source?: string, lineno?: number, column?: number, error?: any): any;
}

interface PeerConnectionLifecycleCallback {
    (pc: RTCPeerConnection, windowId: number, eventType: RTCLifecycleEvent): void;
}

interface PerformanceObserverCallback {
    (entries: PerformanceObserverEntryList, observer: PerformanceObserver): void;
}

interface PlacesEventCallback {
    (events: PlacesEvent[]): void;
}

interface PositionCallback {
    (position: GeolocationPosition): void;
}

interface PositionErrorCallback {
    (positionError: GeolocationPositionError): void;
}

interface PrintCallback {
    (ctx: MozCanvasPrintState): void;
}

interface PromiseDocumentFlushedCallback {
    (): any;
}

interface PromiseReturner {
    (): any;
}

interface QueuingStrategySize {
    (chunk?: any): number;
}

interface RTCPeerConnectionErrorCallback {
    (error: DOMException): void;
}

interface RTCSessionDescriptionCallback {
    (description: RTCSessionDescriptionInit): void;
}

interface ReportingObserverCallback {
    (reports: Report[], observer: ReportingObserver): void;
}

interface ResizeObserverCallback {
    (entries: ResizeObserverEntry[], observer: ResizeObserver): void;
}

interface SchedulerPostTaskCallback {
    (): any;
}

interface SessionStoreUtilsFrameCallback {
    (frame: WindowProxy, index: number): void;
}

interface SetDeleteBooleanCallback {
    (value: boolean, index: number): void;
}

interface SetDeleteInterfaceCallback {
    (value: TestInterfaceObservableArray, index: number): void;
}

interface SetDeleteObjectCallback {
    (value: any, index: number): void;
}

interface TestThrowingCallback {
    (): void;
}

interface UniFFICallbackHandler {
    (objectId: UniFFICallbackObjectHandle, methodId: UniFFICallbackMethodId, aArgs: ArrayBuffer): void;
}

interface ValidateAssertionCallback {
    (assertion: string, origin: string): RTCIdentityValidationResult | PromiseLike<RTCIdentityValidationResult>;
}

interface VideoFrameOutputCallback {
    (output: VideoFrame): void;
}

interface VideoFrameRequestCallback {
    (now: DOMHighResTimeStamp, metadata: VideoFrameCallbackMetadata): void;
}

interface VoidFunction {
    (): void;
}

interface WebCodecsErrorCallback {
    (error: DOMException): void;
}

interface WebExtensionLocalizeCallback {
    (unlocalizedText: string): string;
}

interface WebrtcGlobalLoggingCallback {
    (logMessages: string[]): void;
}

interface WebrtcGlobalStatisticsCallback {
    (reports: WebrtcGlobalStatisticsReport): void;
}

interface WebrtcGlobalStatisticsHistoryCallback {
    (reports: WebrtcGlobalStatisticsReport): void;
}

interface WebrtcGlobalStatisticsHistoryPcIdsCallback {
    (pcIds: string[]): void;
}

interface XRFrameRequestCallback {
    (time: DOMHighResTimeStamp, frame: XRFrame): void;
}

interface mozPacketCallback {
    (level: number, type: mozPacketDumpType, sending: boolean, packet: ArrayBuffer): void;
}

interface HTMLElementTagNameMap {
    "a": HTMLAnchorElement;
    "abbr": HTMLElement;
    "acronym": HTMLElement;
    "address": HTMLElement;
    "applet": HTMLUnknownElement;
    "area": HTMLAreaElement;
    "article": HTMLElement;
    "aside": HTMLElement;
    "audio": HTMLAudioElement;
    "b": HTMLElement;
    "base": HTMLBaseElement;
    "basefont": HTMLElement;
    "bdi": HTMLElement;
    "bdo": HTMLElement;
    "bgsound": HTMLUnknownElement;
    "big": HTMLElement;
    "blockquote": HTMLQuoteElement;
    "body": HTMLBodyElement;
    "br": HTMLBRElement;
    "button": HTMLButtonElement;
    "canvas": HTMLCanvasElement;
    "caption": HTMLTableCaptionElement;
    "center": HTMLElement;
    "cite": HTMLElement;
    "code": HTMLElement;
    "col": HTMLTableColElement;
    "colgroup": HTMLTableColElement;
    "data": HTMLDataElement;
    "datalist": HTMLDataListElement;
    "dd": HTMLElement;
    "del": HTMLModElement;
    "details": HTMLDetailsElement;
    "dfn": HTMLElement;
    "dialog": HTMLDialogElement;
    "dir": HTMLDirectoryElement;
    "div": HTMLDivElement;
    "dl": HTMLDListElement;
    "dt": HTMLElement;
    "em": HTMLElement;
    "embed": HTMLEmbedElement;
    "fieldset": HTMLFieldSetElement;
    "figcaption": HTMLElement;
    "figure": HTMLElement;
    "font": HTMLFontElement;
    "footer": HTMLElement;
    "form": HTMLFormElement;
    "frame": HTMLFrameElement;
    "frameset": HTMLFrameSetElement;
    "h1": HTMLHeadingElement;
    "h2": HTMLHeadingElement;
    "h3": HTMLHeadingElement;
    "h4": HTMLHeadingElement;
    "h5": HTMLHeadingElement;
    "h6": HTMLHeadingElement;
    "head": HTMLHeadElement;
    "header": HTMLElement;
    "hgroup": HTMLElement;
    "hr": HTMLHRElement;
    "html": HTMLHtmlElement;
    "i": HTMLElement;
    "iframe": HTMLIFrameElement;
    "image": HTMLElement;
    "img": HTMLImageElement;
    "input": HTMLInputElement;
    "ins": HTMLModElement;
    "kbd": HTMLElement;
    "keygen": HTMLUnknownElement;
    "label": HTMLLabelElement;
    "legend": HTMLLegendElement;
    "li": HTMLLIElement;
    "link": HTMLLinkElement;
    "listing": HTMLPreElement;
    "main": HTMLElement;
    "map": HTMLMapElement;
    "mark": HTMLElement;
    "marquee": HTMLMarqueeElement;
    "menu": HTMLMenuElement;
    "meta": HTMLMetaElement;
    "meter": HTMLMeterElement;
    "multicol": HTMLUnknownElement;
    "nav": HTMLElement;
    "nobr": HTMLElement;
    "noembed": HTMLElement;
    "noframes": HTMLElement;
    "noscript": HTMLElement;
    "object": HTMLObjectElement;
    "ol": HTMLOListElement;
    "optgroup": HTMLOptGroupElement;
    "option": HTMLOptionElement;
    "output": HTMLOutputElement;
    "p": HTMLParagraphElement;
    "param": HTMLParamElement;
    "picture": HTMLPictureElement;
    "plaintext": HTMLElement;
    "pre": HTMLPreElement;
    "progress": HTMLProgressElement;
    "q": HTMLQuoteElement;
    "rb": HTMLElement;
    "rp": HTMLElement;
    "rt": HTMLElement;
    "rtc": HTMLElement;
    "ruby": HTMLElement;
    "s": HTMLElement;
    "samp": HTMLElement;
    "script": HTMLScriptElement;
    "search": HTMLElement;
    "section": HTMLElement;
    "select": HTMLSelectElement;
    "slot": HTMLSlotElement;
    "small": HTMLElement;
    "source": HTMLSourceElement;
    "span": HTMLSpanElement;
    "strike": HTMLElement;
    "strong": HTMLElement;
    "style": HTMLStyleElement;
    "sub": HTMLElement;
    "summary": HTMLElement;
    "sup": HTMLElement;
    "table": HTMLTableElement;
    "tbody": HTMLTableSectionElement;
    "td": HTMLTableCellElement;
    "template": HTMLTemplateElement;
    "textarea": HTMLTextAreaElement;
    "tfoot": HTMLTableSectionElement;
    "th": HTMLTableCellElement;
    "thead": HTMLTableSectionElement;
    "time": HTMLTimeElement;
    "title": HTMLTitleElement;
    "tr": HTMLTableRowElement;
    "track": HTMLTrackElement;
    "tt": HTMLElement;
    "u": HTMLElement;
    "ul": HTMLUListElement;
    "var": HTMLElement;
    "video": HTMLVideoElement;
    "wbr": HTMLElement;
    "xmp": HTMLPreElement;
}

interface HTMLElementDeprecatedTagNameMap {
}

interface SVGElementTagNameMap {
}

interface MathMLElementTagNameMap {
}

/** @deprecated Directly use HTMLElementTagNameMap or SVGElementTagNameMap as appropriate, instead. */
type ElementTagNameMap = HTMLElementTagNameMap & Pick<SVGElementTagNameMap, Exclude<keyof SVGElementTagNameMap, keyof HTMLElementTagNameMap>>;

declare var Audio: {
    new(src?: string): HTMLAudioElement;
};
declare var Image: {
    new(width?: number, height?: number): HTMLImageElement;
};
declare var Option: {
    new(text?: string, value?: string, defaultSelected?: boolean, selected?: boolean): HTMLOptionElement;
};
declare var webkitSpeechGrammar: {
    new(): SpeechGrammar;
};
declare var webkitSpeechGrammarList: {
    new(): SpeechGrammarList;
};
declare var webkitSpeechRecognition: {
    new(): SpeechRecognition;
};
declare var Glean: GleanImpl;
declare var GleanPings: GleanPingsImpl;
declare var InstallTrigger: InstallTriggerImpl | null;
declare var browserDOMWindow: nsIBrowserDOMWindow | null;
declare var browsingContext: BrowsingContext;
declare var clientInformation: Navigator;
declare var clientPrincipal: Principal | null;
declare var closed: boolean;
declare var content: any;
declare var controllers: XULControllers;
declare var customElements: CustomElementRegistry;
declare var desktopToDeviceScale: number;
declare var devicePixelRatio: number;
declare var docShell: nsIDocShell | null;
declare var document: Document | null;
declare var event: Event | undefined;
declare var external: External;
declare var frameElement: Element | null;
declare var frames: WindowProxy;
declare var fullScreen: boolean;
declare var history: History;
declare var innerHeight: number;
declare var innerWidth: number;
declare var intlUtils: IntlUtils;
declare var isChromeWindow: boolean;
declare var isFullyOccluded: boolean;
declare var isInFullScreenTransition: boolean;
// @ts-ignore
declare var isInstance: IsInstance<Window>;
declare var length: number;
declare var location: Location;
declare var locationbar: BarProp;
declare var menubar: BarProp;
declare var messageManager: ChromeMessageBroadcaster;
declare var mozInnerScreenX: number;
declare var mozInnerScreenY: number;
/** @deprecated */
declare const name: void;
declare var navigation: Navigation;
declare var navigator: Navigator;
declare var ondevicelight: ((this: Window, ev: Event) => any) | null;
declare var ondevicemotion: ((this: Window, ev: Event) => any) | null;
declare var ondeviceorientation: ((this: Window, ev: Event) => any) | null;
declare var ondeviceorientationabsolute: ((this: Window, ev: Event) => any) | null;
declare var onorientationchange: ((this: Window, ev: Event) => any) | null;
declare var onuserproximity: ((this: Window, ev: Event) => any) | null;
declare var onvrdisplayactivate: ((this: Window, ev: Event) => any) | null;
declare var onvrdisplayconnect: ((this: Window, ev: Event) => any) | null;
declare var onvrdisplaydeactivate: ((this: Window, ev: Event) => any) | null;
declare var onvrdisplaydisconnect: ((this: Window, ev: Event) => any) | null;
declare var onvrdisplaypresentchange: ((this: Window, ev: Event) => any) | null;
declare var opener: any;
declare var orientation: number;
declare var outerHeight: number;
declare var outerWidth: number;
declare var pageXOffset: number;
declare var pageYOffset: number;
declare var paintWorklet: Worklet;
declare var parent: WindowProxy | null;
declare var performance: Performance | null;
declare var personalbar: BarProp;
declare var realFrameElement: Element | null;
declare var screen: Screen;
declare var screenEdgeSlopX: number;
declare var screenEdgeSlopY: number;
declare var screenLeft: number;
declare var screenTop: number;
declare var screenX: number;
declare var screenY: number;
declare var scrollMaxX: number;
declare var scrollMaxY: number;
declare var scrollMinX: number;
declare var scrollMinY: number;
declare var scrollX: number;
declare var scrollY: number;
declare var scrollbars: BarProp;
declare var self: WindowProxy;
declare var status: string;
declare var statusbar: BarProp;
declare var toolbar: BarProp;
declare var top: WindowProxy | null;
declare var visualViewport: VisualViewport;
declare var window: WindowProxy;
declare var windowGlobalChild: WindowGlobalChild | null;
declare var windowRoot: WindowRoot | null;
declare var windowState: number;
declare var windowUtils: nsIDOMWindowUtils;
declare function alert(): void;
declare function alert(message: string): void;
declare function blur(): void;
declare function cancelIdleCallback(handle: number): void;
declare function captureEvents(): void;
declare function close(): void;
declare function confirm(message?: string): boolean;
declare function dump(str: string): void;
declare function find(str?: string, caseSensitive?: boolean, backwards?: boolean, wrapAround?: boolean, wholeWord?: boolean, searchInFrames?: boolean, showDialog?: boolean): boolean;
declare function focus(): void;
declare function getAttention(): void;
declare function getAttentionWithCycleCount(aCycleCount: number): void;
declare function getComputedStyle(elt: Element, pseudoElt?: string | null): CSSStyleDeclaration | null;
declare function getDefaultComputedStyle(elt: Element, pseudoElt?: string): CSSStyleDeclaration | null;
declare function getGroupMessageManager(aGroup: string): ChromeMessageBroadcaster;
declare function getInterface(iid: any): any;
declare function getRegionalPrefsLocales(): string[];
declare function getSelection(): Selection | null;
declare function getWebExposedLocales(): string[];
declare function getWorkspaceID(): string;
declare function matchMedia(query: string): MediaQueryList | null;
declare function maximize(): void;
declare function minimize(): void;
declare function moveBy(x: number, y: number): void;
declare function moveTo(x: number, y: number): void;
declare function moveToWorkspace(workspaceID: string): void;
declare function mozScrollSnap(): void;
declare function notifyDefaultButtonLoaded(defaultButton: Element): void;
declare function open(url?: string | URL, target?: string, features?: string): WindowProxy | null;
declare function openDialog(url?: string, name?: string, options?: string, ...extraArguments: any[]): WindowProxy | null;
declare function postMessage(message: any, targetOrigin: string, transfer?: any[]): void;
declare function postMessage(message: any, options?: WindowPostMessageOptions): void;
declare function print(): void;
declare function printPreview(settings?: nsIPrintSettings | null, listener?: nsIWebProgressListener | null, docShellToPreviewInto?: nsIDocShell | null): WindowProxy | null;
declare function promiseDocumentFlushed(callback: PromiseDocumentFlushedCallback): Promise<any>;
declare function prompt(message?: string, _default?: string): string | null;
declare function releaseEvents(): void;
declare function requestIdleCallback(callback: IdleRequestCallback, options?: IdleRequestOptions): number;
declare function resizeBy(x: number, y: number): void;
declare function resizeTo(x: number, y: number): void;
declare function restore(): void;
declare function scroll(x: number, y: number): void;
declare function scroll(options?: ScrollToOptions): void;
declare function scrollBy(x: number, y: number): void;
declare function scrollBy(options?: ScrollToOptions): void;
declare function scrollByLines(numLines: number, options?: ScrollOptions): void;
declare function scrollByPages(numPages: number, options?: ScrollOptions): void;
declare function scrollTo(x: number, y: number): void;
declare function scrollTo(options?: ScrollToOptions): void;
declare function setCursor(cursor: string): void;
declare function setResizable(resizable: boolean): void;
declare function setScrollMarks(marks: number[], onHorizontalScrollbar?: boolean): void;
declare function shouldReportForServiceWorkerScope(aScope: string): boolean;
declare function sizeToContent(): void;
declare function sizeToContentConstrained(constraints?: SizeToContentConstraints): void;
declare function stop(): void;
declare function updateCommands(action: string): void;
declare function toString(): string;
// @ts-ignore
declare var isInstance: IsInstance<EventTarget>;
declare var ownerGlobal: WindowProxy | null;
declare function dispatchEvent(event: Event): boolean;
declare function getEventHandler(type: string): EventHandler;
declare function setEventHandler(type: string, handler: EventHandler): void;
declare function cancelAnimationFrame(handle: number): void;
declare function requestAnimationFrame(callback: FrameRequestCallback): number;
declare var crypto: Crypto;
declare var onabort: ((this: Window, ev: Event) => any) | null;
declare var onanimationcancel: ((this: Window, ev: Event) => any) | null;
declare var onanimationend: ((this: Window, ev: Event) => any) | null;
declare var onanimationiteration: ((this: Window, ev: Event) => any) | null;
declare var onanimationstart: ((this: Window, ev: Event) => any) | null;
declare var onauxclick: ((this: Window, ev: Event) => any) | null;
declare var onbeforeinput: ((this: Window, ev: Event) => any) | null;
declare var onbeforetoggle: ((this: Window, ev: Event) => any) | null;
declare var onblur: ((this: Window, ev: Event) => any) | null;
declare var oncancel: ((this: Window, ev: Event) => any) | null;
declare var oncanplay: ((this: Window, ev: Event) => any) | null;
declare var oncanplaythrough: ((this: Window, ev: Event) => any) | null;
declare var onchange: ((this: Window, ev: Event) => any) | null;
declare var onclick: ((this: Window, ev: Event) => any) | null;
declare var onclose: ((this: Window, ev: Event) => any) | null;
declare var oncontentvisibilityautostatechange: ((this: Window, ev: Event) => any) | null;
declare var oncontextlost: ((this: Window, ev: Event) => any) | null;
declare var oncontextmenu: ((this: Window, ev: Event) => any) | null;
declare var oncontextrestored: ((this: Window, ev: Event) => any) | null;
declare var oncopy: ((this: Window, ev: Event) => any) | null;
declare var oncuechange: ((this: Window, ev: Event) => any) | null;
declare var oncut: ((this: Window, ev: Event) => any) | null;
declare var ondblclick: ((this: Window, ev: Event) => any) | null;
declare var ondrag: ((this: Window, ev: Event) => any) | null;
declare var ondragend: ((this: Window, ev: Event) => any) | null;
declare var ondragenter: ((this: Window, ev: Event) => any) | null;
declare var ondragexit: ((this: Window, ev: Event) => any) | null;
declare var ondragleave: ((this: Window, ev: Event) => any) | null;
declare var ondragover: ((this: Window, ev: Event) => any) | null;
declare var ondragstart: ((this: Window, ev: Event) => any) | null;
declare var ondrop: ((this: Window, ev: Event) => any) | null;
declare var ondurationchange: ((this: Window, ev: Event) => any) | null;
declare var onemptied: ((this: Window, ev: Event) => any) | null;
declare var onended: ((this: Window, ev: Event) => any) | null;
declare var onfocus: ((this: Window, ev: Event) => any) | null;
declare var onformdata: ((this: Window, ev: Event) => any) | null;
declare var ongotpointercapture: ((this: Window, ev: Event) => any) | null;
declare var oninput: ((this: Window, ev: Event) => any) | null;
declare var oninvalid: ((this: Window, ev: Event) => any) | null;
declare var onkeydown: ((this: Window, ev: Event) => any) | null;
declare var onkeypress: ((this: Window, ev: Event) => any) | null;
declare var onkeyup: ((this: Window, ev: Event) => any) | null;
declare var onload: ((this: Window, ev: Event) => any) | null;
declare var onloadeddata: ((this: Window, ev: Event) => any) | null;
declare var onloadedmetadata: ((this: Window, ev: Event) => any) | null;
declare var onloadstart: ((this: Window, ev: Event) => any) | null;
declare var onlostpointercapture: ((this: Window, ev: Event) => any) | null;
declare var onmousedown: ((this: Window, ev: Event) => any) | null;
declare var onmouseenter: ((this: Window, ev: Event) => any) | null;
declare var onmouseleave: ((this: Window, ev: Event) => any) | null;
declare var onmousemove: ((this: Window, ev: Event) => any) | null;
declare var onmouseout: ((this: Window, ev: Event) => any) | null;
declare var onmouseover: ((this: Window, ev: Event) => any) | null;
declare var onmouseup: ((this: Window, ev: Event) => any) | null;
declare var onmozfullscreenchange: ((this: Window, ev: Event) => any) | null;
declare var onmozfullscreenerror: ((this: Window, ev: Event) => any) | null;
declare var onpaste: ((this: Window, ev: Event) => any) | null;
declare var onpause: ((this: Window, ev: Event) => any) | null;
declare var onplay: ((this: Window, ev: Event) => any) | null;
declare var onplaying: ((this: Window, ev: Event) => any) | null;
declare var onpointercancel: ((this: Window, ev: Event) => any) | null;
declare var onpointerdown: ((this: Window, ev: Event) => any) | null;
declare var onpointerenter: ((this: Window, ev: Event) => any) | null;
declare var onpointerleave: ((this: Window, ev: Event) => any) | null;
declare var onpointermove: ((this: Window, ev: Event) => any) | null;
declare var onpointerout: ((this: Window, ev: Event) => any) | null;
declare var onpointerover: ((this: Window, ev: Event) => any) | null;
declare var onpointerup: ((this: Window, ev: Event) => any) | null;
declare var onprogress: ((this: Window, ev: Event) => any) | null;
declare var onratechange: ((this: Window, ev: Event) => any) | null;
declare var onreset: ((this: Window, ev: Event) => any) | null;
declare var onresize: ((this: Window, ev: Event) => any) | null;
declare var onscroll: ((this: Window, ev: Event) => any) | null;
declare var onscrollend: ((this: Window, ev: Event) => any) | null;
declare var onsecuritypolicyviolation: ((this: Window, ev: Event) => any) | null;
declare var onseeked: ((this: Window, ev: Event) => any) | null;
declare var onseeking: ((this: Window, ev: Event) => any) | null;
declare var onselect: ((this: Window, ev: Event) => any) | null;
declare var onselectionchange: ((this: Window, ev: Event) => any) | null;
declare var onselectstart: ((this: Window, ev: Event) => any) | null;
declare var onslotchange: ((this: Window, ev: Event) => any) | null;
declare var onstalled: ((this: Window, ev: Event) => any) | null;
declare var onsubmit: ((this: Window, ev: Event) => any) | null;
declare var onsuspend: ((this: Window, ev: Event) => any) | null;
declare var ontimeupdate: ((this: Window, ev: Event) => any) | null;
declare var ontoggle: ((this: Window, ev: Event) => any) | null;
declare var ontransitioncancel: ((this: Window, ev: Event) => any) | null;
declare var ontransitionend: ((this: Window, ev: Event) => any) | null;
declare var ontransitionrun: ((this: Window, ev: Event) => any) | null;
declare var ontransitionstart: ((this: Window, ev: Event) => any) | null;
declare var onvolumechange: ((this: Window, ev: Event) => any) | null;
declare var onwaiting: ((this: Window, ev: Event) => any) | null;
declare var onwebkitanimationend: ((this: Window, ev: Event) => any) | null;
declare var onwebkitanimationiteration: ((this: Window, ev: Event) => any) | null;
declare var onwebkitanimationstart: ((this: Window, ev: Event) => any) | null;
declare var onwebkittransitionend: ((this: Window, ev: Event) => any) | null;
declare var onwheel: ((this: Window, ev: Event) => any) | null;
declare var onerror: ((this: Window, ev: Event) => any) | null;
declare var speechSynthesis: SpeechSynthesis;
declare var ontouchcancel: ((this: Window, ev: Event) => any) | null;
declare var ontouchend: ((this: Window, ev: Event) => any) | null;
declare var ontouchmove: ((this: Window, ev: Event) => any) | null;
declare var ontouchstart: ((this: Window, ev: Event) => any) | null;
declare var onafterprint: ((this: Window, ev: Event) => any) | null;
declare var onbeforeprint: ((this: Window, ev: Event) => any) | null;
declare var onbeforeunload: ((this: Window, ev: Event) => any) | null;
declare var ongamepadconnected: ((this: Window, ev: Event) => any) | null;
declare var ongamepaddisconnected: ((this: Window, ev: Event) => any) | null;
declare var onhashchange: ((this: Window, ev: Event) => any) | null;
declare var onlanguagechange: ((this: Window, ev: Event) => any) | null;
declare var onmessage: ((this: Window, ev: Event) => any) | null;
declare var onmessageerror: ((this: Window, ev: Event) => any) | null;
declare var onoffline: ((this: Window, ev: Event) => any) | null;
declare var ononline: ((this: Window, ev: Event) => any) | null;
declare var onpagehide: ((this: Window, ev: Event) => any) | null;
declare var onpageshow: ((this: Window, ev: Event) => any) | null;
declare var onpopstate: ((this: Window, ev: Event) => any) | null;
declare var onrejectionhandled: ((this: Window, ev: Event) => any) | null;
declare var onstorage: ((this: Window, ev: Event) => any) | null;
declare var onunhandledrejection: ((this: Window, ev: Event) => any) | null;
declare var onunload: ((this: Window, ev: Event) => any) | null;
declare var localStorage: Storage | null;
declare var caches: CacheStorage;
declare var crossOriginIsolated: boolean;
declare var indexedDB: IDBFactory | null;
declare var isSecureContext: boolean;
declare var origin: string;
declare var scheduler: Scheduler;
declare var trustedTypes: TrustedTypePolicyFactory;
declare function atob(atob: string): string;
declare function btoa(btoa: string): string;
declare function clearInterval(handle?: number): void;
declare function clearTimeout(handle?: number): void;
declare function createImageBitmap(aImage: ImageBitmapSource, aOptions?: ImageBitmapOptions): Promise<ImageBitmap>;
declare function createImageBitmap(aImage: ImageBitmapSource, aSx: number, aSy: number, aSw: number, aSh: number, aOptions?: ImageBitmapOptions): Promise<ImageBitmap>;
declare function fetch(input: RequestInfo | URL, init?: RequestInit): Promise<Response>;
declare function queueMicrotask(callback: VoidFunction): void;
declare function reportError(e: any): void;
declare function setInterval(handler: Function, timeout?: number, ...arguments: any[]): number;
declare function setInterval(handler: string, timeout?: number, ...unused: any[]): number;
declare function setTimeout(handler: Function, timeout?: number, ...arguments: any[]): number;
declare function setTimeout(handler: string, timeout?: number, ...unused: any[]): number;
declare function structuredClone(value: any, options?: StructuredSerializeOptions): any;
declare var sessionStorage: Storage | null;
declare function addEventListener<K extends keyof WindowEventMap>(type: K, listener: (this: Window, ev: WindowEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
declare function addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
declare function removeEventListener<K extends keyof WindowEventMap>(type: K, listener: (this: Window, ev: WindowEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
declare function removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
type AlgorithmIdentifier = any;
type Base64URLString = string;
type BinaryData = ArrayBuffer | ArrayBufferView;
type BlobPart = BufferSource | Blob | string;
type BodyInit = XMLHttpRequestBodyInit;
type COSEAlgorithmIdentifier = number;
type CanvasImageSource = HTMLOrSVGImageElement | HTMLCanvasElement | HTMLVideoElement | OffscreenCanvas | ImageBitmap | VideoFrame;
type CanvasSource = HTMLCanvasElement | OffscreenCanvas;
type ClipboardItemData = Promise<ClipboardItemDataType>;
type ClipboardItemDataType = string | Blob;
type ClipboardItems = ClipboardItem[];
type CollectedFormDataValue = any;
type ConstrainBoolean = boolean | ConstrainBooleanParameters;
type ConstrainDOMString = string | string[] | ConstrainDOMStringParameters;
type ConstrainDouble = number | ConstrainDoubleRange;
type ConstrainLong = number | ConstrainLongRange;
type DOMHighResTimeStamp = number;
type DOMTimeStamp = number;
type EpochTimeStamp = number;
type FileSystemWriteChunkType = BufferSource | Blob | string | WriteParams;
type Float32List = Float32Array | GLfloat[];
type FormDataEntryValue = Blob | Directory | string;
type FormDataValue = any;
type GLbitfield = number;
type GLboolean = boolean;
type GLclampf = number;
type GLenum = number;
type GLfloat = number;
type GLint = number;
type GLint64 = number;
type GLintptr = number;
type GLsizei = number;
type GLsizeiptr = number;
type GLuint = number;
type GLuint64 = number;
type GPUBindingResource = GPUSampler | GPUTextureView | GPUBufferBinding;
type GPUBufferDynamicOffset = number;
type GPUBufferUsageFlags = number;
type GPUColor = number[] | GPUColorDict;
type GPUColorWriteFlags = number;
type GPUDepthBias = number;
type GPUExtent3D = GPUIntegerCoordinate[] | GPUExtent3DDict;
type GPUFlagsConstant = number;
type GPUIndex32 = number;
type GPUIntegerCoordinate = number;
type GPUIntegerCoordinateOut = number;
type GPUMapModeFlags = number;
type GPUOrigin2D = GPUIntegerCoordinate[] | GPUOrigin2DDict;
type GPUOrigin3D = GPUIntegerCoordinate[] | GPUOrigin3DDict;
type GPUPipelineConstantValue = number;
type GPUSampleMask = number;
type GPUShaderStageFlags = number;
type GPUSignedOffset32 = number;
type GPUSize32 = number;
type GPUSize32Out = number;
type GPUSize64 = number;
type GPUSize64Out = number;
type GPUStencilValue = number;
type GPUTextureUsageFlags = number;
type GeometryNode = Text | Element | Document;
type HTMLOrSVGImageElement = HTMLImageElement | SVGImageElement;
type HeadersInit = string[][] | Record<string, string>;
type HistogramID = string;
type HistogramKey = string;
type ImageBitmapSource = CanvasImageSource | Blob | CanvasRenderingContext2D | ImageData;
type ImageBufferSource = ArrayBufferView | ArrayBuffer | ReadableStream;
type Int32List = Int32Array | GLint[];
type KeyFormat = string;
type KeyType = string;
type KeyUsage = string;
type L10nArgs = Record<string, string | number | null>;
type L10nKey = string | L10nIdArgs;
type L10nResourceId = string | ResourceId;
type MatchGlobOrString = MatchGlob | string;
type MatchPatternSetOrStringSequence = MatchPatternSet | string[];
type MessageEventSource = WindowProxy | MessagePort | ServiceWorker;
type NodeId = number;
type NodeSize = number;
type OffscreenRenderingContext = OffscreenCanvasRenderingContext2D | ImageBitmapRenderingContext | WebGLRenderingContext | WebGL2RenderingContext | GPUCanvasContext;
type OnBeforeUnloadEventHandler = OnBeforeUnloadEventHandlerNonNull | null;
type OnErrorEventHandler = OnErrorEventHandlerNonNull | null;
type PerformanceEntryList = PerformanceEntry[];
type RTCRtpTransform = RTCRtpScriptTransform;
type ReadableStreamReader = ReadableStreamDefaultReader | ReadableStreamBYOBReader;
type ReportList = Report[];
type RequestInfo = Request | string;
type SanitizerAttribute = string | SanitizerAttributeNamespace;
type SanitizerElement = string | SanitizerElementNamespace;
type SanitizerElementWithAttributes = string | SanitizerElementNamespaceWithAttributes;
type SanitizerInput = DocumentFragment | Document;
type StringOrOpenPopupOptions = string | OpenPopupOptions;
type StructuredClonable = any;
type Uint32List = Uint32Array | GLuint[];
type UniFFICallbackInterfaceId = number;
type UniFFICallbackMethodId = number;
type UniFFICallbackObjectHandle = number;
type UniFFIFunctionId = number;
type UniFFIPointerId = number;
type UniFFIScaffoldingValue = number | ArrayBuffer | UniFFIPointer;
type UnrestrictedDoubleOrKeyframeAnimationOptions = number | KeyframeAnimationOptions;
type VibratePattern = number | number[];
type XMLHttpRequestBodyInit = Blob | BufferSource | FormData | URLSearchParams | string;
type XRWebGLRenderingContext = WebGLRenderingContext | WebGL2RenderingContext;
type XSLTParameterValue = number | boolean | string | Node | Node[] | XPathResult;
type nsContentPolicyType = number;
type AlignSetting = "center" | "end" | "left" | "right" | "start";
type AlphaOption = "discard" | "keep";
type AnimationPlayState = "finished" | "idle" | "paused" | "running";
type AnimationReplaceState = "active" | "persisted" | "removed";
type AudioContextState = "closed" | "running" | "suspended";
type AudioSampleFormat = "f32" | "f32-planar" | "s16" | "s16-planar" | "s32" | "s32-planar" | "u8" | "u8-planar";
type AutoKeyword = "auto";
type AutoplayPolicy = "allowed" | "allowed-muted" | "disallowed";
type AutoplayPolicyMediaType = "audiocontext" | "mediaelement";
type AvcBitstreamFormat = "annexb" | "avc";
type Base64URLDecodePadding = "ignore" | "reject" | "require";
type BinaryType = "arraybuffer" | "blob";
type BiquadFilterType = "allpass" | "bandpass" | "highpass" | "highshelf" | "lowpass" | "lowshelf" | "notch" | "peaking";
type BitrateMode = "constant" | "variable";
type CSSBoxType = "border" | "content" | "margin" | "padding";
type CSSStyleSheetParsingMode = "agent" | "author" | "user";
type CacheStorageNamespace = "chrome" | "content";
type CallbackDebuggerNotificationPhase = "post" | "pre";
type CanvasDirection = "inherit" | "ltr" | "rtl";
type CanvasFontKerning = "auto" | "none" | "normal";
type CanvasFontStretch = "condensed" | "expanded" | "extra-condensed" | "extra-expanded" | "normal" | "semi-condensed" | "semi-expanded" | "ultra-condensed" | "ultra-expanded";
type CanvasFontVariantCaps = "all-petite-caps" | "all-small-caps" | "normal" | "petite-caps" | "small-caps" | "titling-caps" | "unicase";
type CanvasLineCap = "butt" | "round" | "square";
type CanvasLineJoin = "bevel" | "miter" | "round";
type CanvasTextAlign = "center" | "end" | "left" | "right" | "start";
type CanvasTextBaseline = "alphabetic" | "bottom" | "hanging" | "ideographic" | "middle" | "top";
type CanvasTextRendering = "auto" | "geometricPrecision" | "optimizeLegibility" | "optimizeSpeed";
type CanvasWindingRule = "evenodd" | "nonzero";
type CaretChangedReason = "dragcaret" | "longpressonemptycontent" | "presscaret" | "releasecaret" | "scroll" | "taponcaret" | "updateposition" | "visibilitychange";
type ChannelCountMode = "clamped-max" | "explicit" | "max";
type ChannelInterpretation = "discrete" | "speakers";
type CheckerboardReason = "recent" | "severe";
type CodecState = "closed" | "configured" | "unconfigured";
type ColorGamut = "p3" | "rec2020" | "srgb";
type ColorSpaceConversion = "default" | "none";
type CompositeOperation = "accumulate" | "add" | "replace";
type CompressionFormat = "deflate" | "deflate-raw" | "gzip";
type ConnectionType = "bluetooth" | "cellular" | "ethernet" | "none" | "other" | "unknown" | "wifi";
type ConsoleLevel = "error" | "log" | "warning";
type ConsoleLogLevel = "All" | "Clear" | "Debug" | "Dir" | "Dirxml" | "Error" | "Group" | "GroupEnd" | "Info" | "Log" | "Off" | "Profile" | "ProfileEnd" | "Time" | "TimeEnd" | "TimeLog" | "Trace" | "Warn";
type ContentScriptExecutionWorld = "ISOLATED" | "MAIN";
type ContentScriptRunAt = "document_end" | "document_idle" | "document_start";
type CredentialMediationRequirement = "conditional" | "optional" | "required" | "silent";
type DebuggerNotificationType = "cancelAnimationFrame" | "clearInterval" | "clearTimeout" | "domEvent" | "requestAnimationFrame" | "requestAnimationFrameCallback" | "setInterval" | "setIntervalCallback" | "setTimeout" | "setTimeoutCallback";
type DecoderDoctorReportType = "mediacannotinitializepulseaudio" | "mediacannotplaynodecoders" | "mediadecodeerror" | "mediadecodewarning" | "medianodecoders" | "mediaplatformdecodernotfound" | "mediaunsupportedlibavcodec" | "mediawidevinenowmf" | "mediawmfneeded";
type DirectionSetting = "" | "lr" | "rl";
type DisplayMode = "browser" | "fullscreen" | "minimal-ui" | "standalone";
type DistanceModelType = "exponential" | "inverse" | "linear";
type EncodedAudioChunkType = "delta" | "key";
type EncodedVideoChunkType = "delta" | "key";
type EndingType = "native" | "transparent";
type EventCallbackDebuggerNotificationType = "global" | "node" | "websocket" | "worker" | "xhr";
type FetchState = "aborted" | "complete" | "errored" | "requesting" | "responding";
type FileSystemHandleKind = "directory" | "file";
type FileType = "directory" | "other" | "regular";
type FillMode = "auto" | "backwards" | "both" | "forwards" | "none";
type FlexItemClampState = "clamped_to_max" | "clamped_to_min" | "unclamped";
type FlexLineGrowthState = "growing" | "shrinking";
type FlexPhysicalDirection = "horizontal-lr" | "horizontal-rl" | "vertical-bt" | "vertical-tb";
type FontFaceLoadStatus = "error" | "loaded" | "loading" | "unloaded";
type FontFaceSetLoadStatus = "loaded" | "loading";
type GPUAddressMode = "clamp-to-edge" | "mirror-repeat" | "repeat";
type GPUAutoLayoutMode = "auto";
type GPUBlendFactor = "constant" | "dst" | "dst-alpha" | "one" | "one-minus-constant" | "one-minus-dst" | "one-minus-dst-alpha" | "one-minus-src" | "one-minus-src-alpha" | "src" | "src-alpha" | "src-alpha-saturated" | "zero";
type GPUBlendOperation = "add" | "max" | "min" | "reverse-subtract" | "subtract";
type GPUBufferBindingType = "read-only-storage" | "storage" | "uniform";
type GPUBufferMapState = "mapped" | "pending" | "unmapped";
type GPUCanvasAlphaMode = "opaque" | "premultiplied";
type GPUCompareFunction = "always" | "equal" | "greater" | "greater-equal" | "less" | "less-equal" | "never" | "not-equal";
type GPUCompilationMessageType = "error" | "info" | "warning";
type GPUCullMode = "back" | "front" | "none";
type GPUErrorFilter = "internal" | "out-of-memory" | "validation";
type GPUFeatureName = "bgra8unorm-storage" | "depth-clip-control" | "depth32float-stencil8" | "float32-filterable" | "indirect-first-instance" | "rg11b10ufloat-renderable" | "shader-f16" | "texture-compression-astc" | "texture-compression-bc" | "texture-compression-etc2" | "timestamp-query";
type GPUFilterMode = "linear" | "nearest";
type GPUFrontFace = "ccw" | "cw";
type GPUIndexFormat = "uint16" | "uint32";
type GPULoadOp = "clear" | "load";
type GPUMipmapFilterMode = "linear" | "nearest";
type GPUPowerPreference = "high-performance" | "low-power";
type GPUPrimitiveTopology = "line-list" | "line-strip" | "point-list" | "triangle-list" | "triangle-strip";
type GPUSamplerBindingType = "comparison" | "filtering" | "non-filtering";
type GPUStencilOperation = "decrement-clamp" | "decrement-wrap" | "increment-clamp" | "increment-wrap" | "invert" | "keep" | "replace" | "zero";
type GPUStorageTextureAccess = "read-only" | "read-write" | "write-only";
type GPUStoreOp = "discard" | "store";
type GPUTextureAspect = "all" | "depth-only" | "stencil-only";
type GPUTextureDimension = "1d" | "2d" | "3d";
type GPUTextureFormat = "bc1-rgba-unorm" | "bc1-rgba-unorm-srgb" | "bc2-rgba-unorm" | "bc2-rgba-unorm-srgb" | "bc3-rgba-unorm" | "bc3-rgba-unorm-srgb" | "bc4-r-snorm" | "bc4-r-unorm" | "bc5-rg-snorm" | "bc5-rg-unorm" | "bc6h-rgb-float" | "bc6h-rgb-ufloat" | "bc7-rgba-unorm" | "bc7-rgba-unorm-srgb" | "bgra8unorm" | "bgra8unorm-srgb" | "depth16unorm" | "depth24plus" | "depth24plus-stencil8" | "depth32float" | "depth32float-stencil8" | "r16float" | "r16sint" | "r16uint" | "r32float" | "r32sint" | "r32uint" | "r8sint" | "r8snorm" | "r8uint" | "r8unorm" | "rg11b10ufloat" | "rg16float" | "rg16sint" | "rg16uint" | "rg32float" | "rg32sint" | "rg32uint" | "rg8sint" | "rg8snorm" | "rg8uint" | "rg8unorm" | "rgb10a2unorm" | "rgb9e5ufloat" | "rgba16float" | "rgba16sint" | "rgba16uint" | "rgba32float" | "rgba32sint" | "rgba32uint" | "rgba8sint" | "rgba8snorm" | "rgba8uint" | "rgba8unorm" | "rgba8unorm-srgb" | "stencil8";
type GPUTextureSampleType = "depth" | "float" | "sint" | "uint" | "unfilterable-float";
type GPUTextureViewDimension = "1d" | "2d" | "2d-array" | "3d" | "cube" | "cube-array";
type GPUVertexFormat = "float16x2" | "float16x4" | "float32" | "float32x2" | "float32x3" | "float32x4" | "sint16x2" | "sint16x4" | "sint32" | "sint32x2" | "sint32x3" | "sint32x4" | "sint8x2" | "sint8x4" | "snorm16x2" | "snorm16x4" | "snorm8x2" | "snorm8x4" | "uint16x2" | "uint16x4" | "uint32" | "uint32x2" | "uint32x3" | "uint32x4" | "uint8x2" | "uint8x4" | "unorm16x2" | "unorm16x4" | "unorm8x2" | "unorm8x4";
type GPUVertexStepMode = "instance" | "vertex";
type GamepadHand = "" | "left" | "right";
type GamepadHapticActuatorType = "vibration";
type GamepadLightIndicatorType = "on-off" | "rgb";
type GamepadMappingType = "" | "standard" | "xr-standard";
type GetUserMediaRequestType = "getusermedia" | "recording-device-stopped" | "selectaudiooutput";
type GridDeclaration = "explicit" | "implicit";
type GridTrackState = "removed" | "repeat" | "static";
type HDCPVersion = "1.0" | "1.1" | "1.2" | "1.3" | "1.4" | "2.0" | "2.1" | "2.2" | "2.3";
type HardwareAcceleration = "no-preference" | "prefer-hardware" | "prefer-software";
type HashAlgorithm = "sha256" | "sha384" | "sha512";
type HdrMetadataType = "smpteSt2086" | "smpteSt2094-10" | "smpteSt2094-40";
type HeadersGuardEnum = "immutable" | "none" | "request" | "request-no-cors" | "response";
type HighlightType = "grammar-error" | "highlight" | "spelling-error";
type IDBCursorDirection = "next" | "nextunique" | "prev" | "prevunique";
type IDBRequestReadyState = "done" | "pending";
type IDBTransactionDurability = "default" | "relaxed" | "strict";
type IDBTransactionMode = "cleanup" | "readonly" | "readwrite" | "readwriteflush" | "versionchange";
type IdentityLoginTargetType = "popup" | "redirect";
type ImageOrientation = "flipY" | "from-image" | "none";
type ImportESModuleTargetGlobal = "contextual" | "current" | "devtools" | "shared";
type InspectorPropertyType = "color" | "gradient" | "timing-function";
type IterationCompositeOperation = "accumulate" | "replace";
type JSRFPTarget = "RoundWindowSize" | "SiteSpecificZoom" | "CSSPrefersColorScheme";
type L10nFileSourceHasFileStatus = "missing" | "present" | "unknown";
type LatencyMode = "quality" | "realtime";
type LineAlignSetting = "center" | "end" | "start";
type LockMode = "exclusive" | "shared";
type MIDIPortConnectionState = "closed" | "open" | "pending";
type MIDIPortDeviceState = "connected" | "disconnected";
type MIDIPortType = "input" | "output";
type MediaControlKey = "focus" | "nexttrack" | "pause" | "play" | "playpause" | "previoustrack" | "seekbackward" | "seekforward" | "seekto" | "skipad" | "stop";
type MediaDecodingType = "file" | "media-source";
type MediaDeviceKind = "audioinput" | "audiooutput" | "videoinput";
type MediaEncodingType = "record" | "transmission";
type MediaKeyMessageType = "individualization-request" | "license-release" | "license-renewal" | "license-request";
type MediaKeySessionType = "persistent-license" | "temporary";
type MediaKeyStatus = "expired" | "internal-error" | "output-downscaled" | "output-restricted" | "released" | "status-pending" | "usable";
type MediaKeysRequirement = "not-allowed" | "optional" | "required";
type MediaSessionAction = "nexttrack" | "pause" | "play" | "previoustrack" | "seekbackward" | "seekforward" | "seekto" | "skipad" | "stop";
type MediaSessionPlaybackState = "none" | "paused" | "playing";
type MediaSourceEndOfStreamError = "decode" | "network";
type MediaSourceReadyState = "closed" | "ended" | "open";
type MediaStreamTrackState = "ended" | "live";
type MozContentPolicyType = "beacon" | "csp_report" | "fetch" | "font" | "image" | "imageset" | "main_frame" | "media" | "object" | "object_subrequest" | "other" | "ping" | "script" | "speculative" | "stylesheet" | "sub_frame" | "web_manifest" | "websocket" | "xml_dtd" | "xmlhttprequest" | "xslt";
type MozUrlClassificationFlags = "any_basic_tracking" | "any_social_tracking" | "any_strict_tracking" | "cryptomining" | "cryptomining_content" | "emailtracking" | "emailtracking_content" | "fingerprinting" | "fingerprinting_content" | "socialtracking" | "socialtracking_facebook" | "socialtracking_linkedin" | "socialtracking_twitter" | "tracking" | "tracking_ad" | "tracking_analytics" | "tracking_content" | "tracking_social";
type NavigationFocusReset = "after-transition" | "manual";
type NavigationHistoryBehavior = "auto" | "push" | "replace";
type NavigationScrollBehavior = "after-transition" | "manual";
type NavigationTimingType = "back_forward" | "navigate" | "prerender" | "reload";
type NavigationType = "push" | "reload" | "replace" | "traverse";
type NotificationDirection = "auto" | "ltr" | "rtl";
type NotificationPermission = "default" | "denied" | "granted";
type OffscreenRenderingContextId = "2d" | "bitmaprenderer" | "webgl" | "webgl2" | "webgpu";
type OpusBitstreamFormat = "ogg" | "opus";
type OrientationLockType = "any" | "landscape" | "landscape-primary" | "landscape-secondary" | "natural" | "portrait" | "portrait-primary" | "portrait-secondary";
type OrientationType = "landscape-primary" | "landscape-secondary" | "portrait-primary" | "portrait-secondary";
type OscillatorType = "custom" | "sawtooth" | "sine" | "square" | "triangle";
type OverSampleType = "2x" | "4x" | "none";
type OverridableErrorCategory = "domain-mismatch" | "expired-or-not-yet-valid" | "trust-error" | "unset";
type PCError = "InvalidAccessError" | "InvalidCharacterError" | "InvalidModificationError" | "InvalidStateError" | "NotReadableError" | "NotSupportedError" | "OperationError" | "RangeError" | "SyntaxError" | "TypeError" | "UnknownError";
type PCObserverStateType = "ConnectionState" | "IceConnectionState" | "IceGatheringState" | "None" | "SignalingState";
type PanningModelType = "HRTF" | "equalpower";
type PaymentComplete = "fail" | "success" | "unknown";
type PaymentShippingType = "delivery" | "pickup" | "shipping";
type PermissionName = "geolocation" | "midi" | "notifications" | "persistent-storage" | "push" | "screen-wake-lock" | "storage-access";
type PermissionState = "denied" | "granted" | "prompt";
type PermitUnloadAction = "dontUnload" | "prompt" | "unload";
type PlacesEventType = "bookmark-added" | "bookmark-guid-changed" | "bookmark-keyword-changed" | "bookmark-moved" | "bookmark-removed" | "bookmark-tags-changed" | "bookmark-time-changed" | "bookmark-title-changed" | "bookmark-url-changed" | "favicon-changed" | "history-cleared" | "none" | "page-removed" | "page-title-changed" | "page-visited" | "pages-rank-changed" | "purge-caches";
type PlaybackDirection = "alternate" | "alternate-reverse" | "normal" | "reverse";
type PopupBlockerState = "openAbused" | "openAllowed" | "openBlocked" | "openControlled" | "openOverridden";
type PositionAlignSetting = "auto" | "center" | "line-left" | "line-right";
type PredefinedColorSpace = "display-p3" | "srgb";
type PrefersColorSchemeOverride = "dark" | "light" | "none";
type PremultiplyAlpha = "default" | "none" | "premultiply";
type PresentationStyle = "attachment" | "inline" | "unspecified";
type PrivateAttributionImpressionType = "click" | "view";
type PromiseDebuggingState = "fulfilled" | "pending" | "rejected";
type PushEncryptionKeyName = "auth" | "p256dh";
type RTCBundlePolicy = "balanced" | "max-bundle" | "max-compat";
type RTCCodecType = "decode" | "encode";
type RTCDataChannelState = "closed" | "closing" | "connecting" | "open";
type RTCDataChannelType = "arraybuffer" | "blob";
type RTCDtlsTransportState = "closed" | "connected" | "connecting" | "failed" | "new";
type RTCEncodedVideoFrameType = "delta" | "empty" | "key";
type RTCIceCandidateType = "host" | "prflx" | "relay" | "srflx";
type RTCIceComponent = "rtcp" | "rtp";
type RTCIceConnectionState = "checking" | "closed" | "completed" | "connected" | "disconnected" | "failed" | "new";
type RTCIceCredentialType = "password";
type RTCIceGathererState = "complete" | "gathering" | "new";
type RTCIceGatheringState = "complete" | "gathering" | "new";
type RTCIceProtocol = "tcp" | "udp";
type RTCIceTcpCandidateType = "active" | "passive" | "so";
type RTCIceTransportPolicy = "all" | "relay";
type RTCIceTransportState = "checking" | "closed" | "completed" | "connected" | "disconnected" | "failed" | "new";
type RTCLifecycleEvent = "connectionstatechange" | "iceconnectionstatechange" | "icegatheringstatechange" | "initialized";
type RTCPeerConnectionState = "closed" | "connected" | "connecting" | "disconnected" | "failed" | "new";
type RTCPriorityType = "high" | "low" | "medium" | "very-low";
type RTCRtpTransceiverDirection = "inactive" | "recvonly" | "sendonly" | "sendrecv" | "stopped";
type RTCSctpTransportState = "closed" | "connected" | "connecting";
type RTCSdpType = "answer" | "offer" | "pranswer" | "rollback";
type RTCSignalingState = "closed" | "have-local-offer" | "have-local-pranswer" | "have-remote-offer" | "have-remote-pranswer" | "stable";
type RTCStatsIceCandidatePairState = "cancelled" | "failed" | "frozen" | "inprogress" | "succeeded" | "waiting";
type RTCStatsType = "candidate-pair" | "codec" | "csrc" | "data-channel" | "inbound-rtp" | "local-candidate" | "media-source" | "outbound-rtp" | "peer-connection" | "remote-candidate" | "remote-inbound-rtp" | "remote-outbound-rtp" | "session" | "track" | "transport";
type ReadableStreamReaderMode = "byob";
type RecordingState = "inactive" | "paused" | "recording";
type ReferrerPolicy = "" | "no-referrer" | "no-referrer-when-downgrade" | "origin" | "origin-when-cross-origin" | "same-origin" | "strict-origin" | "strict-origin-when-cross-origin" | "unsafe-url";
type RenderBlockingStatusType = "blocking" | "non-blocking";
type RequestCache = "default" | "force-cache" | "no-cache" | "no-store" | "only-if-cached" | "reload";
type RequestCredentials = "include" | "omit" | "same-origin";
type RequestDestination = "" | "audio" | "audioworklet" | "document" | "embed" | "font" | "frame" | "iframe" | "image" | "manifest" | "object" | "paintworklet" | "report" | "script" | "sharedworker" | "style" | "track" | "video" | "worker" | "xslt";
type RequestMode = "cors" | "navigate" | "no-cors" | "same-origin";
type RequestPriority = "auto" | "high" | "low";
type RequestRedirect = "error" | "follow" | "manual";
type ResizeObserverBoxOptions = "border-box" | "content-box" | "device-pixel-content-box";
type ResponseType = "basic" | "cors" | "default" | "error" | "opaque" | "opaqueredirect";
type ScreenColorGamut = "p3" | "rec2020" | "srgb";
type ScrollBehavior = "auto" | "instant" | "smooth";
type ScrollLogicalPosition = "center" | "end" | "nearest" | "start";
type ScrollRestoration = "auto" | "manual";
type ScrollSetting = "" | "up";
type ScrollState = "started" | "stopped";
type SecurityPolicyViolationEventDisposition = "enforce" | "report";
type SelectionMode = "end" | "preserve" | "select" | "start";
type SelectorWarningKind = "UnconstrainedHas";
type ServiceWorkerState = "activated" | "activating" | "installed" | "installing" | "parsed" | "redundant";
type ServiceWorkerUpdateViaCache = "all" | "imports" | "none";
type ShadowRootMode = "closed" | "open";
type SlotAssignmentMode = "manual" | "named";
type SocketReadyState = "closed" | "closing" | "halfclosed" | "open" | "opening";
type SourceBufferAppendMode = "segments" | "sequence";
type SpeechRecognitionErrorCode = "aborted" | "audio-capture" | "bad-grammar" | "language-not-supported" | "network" | "no-speech" | "not-allowed" | "service-not-allowed";
type SpeechSynthesisErrorCode = "audio-busy" | "audio-hardware" | "canceled" | "interrupted" | "invalid-argument" | "language-unavailable" | "network" | "synthesis-failed" | "synthesis-unavailable" | "text-too-long" | "voice-unavailable";
type StreamFilterStatus = "closed" | "disconnected" | "failed" | "finishedtransferringdata" | "suspended" | "transferringdata" | "uninitialized";
type StringType = "inline" | "literal" | "other" | "stringbuffer";
type SupportedType = "application/xhtml+xml" | "application/xml" | "image/svg+xml" | "text/html" | "text/xml";
type TCPReadyState = "closed" | "closing" | "connecting" | "open";
type TCPSocketBinaryType = "arraybuffer" | "string";
type TaskPriority = "background" | "user-blocking" | "user-visible";
type TextTrackKind = "captions" | "chapters" | "descriptions" | "metadata" | "subtitles";
type TextTrackMode = "disabled" | "hidden" | "showing";
type TouchEventsOverride = "disabled" | "enabled" | "none";
type TransferFunction = "hlg" | "pq" | "srgb";
type UniFFIScaffoldingCallCode = "error" | "internal-error" | "success";
type VRDisplayEventReason = "mounted" | "navigation" | "requested" | "unmounted";
type VREye = "left" | "right";
type VideoColorPrimaries = "bt2020" | "bt470bg" | "bt709" | "smpte170m" | "smpte432";
type VideoEncoderBitrateMode = "constant" | "quantizer" | "variable";
type VideoMatrixCoefficients = "bt2020-ncl" | "bt470bg" | "bt709" | "rgb" | "smpte170m";
type VideoPixelFormat = "BGRA" | "BGRX" | "I420" | "I420A" | "I420AP10" | "I420AP12" | "I420P10" | "I420P12" | "I422" | "I422A" | "I422AP10" | "I422AP12" | "I422P10" | "I422P12" | "I444" | "I444A" | "I444AP10" | "I444AP12" | "I444P10" | "I444P12" | "NV12" | "RGBA" | "RGBX";
type VideoTransferCharacteristics = "bt709" | "hlg" | "iec61966-2-1" | "linear" | "pq" | "smpte170m";
type VisibilityState = "hidden" | "visible";
type WakeLockType = "screen";
type WebGLPowerPreference = "default" | "high-performance" | "low-power";
type WebIDLProcType = "browser" | "extension" | "file" | "forkServer" | "gmpPlugin" | "gpu" | "inference" | "ipdlUnitTest" | "preallocated" | "privilegedabout" | "privilegedmozilla" | "rdd" | "socket" | "unknown" | "utility" | "vr" | "web" | "webIsolated" | "webServiceWorker" | "withCoopCoep";
type WebIDLUtilityActorName = "audioDecoder_AppleMedia" | "audioDecoder_Generic" | "audioDecoder_WMF" | "jSOracle" | "mfMediaEngineCDM" | "unknown" | "windowsFileDialog" | "windowsUtils";
type WebTransportCongestionControl = "default" | "low-latency" | "throughput";
type WebTransportErrorSource = "session" | "stream";
type WebTransportReliabilityMode = "pending" | "reliable-only" | "supports-unreliable";
type WireframeRectType = "background" | "image" | "text" | "unknown";
type WorkerType = "classic" | "module";
type WriteCommandType = "seek" | "truncate" | "write";
type WriteMode = "append" | "appendOrCreate" | "create" | "overwrite";
type XMLHttpRequestResponseType = "" | "arraybuffer" | "blob" | "document" | "json" | "text";
type XREye = "left" | "none" | "right";
type XRHandedness = "left" | "none" | "right";
type XRReferenceSpaceType = "bounded-floor" | "local" | "local-floor" | "unbounded" | "viewer";
type XRSessionMode = "immersive-ar" | "immersive-vr" | "inline";
type XRTargetRayMode = "gaze" | "screen" | "tracked-pointer";
type XRVisibilityState = "hidden" | "visible" | "visible-blurred";
type mozPacketDumpType = "rtcp" | "rtp" | "srtcp" | "srtp";

/////////////////////////////
/// Window Iterable APIs
/////////////////////////////

interface AbortSignal {
    any(signals: Iterable<AbortSignal>): AbortSignal;
}

interface AudioParam {
    setValueCurveAtTime(values: Iterable<number>, startTime: number, duration: number): AudioParam;
}

interface AudioParamMap extends ReadonlyMap<string, AudioParam> {
}

interface AudioTrackList {
    [Symbol.iterator](): IterableIterator<AudioTrack>;
}

interface BaseAudioContext {
    createIIRFilter(feedforward: Iterable<number>, feedback: Iterable<number>): IIRFilterNode;
    createPeriodicWave(real: Iterable<number>, imag: Iterable<number>, constraints?: PeriodicWaveConstraints): PeriodicWave;
}

interface CSSKeyframesRule {
    [Symbol.iterator](): IterableIterator<CSSKeyframeRule>;
}

interface CSSRuleList {
    [Symbol.iterator](): IterableIterator<CSSRule>;
}

interface CSSStyleDeclaration {
    [Symbol.iterator](): IterableIterator<string>;
}

interface Cache {
    addAll(requests: Iterable<RequestInfo>): Promise<void>;
}

interface CanonicalBrowsingContext {
    countSiteOrigins(roots: Iterable<BrowsingContext>): number;
}

interface CanvasPathDrawingStyles {
    setLineDash(segments: Iterable<number>): void;
}

interface CanvasPathMethods {
    roundRect(x: number, y: number, w: number, h: number, radii?: number | DOMPointInit | Iterable<number | DOMPointInit>): void;
}

interface CustomStateSet extends Set<string> {
}

interface DOMLocalization {
    translateElements(aElements: Iterable<Element>): Promise<void>;
}

interface DOMParser {
    parseFromBuffer(buf: Iterable<number>, type: SupportedType): Document;
}

interface DOMRectList {
    [Symbol.iterator](): IterableIterator<DOMRect>;
}

interface DOMStringList {
    [Symbol.iterator](): IterableIterator<string>;
}

interface DOMTokenList {
    [Symbol.iterator](): IterableIterator<string | null>;
    entries(): IterableIterator<[number, string | null]>;
    keys(): IterableIterator<number>;
    values(): IterableIterator<string | null>;
}

interface DataTransferItemList {
    [Symbol.iterator](): IterableIterator<DataTransferItem>;
}

interface Document {
    createTouchList(touches: Iterable<Touch>): TouchList;
}

interface EventCounts extends ReadonlyMap<string, number> {
}

interface FileList {
    [Symbol.iterator](): IterableIterator<File>;
}

interface FormData {
    [Symbol.iterator](): IterableIterator<[string, FormDataEntryValue]>;
    entries(): IterableIterator<[string, FormDataEntryValue]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<FormDataEntryValue>;
}

interface GPUAdapter {
    requestAdapterInfo(unmaskHints?: Iterable<string>): Promise<GPUAdapterInfo>;
}

interface GPUBindingCommandsMixin {
    setBindGroup(index: GPUIndex32, bindGroup: GPUBindGroup, dynamicOffsets?: Iterable<GPUBufferDynamicOffset>): void;
}

interface GPUCommandEncoder {
    copyBufferToTexture(source: GPUImageCopyBuffer, destination: GPUImageCopyTexture, copySize: Iterable<GPUIntegerCoordinate>): void;
    copyTextureToBuffer(source: GPUImageCopyTexture, destination: GPUImageCopyBuffer, copySize: Iterable<GPUIntegerCoordinate>): void;
    copyTextureToTexture(source: GPUImageCopyTexture, destination: GPUImageCopyTexture, copySize: Iterable<GPUIntegerCoordinate>): void;
}

interface GPUQueue {
    copyExternalImageToTexture(source: GPUImageCopyExternalImage, destination: GPUImageCopyTextureTagged, copySize: Iterable<GPUIntegerCoordinate>): void;
    submit(buffers: Iterable<GPUCommandBuffer>): void;
    writeTexture(destination: GPUImageCopyTexture, data: BufferSource, dataLayout: GPUImageDataLayout, size: Iterable<GPUIntegerCoordinate>): void;
}

interface GPURenderPassEncoder {
    executeBundles(bundles: Iterable<GPURenderBundle>): void;
    setBlendConstant(color: Iterable<number>): void;
}

interface GPUSupportedFeatures extends ReadonlySet<string> {
}

interface GleanCustomDistribution {
    accumulateSamples(aSamples: Iterable<number>): void;
}

interface GleanStringList {
    set(aValue: Iterable<string>): void;
}

interface GridLines {
    [Symbol.iterator](): IterableIterator<GridLine>;
}

interface GridTracks {
    [Symbol.iterator](): IterableIterator<GridTrack>;
}

interface HTMLAllCollection {
    [Symbol.iterator](): IterableIterator<Element>;
}

interface HTMLCollectionBase {
    [Symbol.iterator](): IterableIterator<Element>;
}

interface HTMLFormElement {
    [Symbol.iterator](): IterableIterator<Element>;
}

interface HTMLInputElement {
    mozSetDndFilesAndDirectories(list: Iterable<File | Directory>): void;
    mozSetFileArray(files: Iterable<File>): void;
    mozSetFileNameArray(fileNames: Iterable<string>): void;
}

interface HTMLSelectElement {
    [Symbol.iterator](): IterableIterator<Element>;
}

interface Headers {
    [Symbol.iterator](): IterableIterator<[string, string]>;
    entries(): IterableIterator<[string, string]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string>;
}

interface HeapSnapshot {
    computeShortestPaths(start: NodeId, targets: Iterable<NodeId>, maxNumPaths: number): any;
}

interface Highlight extends Set<AbstractRange> {
}

interface HighlightRegistry extends Map<string, Highlight> {
}

interface IDBDatabase {
    transaction(storeNames: string | Iterable<string>, mode?: IDBTransactionMode, options?: IDBTransactionOptions): IDBTransaction;
}

interface IDBObjectStore {
    createIndex(name: string, keyPath: string | Iterable<string>, optionalParameters?: IDBIndexParameters): IDBIndex;
}

interface ImageTrackList {
    [Symbol.iterator](): IterableIterator<ImageTrack>;
}

interface IntlUtils {
    getDisplayNames(locales: Iterable<string>, options?: DisplayNameOptions): DisplayNameResult;
}

interface L10nFileSource {
    createMock(name: string, metasource: string, locales: Iterable<string>, prePath: string, fs: Iterable<L10nFileSourceMockFile>): L10nFileSource;
}

interface L10nRegistry {
    generateBundles(aLocales: Iterable<string>, aResourceIds: Iterable<L10nResourceId>): FluentBundleAsyncIterator;
    generateBundlesSync(aLocales: Iterable<string>, aResourceIds: Iterable<L10nResourceId>): FluentBundleIterator;
    registerSources(aSources: Iterable<L10nFileSource>): void;
    removeSources(aSources: Iterable<string>): void;
    updateSources(aSources: Iterable<L10nFileSource>): void;
}

interface Localization {
    addResourceIds(aResourceIds: Iterable<L10nResourceId>): void;
    formatMessages(aKeys: Iterable<L10nKey>): Promise<Iterable<L10nMessage | null>>;
    formatMessagesSync(aKeys: Iterable<L10nKey>): Iterable<L10nMessage | null>;
    formatValues(aKeys: Iterable<L10nKey>): Promise<Iterable<string | null>>;
    formatValuesSync(aKeys: Iterable<L10nKey>): Iterable<string | null>;
    removeResourceIds(aResourceIds: Iterable<L10nResourceId>): number;
}

interface MIDIInputMap extends ReadonlyMap<string, MIDIInput> {
}

interface MIDIOutput {
    send(data: Iterable<number>, timestamp?: DOMHighResTimeStamp): void;
}

interface MIDIOutputMap extends ReadonlyMap<string, MIDIOutput> {
}

interface MediaKeyStatusMap {
    [Symbol.iterator](): IterableIterator<[ArrayBuffer, MediaKeyStatus]>;
    entries(): IterableIterator<[ArrayBuffer, MediaKeyStatus]>;
    keys(): IterableIterator<ArrayBuffer>;
    values(): IterableIterator<MediaKeyStatus>;
}

interface MediaList {
    [Symbol.iterator](): IterableIterator<string>;
}

interface MessageEvent {
    initMessageEvent(type: string, bubbles?: boolean, cancelable?: boolean, data?: any, origin?: string, lastEventId?: string, source?: MessageEventSource | null, ports?: Iterable<MessagePort>): void;
}

interface MessagePort {
    postMessage(message: any, transferable: Iterable<any>): void;
}

interface MimeTypeArray {
    [Symbol.iterator](): IterableIterator<MimeType>;
}

interface MozDocumentObserver {
    observe(matchers: Iterable<MozDocumentMatcher>): void;
}

interface MozSharedMap {
    [Symbol.iterator](): IterableIterator<[string, StructuredClonable]>;
    entries(): IterableIterator<[string, StructuredClonable]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<StructuredClonable>;
}

interface MozStorageAsyncStatementParams {
    [Symbol.iterator](): IterableIterator<any>;
}

interface MozStorageStatementParams {
    [Symbol.iterator](): IterableIterator<any>;
}

interface NamedNodeMap {
    [Symbol.iterator](): IterableIterator<Attr>;
}

interface Navigator {
    requestMediaKeySystemAccess(keySystem: string, supportedConfigurations: Iterable<MediaKeySystemConfiguration>): Promise<MediaKeySystemAccess>;
    vibrate(pattern: Iterable<number>): boolean;
}

interface NodeList {
    [Symbol.iterator](): IterableIterator<Node | null>;
    entries(): IterableIterator<[number, Node | null]>;
    keys(): IterableIterator<number>;
    values(): IterableIterator<Node | null>;
}

interface PaintRequestList {
    [Symbol.iterator](): IterableIterator<PaintRequest>;
}

interface PeerConnectionObserver {
    fireTrackEvent(receiver: RTCRtpReceiver, streams: Iterable<MediaStream>): void;
}

interface PlacesEventCounts extends ReadonlyMap<string, number> {
}

interface Plugin {
    [Symbol.iterator](): IterableIterator<MimeType>;
}

interface PluginArray {
    [Symbol.iterator](): IterableIterator<Plugin>;
}

interface RTCRtpTransceiver {
    setCodecPreferences(codecs: Iterable<RTCRtpCodec>): void;
}

interface RTCStatsReport extends ReadonlyMap<string, any> {
}

interface SVGLengthList {
    [Symbol.iterator](): IterableIterator<SVGLength>;
}

interface SVGNumberList {
    [Symbol.iterator](): IterableIterator<SVGNumber>;
}

interface SVGPointList {
    [Symbol.iterator](): IterableIterator<SVGPoint>;
}

interface SVGStringList {
    [Symbol.iterator](): IterableIterator<string>;
}

interface SVGTransformList {
    [Symbol.iterator](): IterableIterator<SVGTransform>;
}

interface Screen {
    mozLockOrientation(orientation: Iterable<string>): boolean;
}

interface ServiceWorker {
    postMessage(message: any, transferable: Iterable<any>): void;
}

interface SourceBufferList {
    [Symbol.iterator](): IterableIterator<SourceBuffer>;
}

interface SpeechGrammarList {
    [Symbol.iterator](): IterableIterator<SpeechGrammar>;
}

interface SpeechRecognitionResult {
    [Symbol.iterator](): IterableIterator<SpeechRecognitionAlternative>;
}

interface SpeechRecognitionResultList {
    [Symbol.iterator](): IterableIterator<SpeechRecognitionResult>;
}

interface StyleSheetList {
    [Symbol.iterator](): IterableIterator<CSSStyleSheet>;
}

interface SubtleCrypto {
    deriveKey(algorithm: AlgorithmIdentifier, baseKey: CryptoKey, derivedKeyType: AlgorithmIdentifier, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<any>;
    generateKey(algorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<any>;
    importKey(format: KeyFormat, keyData: any, algorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<any>;
    unwrapKey(format: KeyFormat, wrappedKey: BufferSource, unwrappingKey: CryptoKey, unwrapAlgorithm: AlgorithmIdentifier, unwrappedKeyAlgorithm: AlgorithmIdentifier, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<any>;
}

interface TestInterfaceIterableDouble {
    [Symbol.iterator](): IterableIterator<[string, string]>;
    entries(): IterableIterator<[string, string]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string>;
}

interface TestInterfaceIterableDoubleUnion {
    [Symbol.iterator](): IterableIterator<[string, string | number]>;
    entries(): IterableIterator<[string, string | number]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string | number>;
}

interface TestInterfaceIterableSingle {
    [Symbol.iterator](): IterableIterator<number>;
    entries(): IterableIterator<[number, number]>;
    keys(): IterableIterator<number>;
    values(): IterableIterator<number>;
}

interface TestInterfaceJS {
    anySequenceLength(seq: Iterable<any>): number;
    objectSequenceLength(seq: Iterable<any>): number;
    testSequenceOverload(arg: Iterable<string>): void;
    testSequenceUnion(arg: Iterable<string> | string): void;
}

interface TestInterfaceMaplike extends Map<string, number> {
}

interface TestInterfaceMaplikeJSObject extends ReadonlyMap<string, any> {
}

interface TestInterfaceMaplikeObject extends ReadonlyMap<string, TestInterfaceMaplike> {
}

interface TestInterfaceSetlike extends Set<string> {
}

interface TestInterfaceSetlikeNode extends Set<Node> {
}

interface TestReflectedHTMLAttribute {
    setReflectedHTMLAttributeValue(seq: Iterable<Element>): void;
}

interface TextTrackCueList {
    [Symbol.iterator](): IterableIterator<VTTCue>;
}

interface TextTrackList {
    [Symbol.iterator](): IterableIterator<TextTrack>;
}

interface TouchList {
    [Symbol.iterator](): IterableIterator<Touch>;
}

interface TreeColumns {
    [Symbol.iterator](): IterableIterator<TreeColumn>;
}

interface URLSearchParams {
    [Symbol.iterator](): IterableIterator<[string, string]>;
    entries(): IterableIterator<[string, string]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string>;
}

interface VRDisplay {
    requestPresent(layers: Iterable<VRLayer>): Promise<void>;
}

interface VideoTrackList {
    [Symbol.iterator](): IterableIterator<VideoTrack>;
}

interface WEBGL_draw_buffers {
    drawBuffersWEBGL(buffers: Iterable<GLenum>): void;
}

interface WebGL2RenderingContextBase {
    clearBufferfv(buffer: GLenum, drawbuffer: GLint, values: Iterable<GLfloat>, srcOffset?: GLuint): void;
    clearBufferiv(buffer: GLenum, drawbuffer: GLint, values: Iterable<GLint>, srcOffset?: GLuint): void;
    clearBufferuiv(buffer: GLenum, drawbuffer: GLint, values: Iterable<GLuint>, srcOffset?: GLuint): void;
    drawBuffers(buffers: Iterable<GLenum>): void;
    getActiveUniforms(program: WebGLProgram, uniformIndices: Iterable<GLuint>, pname: GLenum): any;
    getUniformIndices(program: WebGLProgram, uniformNames: Iterable<string>): Iterable<GLuint> | null;
    invalidateFramebuffer(target: GLenum, attachments: Iterable<GLenum>): void;
    invalidateSubFramebuffer(target: GLenum, attachments: Iterable<GLenum>, x: GLint, y: GLint, width: GLsizei, height: GLsizei): void;
    transformFeedbackVaryings(program: WebGLProgram, varyings: Iterable<string>, bufferMode: GLenum): void;
    uniform1fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform1iv(location: WebGLUniformLocation | null, data: Iterable<GLint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform1uiv(location: WebGLUniformLocation | null, data: Iterable<GLuint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2iv(location: WebGLUniformLocation | null, data: Iterable<GLint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform2uiv(location: WebGLUniformLocation | null, data: Iterable<GLuint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3iv(location: WebGLUniformLocation | null, data: Iterable<GLint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform3uiv(location: WebGLUniformLocation | null, data: Iterable<GLuint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4iv(location: WebGLUniformLocation | null, data: Iterable<GLint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniform4uiv(location: WebGLUniformLocation | null, data: Iterable<GLuint>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix2x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix2x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix3x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    uniformMatrix4x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>, srcOffset?: GLuint, srcLength?: GLuint): void;
    vertexAttribI4iv(index: GLuint, values: Iterable<GLint>): void;
    vertexAttribI4uiv(index: GLuint, values: Iterable<GLuint>): void;
}

interface WebGLRenderingContext {
    uniform1fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>): void;
    uniform1iv(location: WebGLUniformLocation | null, data: Iterable<GLint>): void;
    uniform2fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>): void;
    uniform2iv(location: WebGLUniformLocation | null, data: Iterable<GLint>): void;
    uniform3fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>): void;
    uniform3iv(location: WebGLUniformLocation | null, data: Iterable<GLint>): void;
    uniform4fv(location: WebGLUniformLocation | null, data: Iterable<GLfloat>): void;
    uniform4iv(location: WebGLUniformLocation | null, data: Iterable<GLint>): void;
    uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>): void;
    uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>): void;
    uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Iterable<GLfloat>): void;
}

interface WebGLRenderingContextBase {
    vertexAttrib1fv(indx: GLuint, values: Iterable<GLfloat>): void;
    vertexAttrib2fv(indx: GLuint, values: Iterable<GLfloat>): void;
    vertexAttrib3fv(indx: GLuint, values: Iterable<GLfloat>): void;
    vertexAttrib4fv(indx: GLuint, values: Iterable<GLfloat>): void;
}

interface WebSocket {
    createServerWebSocket(url: string, protocols: Iterable<string>, transportProvider: nsITransportProvider, negotiatedExtensions: string): WebSocket;
}

interface Window {
    postMessage(message: any, targetOrigin: string, transfer?: Iterable<any>): void;
    setScrollMarks(marks: Iterable<number>, onHorizontalScrollbar?: boolean): void;
}

interface Worker {
    postMessage(message: any, transfer: Iterable<any>): void;
}

interface XRInputSourceArray {
    [Symbol.iterator](): IterableIterator<XRInputSource>;
    entries(): IterableIterator<[number, XRInputSource]>;
    keys(): IterableIterator<number>;
    values(): IterableIterator<XRInputSource>;
}

interface XSLTProcessor {
    setParameter(namespaceURI: string | null, localName: string, value: Iterable<Node>): void;
}

/////////////////////////////
/// Window Async Iterable APIs
/////////////////////////////

interface FileSystemDirectoryHandle {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, FileSystemHandle]>;
    entries(): AsyncIterableIterator<[string, FileSystemHandle]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<FileSystemHandle>;
}

interface ReadableStream {
    [Symbol.asyncIterator](options?: ReadableStreamIteratorOptions): AsyncIterableIterator<any>;
    values(options?: ReadableStreamIteratorOptions): AsyncIterableIterator<any>;
}

interface TestInterfaceAsyncIterableDouble {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, string]>;
    entries(): AsyncIterableIterator<[string, string]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<string>;
}

interface TestInterfaceAsyncIterableDoubleUnion {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, string | number]>;
    entries(): AsyncIterableIterator<[string, string | number]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<string | number>;
}

interface TestInterfaceAsyncIterableSingle {
    [Symbol.asyncIterator](): AsyncIterableIterator<number>;
    values(): AsyncIterableIterator<number>;
}

interface TestInterfaceAsyncIterableSingleWithArgs {
    [Symbol.asyncIterator](options?: TestInterfaceAsyncIteratorOptions): AsyncIterableIterator<number>;
    values(options?: TestInterfaceAsyncIteratorOptions): AsyncIterableIterator<number>;
}
