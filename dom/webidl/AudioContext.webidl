/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

callback DecodeSuccessCallback = void (AudioBuffer decodedData);
callback DecodeErrorCallback = void ();

[Constructor, PrefControlled]
interface AudioContext : EventTarget {

    readonly attribute AudioDestinationNode destination;
    readonly attribute float sampleRate;
    readonly attribute double currentTime;
    readonly attribute AudioListener listener;

    [Creator, Throws]
    AudioBuffer createBuffer(unsigned long numberOfChannels, unsigned long length, float sampleRate);

    [Creator, Throws]
    AudioBuffer? createBuffer(ArrayBuffer buffer, boolean mixToMono);

    void decodeAudioData(ArrayBuffer audioData,
                         DecodeSuccessCallback successCallback,
                         optional DecodeErrorCallback errorCallback);

    // AudioNode creation 
    [Creator]
    AudioBufferSourceNode createBufferSource();

    [Creator]
    MediaStreamAudioDestinationNode createMediaStreamDestination();

    [Creator, Throws]
    ScriptProcessorNode createScriptProcessor(optional unsigned long bufferSize = 0,
                                              optional unsigned long numberOfInputChannels = 2,
                                              optional unsigned long numberOfOutputChannels = 2);

    [Creator]
    AnalyserNode createAnalyser();
    [Creator]
    GainNode createGain();
    [Creator, Throws]
    DelayNode createDelay(optional double maxDelayTime = 1);
    [Creator]
    BiquadFilterNode createBiquadFilter();
    [Creator]
    WaveShaperNode createWaveShaper();
    [Creator]
    PannerNode createPanner();
    [Creator]
    ConvolverNode createConvolver();

    [Creator, Throws]
    ChannelSplitterNode createChannelSplitter(optional unsigned long numberOfOutputs = 6);
    [Creator, Throws]
    ChannelMergerNode createChannelMerger(optional unsigned long numberOfInputs = 6);

    [Creator]
    DynamicsCompressorNode createDynamicsCompressor();

    [Creator, Throws]
    WaveTable createWaveTable(Float32Array real, Float32Array imag);

};

/*
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/audio/raw-file/tip/webaudio/specification.html#AlternateNames
 */
[PrefControlled]
partial interface AudioContext {
    // Same as createGain()
    [Creator]
    GainNode createGainNode();
    
    // Same as createDelay()
    [Creator, Throws]
    DelayNode createDelayNode(optional double maxDelayTime = 1);

    // Same as createScriptProcessor()
    [Creator, Throws]
    ScriptProcessorNode createJavaScriptNode(optional unsigned long bufferSize = 0,
                                             optional unsigned long numberOfInputChannels = 2,
                                             optional unsigned long numberOfOutputChannels = 2);
};


