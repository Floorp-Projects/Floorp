// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IIRFilter_h
#define IIRFilter_h

typedef nsTArray<double> AudioDoubleArray;

namespace blink {

class IIRFilter final {
public:
    // The maximum IIR filter order.  This also limits the number of feedforward coefficients.  The
    // maximum number of coefficients is 20 according to the spec.
    const static size_t kMaxOrder = 19;
    IIRFilter(const AudioDoubleArray* feedforwardCoef, const AudioDoubleArray* feedbackCoef);
    ~IIRFilter();

    void process(const float* sourceP, float* destP, size_t framesToProcess);

    void reset();

    void getFrequencyResponse(int nFrequencies,
        const float* frequency,
        float* magResponse,
        float* phaseResponse);

    bool buffersAreZero();

private:
    // Filter memory
    //
    // For simplicity, we assume |m_xBuffer| and |m_yBuffer| have the same length, and the length is
    // a power of two.  Since the number of coefficients has a fixed upper length, the size of
    // xBuffer and yBuffer is fixed. |m_xBuffer| holds the old input values and |m_yBuffer| holds
    // the old output values needed to compute the new output value.
    //
    // m_yBuffer[m_bufferIndex] holds the most recent output value, say, y[n].  Then
    // m_yBuffer[m_bufferIndex - k] is y[n - k].  Similarly for m_xBuffer.
    //
    // To minimize roundoff, these arrays are double's instead of floats.
    AudioDoubleArray m_xBuffer;
    AudioDoubleArray m_yBuffer;

    // Index into the xBuffer and yBuffer arrays where the most current x and y values should be
    // stored.  xBuffer[bufferIndex] corresponds to x[n], the current x input value and
    // yBuffer[bufferIndex] is where y[n], the current output value.
    int m_bufferIndex;

    // Coefficients of the IIR filter.
    const AudioDoubleArray* m_feedback;
    const AudioDoubleArray* m_feedforward;
};

} // namespace blink

#endif // IIRFilter_h
