/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://immersive-web.github.io/webxr/
 */


[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRSystem : EventTarget {
  // Methods
  [Throws]
  Promise<boolean> isSessionSupported(XRSessionMode mode);
  [NewObject, NeedsCallerType]
  Promise<XRSession> requestSession(XRSessionMode mode, optional XRSessionInit options = {});

  // Events
  attribute EventHandler ondevicechange;
};

enum XRSessionMode {
  "inline",
  "immersive-vr",
  "immersive-ar",
};

dictionary XRSessionInit {
  sequence<any> requiredFeatures;
  sequence<any> optionalFeatures;
};

enum XRVisibilityState {
  "visible",
  "visible-blurred",
  "hidden",
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRSession : EventTarget {
  // Attributes
  readonly attribute XRVisibilityState visibilityState;
  [SameObject] readonly attribute XRRenderState renderState;
  [SameObject] readonly attribute XRInputSourceArray inputSources;

  // Methods
  [Throws]
  void updateRenderState(optional XRRenderStateInit state = {});
  [NewObject]
  Promise<XRReferenceSpace> requestReferenceSpace(XRReferenceSpaceType type);

  [Throws]
  long requestAnimationFrame(XRFrameRequestCallback callback);
  [Throws]
  void cancelAnimationFrame(long handle);

  [Throws]
  Promise<void> end();

  // Events
  attribute EventHandler onend;
  attribute EventHandler oninputsourceschange;
  attribute EventHandler onselect;
  attribute EventHandler onselectstart;
  attribute EventHandler onselectend;
  attribute EventHandler onsqueeze;
  attribute EventHandler onsqueezestart;
  attribute EventHandler onsqueezeend;
  attribute EventHandler onvisibilitychange;
};

dictionary XRRenderStateInit {
  double depthNear;
  double depthFar;
  double inlineVerticalFieldOfView;
  XRWebGLLayer? baseLayer;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRRenderState {
  readonly attribute double depthNear;
  readonly attribute double depthFar;
  readonly attribute double? inlineVerticalFieldOfView;
  readonly attribute XRWebGLLayer? baseLayer;
};

callback XRFrameRequestCallback = void (DOMHighResTimeStamp time, XRFrame frame);

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRFrame {
  [SameObject] readonly attribute XRSession session;

  [Throws] XRViewerPose? getViewerPose(XRReferenceSpace referenceSpace);
  [Throws] XRPose? getPose(XRSpace space, XRSpace baseSpace);
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRSpace : EventTarget {

};

enum XRReferenceSpaceType {
  "viewer",
  "local",
  "local-floor",
  "bounded-floor",
  "unbounded"
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRReferenceSpace : XRSpace {
  [NewObject]
  XRReferenceSpace getOffsetReferenceSpace(XRRigidTransform originOffset);

  attribute EventHandler onreset;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRBoundedReferenceSpace : XRReferenceSpace {
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [Frozen, Cached, Pure]
  readonly attribute sequence<DOMPointReadOnly> boundsGeometry;
};

enum XREye {
  "none",
  "left",
  "right"
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRView {
  readonly attribute XREye eye;
  [Throws]
  readonly attribute Float32Array projectionMatrix;
  [Throws, SameObject]
  readonly attribute XRRigidTransform transform;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRViewport {
  readonly attribute long x;
  readonly attribute long y;
  readonly attribute long width;
  readonly attribute long height;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRRigidTransform {
  [Throws]
  constructor(optional DOMPointInit position = {}, optional DOMPointInit orientation = {});
  [SameObject] readonly attribute DOMPointReadOnly position;
  [SameObject] readonly attribute DOMPointReadOnly orientation;
  [Throws]
  readonly attribute Float32Array matrix;
  [SameObject] readonly attribute XRRigidTransform inverse;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRPose {
  [SameObject] readonly attribute XRRigidTransform transform;
  readonly attribute boolean emulatedPosition;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRViewerPose : XRPose {
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [Constant, Cached, Frozen]
  readonly attribute sequence<XRView> views;
};

enum XRHandedness {
  "none",
  "left",
  "right"
};

enum XRTargetRayMode {
  "gaze",
  "tracked-pointer",
  "screen"
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRInputSource {
  readonly attribute XRHandedness handedness;
  readonly attribute XRTargetRayMode targetRayMode;
  [SameObject] readonly attribute XRSpace targetRaySpace;
  [SameObject] readonly attribute XRSpace? gripSpace;
  // TODO: Use FrozenArray once available. (Bug 1236777)
  [Constant, Cached, Frozen]
  readonly attribute sequence<DOMString> profiles;
  // https://immersive-web.github.io/webxr-gamepads-module/
  [SameObject] readonly attribute Gamepad? gamepad;
};


[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRInputSourceArray {
  iterable<XRInputSource>;
  readonly attribute unsigned long length;
  getter XRInputSource(unsigned long index);
};

typedef (WebGLRenderingContext or
         WebGL2RenderingContext) XRWebGLRenderingContext;

dictionary XRWebGLLayerInit {
  boolean antialias = true;
  boolean depth = true;
  boolean stencil = false;
  boolean alpha = true;
  boolean ignoreDepthValues = false;
  double framebufferScaleFactor = 1.0;
};

[Pref="dom.vr.webxr.enabled", SecureContext, Exposed=Window]
interface XRWebGLLayer {
  [Throws]
  constructor(XRSession session,
              XRWebGLRenderingContext context,
              optional XRWebGLLayerInit layerInit = {});
  // Attributes
  readonly attribute boolean antialias;
  readonly attribute boolean ignoreDepthValues;

  [SameObject] readonly attribute WebGLFramebuffer? framebuffer;
  readonly attribute unsigned long framebufferWidth;
  readonly attribute unsigned long framebufferHeight;

  // Methods
  XRViewport? getViewport(XRView view);

  // Static Methods
  static double getNativeFramebufferScaleFactor(XRSession session);
};
