/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#the-workerglobalscope-common-interface
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and Opera
 * Software ASA.
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Global=(Worker,DedicatedWorker),
 Exposed=DedicatedWorker]
interface DedicatedWorkerGlobalScope : WorkerGlobalScope {
  [Replaceable]
  readonly attribute DOMString name;

  [Throws]
  undefined postMessage(any message, sequence<object> transfer);
  [Throws]
  undefined postMessage(any message, optional StructuredSerializeOptions options = {});

  undefined close();

  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

// https://w3c.github.io/webrtc-encoded-transform/#RTCEncodedAudioFrame-methods
partial interface DedicatedWorkerGlobalScope {
  [Pref="media.peerconnection.enabled",
   Pref="media.peerconnection.scripttransform.enabled"] attribute EventHandler onrtctransform;
};

// https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#animation-frames
DedicatedWorkerGlobalScope includes AnimationFrameProvider;
